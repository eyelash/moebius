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
		if (id1 == TupleType::id) {
			const TupleType* tuple_type1 = static_cast<const TupleType*>(type1);
			const TupleType* tuple_type2 = static_cast<const TupleType*>(type2);
			return compare(tuple_type1->get_types(), tuple_type2->get_types());
		}
		if (id1 == ClosureType::id) {
			const ClosureType* closure_type1 = static_cast<const ClosureType*>(type1);
			const ClosureType* closure_type2 = static_cast<const ClosureType*>(type2);
			if (closure_type1->get_function() != closure_type2->get_function()) {
				return closure_type1->get_function() - closure_type2->get_function();
			}
			return compare(closure_type1->get_environment_types(), closure_type2->get_environment_types());
		}
		if (id1 == StructType::id) {
			const StructType* struct_type1 = static_cast<const StructType*>(type1);
			const StructType* struct_type2 = static_cast<const StructType*>(type2);
			if (std::ptrdiff_t diff = compare(struct_type1->get_field_names(), struct_type2->get_field_names())) {
				return diff;
			}
			return compare(struct_type1->get_field_types(), struct_type2->get_field_types());
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
	static std::ptrdiff_t compare(const std::vector<std::string>& strings1, const std::vector<std::string>& strings2) {
		if (strings1.size() != strings2.size()) {
			return strings1.size() - strings2.size();
		}
		for (std::size_t i = 0; i < strings1.size(); ++i) {
			if (int diff = strings1[i].compare(strings2[i])) {
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
	struct FunctionTableKey {
		const Function* old_function;
		std::vector<const Type*> argument_types;
		FunctionTableKey(const Function* old_function, const std::vector<const Type*>& argument_types): old_function(old_function), argument_types(argument_types) {}
		FunctionTableKey(const Function* old_function): old_function(old_function) {}
		FunctionTableKey() {}
		bool operator <(const FunctionTableKey& rhs) const {
			if (old_function != rhs.old_function) {
				return old_function < rhs.old_function;
			}
			return compare(argument_types, rhs.argument_types) < 0;
		}
	};
	struct FunctionTableValue {
		const Function* new_function = nullptr;
		std::size_t pass = 0;
	};
	class FunctionTable {
		std::map<FunctionTableKey, FunctionTableValue> functions;
	public:
		bool recursion = true;
		std::size_t pass = 0;
		FunctionTableValue& operator [](const FunctionTableKey& key) {
			return functions[key];
		}
	};
	Program* program;
	FunctionTable& function_table;
	const FunctionTableKey& key;
	std::map<const Expression*, const Expression*> cache;
	Pass1(Program* program, FunctionTable& function_table, const FunctionTableKey& key): program(program), function_table(function_table), key(key) {}
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
	Expression* visit_tuple(const Tuple& tuple) override {
		TupleType* type = new TupleType();
		Tuple* new_tuple = new Tuple(type);
		for (const Expression* expression: tuple.get_expressions()) {
			const Expression* new_expression = cache[expression];
			type->add_type(new_expression->get_type());
			new_tuple->add_expression(new_expression);
		}
		return new_tuple;
	}
	Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const std::size_t argument_index = tuple_access.get_index();
		const Expression* tuple = cache[tuple_access.get_tuple()];
		const TupleType* tuple_type = static_cast<const TupleType*>(tuple->get_type());
		const Type* type = tuple_type->get_types()[argument_index];
		return new TupleAccess(tuple, argument_index, type);
	}
	Expression* visit_struct(const Struct& struct_) override {
		StructType* type = new StructType();
		Struct* new_struct = new Struct(type);
		for (std::size_t i = 0; i < struct_.get_expressions().size(); ++i) {
			const std::string& name = struct_.get_names()[i];
			const Expression* new_expression = cache[struct_.get_expressions()[i]];
			type->add_field(name, new_expression->get_type());
			new_struct->add_field(name, new_expression);
		}
		return new_struct;
	}
	Expression* visit_struct_access(const StructAccess& struct_access) override {
		const Expression* struct_ = cache[struct_access.get_struct()];
		if (struct_->get_type_id() != StructType::id) {
			error(struct_access, "struct access to non-struct");
		}
		const StructType* struct_type = static_cast<const StructType*>(struct_->get_type());
		if (!struct_type->has_field(struct_access.get_name())) {
			error(struct_access, "struct has no such field");
		}
		const std::size_t index = struct_type->get_index(struct_access.get_name());
		const Type* type = struct_type->get_field_types()[index];
		return new StructAccess(struct_, struct_access.get_name(), type);
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
		const Type* type = key.argument_types[argument_index];
		return new Argument(argument_index, type);
	}
	Expression* visit_call(const Call& call) override {
		Call* new_call = new Call();
		FunctionTableKey new_key;
		for (const Expression* argument: call.get_arguments()) {
			const Expression* new_argument = cache[argument];
			new_key.argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		const Expression* object = new_call->get_object();
		const Function* old_function = call.get_function();
		if (old_function == nullptr) {
			if (object->get_type_id() != ClosureType::id) {
				error(call, "call to a value that is not a function");
			}
			old_function = static_cast<const ClosureType*>(object->get_type())->get_function();
		}
		new_key.old_function = old_function;
		if (call.get_arguments().size() != old_function->get_arguments()) {
			error(call, format("call with % arguments to a function that accepts % arguments", print_number(call.get_arguments().size() - 1), print_number(old_function->get_arguments() - 1)));
		}

		if (function_table[new_key].pass < function_table.pass) {
			function_table[new_key].pass = function_table.pass;
			Pass1 pass1(program, function_table, new_key);
			Function* new_function = new Function(new_key.argument_types);
			program->add_function(new_function);
			if (function_table[new_key].new_function) {
				new_function->set_return_type(function_table[new_key].new_function->get_return_type());
			}
			else {
				new_function->set_return_type(NeverType::get());
			}
			function_table[new_key].new_function = new_function;
			pass1.evaluate(new_function->get_block(), old_function->get_block());
			const Expression* new_expression = pass1.cache[old_function->get_expression()];
			new_function->set_expression(new_expression);
			new_function->set_return_type(new_expression->get_type());
			new_call->set_type(new_expression->get_type());
			new_call->set_function(new_function);
		}
		else {
			// detect recursion
			if (function_table[new_key].new_function->get_return_type()->get_id() == NeverType::id) {
				function_table.recursion = true;
			}
			new_call->set_type(function_table[new_key].new_function->get_return_type());
			new_call->set_function(function_table[new_key].new_function);
		}
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(cache[argument]);
		}
		if (intrinsic.name_equals("putChar")) {
			if (new_intrinsic->get_arguments().size() != 1) {
				error(intrinsic, "putChar takes exactly 1 argument");
			}
			if (new_intrinsic->get_arguments()[0]->get_type_id() != NumberType::id) {
				error(intrinsic, "argument of putChar must be a number");
			}
			new_intrinsic->set_type(VoidType::get());
		}
		else if (intrinsic.name_equals("getChar")) {
			if (new_intrinsic->get_arguments().size() != 0) {
				error(intrinsic, "getChar takes no argument");
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
	static const Program* run(const Program& program) {
		const Function* main_function = program.get_main_function();
		Program* new_program;
		FunctionTable function_table;
		// TODO: prevent infinite loop
		while (function_table.recursion) {
			new_program = new Program();
			Pass1 pass1(new_program, function_table, FunctionTableKey(main_function));
			Function* new_function = new Function(VoidType::get());
			new_program->add_function(new_function);
			function_table.recursion = false;
			function_table.pass += 1;
			pass1.evaluate(new_function->get_block(), main_function->get_block());
			new_function->set_expression(pass1.cache[main_function->get_expression()]);
		}
		return new_program;
	}
};

class Lowering: public Visitor<Expression*> {
	Program* program;
	std::map<const Function*, const Function*>& function_table;
	std::map<const Expression*, Expression*> cache;
	void evaluate(Block* destination_block, const Block& source_block) {
		for (const Expression* expression: source_block) {
			Expression* result = visit(*this, expression);
			cache[expression] = result;
			destination_block->add_expression(result);
		}
		destination_block->set_result(cache[source_block.get_result()]);
	}
	Lowering(Program* program, std::map<const Function*, const Function*>& function_table): program(program), function_table(function_table) {}
public:
	Expression* visit_number(const Number& number) override {
		return new Number(number.get_value());
	}
	Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = cache[binary_expression.get_left()];
		const Expression* right = cache[binary_expression.get_right()];
		return new BinaryExpression(binary_expression.get_operation(), left, right);
	}
	Expression* visit_if(const If& if_) override {
		const Expression* condition = cache[if_.get_condition()];
		If* new_if = new If(condition, if_.get_type());
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	Expression* visit_tuple(const Tuple& tuple) override {
		Tuple* new_tuple = new Tuple();
		for (const Expression* expression: tuple.get_expressions()) {
			new_tuple->add_expression(cache[expression]);
		}
		return new_tuple;
	}
	Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const Expression* tuple = cache[tuple_access.get_tuple()];
		return new TupleAccess(tuple, tuple_access.get_index());
	}
	Expression* visit_struct(const Struct& struct_) override {
		Tuple* tuple = new Tuple();
		for (const Expression* expression: struct_.get_expressions()) {
			tuple->add_expression(cache[expression]);
		}
		return tuple;
	}
	Expression* visit_struct_access(const StructAccess& struct_access) override {
		const Expression* tuple = cache[struct_access.get_struct()];
		const StructType* struct_type = static_cast<const StructType*>(struct_access.get_struct()->get_type());
		const std::size_t index = struct_type->get_index(struct_access.get_name());
		return new TupleAccess(tuple, index);
	}
	Expression* visit_closure(const Closure& closure) override {
		Tuple* tuple = new Tuple();
		for (const Expression* expression: closure.get_environment_expressions()) {
			tuple->add_expression(cache[expression]);
		}
		return tuple;
	}
	Expression* visit_closure_access(const ClosureAccess& closure_access) override {
		const Expression* tuple = cache[closure_access.get_closure()];
		return new TupleAccess(tuple, closure_access.get_index());
	}
	Expression* visit_argument(const Argument& argument) override {
		return new Argument(argument.get_index(), argument.get_type());
	}
	Expression* visit_call(const Call& call) override {
		Call* new_call = new Call();
		for (const Expression* argument: call.get_arguments()) {
			new_call->add_argument(cache[argument]);
		}
		if (function_table[call.get_function()] == nullptr) {
			Function* new_function = new Function(call.get_function()->get_argument_types(), call.get_function()->get_return_type());
			program->add_function(new_function);
			function_table[call.get_function()] = new_function;
			Lowering lowering(program, function_table);
			lowering.evaluate(new_function->get_block(), call.get_function()->get_block());
		}
		new_call->set_type(function_table[call.get_function()]->get_return_type());
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name(), intrinsic.get_type());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(cache[argument]);
		}
		return new_intrinsic;
	}
	Expression* visit_bind(const Bind& bind) override {
		const Expression* left = cache[bind.get_left()];
		const Expression* right = cache[bind.get_right()];
		return new Bind(left, right);
	}
	static const Program* run(const Program& program) {
		const Function* main_function = program.get_main_function();
		Program* new_program = new Program();
		std::map<const Function*, const Function*> function_table;
		Lowering lowering(new_program, function_table);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		lowering.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// inlining
class Pass2 {
	struct FunctionTableEntry {
		const Function* new_function = nullptr;
		std::size_t callers = 0;
		bool should_inline() const {
			return callers == 1;
		}
	};
	using FunctionTable = std::map<const Function*, FunctionTableEntry>;
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
		void visit_tuple(const Tuple& tuple) override {}
		void visit_tuple_access(const TupleAccess& tuple_access) override {}
		void visit_struct(const Struct& struct_) override {}
		void visit_struct_access(const StructAccess& struct_access) override {}
		void visit_closure(const Closure& closure) override {}
		void visit_closure_access(const ClosureAccess& closure_access) override {}
		void visit_argument(const Argument& argument) override {}
		void visit_call(const Call& call) override {
			if (function_table[call.get_function()].callers == 0) {
				function_table[call.get_function()].callers += 1;
				evaluate(call.get_function()->get_block());
			}
			else {
				function_table[call.get_function()].callers += 1;
			}
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {}
		void visit_bind(const Bind& bind) override {}
	};
	class Replace: public Visitor<Expression*> {
		Program* program;
		Block* current_block;
		FunctionTable& function_table;
		const Function* function;
		std::vector<Expression*> arguments;
		std::map<const Expression*, Expression*> cache;
		template <class T, class... A> T* create(A&&... arguments) {
			T* expression = new T(std::forward<A>(arguments)...);
			current_block->add_expression(expression);
			return expression;
		}
	public:
		Replace(Program* program, FunctionTable& function_table, const Function* function): program(program), function_table(function_table), function(function) {}
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
		Expression* visit_tuple(const Tuple& tuple) override {
			Tuple* new_tuple = create<Tuple>(tuple.get_type());
			for (const Expression* expression: tuple.get_expressions()) {
				new_tuple->add_expression(cache[expression]);
			}
			return new_tuple;
		}
		Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
			const Expression* tuple = cache[tuple_access.get_tuple()];
			return create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
		}
		Expression* visit_struct(const Struct& struct_) override {
			return nullptr;
		}
		Expression* visit_struct_access(const StructAccess& struct_access) override {
			return nullptr;
		}
		Expression* visit_closure(const Closure& closure) override {
			return nullptr;
		}
		Expression* visit_closure_access(const ClosureAccess& closure_access) override {
			return nullptr;
		}
		Expression* visit_argument(const Argument& argument) override {
			if (function_table[function].should_inline()) {
				return arguments[argument.get_index()];
			}
			else {
				return create<Argument>(argument.get_index(), argument.get_type());
			}
		}
		Expression* visit_call(const Call& call) override {
			if (function_table[call.get_function()].should_inline()) {
				Replace replace(program, function_table, call.get_function());
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
				if (function_table[call.get_function()].new_function == nullptr) {
					Function* new_function = new Function(call.get_function()->get_argument_types(), call.get_function()->get_return_type());
					program->add_function(new_function);
					function_table[call.get_function()].new_function = new_function;
					Replace replace(program, function_table, call.get_function());
					replace.evaluate(new_function->get_block(), call.get_function()->get_block());
				}
				new_call->set_type(function_table[call.get_function()].new_function->get_return_type());
				new_call->set_function(function_table[call.get_function()].new_function);
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
		Replace replace(new_program, function_table, main_function);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		replace.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};
