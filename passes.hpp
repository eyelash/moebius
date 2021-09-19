#pragma once

#include "ast.hpp"

// type checking and monomorphization
class Pass1: public Visitor<const Expression*> {
	static StringView print_type(TypeId id) {
		switch (id) {
			case TypeId::INT: return "Int";
			case TypeId::CLOSURE: return "Function";
			case TypeId::VOID: return "Void";
			case TypeId::ARRAY: return "Array";
			case TypeId::TYPE: return "Type";
			default: return StringView();
		}
	}
	static StringView print_type(const Type* type) {
		return print_type(type->get_id());
	}
	template <class T> [[noreturn]] void error(const Expression& expression, const T& t) {
		print_error(Printer(std::cerr), expression.get_position(), t);
		std::exit(EXIT_FAILURE);
	}
	struct FunctionTableKey {
		const Function* old_function;
		std::vector<const Type*> argument_types;
		FunctionTableKey(const Function* old_function): old_function(old_function) {}
		FunctionTableKey() {}
		bool operator <(const FunctionTableKey& rhs) const {
			if (old_function != rhs.old_function) {
				return old_function < rhs.old_function;
			}
			return TypeCompare::compare(argument_types, rhs.argument_types) < 0;
		}
	};
	using FunctionTable = std::map<FunctionTableKey, Function*>;
	Program* program;
	Block* current_block;
	FunctionTable& function_table;
	const FunctionTableKey& key;
	std::map<const Expression*, const Expression*> expression_table;
	Pass1(Program* program, FunctionTable& function_table, const FunctionTableKey& key): program(program), function_table(function_table), key(key) {}
	const Expression* evaluate(Block* destination_block, const Block& source_block) {
		Block* previous_block = current_block;
		current_block = destination_block;
		for (const Expression* expression: source_block) {
			expression_table[expression] = visit(*this, expression);
		}
		current_block = previous_block;
		return destination_block->get_result();
	}
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		current_block->add_expression(expression);
		return expression;
	}
public:
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		if (left->has_type(TypeId::INT) && right->has_type(TypeId::INT)) {
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		if (condition->has_type(TypeId::INT)) {
			If* new_if = create<If>(condition);
			const Expression* then_expression = evaluate(new_if->get_then_block(), if_.get_then_block());
			const Expression* else_expression = evaluate(new_if->get_else_block(), if_.get_else_block());
			if (then_expression->get_type() == else_expression->get_type()) {
				new_if->set_type(then_expression->get_type());
			}
			else if (then_expression->get_type_id() == TypeId::NEVER) {
				new_if->set_type(else_expression->get_type());
			}
			else if (else_expression->get_type_id() == TypeId::NEVER) {
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
	const Expression* visit_tuple(const Tuple& tuple) override {
		TupleType type;
		Tuple* new_tuple = create<Tuple>();
		for (const Expression* expression: tuple.get_expressions()) {
			const Expression* new_expression = expression_table[expression];
			type.add_type(new_expression->get_type());
			new_tuple->add_expression(new_expression);
		}
		new_tuple->set_type(TypeInterner::intern(&type));
		return new_tuple;
	}
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const std::size_t argument_index = tuple_access.get_index();
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		const TupleType* tuple_type = static_cast<const TupleType*>(tuple->get_type());
		const Type* type = tuple_type->get_types()[argument_index];
		return create<TupleAccess>(tuple, argument_index, type);
	}
	const Expression* visit_struct_instantiation(const StructInstantiation& struct_instantiation) override {
		const Expression* type_expression = expression_table[struct_instantiation.get_type_expression()];
		if (type_expression->get_type_id() != TypeId::TYPE) {
			error(struct_instantiation, "struct instantiation requires a struct type");
		}
		const Type* type = static_cast<const TypeType*>(type_expression->get_type())->get_type();
		if (type->get_id() != TypeId::STRUCT) {
			error(struct_instantiation, "struct instantiation requires a struct type");
		}
		const StructType* struct_type = static_cast<const StructType*>(type);
		if (struct_instantiation.get_expressions().size() != struct_type->get_field_types().size()) {
			error(struct_instantiation, "invalid number of fields");
		}
		StructInstantiation* new_struct_instantiation = create<StructInstantiation>(type_expression, type);
		for (std::size_t i = 0; i < struct_instantiation.get_expressions().size(); ++i) {
			const std::string& name = struct_instantiation.get_names()[i];
			if (name != struct_type->get_field_names()[i]) {
				error(struct_instantiation, format("invalid field name \"%\"", name));
			}
			const Expression* new_expression = expression_table[struct_instantiation.get_expressions()[i]];
			if (new_expression->get_type_id() == TypeId::ARRAY) {
				error(struct_instantiation, "structs containing arrays are not yet supported");
			}
			if (new_expression->get_type() != struct_type->get_field_types()[i]) {
				error(struct_instantiation, format("invalid type for field \"%\"", name));
			}
			new_struct_instantiation->add_field(name, new_expression);
		}
		return new_struct_instantiation;
	}
	const Expression* visit_struct_access(const StructAccess& struct_access) override {
		const Expression* struct_ = expression_table[struct_access.get_struct()];
		if (struct_->get_type_id() != TypeId::STRUCT) {
			error(struct_access, "struct access to non-struct");
		}
		const StructType* struct_type = static_cast<const StructType*>(struct_->get_type());
		if (!struct_type->has_field(struct_access.get_name())) {
			error(struct_access, "struct has no such field");
		}
		const std::size_t index = struct_type->get_index(struct_access.get_name());
		const Type* type = struct_type->get_field_types()[index];
		return create<StructAccess>(struct_, struct_access.get_name(), type);
	}
	const Expression* visit_closure(const Closure& closure) override {
		ClosureType type(closure.get_function());
		Closure* new_closure = create<Closure>(nullptr);
		for (const Expression* expression: closure.get_environment_expressions()) {
			const Expression* new_expression = expression_table[expression];
			if (new_expression->get_type_id() == TypeId::ARRAY) {
				error(closure, "closures capturing arrays are not yet supported");
			}
			type.add_environment_type(new_expression->get_type());
			new_closure->add_environment_expression(new_expression);
		}
		new_closure->set_type(TypeInterner::intern(&type));
		return new_closure;
	}
	const Expression* visit_closure_access(const ClosureAccess& closure_access) override {
		const std::size_t argument_index = closure_access.get_index();
		const Expression* closure = expression_table[closure_access.get_closure()];
		const ClosureType* closure_type = static_cast<const ClosureType*>(closure->get_type());
		const Type* type = closure_type->get_environment_types()[argument_index];
		return create<ClosureAccess>(closure, argument_index, type);
	}
	const Expression* visit_argument(const Argument& argument) override {
		const std::size_t argument_index = argument.get_index();
		const Type* type = key.argument_types[argument_index];
		return create<Argument>(argument_index, type);
	}
	const Expression* visit_call(const Call& call) override {
		Call* new_call = create<Call>();
		FunctionTableKey new_key;
		for (const Expression* argument: call.get_arguments()) {
			const Expression* new_argument = expression_table[argument];
			new_key.argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		const Expression* object = new_call->get_object();
		const Function* old_function = call.get_function();
		if (old_function == nullptr) {
			if (object->get_type_id() != TypeId::CLOSURE) {
				error(call, "call to a value that is not a function");
			}
			old_function = static_cast<const ClosureType*>(object->get_type())->get_function();
		}
		new_key.old_function = old_function;
		if (call.get_arguments().size() != old_function->get_arguments()) {
			error(call, format("call with % arguments to a function that accepts % arguments", print_number(call.get_arguments().size() - 1), print_number(old_function->get_arguments() - 1)));
		}

		if (function_table[new_key] == nullptr) {
			Pass1 pass1(program, function_table, new_key);
			Function* new_function = new Function(new_key.argument_types, old_function->get_return_type());
			program->add_function(new_function);
			function_table[new_key] = new_function;
			const Expression* new_expression = pass1.evaluate(new_function->get_block(), old_function->get_block());
			if (new_function->get_return_type() && new_function->get_return_type() != new_expression->get_type()) {
				error(call, format("function does not return the declared return type %", print_type(new_function->get_return_type())));
			}
			new_function->set_return_type(new_expression->get_type());
		}
		else {
			// detect recursion
			if (function_table[new_key]->get_return_type() == nullptr) {
				error(call, "cannot determine return type of recursive call");
			}
		}
		new_call->set_type(function_table[new_key]->get_return_type());
		new_call->set_function(function_table[new_key]);
		return new_call;
	}
	void ensure_argument_types(const Intrinsic& intrinsic, const Intrinsic* new_intrinsic, std::initializer_list<TypeId> types) {
		const std::vector<const Expression*>& arguments = new_intrinsic->get_arguments();
		if (arguments.size() != types.size()) {
			error(intrinsic, format("% takes % argument(s)", intrinsic.get_name(), print_number(types.size())));
		}
		std::size_t i = 0;
		for (TypeId id: types) {
			if (!arguments[i]->has_type(id)) {
				error(intrinsic, format("argument % of % must be of type %", print_number(i + 1), intrinsic.get_name(), print_type(id)));
			}
			++i;
		}
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(expression_table[argument]);
		}
		if (intrinsic.name_equals("putChar")) {
			ensure_argument_types(intrinsic, new_intrinsic, {TypeId::INT});
			new_intrinsic->set_type(TypeInterner::get_void_type());
		}
		else if (intrinsic.name_equals("getChar")) {
			ensure_argument_types(intrinsic, new_intrinsic, {});
			new_intrinsic->set_type(TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("arrayNew")) {
			for (const Expression* argument: new_intrinsic->get_arguments()) {
				if (!argument->has_type(TypeId::INT)) {
					error(intrinsic, "array elements must be numbers");
				}
			}
			new_intrinsic->set_type(TypeInterner::get_array_type());
		}
		else if (intrinsic.name_equals("arrayGet")) {
			ensure_argument_types(intrinsic, new_intrinsic, {TypeId::ARRAY, TypeId::INT});
			new_intrinsic->set_type(TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("arrayLength")) {
			ensure_argument_types(intrinsic, new_intrinsic, {TypeId::ARRAY});
			new_intrinsic->set_type(TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("arraySplice")) {
			if (new_intrinsic->get_arguments().size() < 3) {
				error(intrinsic, "arraySplice takes at least 3 arguments");
			}
			if (!new_intrinsic->get_arguments()[0]->has_type(TypeId::ARRAY)) {
				error(intrinsic, "first argument of arraySplice must be an array");
			}
			if (!new_intrinsic->get_arguments()[1]->has_type(TypeId::INT)) {
				error(intrinsic, "second argument of arraySplice must be a number");
			}
			if (!new_intrinsic->get_arguments()[2]->has_type(TypeId::INT)) {
				error(intrinsic, "third argument of arraySplice must be a number");
			}
			if (new_intrinsic->get_arguments().size() == 4) {
				if (!(new_intrinsic->get_arguments()[3]->has_type(TypeId::INT) || new_intrinsic->get_arguments()[3]->has_type(TypeId::ARRAY))) {
					error(intrinsic, "argument 4 of arraySplice must be a number or an array");
				}
			}
			else {
				for (std::size_t i = 3; i < new_intrinsic->get_arguments().size(); ++i) {
					if (!new_intrinsic->get_arguments()[i]->has_type(TypeId::INT)) {
						error(intrinsic, format("argument % of arraySplice must be a number", print_number(i + 1)));
					}
				}
			}
			new_intrinsic->set_type(TypeInterner::get_array_type());
		}
		else if (intrinsic.name_equals("arrayCopy")) {
			new_intrinsic->set_type(TypeInterner::get_array_type());
		}
		else if (intrinsic.name_equals("arrayFree")) {
			new_intrinsic->set_type(TypeInterner::get_void_type());
		}
		return new_intrinsic;
	}
	const Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		if (left->get_type_id() != TypeId::VOID || right->get_type_id() != TypeId::VOID) {
			error(bind, "arguments of bind must be of type Void");
		}
		return create<Bind>(left, right);
	}
	const Expression* visit_return(const Return& return_) override {
		const Expression* expression = expression_table[return_.get_expression()];
		return create<Return>(expression);
	}
	const Expression* visit_type_literal(const TypeLiteral& type_literal) override {
		const Type* type = static_cast<const TypeType*>(type_literal.get_type())->get_type();
		return create<TypeLiteral>(type);
	}
	const Expression* visit_struct_definition(const StructDefinition& struct_definition) override {
		StructDefinition* new_struct_definition = create<StructDefinition>();
		StructType struct_type;
		for (std::size_t i = 0; i < struct_definition.get_type_expressions().size(); ++i) {
			const std::string& name = struct_definition.get_names()[i];
			const Expression* type_expression = expression_table[struct_definition.get_type_expressions()[i]];
			new_struct_definition->add_field(name, type_expression);
			if (type_expression->get_type_id() != TypeId::TYPE) {
				error(struct_definition, "fields must be types");
			}
			const Type* type = static_cast<const TypeType*>(type_expression->get_type())->get_type();
			struct_type.add_field(name, type);
		}
		new_struct_definition->set_type(TypeInterner::get_type_type(TypeInterner::intern(&struct_type)));
		return new_struct_definition;
	}
	const Expression* visit_type_assert(const TypeAssert& type_assert) override {
		const Expression* expression = expression_table[type_assert.get_expression()];
		const Expression* type_expression = expression_table[type_assert.get_type()];
		if (type_expression->get_type_id() != TypeId::TYPE) {
			error(*type_assert.get_type(), "expression is not a type");
		}
		const Type* type = static_cast<const TypeType*>(type_expression->get_type())->get_type();
		if (expression->get_type() != type) {
			error(*type_assert.get_type(), format("expression does not have the declared type %", print_type(type)));
		}
		return create<TypeAssert>(expression, type_expression);
	}
	const Expression* visit_return_type(const ReturnType& return_type) override {
		const Expression* type_expression = expression_table[return_type.get_type()];
		if (type_expression->get_type_id() != TypeId::TYPE) {
			error(*return_type.get_type(), "return type must be a type");
		}
		const Type* type = static_cast<const TypeType*>(type_expression->get_type())->get_type();
		function_table[key]->set_return_type(type);
		return create<ReturnType>(type_expression);
	}
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Pass1 pass1(new_program.get(), function_table, FunctionTableKey(main_function));
		Function* new_function = new Function(TypeInterner::get_void_type());
		new_program->add_function(new_function);
		pass1.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// inlining
class Pass2 {
	struct FunctionTableEntry {
		const Function* new_function = nullptr;
		std::size_t expressions = 0;
		std::size_t calls = 0;
		std::size_t callers = 0;
		bool evaluating = false;
		bool recursive = false;
		bool should_inline() const {
			if (recursive) return false;
			if (callers == 0) return false; // the main function
			if (callers == 1) return true;
			return expressions <= 5 && calls == 0;
		}
	};
	using FunctionTable = std::map<const Function*, FunctionTableEntry>;
	class Analyze: public Visitor<void> {
		FunctionTable& function_table;
		const Function* function;
	public:
		Analyze(FunctionTable& function_table, const Function* function): function_table(function_table) {}
		void evaluate(const Block& block) {
			for (const Expression* expression: block) {
				visit(*this, expression);
				function_table[function].expressions += 1;
			}
		}
		void visit_if(const If& if_) override {
			evaluate(if_.get_then_block());
			evaluate(if_.get_else_block());
		}
		void visit_call(const Call& call) override {
			if (function_table[call.get_function()].callers == 0) {
				function_table[call.get_function()].callers += 1;
				function_table[call.get_function()].evaluating = true;
				Analyze analyze(function_table, call.get_function());
				analyze.evaluate(call.get_function()->get_block());
				function_table[call.get_function()].evaluating = false;
			}
			else {
				function_table[call.get_function()].callers += 1;
				if (function_table[call.get_function()].evaluating) {
					function_table[call.get_function()].recursive = true;
				}
			}
			function_table[function].calls += 1;
		}
	};
	class Replace: public Visitor<const Expression*> {
		Program* program;
		Block* current_block;
		FunctionTable& function_table;
		const Function* function;
		std::vector<const Expression*> arguments;
		std::size_t level;
		std::map<const Expression*, const Expression*> expression_table;
		template <class T, class... A> T* create(A&&... arguments) {
			T* expression = new T(std::forward<A>(arguments)...);
			current_block->add_expression(expression);
			return expression;
		}
	public:
		Replace(Program* program, FunctionTable& function_table, const Function* function): program(program), function_table(function_table), function(function), level(0) {}
		const Expression* evaluate(Block* destination_block, const Block& source_block) {
			Block* previous_block = current_block;
			current_block = destination_block;
			++level;
			for (const Expression* expression: source_block) {
				expression_table[expression] = visit(*this, expression);
			}
			--level;
			current_block = previous_block;
			return expression_table[source_block.get_result()];
		}
		const Expression* visit_int_literal(const IntLiteral& int_literal) override {
			return create<IntLiteral>(int_literal.get_value());
		}
		const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
			const Expression* left = expression_table[binary_expression.get_left()];
			const Expression* right = expression_table[binary_expression.get_right()];
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		const Expression* visit_if(const If& if_) override {
			const Expression* condition = expression_table[if_.get_condition()];
			If* new_if = create<If>(condition, if_.get_type());
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			return new_if;
		}
		const Expression* visit_tuple(const Tuple& tuple) override {
			Tuple* new_tuple = create<Tuple>(tuple.get_type());
			for (const Expression* expression: tuple.get_expressions()) {
				new_tuple->add_expression(expression_table[expression]);
			}
			return new_tuple;
		}
		const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
			const Expression* tuple = expression_table[tuple_access.get_tuple()];
			return create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
		}
		const Expression* visit_struct_instantiation(const StructInstantiation& struct_instantiation) override {
			Tuple* tuple = create<Tuple>();
			for (const Expression* expression: struct_instantiation.get_expressions()) {
				tuple->add_expression(expression_table[expression]);
			}
			return tuple;
		}
		const Expression* visit_struct_access(const StructAccess& struct_access) override {
			const Expression* tuple = expression_table[struct_access.get_struct()];
			const StructType* struct_type = static_cast<const StructType*>(struct_access.get_struct()->get_type());
			const std::size_t index = struct_type->get_index(struct_access.get_name());
			return create<TupleAccess>(tuple, index, struct_access.get_type());
		}
		const Expression* visit_closure(const Closure& closure) override {
			Tuple* tuple = create<Tuple>();
			for (const Expression* expression: closure.get_environment_expressions()) {
				tuple->add_expression(expression_table[expression]);
			}
			return tuple;
		}
		const Expression* visit_closure_access(const ClosureAccess& closure_access) override {
			const Expression* tuple = expression_table[closure_access.get_closure()];
			return create<TupleAccess>(tuple, closure_access.get_index(), closure_access.get_type());
		}
		const Expression* visit_argument(const Argument& argument) override {
			if (function_table[function].should_inline()) {
				return arguments[argument.get_index()];
			}
			else {
				return create<Argument>(argument.get_index(), argument.get_type());
			}
		}
		const Expression* visit_call(const Call& call) override {
			if (function_table[call.get_function()].should_inline()) {
				Replace replace(program, function_table, call.get_function());
				for (const Expression* argument: call.get_arguments()) {
					replace.arguments.push_back(expression_table[argument]);
				}
				return replace.evaluate(current_block, call.get_function()->get_block());
			}
			else {
				Call* new_call = create<Call>(call.get_type());
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
				new_call->set_function(function_table[call.get_function()].new_function);
				return new_call;
			}
		}
		const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
			Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), intrinsic.get_type());
			for (const Expression* argument: intrinsic.get_arguments()) {
				new_intrinsic->add_argument(expression_table[argument]);
			}
			return new_intrinsic;
		}
		const Expression* visit_bind(const Bind& bind) override {
			const Expression* left = expression_table[bind.get_left()];
			const Expression* right = expression_table[bind.get_right()];
			return create<Bind>(left, right);
		}
		const Expression* visit_return(const Return& return_) override {
			const Expression* expression = expression_table[return_.get_expression()];
			if (function_table[function].should_inline() && level == 1) {
				return expression;
			}
			else {
				return create<Return>(expression);
			}
		}
		const Expression* visit_type_literal(const TypeLiteral& type_literal) override {
			return nullptr;
		}
		const Expression* visit_struct_definition(const StructDefinition& struct_definition) override {
			return nullptr;
		}
		const Expression* visit_type_assert(const TypeAssert& type_assert) override {
			return nullptr;
		}
		const Expression* visit_return_type(const ReturnType& return_type) override {
			return nullptr;
		}
	};
public:
	Pass2() = delete;
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Analyze analyze(function_table, main_function);
		analyze.evaluate(main_function->get_block());
		Replace replace(new_program.get(), function_table, main_function);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		replace.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// remove empty tuples
class Pass3: public Visitor<const Expression*> {
	class GetTupleElement: public Visitor<const Expression*> {
		std::size_t index;
	public:
		GetTupleElement(std::size_t index): index(index) {}
		const Expression* visit_tuple(const Tuple& tuple) override {
			return tuple.get_expressions()[index];
		}
	};
	Program* program;
	Block* current_block;
	using FunctionTable = std::map<const Function*, const Function*>;
	FunctionTable& function_table;
	const Function* function;
	std::map<const Expression*, const Expression*> expression_table;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		current_block->add_expression(expression);
		return expression;
	}
	static bool is_empty_tuple(const Type* type) {
		if (type->get_id() == TypeId::TUPLE) {
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
			const Expression* new_expression = visit(*this, expression);
			if (new_expression) {
				expression_table[expression] = new_expression;
			}
		}
		current_block = previous_block;
	}
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, if_.get_type());
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	const Expression* visit_tuple(const Tuple& tuple) override {
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
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		if (is_empty_tuple(&tuple_access)) {
			return nullptr;
		}
		GetTupleElement gte(tuple_access.get_index());
		if (const Expression* expression = visit(gte, tuple_access.get_tuple())) {
			return expression_table[expression];
		}
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		const std::vector<const Type*>& tuple_types = static_cast<const TupleType*>(tuple->get_type())->get_types();
		const std::size_t index = adjust_index(tuple_types, tuple_access.get_index());
		return create<TupleAccess>(tuple, index, tuple_access.get_type());
	}
	Expression* visit_argument(const Argument& argument) override {
		if (is_empty_tuple(&argument)) {
			return nullptr;
		}
		const std::size_t index = adjust_index(function->get_argument_types(), argument.get_index());
		return create<Argument>(index, argument.get_type());
	}
	const Expression* visit_call(const Call& call) override {
		Call* new_call = create<Call>(call.get_type());
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
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), intrinsic.get_type());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(expression_table[argument]);
		}
		return new_intrinsic;
	}
	const Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		return create<Bind>(left, right);
	}
	const Expression* visit_return(const Return& return_) override {
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
class Pass4: public Visitor<const Expression*> {
	class UsageAnalysisTable {
	public:
		std::map<const Block*, std::map<const Expression*, std::pair<const Expression*, std::size_t>>> usages;
		std::map<const Block*, std::vector<const Expression*>> frees;
		std::map<const Expression*, std::size_t> levels;
	};
	class UsageAnalysis1: public Visitor<void> {
		const Block* current_block;
		std::size_t current_level;
		UsageAnalysisTable& table;
	public:
		UsageAnalysis1(UsageAnalysisTable& table): current_block(nullptr), current_level(0), table(table) {}
		void add_usage(const Expression* resource, const Expression* consumer, std::size_t argument_index) {
			table.usages[current_block][resource] = std::make_pair(consumer, argument_index);
		}
		void propagate_usages(const Block* block, const Expression* consumer) {
			for (auto& entry: table.usages[block]) {
				const Expression* resource = entry.first;
				if (table.levels[resource] <= current_level) {
					add_usage(resource, consumer, 0);
				}
			}
		}
		void evaluate(const Block& block) {
			const Block* previous_block = current_block;
			current_block = &block;
			++current_level;
			for (const Expression* expression: block) {
				if (expression->get_type_id() == TypeId::ARRAY) {
					table.levels[expression] = current_level;
				}
				visit(*this, expression);
			}
			--current_level;
			current_block = previous_block;
		}
		void visit_if(const If& if_) override {
			evaluate(if_.get_then_block());
			evaluate(if_.get_else_block());
			propagate_usages(&if_.get_then_block(), &if_);
			propagate_usages(&if_.get_else_block(), &if_);
		}
		void visit_call(const Call& call) override {
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				const Expression* argument = call.get_arguments()[i];
				if (argument->get_type_id() == TypeId::ARRAY) {
					add_usage(argument, &call, i);
				}
			}
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {
			for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
				const Expression* argument = intrinsic.get_arguments()[i];
				if (argument->get_type_id() == TypeId::ARRAY) {
					add_usage(argument, &intrinsic, i);
				}
			}
		}
		void visit_return(const Return& return_) override {
			if (return_.get_expression()->get_type_id() == TypeId::ARRAY) {
				add_usage(return_.get_expression(), &return_, 0);
			}
		}
	};
	class UsageAnalysis2: public Visitor<void> {
		const Block* current_block;
		UsageAnalysisTable& table;
		std::size_t current_level;
	public:
		UsageAnalysis2(UsageAnalysisTable& table): current_block(nullptr), table(table), current_level(0) {}
		bool is_last_use(const Expression* resource, const Expression* consumer, std::size_t argument_index) {
			return table.usages[current_block][resource] == std::make_pair(consumer, argument_index);
		}
		void remove_invalid_usages(const Block* block, const Expression* consumer) {
			auto iterator = table.usages[block].begin();
			while (iterator != table.usages[block].end()) {
				const Expression* resource = iterator->first;
				if (table.levels[resource] <= current_level && !is_last_use(resource, consumer, 0)) {
					iterator = table.usages[block].erase(iterator);
				}
				else {
					++iterator;
				}
			}
		}
		void ensure_frees(const Block* source_block, const Block* target_block) {
			for (auto& entry: table.usages[source_block]) {
				const Expression* resource = entry.first;
				if (table.usages[target_block].find(resource) == table.usages[target_block].end() && table.levels[resource] < current_level + 1) {
					// if a resource from the source block has no usage in the target block, add it to the free list
					table.frees[target_block].push_back(resource);
				}
			}
		}
		void evaluate(const Block& block) {
			const Block* previous_block = current_block;
			current_block = &block;
			++current_level;
			for (const Expression* expression: block) {
				visit(*this, expression);
			}
			--current_level;
			current_block = previous_block;
		}
		void visit_if(const If& if_) override {
			remove_invalid_usages(&if_.get_then_block(), &if_);
			remove_invalid_usages(&if_.get_else_block(), &if_);
			ensure_frees(&if_.get_then_block(), &if_.get_else_block());
			ensure_frees(&if_.get_else_block(), &if_.get_then_block());
			evaluate(if_.get_then_block());
			evaluate(if_.get_else_block());
		}
	};
	class Scope {
		Scope*& current_scope;
		Scope* previous_scope;
	public:
		Block* block;
		const Block* old_block;
		Scope(Scope*& current_scope, Block* block, const Block* old_block): current_scope(current_scope), block(block), old_block(old_block) {
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
	UsageAnalysisTable& usage_analysis;
	std::map<const Expression*, const Expression*> expression_table;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		current_scope->block->add_expression(expression);
		return expression;
	}
	bool is_last_use(const Expression* resource, const Expression* consumer, std::size_t argument_index) {
		return usage_analysis.usages[current_scope->old_block][resource] == std::make_pair(consumer, argument_index);
	}
	bool is_unused(const Expression* resource) {
		return usage_analysis.usages[current_scope->old_block].count(resource) == 0;
	}
	const Expression* copy(const Expression* resource) {
		Intrinsic* copy_intrinsic = create<Intrinsic>("arrayCopy", TypeInterner::get_array_type());
		copy_intrinsic->add_argument(resource);
		return copy_intrinsic;
	}
	void free(const Expression* resource) {
		Intrinsic* free_intrinsic = create<Intrinsic>("arrayFree", TypeInterner::get_void_type());
		free_intrinsic->add_argument(resource);
	}
public:
	Pass4(Program* program, FunctionTable& function_table, UsageAnalysisTable& usage_analysis): program(program), function_table(function_table), usage_analysis(usage_analysis) {}
	void evaluate(Block* destination_block, const Block& source_block) {
		Scope scope(current_scope, destination_block, &source_block);
		for (const Expression* expression: usage_analysis.frees[current_scope->old_block]) {
			free(expression_table[expression]);
		}
		for (const Expression* expression: source_block) {
			expression_table[expression] = visit(*this, expression);
		}
	}
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, if_.get_type());
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	const Expression* visit_tuple(const Tuple& tuple) override {
		Tuple* new_tuple = create<Tuple>(tuple.get_type());
		for (const Expression* expression: tuple.get_expressions()) {
			new_tuple->add_expression(expression_table[expression]);
		}
		return new_tuple;
	}
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		return create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
	}
	const Expression* visit_argument(const Argument& argument) override {
		return create<Argument>(argument.get_index(), argument.get_type());
	}
	const Expression* visit_call(const Call& call) override {
		Call* new_call = new Call(call.get_type());
		for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
			const Expression* argument = call.get_arguments()[i];
			if (argument->get_type_id() == TypeId::ARRAY && !is_last_use(argument, &call, i)) {
				new_call->add_argument(copy(expression_table[argument]));
			}
			else {
				new_call->add_argument(expression_table[argument]);
			}
		}
		current_scope->block->add_expression(new_call);
		if (function_table[call.get_function()] == nullptr) {
			Function* new_function = new Function(call.get_function()->get_argument_types(), call.get_function()->get_return_type());
			program->add_function(new_function);
			function_table[call.get_function()] = new_function;
			Pass4 pass4(program, function_table, usage_analysis);
			pass4.evaluate(new_function->get_block(), call.get_function()->get_block());
		}
		new_call->set_function(function_table[call.get_function()]);
		if (call.get_type_id() == TypeId::ARRAY && is_unused(&call)) {
			free(new_call);
		}
		return new_call;
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name(), intrinsic.get_type());
		for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
			const Expression* argument = intrinsic.get_arguments()[i];
			const bool is_borrowed = intrinsic.name_equals("arrayGet") || intrinsic.name_equals("arrayLength") || (intrinsic.name_equals("arraySplice") && i == 3);
			if (argument->get_type_id() == TypeId::ARRAY && !is_borrowed && !is_last_use(argument, &intrinsic, i)) {
				new_intrinsic->add_argument(copy(expression_table[argument]));
			}
			else {
				new_intrinsic->add_argument(expression_table[argument]);
			}
		}
		current_scope->block->add_expression(new_intrinsic);
		for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
			const Expression* argument = intrinsic.get_arguments()[i];
			const bool is_borrowed = intrinsic.name_equals("arrayGet") || intrinsic.name_equals("arrayLength") || (intrinsic.name_equals("arraySplice") && i == 3);
			if (argument->get_type_id() == TypeId::ARRAY && is_borrowed && is_last_use(argument, &intrinsic, i)) {
				free(expression_table[argument]);
			}
		}
		if (intrinsic.get_type_id() == TypeId::ARRAY && is_unused(&intrinsic)) {
			free(new_intrinsic);
		}
		return new_intrinsic;
	}
	const Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		return create<Bind>(left, right);
	}
	const Expression* visit_return(const Return& return_) override {
		const Expression* expression = return_.get_expression();
		if (expression->get_type_id() == TypeId::ARRAY && !is_last_use(expression, &return_, 0)) {
			return create<Return>(copy(expression_table[expression]));
		}
		else {
			return create<Return>(expression_table[expression]);
		}
	}
	static void perform_usage_analysis(const Program& program, UsageAnalysisTable& table) {
		for (const Function* function: program) {
			UsageAnalysis1 usage_analysis1(table);
			usage_analysis1.evaluate(function->get_block());
			UsageAnalysis2 usage_analysis2(table);
			usage_analysis2.evaluate(function->get_block());
		}
	}
	static std::unique_ptr<Program> run(const Program& program) {
		UsageAnalysisTable usage_analysis;
		perform_usage_analysis(program, usage_analysis);
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Pass4 pass4(new_program.get(), function_table, usage_analysis);
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		pass4.evaluate(new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

class TailCallData {
public:
	std::map<const Expression*, bool> tail_call_expressions;
	std::map<const Function*, bool> tail_call_functions;
	bool is_tail_call(const Call& call) const {
		return tail_call_expressions.find(&call) != tail_call_expressions.end();
	}
	bool has_tail_call(const Function* function) const {
		return tail_call_functions.find(function) != tail_call_functions.end();
	}
};

// tail call optimization
class Pass5: public Visitor<void> {
	const Function* function;
	TailCallData& data;
public:
	Pass5(const Function* function, TailCallData& data): function(function), data(data) {}
	void evaluate(const Block& block) {
		visit(*this, block.get_result());
	}
	void visit_if(const If& if_) override {
		evaluate(if_.get_then_block());
		evaluate(if_.get_else_block());
	}
	void visit_call(const Call& call) override {
		if (call.get_function() == function) {
			data.tail_call_expressions[&call] = true;
			data.tail_call_functions[function] = true;
		}
	}
	static void run(const Program& program, TailCallData& data) {
		for (const Function* function: program) {
			Pass5 pass5(function, data);
			pass5.evaluate(function->get_block());
		}
	}
};
