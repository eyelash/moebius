#pragma once

#include "ast.hpp"

// type checking and monomorphization
class Pass1: public Visitor<Expression*> {
	static std::ptrdiff_t compare(const Type* type1, const Type* type2) {
		const int id1 = type1->get_id();
		const int id2 = type2->get_id();
		if (id1 != id2) {
			return id1 - id2;
		}
		if (id1 == ClosureType::id) {
			const ClosureType* closure_type1 = static_cast<const ClosureType*>(type1);
			const ClosureType* closure_type2 = static_cast<const ClosureType*>(type2);
			if (closure_type1->get_function() != closure_type2->get_function()) {
				return closure_type1->get_function() - closure_type2->get_function();
			}
			return compare(closure_type1->get_environment_types(), closure_type2->get_environment_types());
		}
		return 0;
	}
	static std::ptrdiff_t compare(const std::vector<const Type*>& types1, const std::vector<const Type*>& types2) {
		if (types1.size() != types2.size()) {
			return types1.size() - types2.size();
		}
		for (std::size_t i = 0; i < types1.size(); ++i) {
			if (std::ptrdiff_t diff = compare(types1[i], types2[i])) {
				return diff;
			}
		}
		return 0;
	}
	static StringView print_type(const Type* type) {
		switch (type->get_id()) {
			case NumberType::id: return "Number";
			case ClosureType::id: return "Function";
			case VoidType::id: return "Void";
			default: return StringView();
		}
	}
	template <class T> [[noreturn]] void error(const Expression& expression, const T& t) {
		FilePrinter printer(stderr);
		expression.get_position().print_error(printer, t);
		std::exit(EXIT_FAILURE);
	}
	template <class T> [[noreturn]] void error(const T& t) {
		FilePrinter printer(stderr);
		printer.print(bold(red("error: ")));
		printer.print(t);
		printer.print('\n');
		std::exit(EXIT_FAILURE);
	}
	struct FunctionTableEntry {
		const Function* old_function;
		std::vector<const Type*> argument_types;
		const Function* new_function = nullptr;
		FunctionTableEntry(const Function* old_function, const std::vector<const Type*>& argument_types): old_function(old_function), argument_types(argument_types) {}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::size_t recursion = -1;
		std::size_t look_up(const Function* old_function, const std::vector<const Type*>& argument_types) const {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				const FunctionTableEntry& entry = functions[index];
				if (entry.old_function == old_function && compare(entry.argument_types, argument_types) == 0) {
					return index;
				}
			}
			return index;
		}
		void add_entry(const Function* old_function, const std::vector<const Type*>& argument_types) {
			functions.emplace_back(old_function, argument_types);
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
		void clear(std::size_t index) {
			functions.erase(functions.begin() + index + 1, functions.end());
		}
	};
	Program* program;
	FunctionTable& function_table;
	std::size_t index;
	std::map<const Expression*, const Expression*> cache;
	Pass1(Program* program, FunctionTable& function_table, std::size_t index): program(program), function_table(function_table), index(index) {}
	void evaluate(Block* destination_block, const Block& source_block) {
		for (const Expression* expression: source_block) {
			Expression* result = visit(*this, expression);
			cache[expression] = result;
			destination_block->add_expression(result);
		}
	}
public:
	Expression* visit_number(const Number& number) override {
		return new Number(number.get_value());
	}
	Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = cache[binary_expression.get_left()];
		const Expression* right = cache[binary_expression.get_right()];
		if ((left->get_type_id() == NumberType::id || left->get_type_id() == NeverType::id) && (right->get_type_id() == NumberType::id || right->get_type_id() == NeverType::id)) {
			return new BinaryExpression(binary_expression.get_operation(), left, right);
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	Expression* visit_if(const If& if_) override {
		const Expression* condition = cache[if_.get_condition()];
		if (condition->get_type_id() == NumberType::id || condition->get_type_id() == NeverType::id) {
			If* new_if = new If(condition);
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			const Expression* then_expression = cache[if_.get_then_expression()];
			const Expression* else_expression = cache[if_.get_else_expression()];
			new_if->set_then_expression(then_expression);
			new_if->set_else_expression(else_expression);
			if (compare(then_expression->get_type(), else_expression->get_type()) == 0) {
				new_if->set_type(then_expression->get_type());
			}
			else if (then_expression->get_type_id() == NeverType::id) {
				new_if->set_type(else_expression->get_type());
			}
			else if (else_expression->get_type_id() == NeverType::id) {
				new_if->set_type(then_expression->get_type());
			}
			else {
				error(if_, "if and else branches must return values of the same type");
			}
			return new_if;
		}
		else {
			error(if_, "type of condition must be a number");
		}
	}
	Expression* visit_closure(const Closure& closure) override {
		ClosureType* type = new ClosureType(closure.get_function());
		Closure* new_closure = new Closure(nullptr, type);
		for (const Expression* expression: closure.get_environment_expressions()) {
			const Expression* new_expression = cache[expression];
			type->add_environment_type(new_expression->get_type());
			new_closure->add_environment_expression(new_expression);
		}
		return new_closure;
	}
	Expression* visit_closure_access(const ClosureAccess& closure_access) override {
		const std::size_t argument_index = closure_access.get_index();
		const Expression* closure = cache[closure_access.get_closure()];
		const ClosureType* closure_type = static_cast<const ClosureType*>(closure->get_type());
		const Type* type = closure_type->get_environment_types()[argument_index];
		return new ClosureAccess(closure, argument_index, type);
	}
	Expression* visit_argument(const Argument& argument) override {
		const std::size_t argument_index = argument.get_index();
		const Type* type = function_table[index].argument_types[argument_index];
		return new Argument(argument_index, type);
	}
	Expression* visit_call(const Call& call) override {
		Call* new_call = new Call();
		std::vector<const Type*> argument_types;
		for (const Expression* argument: call.get_arguments()) {
			const Expression* new_argument = cache[argument];
			argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		const Expression* object = new_call->get_object();
		if (object->get_type_id() != ClosureType::id) {
			error("call to a value that is not a function");
		}
		const Function* old_function = static_cast<const ClosureType*>(object->get_type())->get_function();
		if (call.get_arguments().size() != old_function->get_arguments()) {
			error(format("call with % arguments to a function that accepts % arguments", print_number(call.get_arguments().size() - 1), print_number(old_function->get_arguments() - 1)));
		}

		const std::size_t new_index = function_table.look_up(old_function, argument_types);
		if (new_index == function_table.size()) {
			function_table.add_entry(old_function, argument_types);
			Pass1 pass1(program, function_table, new_index);
			Function* new_function = new Function();
			program->add_function(new_function);
			new_function->add_argument_types(argument_types);
			pass1.evaluate(new_function->get_block(), old_function->get_block());
			const Expression* new_expression = pass1.cache[old_function->get_expression()];
			new_function->set_expression(new_expression);
			new_function->set_return_type(new_expression->get_type());
			function_table[new_index].new_function = new_function;
			if (function_table.recursion == new_index) {
				// reevaluate the expression in case of recursion
				function_table.recursion = -1;
				function_table.clear(new_index);
				new_function->get_block()->clear();
				pass1.evaluate(new_function->get_block(), old_function->get_block());
				new_expression = pass1.cache[old_function->get_expression()];
				new_function->set_expression(new_expression);
				new_function->set_return_type(new_expression->get_type());
			}
			new_call->set_type(new_expression->get_type());
			new_call->set_function(new_function);
		}
		else {
			// detect recursion
			if (function_table[new_index].new_function == nullptr) {
				if (new_index < function_table.recursion) {
					function_table.recursion = new_index;
				}
				new_call->set_type(NeverType::get());
			}
			else {
				new_call->set_type(function_table[new_index].new_function->get_return_type());
				new_call->set_function(function_table[new_index].new_function);
			}
		}
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name(), intrinsic.get_type());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(cache[argument]);
		}
		if (intrinsic.name_equals("putChar")) {
			if (new_intrinsic->get_arguments().size() != 1) {
				error("putChar takes exactly 1 argument");
			}
			if (new_intrinsic->get_arguments()[0]->get_type_id() != NumberType::id) {
				error("argument of putChar must be a number");
			}
			new_intrinsic->set_type(VoidType::get());
		}
		else if (intrinsic.name_equals("getChar")) {
			if (new_intrinsic->get_arguments().size() != 0) {
				error("getChar takes no argument");
			}
			new_intrinsic->set_type(NumberType::get());
		}
		return new_intrinsic;
	}
	Expression* visit_bind(const Bind& bind) override {
		const Expression* left = cache[bind.get_left()];
		const Expression* right = cache[bind.get_right()];
		return new Bind(left, right);
	}
	static const Program* run(const Function* main_function) {
		Program* new_program = new Program();
		FunctionTable function_table;
		Pass1 pass1(new_program, function_table, 0);
		Function* new_function = new Function(VoidType::get());
		new_program->add_function(new_function);
		pass1.evaluate(new_function->get_block(), main_function->get_block());
		new_function->set_expression(pass1.cache[main_function->get_expression()]);
		return new_program;
	}
};

// inlining
class Pass2 {
	struct FunctionTableEntry {
		const Function* function;
		const Function* new_function = nullptr;
		std::size_t callers = 0;
		FunctionTableEntry(const Function* function): function(function) {}
		bool should_inline() const {
			return callers == 1;
		}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::size_t look_up(const Function* function) const {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				if (functions[index].function == function) {
					return index;
				}
			}
			return index;
		}
		void add_entry(const Function* function) {
			functions.emplace_back(function);
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
	};
	class Analyze: public Visitor<void> {
		FunctionTable& function_table;
	public:
		Analyze(FunctionTable& function_table): function_table(function_table) {}
		void evaluate(const Block& block) {
			for (const Expression* expression: block) {
				visit(*this, expression);
			}
		}
		void visit_number(const Number& number) override {}
		void visit_binary_expression(const BinaryExpression& binary_expression) override {}
		void visit_if(const If& if_) override {
			evaluate(if_.get_then_block());
			evaluate(if_.get_else_block());
		}
		void visit_closure(const Closure& closure) override {}
		void visit_closure_access(const ClosureAccess& closure_access) override {}
		void visit_argument(const Argument& argument) override {}
		void visit_call(const Call& call) override {
			const std::size_t new_index = function_table.look_up(call.get_function());
			if (new_index == function_table.size()) {
				function_table.add_entry(call.get_function());
				evaluate(call.get_function()->get_block());
			}
			++function_table[new_index].callers;
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {}
		void visit_bind(const Bind& bind) override {}
	};
	class Replace: public Visitor<Expression*> {
		Program* program;
		Block* current_block;
		FunctionTable& function_table;
		std::size_t index;
		std::vector<Expression*> arguments;
		std::map<const Expression*, Expression*> cache;
		template <class T, class... A> T* create(A&&... arguments) {
			T* expression = new T(std::forward<A>(arguments)...);
			current_block->add_expression(expression);
			return expression;
		}
	public:
		Replace(Program* program, FunctionTable& function_table, std::size_t index): program(program), function_table(function_table), index(index) {}
		void evaluate(Block* destination_block, const Block& source_block) {
			Block* previous_block = current_block;
			current_block = destination_block;
			for (const Expression* expression: source_block) {
				cache[expression] = visit(*this, expression);
			}
			destination_block->set_result(cache[source_block.get_result()]);
			current_block = previous_block;
		}
		Expression* visit_number(const Number& number) override {
			return create<Number>(number.get_value());
		}
		Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
			const Expression* left = cache[binary_expression.get_left()];
			const Expression* right = cache[binary_expression.get_right()];
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		Expression* visit_if(const If& if_) override {
			const Expression* condition = cache[if_.get_condition()];
			If* new_if = create<If>(condition, if_.get_type());
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			return new_if;
		}
		Expression* visit_closure(const Closure& closure) override {
			Closure* new_closure = create<Closure>(nullptr, closure.get_type());
			for (const Expression* expression: closure.get_environment_expressions()) {
				new_closure->add_environment_expression(cache[expression]);
			}
			return new_closure;
		}
		Expression* visit_closure_access(const ClosureAccess& closure_access) override {
			const Expression* closure = cache[closure_access.get_closure()];
			return create<ClosureAccess>(closure, closure_access.get_index(), closure_access.get_type());
		}
		Expression* visit_argument(const Argument& argument) override {
			if (function_table[index].should_inline()) {
				return arguments[argument.get_index()];
			}
			else {
				return create<Argument>(argument.get_index(), argument.get_type());
			}
		}
		Expression* visit_call(const Call& call) override {
			const std::size_t new_index = function_table.look_up(call.get_function());
			if (function_table[new_index].should_inline()) {
				Replace replace(program, function_table, new_index);
				for (const Expression* argument: call.get_arguments()) {
					replace.arguments.push_back(cache[argument]);
				}
				replace.evaluate(current_block, call.get_function()->get_block());
				return replace.cache[call.get_function()->get_expression()];
			}
			else {
				Call* new_call = create<Call>();
				for (const Expression* argument: call.get_arguments()) {
					new_call->add_argument(cache[argument]);
				}
				if (function_table[new_index].new_function == nullptr) {
					Function* new_function = new Function(call.get_function()->get_return_type());
					program->add_function(new_function);
					new_function->add_argument_types(call.get_function()->get_argument_types());
					function_table[new_index].new_function = new_function;
					Replace replace(program, function_table, new_index);
					replace.evaluate(new_function->get_block(), call.get_function()->get_block());
				}
				new_call->set_type(function_table[new_index].new_function->get_return_type());
				new_call->set_function(function_table[new_index].new_function);
				return new_call;
			}
		}
		Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
			Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), intrinsic.get_type());
			for (const Expression* argument: intrinsic.get_arguments()) {
				new_intrinsic->add_argument(cache[argument]);
			}
			return new_intrinsic;
		}
		Expression* visit_bind(const Bind& bind) override {
			const Expression* left = cache[bind.get_left()];
			const Expression* right = cache[bind.get_right()];
			return create<Bind>(left, right);
		}
	};
public:
	Pass2() = delete;
	static const Program* run(const Program& program) {
		const Function* main_function = program.get_main_function();
		Program* new_program = new Program();
		FunctionTable function_table;
		Analyze analyze(function_table);
		analyze.evaluate(main_function->get_block());
		Replace replace(new_program, function_table, 0);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		replace.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};
