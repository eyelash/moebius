#pragma once

#include "ast.hpp"

// type checking and monomorphization
class Pass1: public Visitor<Expression*> {
	static StringView print_type(const Type* type) {
		switch (type->get_id()) {
			case NumberType::id: return "Number";
			case ClosureType::id: return "Function";
			case VoidType::id: return "Void";
			default: return StringView();
		}
	}
	template <class T> [[noreturn]] void error(const Expression& expression, const T& t) {
		Printer printer(std::cerr);
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
			return TypeCompare::compare(argument_types, rhs.argument_types) < 0;
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
	std::map<const Expression*, const Expression*> expression_table;
	Pass1(Program* program, FunctionTable& function_table, const FunctionTableKey& key): program(program), function_table(function_table), key(key) {}
	const Expression* evaluate(Block* destination_block, const Block& source_block) {
		for (const Expression* expression: source_block) {
			Expression* result = visit(*this, expression);
			expression_table[expression] = result;
			destination_block->add_expression(result);
		}
		return destination_block->get_result();
	}
public:
	Expression* visit_number(const Number& number) override {
		return new Number(number.get_value());
	}
	Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		if (left->has_type<NumberType>() && right->has_type<NumberType>()) {
			return new BinaryExpression(binary_expression.get_operation(), left, right);
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		if (condition->has_type<NumberType>()) {
			If* new_if = new If(condition);
			const Expression* then_expression = evaluate(new_if->get_then_block(), if_.get_then_block());
			const Expression* else_expression = evaluate(new_if->get_else_block(), if_.get_else_block());
			if (then_expression->get_type() == else_expression->get_type()) {
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
		TupleType type;
		Tuple* new_tuple = new Tuple();
		for (const Expression* expression: tuple.get_expressions()) {
			const Expression* new_expression = expression_table[expression];
			type.add_type(new_expression->get_type());
			new_tuple->add_expression(new_expression);
		}
		new_tuple->set_type(TypeInterner::intern(&type));
		return new_tuple;
	}
	Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const std::size_t argument_index = tuple_access.get_index();
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		const TupleType* tuple_type = static_cast<const TupleType*>(tuple->get_type());
		const Type* type = tuple_type->get_types()[argument_index];
		return new TupleAccess(tuple, argument_index, type);
	}
	Expression* visit_struct(const Struct& struct_) override {
		StructType type;
		Struct* new_struct = new Struct();
		for (std::size_t i = 0; i < struct_.get_expressions().size(); ++i) {
			const std::string& name = struct_.get_names()[i];
			const Expression* new_expression = expression_table[struct_.get_expressions()[i]];
			if (new_expression->get_type_id() == ArrayType::id) {
				error(struct_, "arrays inside structs are not yet supported");
			}
			type.add_field(name, new_expression->get_type());
			new_struct->add_field(name, new_expression);
		}
		new_struct->set_type(TypeInterner::intern(&type));
		return new_struct;
	}
	Expression* visit_struct_access(const StructAccess& struct_access) override {
		const Expression* struct_ = expression_table[struct_access.get_struct()];
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
		ClosureType type(closure.get_function());
		Closure* new_closure = new Closure(nullptr);
		for (const Expression* expression: closure.get_environment_expressions()) {
			const Expression* new_expression = expression_table[expression];
			type.add_environment_type(new_expression->get_type());
			new_closure->add_environment_expression(new_expression);
		}
		new_closure->set_type(TypeInterner::intern(&type));
		return new_closure;
	}
	Expression* visit_closure_access(const ClosureAccess& closure_access) override {
		const std::size_t argument_index = closure_access.get_index();
		const Expression* closure = expression_table[closure_access.get_closure()];
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
			const Expression* new_argument = expression_table[argument];
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
				new_function->set_return_type(TypeInterner::get_never_type());
			}
			function_table[new_key].new_function = new_function;
			const Expression* new_expression = pass1.evaluate(new_function->get_block(), old_function->get_block());
			new_function->set_return_type(new_expression->get_type());
		}
		else {
			// detect recursion
			if (function_table[new_key].new_function->get_return_type()->get_id() == NeverType::id) {
				function_table.recursion = true;
			}
		}
		new_call->set_type(function_table[new_key].new_function->get_return_type());
		new_call->set_function(function_table[new_key].new_function);
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(expression_table[argument]);
		}
		if (intrinsic.name_equals("putChar")) {
			if (new_intrinsic->get_arguments().size() != 1) {
				error(intrinsic, "putChar takes exactly 1 argument");
			}
			if (!new_intrinsic->get_arguments()[0]->has_type<NumberType>()) {
				error(intrinsic, "argument of putChar must be a number");
			}
			new_intrinsic->set_type(TypeInterner::get_void_type());
		}
		else if (intrinsic.name_equals("getChar")) {
			if (new_intrinsic->get_arguments().size() != 0) {
				error(intrinsic, "getChar takes no argument");
			}
			new_intrinsic->set_type(TypeInterner::get_number_type());
		}
		else if (intrinsic.name_equals("arrayNew")) {
			for (const Expression* argument: new_intrinsic->get_arguments()) {
				if (!argument->has_type<NumberType>()) {
					error(intrinsic, "array elements must be numbers");
				}
			}
			new_intrinsic->set_type(TypeInterner::get_array_type());
		}
		else if (intrinsic.name_equals("arrayGet")) {
			if (new_intrinsic->get_arguments().size() != 2) {
				error(intrinsic, "arrayGet takes exactly 2 arguments");
			}
			if (!new_intrinsic->get_arguments()[0]->has_type<ArrayType>()) {
				error(intrinsic, "first argument of arrayGet must be an array");
			}
			if (!new_intrinsic->get_arguments()[1]->has_type<NumberType>()) {
				error(intrinsic, "second argument of arrayGet must be a number");
			}
			new_intrinsic->set_type(TypeInterner::get_number_type());
		}
		else if (intrinsic.name_equals("arrayLength")) {
			if (new_intrinsic->get_arguments().size() != 1) {
				error(intrinsic, "arrayLength takes exactly 1 argument");
			}
			if (!new_intrinsic->get_arguments()[0]->has_type<ArrayType>()) {
				error(intrinsic, "argument of arrayLength must be an array");
			}
			new_intrinsic->set_type(TypeInterner::get_number_type());
		}
		else if (intrinsic.name_equals("arraySplice")) {
			if (new_intrinsic->get_arguments().size() < 3) {
				error(intrinsic, "arraySplice takes at least 3 arguments");
			}
			if (!new_intrinsic->get_arguments()[0]->has_type<ArrayType>()) {
				error(intrinsic, "first argument of arraySplice must be an array");
			}
			if (!new_intrinsic->get_arguments()[1]->has_type<NumberType>()) {
				error(intrinsic, "second argument of arraySplice must be a number");
			}
			if (!new_intrinsic->get_arguments()[2]->has_type<NumberType>()) {
				error(intrinsic, "third argument of arraySplice must be a number");
			}
			if (new_intrinsic->get_arguments().size() == 4) {
				if (!(new_intrinsic->get_arguments()[3]->has_type<NumberType>() || new_intrinsic->get_arguments()[3]->has_type<ArrayType>())) {
					error(intrinsic, "argument 4 of arraySplice must be a number or an array");
				}
			}
			else {
				for (std::size_t i = 3; i < new_intrinsic->get_arguments().size(); ++i) {
					if (!new_intrinsic->get_arguments()[i]->has_type<NumberType>()) {
						error(intrinsic, format("argument % of arraySplice must be a number", print_number(i + 1)));
					}
				}
			}
			new_intrinsic->set_type(TypeInterner::get_array_type());
		}
		else if (intrinsic.name_equals("arraySpliceInplace")) {
			new_intrinsic->set_type(TypeInterner::get_void_type());
		}
		else if (intrinsic.name_equals("arrayCopy")) {
			new_intrinsic->set_type(TypeInterner::get_array_type());
		}
		else if (intrinsic.name_equals("arrayFree")) {
			new_intrinsic->set_type(TypeInterner::get_void_type());
		}
		return new_intrinsic;
	}
	Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		return new Bind(left, right);
	}
	Expression* visit_return(const Return& return_) override {
		const Expression* expression = expression_table[return_.get_expression()];
		return new Return(expression);
	}
	static std::unique_ptr<Program> run_pass(const Function* main_function, FunctionTable& function_table) {
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		Pass1 pass1(new_program.get(), function_table, FunctionTableKey(main_function));
		Function* new_function = new Function(TypeInterner::get_void_type());
		new_program->add_function(new_function);
		function_table.recursion = false;
		function_table.pass += 1;
		pass1.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program;
		FunctionTable function_table;
		// TODO: prevent infinite loop
		while (function_table.recursion) {
			new_program = run_pass(main_function, function_table);
		}
		return new_program;
	}
};

// inlining
class Pass2 {
	struct FunctionTableEntry {
		const Function* new_function = nullptr;
		std::size_t callers = 0;
		bool evaluating = false;
		bool recursive = false;
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
				function_table[call.get_function()].evaluating = true;
				evaluate(call.get_function()->get_block());
				function_table[call.get_function()].evaluating = false;
			}
			else {
				function_table[call.get_function()].callers += 1;
				if (function_table[call.get_function()].evaluating) {
					function_table[call.get_function()].recursive = true;
				}
			}
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {}
		void visit_bind(const Bind& bind) override {}
		void visit_return(const Return& return_) override {}
	};
	class Replace: public Visitor<Expression*> {
		Program* program;
		Block* current_block;
		FunctionTable& function_table;
		const Function* function;
		std::vector<Expression*> arguments;
		std::size_t level;
		std::map<const Expression*, Expression*> expression_table;
		template <class T, class... A> T* create(A&&... arguments) {
			T* expression = new T(std::forward<A>(arguments)...);
			current_block->add_expression(expression);
			return expression;
		}
	public:
		Replace(Program* program, FunctionTable& function_table, const Function* function): program(program), function_table(function_table), function(function), level(0) {}
		void evaluate(Block* destination_block, const Block& source_block) {
			Block* previous_block = current_block;
			current_block = destination_block;
			++level;
			for (const Expression* expression: source_block) {
				expression_table[expression] = visit(*this, expression);
			}
			--level;
			current_block = previous_block;
		}
		Expression* visit_number(const Number& number) override {
			return create<Number>(number.get_value());
		}
		Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
			const Expression* left = expression_table[binary_expression.get_left()];
			const Expression* right = expression_table[binary_expression.get_right()];
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		Expression* visit_if(const If& if_) override {
			const Expression* condition = expression_table[if_.get_condition()];
			If* new_if = create<If>(condition, if_.get_type());
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			return new_if;
		}
		Expression* visit_tuple(const Tuple& tuple) override {
			Tuple* new_tuple = create<Tuple>(tuple.get_type());
			for (const Expression* expression: tuple.get_expressions()) {
				new_tuple->add_expression(expression_table[expression]);
			}
			return new_tuple;
		}
		Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
			const Expression* tuple = expression_table[tuple_access.get_tuple()];
			return create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
		}
		Expression* visit_struct(const Struct& struct_) override {
			Tuple* tuple = create<Tuple>();
			for (const Expression* expression: struct_.get_expressions()) {
				tuple->add_expression(expression_table[expression]);
			}
			return tuple;
		}
		Expression* visit_struct_access(const StructAccess& struct_access) override {
			const Expression* tuple = expression_table[struct_access.get_struct()];
			const StructType* struct_type = static_cast<const StructType*>(struct_access.get_struct()->get_type());
			const std::size_t index = struct_type->get_index(struct_access.get_name());
			return create<TupleAccess>(tuple, index);
		}
		Expression* visit_closure(const Closure& closure) override {
			Tuple* tuple = create<Tuple>();
			for (const Expression* expression: closure.get_environment_expressions()) {
				tuple->add_expression(expression_table[expression]);
			}
			return tuple;
		}
		Expression* visit_closure_access(const ClosureAccess& closure_access) override {
			const Expression* tuple = expression_table[closure_access.get_closure()];
			return create<TupleAccess>(tuple, closure_access.get_index());
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
					replace.arguments.push_back(expression_table[argument]);
				}
				replace.evaluate(current_block, call.get_function()->get_block());
				return replace.expression_table[call.get_function()->get_expression()];
			}
			else {
				Call* new_call = create<Call>();
				for (const Expression* argument: call.get_arguments()) {
					new_call->add_argument(expression_table[argument]);
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
				new_intrinsic->add_argument(expression_table[argument]);
			}
			return new_intrinsic;
		}
		Expression* visit_bind(const Bind& bind) override {
			const Expression* left = expression_table[bind.get_left()];
			const Expression* right = expression_table[bind.get_right()];
			return create<Bind>(left, right);
		}
		Expression* visit_return(const Return& return_) override {
			Expression* expression = expression_table[return_.get_expression()];
			if (function_table[function].should_inline() && level == 1) {
				return expression;
			}
			else {
				return create<Return>(expression);
			}
		}
	};
public:
	Pass2() = delete;
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Analyze analyze(function_table);
		analyze.evaluate(main_function->get_block());
		Replace replace(new_program.get(), function_table, main_function);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		replace.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// remove empty tuples
class Pass3: public Visitor<Expression*> {
	Program* program;
	Block* current_block;
	using FunctionTable = std::map<const Function*, const Function*>;
	FunctionTable& function_table;
	const Function* function;
	std::map<const Expression*, Expression*> expression_table;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		current_block->add_expression(expression);
		return expression;
	}
	static bool is_empty_tuple(const Type* type) {
		if (type->get_id() == TupleType::id) {
			for (const Type* tuple_type: static_cast<const TupleType*>(type)->get_types()) {
				if (!is_empty_tuple(tuple_type)) {
					return false;
				}
			}
			return true;
		}
		return false;
	}
	static bool is_empty_tuple(const Expression* expression) {
		return is_empty_tuple(expression->get_type());
	}
	static std::size_t adjust_index(const std::vector<const Type*>& types, std::size_t old_index) {
		std::size_t new_index = 0;
		for (std::size_t i = 0; i < old_index; ++i) {
			if (!is_empty_tuple(types[i])) {
				++new_index;
			}
		}
		return new_index;
	}
public:
	Pass3(Program* program, FunctionTable& function_table, const Function* function): program(program), function_table(function_table), function(function) {}
	void evaluate(Block* destination_block, const Block& source_block) {
		Block* previous_block = current_block;
		current_block = destination_block;
		for (const Expression* expression: source_block) {
			Expression* new_expression = visit(*this, expression);
			if (new_expression) {
				expression_table[expression] = new_expression;
			}
		}
		current_block = previous_block;
	}
	Expression* visit_number(const Number& number) override {
		return create<Number>(number.get_value());
	}
	Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, if_.get_type());
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	Expression* visit_tuple(const Tuple& tuple) override {
		if (is_empty_tuple(&tuple)) {
			return nullptr;
		}
		Tuple* new_tuple = create<Tuple>(tuple.get_type());
		for (const Expression* expression: tuple.get_expressions()) {
			if (!is_empty_tuple(expression)) {
				new_tuple->add_expression(expression_table[expression]);
			}
		}
		return new_tuple;
	}
	Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		if (is_empty_tuple(&tuple_access)) {
			return nullptr;
		}
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		const std::vector<const Type*>& tuple_types = static_cast<const TupleType*>(tuple->get_type())->get_types();
		const std::size_t index = adjust_index(tuple_types, tuple_access.get_index());
		return create<TupleAccess>(tuple, index, tuple_access.get_type());
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
		if (is_empty_tuple(&argument)) {
			return nullptr;
		}
		const std::size_t index = adjust_index(function->get_argument_types(), argument.get_index());
		return create<Argument>(index, argument.get_type());
	}
	Expression* visit_call(const Call& call) override {
		Call* new_call = create<Call>();
		for (const Expression* argument: call.get_arguments()) {
			if (!is_empty_tuple(argument)) {
				new_call->add_argument(expression_table[argument]);
			}
		}
		if (function_table[call.get_function()] == nullptr) {
			std::vector<const Type*> argument_types;
			for (const Type* type: call.get_function()->get_argument_types()) {
				if (!is_empty_tuple(type)) {
					argument_types.push_back(type);
				}
			}
			Function* new_function = new Function(argument_types, call.get_function()->get_return_type());
			program->add_function(new_function);
			function_table[call.get_function()] = new_function;
			Pass3 pass3(program, function_table, call.get_function());
			pass3.evaluate(new_function->get_block(), call.get_function()->get_block());
		}
		new_call->set_type(function_table[call.get_function()]->get_return_type());
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), intrinsic.get_type());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(expression_table[argument]);
		}
		return new_intrinsic;
	}
	Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		return create<Bind>(left, right);
	}
	Expression* visit_return(const Return& return_) override {
		const Expression* expression = expression_table[return_.get_expression()];
		return create<Return>(expression);
	}
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Pass3 pass3(new_program.get(), function_table, main_function);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		pass3.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// memory management
class Pass4: public Visitor<Expression*> {
	class Scope {
		Scope*& current_scope;
		Scope* previous_scope;
	public:
		Block* block;
		std::vector<const Expression*> arrays;
		Scope(Scope*& current_scope, Block* block): current_scope(current_scope), block(block) {
			previous_scope = current_scope;
			current_scope = this;
		}
		~Scope() {
			current_scope = previous_scope;
		}
	};
	Program* program;
	Scope* current_scope;
	Block* current_block;
	using FunctionTable = std::map<const Function*, const Function*>;
	FunctionTable& function_table;
	std::map<const Expression*, Expression*> expression_table;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		current_scope->block->add_expression(expression);
		return expression;
	}
public:
	Pass4(Program* program, FunctionTable& function_table): program(program), function_table(function_table) {}
	void evaluate(Block* destination_block, const Block& source_block) {
		Scope scope(current_scope, destination_block);
		for (const Expression* expression: source_block) {
			Expression* new_expression = visit(*this, expression);
			expression_table[expression] = new_expression;
			if (expression->get_type_id() == ArrayType::id) {
				current_scope->arrays.push_back(new_expression);
			}
		}
		const Expression* result = expression_table[source_block.get_result()];
	}
	Expression* visit_number(const Number& number) override {
		return create<Number>(number.get_value());
	}
	Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, if_.get_type());
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	Expression* visit_tuple(const Tuple& tuple) override {
		Tuple* new_tuple = create<Tuple>(tuple.get_type());
		for (const Expression* expression: tuple.get_expressions()) {
			new_tuple->add_expression(expression_table[expression]);
		}
		return new_tuple;
	}
	Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
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
		return create<Argument>(argument.get_index(), argument.get_type());
	}
	Expression* visit_call(const Call& call) override {
		std::vector<const Expression*> arguments;
		for (const Expression* argument: call.get_arguments()) {
			argument = expression_table[argument];
			if (argument->get_type_id() == ArrayType::id) {
				Intrinsic* intrinsic = create<Intrinsic>("arrayCopy", TypeInterner::get_array_type());
				intrinsic->add_argument(argument);
				argument = intrinsic;
			}
			arguments.push_back(argument);
		}
		Call* new_call = create<Call>(std::move(arguments));
		if (function_table[call.get_function()] == nullptr) {
			Function* new_function = new Function(call.get_function()->get_argument_types(), call.get_function()->get_return_type());
			program->add_function(new_function);
			function_table[call.get_function()] = new_function;
			Pass4 pass4(program, function_table);
			pass4.evaluate(new_function->get_block(), call.get_function()->get_block());
		}
		new_call->set_type(function_table[call.get_function()]->get_return_type());
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		if (intrinsic.name_equals("arraySplice")) {
			// replace arraySplice with arrayCopy and arraySpliceInplace
			Intrinsic* array = create<Intrinsic>("arrayCopy", TypeInterner::get_array_type());
			array->add_argument(expression_table[intrinsic.get_arguments()[0]]);
			Intrinsic* new_intrinsic = create<Intrinsic>("arraySpliceInplace", TypeInterner::get_void_type());
			new_intrinsic->add_argument(array);
			for (std::size_t i = 1; i < intrinsic.get_arguments().size(); ++i) {
				new_intrinsic->add_argument(expression_table[intrinsic.get_arguments()[i]]);
			}
			return array;
		}
		else {
			Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), intrinsic.get_type());
			for (const Expression* argument: intrinsic.get_arguments()) {
				new_intrinsic->add_argument(expression_table[argument]);
			}
			return new_intrinsic;
		}
	}
	Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		return create<Bind>(left, right);
	}
	Expression* visit_return(const Return& return_) override {
		const Expression* expression = expression_table[return_.get_expression()];
		if (return_.get_expression()->get_type_id() == ArrayType::id) {
			Intrinsic* intrinsic = create<Intrinsic>("arrayCopy", TypeInterner::get_array_type());
			intrinsic->add_argument(expression);
			expression = intrinsic;
		}
		for (const Expression* array: current_scope->arrays) {
			Intrinsic* intrinsic = create<Intrinsic>("arrayFree", TypeInterner::get_void_type());
			intrinsic->add_argument(array);
		}
		return create<Return>(expression);
	}
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Pass4 pass4(new_program.get(), function_table);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		pass4.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};
