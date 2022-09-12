#pragma once

#include "ast.hpp"

// type checking, monomorphization, and constant propagation
class Pass1: public Visitor<const Expression*> {
	template <class T> [[noreturn]] void error(const Expression& expression, const T& t) {
		print_error(Printer(std::cerr), expression.get_position(), t);
		std::exit(EXIT_FAILURE);
	}
	static std::int32_t execute_binary_operation(BinaryOperation operation, std::int32_t left, std::int32_t right) {
		switch (operation) {
			case BinaryOperation::ADD: return left + right;
			case BinaryOperation::SUB: return left - right;
			case BinaryOperation::MUL: return left * right;
			case BinaryOperation::DIV: return left / right;
			case BinaryOperation::REM: return left % right;
			case BinaryOperation::EQ: return left == right;
			case BinaryOperation::NE: return left != right;
			case BinaryOperation::LT: return left < right;
			case BinaryOperation::LE: return left <= right;
			case BinaryOperation::GT: return left > right;
			case BinaryOperation::GE: return left >= right;
			default: return 0;
		}
	}
	struct FunctionTableKey {
		const Function* old_function;
		std::vector<const Type*> argument_types;
		FunctionTableKey(const Function* old_function): old_function(old_function) {}
		FunctionTableKey() {}
		bool operator <(const FunctionTableKey& rhs) const {
			return std::make_pair(old_function, std::ref(argument_types)) < std::make_pair(rhs.old_function, std::ref(rhs.argument_types));
		}
	};
	using FunctionTable = std::map<FunctionTableKey, Function*>;
	Program* program;
	FunctionTable& function_table;
	const FunctionTableKey& key;
	const std::vector<const Expression*>& environment_arguments;
	using ExpressionTable = std::map<const Expression*, const Expression*>;
	ExpressionTable& expression_table;
	Block* destination_block;
	bool omit_return;
	const Expression* result = nullptr;
	Pass1(Program* program, FunctionTable& function_table, const FunctionTableKey& key, const std::vector<const Expression*>& environment_arguments, ExpressionTable& expression_table, Block* destination_block, bool omit_return): program(program), function_table(function_table), key(key), environment_arguments(environment_arguments), expression_table(expression_table), destination_block(destination_block), omit_return(omit_return) {}
	static const Expression* evaluate(Program* program, FunctionTable& function_table, const FunctionTableKey& key, const std::vector<const Expression*>& environment_arguments, ExpressionTable& expression_table, Block* destination_block, const Block& source_block, bool omit_return) {
		Pass1 pass1(program, function_table, key, environment_arguments, expression_table, destination_block, omit_return);
		for (const Expression* expression: source_block) {
			const Expression* new_expression = visit(pass1, expression);
			if (new_expression) {
				expression_table[expression] = new_expression;
			}
		}
		return pass1.result;
	}
	static const Expression* evaluate(Program* program, FunctionTable& function_table, const FunctionTableKey& key, const std::vector<const Expression*>& environment_arguments, Block* destination_block, const Block& source_block) {
		ExpressionTable expression_table;
		return evaluate(program, function_table, key, environment_arguments, expression_table, destination_block, source_block, false);
	}
	static const Expression* evaluate(Program* program, FunctionTable& function_table, const FunctionTableKey& key, Block* destination_block, const Block& source_block) {
		std::vector<const Expression*> environment_arguments;
		ExpressionTable expression_table;
		return evaluate(program, function_table, key, environment_arguments, expression_table, destination_block, source_block, false);
	}
	const Expression* evaluate(Block* destination_block, const Block& source_block, bool omit_return) {
		return evaluate(program, function_table, key, environment_arguments, expression_table, destination_block, source_block, omit_return);
	}
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		destination_block->add_expression(expression);
		return expression;
	}
public:
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		if (left->get_type_id() == TypeId::INT && right->get_type_id() == TypeId::INT) {
			GetInt get_left_int;
			GetInt get_right_int;
			if (visit(get_left_int, left) && visit(get_right_int, right)) {
				return create<IntLiteral>(execute_binary_operation(binary_expression.get_operation(), get_left_int.value, get_right_int.value));
			}
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		else if (left->get_type_id() == TypeId::TYPE && right->get_type_id() == TypeId::TYPE) {
			const Type* left_type = static_cast<const TypeType*>(left->get_type())->get_type();
			const Type* right_type = static_cast<const TypeType*>(right->get_type())->get_type();
			if (binary_expression.get_operation() == BinaryOperation::EQ) {
				return create<IntLiteral>(left_type == right_type);
			}
			else if (binary_expression.get_operation() == BinaryOperation::NE) {
				return create<IntLiteral>(left_type != right_type);
			}
			else {
				error(binary_expression, "invalid binary expression");
			}
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	const Expression* visit_array_literal(const ArrayLiteral& array_literal) override {
		if (array_literal.get_elements().size() == 0) {
			error(array_literal, "emtpy arrays are not yet supported");
		}
		const Type* element_type = expression_table[array_literal.get_elements()[0]]->get_type();
		ArrayLiteral* new_array_literal = create<ArrayLiteral>(TypeInterner::get_array_type(element_type));
		for (const Expression* element: array_literal.get_elements()) {
			const Expression* new_element = expression_table[element];
			if (new_element->get_type() != element_type) {
				error(array_literal, format("array elements must be of type %", print_type(element_type)));
			}
			new_array_literal->add_element(new_element);
		}
		return new_array_literal;
	}
	const Expression* visit_string_literal(const StringLiteral& string_literal) override {
		return create<StringLiteral>(string_literal.get_value());
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		if (condition->get_type_id() != TypeId::INT) {
			error(if_, "type of condition must be a number");
		}
		GetInt get_int;
		if (visit(get_int, condition)) {
			if (get_int.value) {
				return evaluate(destination_block, if_.get_then_block(), true);
			}
			else {
				return evaluate(destination_block, if_.get_else_block(), true);
			}
		}
		else {
			If* new_if = create<If>(condition);
			const Expression* then_expression = evaluate(new_if->get_then_block(), if_.get_then_block(), false);
			const Expression* else_expression = evaluate(new_if->get_else_block(), if_.get_else_block(), false);
			if (then_expression->get_type() != else_expression->get_type()) {
				error(if_, "if and else branches must return values of the same type");
			}
			new_if->set_type(then_expression->get_type());
			return new_if;
		}
	}
	const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		TupleType type;
		TupleLiteral* new_tuple_literal = create<TupleLiteral>();
		for (const Expression* element: tuple_literal.get_elements()) {
			const Expression* new_element = expression_table[element];
			type.add_element_type(new_element->get_type());
			new_tuple_literal->add_element(new_element);
		}
		new_tuple_literal->set_type(TypeInterner::intern(&type));
		return new_tuple_literal;
	}
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const std::size_t index = tuple_access.get_index();
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		if (tuple->get_type_id() != TypeId::TUPLE) {
			error(tuple_access, "tuple access to non-tuple");
		}
		const TupleType* tuple_type = static_cast<const TupleType*>(tuple->get_type());
		if (index >= tuple_type->get_element_types().size()) {
			error(tuple_access, "tuple index out of bounds");
		}
		GetTupleElement get_tuple_element(index);
		if (const Expression* element = visit(get_tuple_element, tuple)) {
			return element;
		}
		const Type* type = tuple_type->get_element_types()[index];
		return create<TupleAccess>(tuple, index, type);
	}
	const Expression* visit_struct_literal(const StructLiteral& struct_literal) override {
		StructType type;
		StructLiteral* new_struct_literal = create<StructLiteral>();
		for (const auto& field: struct_literal.get_fields()) {
			const std::string& field_name = field.first;
			if (type.has_field(field_name)) {
				error(struct_literal, format("duplicate field \"%\"", field_name));
			}
			const Expression* new_field = expression_table[field.second];
			type.add_field(field_name, new_field->get_type());
			new_struct_literal->add_field(field_name, new_field);
		}
		new_struct_literal->set_type(TypeInterner::intern(&type));
		return new_struct_literal;
	}
	const Expression* visit_struct_access(const StructAccess& struct_access) override {
		const Expression* struct_ = expression_table[struct_access.get_struct()];
		if (struct_->get_type_id() == TypeId::STRUCT) {
			const StructType* struct_type = static_cast<const StructType*>(struct_->get_type());
			if (struct_type->has_field(struct_access.get_field_name())) {
				const std::size_t index = struct_type->get_index(struct_access.get_field_name());
				GetTupleElement get_tuple_element(index);
				if (const Expression* element = visit(get_tuple_element, struct_)) {
					return element;
				}
				const Type* type = struct_type->get_fields()[index].second;
				return create<StructAccess>(struct_, struct_access.get_field_name(), type);
			}
		}
		if (struct_access.get_function()) {
			const Expression* function = expression_table[struct_access.get_function()];
			if (function->get_type_id() == TypeId::CLOSURE) {
				const ClosureType* old_closure_type = static_cast<const ClosureType*>(function->get_type());
				ClosureType new_closure_type(old_closure_type->get_function());
				Closure* new_closure = new Closure(nullptr);
				for (std::size_t i = 0; i < old_closure_type->get_environment_types().size(); ++i) {
					const Type* environment_type = old_closure_type->get_environment_types()[i];
					new_closure_type.add_environment_type(environment_type);
					new_closure->add_environment_expression(create<ClosureAccess>(function, i, environment_type));
				}
				new_closure_type.add_environment_type(struct_->get_type());
				new_closure->add_environment_expression(struct_);
				new_closure->set_type(TypeInterner::intern(&new_closure_type));
				destination_block->add_expression(new_closure);
				return new_closure;
			}
		}
		error(struct_access, "invalid struct access");
	}
	const Expression* visit_closure(const Closure& closure) override {
		ClosureType type(closure.get_function());
		Closure* new_closure = create<Closure>(nullptr);
		for (const Expression* expression: closure.get_environment_expressions()) {
			const Expression* new_expression = expression_table[expression];
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
		if (argument.get_argument_type() == ArgumentType::ARGUMENT) {
			const std::size_t argument_index = environment_arguments.size() + argument.get_index();
			const Type* type = key.argument_types[argument_index];
			return create<Argument>(argument_index, type);
		}
		else if (argument.get_argument_type() == ArgumentType::ENVIRONMENT) {
			return environment_arguments[argument.get_index()];
		}
		else if (argument.get_argument_type() == ArgumentType::SELF) {
			ClosureType type(key.old_function);
			Closure* new_closure = create<Closure>(nullptr);
			for (const Expression* argument: environment_arguments) {
				type.add_environment_type(argument->get_type());
				new_closure->add_environment_expression(argument);
			}
			new_closure->set_type(TypeInterner::intern(&type));
			return new_closure;
		}
		return nullptr;
	}
	const Expression* visit_closure_call(const ClosureCall& call) override {
		FunctionCall* new_call = new FunctionCall();
		FunctionTableKey new_key;
		const Expression* closure = expression_table[call.get_closure()];
		if (closure->get_type_id() != TypeId::CLOSURE) {
			error(call, "call to a value that is not a function");
		}
		const ClosureType* closure_type = static_cast<const ClosureType*>(closure->get_type());
		for (std::size_t i = 0; i < closure_type->get_environment_types().size(); ++i) {
			const Type* argument_type = closure_type->get_environment_types()[i];
			const Expression* new_argument = create<ClosureAccess>(closure, i, argument_type);
			new_key.argument_types.push_back(argument_type);
			new_call->add_argument(new_argument);
		}
		for (const Expression* argument: call.get_arguments()) {
			const Expression* new_argument = expression_table[argument];
			new_key.argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		new_key.old_function = closure_type->get_function();
		if (new_key.argument_types.size() != new_key.old_function->get_total_arguments()) {
			const std::size_t expected_arguments = new_key.old_function->get_total_arguments() - closure_type->get_environment_types().size();
			error(call, format("call with % arguments to a function that accepts % arguments", print_number(call.get_arguments().size()), print_number(expected_arguments)));
		}

		if (function_table[new_key] == nullptr) {
			Function* new_function = new Function(new_key.argument_types, nullptr);
			program->add_function(new_function);
			function_table[new_key] = new_function;
			std::vector<const Expression*> environment_arguments;
			for (std::size_t i = 0; i < new_key.old_function->get_environment_arguments(); ++i) {
				const Type* argument_type = closure_type->get_environment_types()[i];
				Argument* argument = new Argument(i, argument_type);
				new_function->get_block()->add_expression(argument);
				environment_arguments.push_back(argument);
			}
			const Expression* new_expression = evaluate(program, function_table, new_key, environment_arguments, new_function->get_block(), new_key.old_function->get_block());
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
		destination_block->add_expression(new_call);
		return new_call;
	}
	const Expression* visit_function_call(const FunctionCall& call) override {
		FunctionCall* new_call = create<FunctionCall>();
		FunctionTableKey new_key;
		for (const Expression* argument: call.get_arguments()) {
			const Expression* new_argument = expression_table[argument];
			new_key.argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		new_key.old_function = call.get_function();

		if (function_table[new_key] == nullptr) {
			Function* new_function = new Function(new_key.argument_types, new_key.old_function->get_return_type());
			program->add_function(new_function);
			function_table[new_key] = new_function;
			const Expression* new_expression = evaluate(program, function_table, new_key, new_function->get_block(), new_key.old_function->get_block());
		}
		new_call->set_type(function_table[new_key]->get_return_type());
		new_call->set_function(function_table[new_key]);
		return new_call;
	}
	void ensure_argument_count(const Intrinsic& intrinsic, std::size_t argument_count) {
		if (intrinsic.get_arguments().size() != argument_count) {
			error(intrinsic, format("% takes % %", intrinsic.get_name(), print_number(argument_count), argument_count == 1 ? "argument" : "arguments"));
		}
	}
	void ensure_argument_types(const Intrinsic& intrinsic, std::initializer_list<const Type*> types) {
		ensure_argument_count(intrinsic, types.size());
		std::size_t i = 0;
		for (const Type* type: types) {
			const Expression* argument = expression_table[intrinsic.get_arguments()[i]];
			if (argument->get_type() != type) {
				error(intrinsic, format("argument % of % must be of type %", print_number(i + 1), intrinsic.get_name(), print_type(type)));
			}
			++i;
		}
	}
	const Type* get_element_type(const Intrinsic& intrinsic, const Type* array_type) {
		if (array_type->get_id() == TypeId::ARRAY) {
			return static_cast<const ArrayType*>(array_type)->get_element_type();
		}
		else {
			error(intrinsic, format("first argument of % must be an array", intrinsic.get_name()));
		}
	}
	Intrinsic* create_intrinsic(const Intrinsic& intrinsic, const Type* type = nullptr) {
		Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), type);
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(expression_table[argument]);
		}
		return new_intrinsic;
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		if (intrinsic.name_equals("typeOf")) {
			ensure_argument_count(intrinsic, 1);
			const Expression* expression = expression_table[intrinsic.get_arguments()[0]];
			return create<TypeLiteral>(expression->get_type());
		}
		else if (intrinsic.name_equals("arrayType")) {
			ensure_argument_count(intrinsic, 1);
			const Expression* element_type_expression = expression_table[intrinsic.get_arguments()[0]];
			if (element_type_expression->get_type_id() != TypeId::TYPE) {
				error(intrinsic, "argument of arrayType must be a type");
			}
			const Type* element_type = static_cast<const TypeType*>(element_type_expression->get_type())->get_type();
			return create<TypeLiteral>(TypeInterner::get_array_type(element_type));
		}
		else if (intrinsic.name_equals("structType")) {
			ensure_argument_count(intrinsic, 1);
			const Expression* struct_type_expression = expression_table[intrinsic.get_arguments()[0]];
			if (struct_type_expression->get_type_id() != TypeId::STRUCT) {
				error(intrinsic, "argument of structType must be a struct");
			}
			const StructType* struct_type = static_cast<const StructType*>(struct_type_expression->get_type());
			StructType new_struct_type;
			for (const auto& field: struct_type->get_fields()) {
				const Type* field_type = field.second;
				if (field_type->get_id() != TypeId::TYPE) {
					error(intrinsic, "struct fields must be types");
				}
				const std::string& field_name = field.first;
				new_struct_type.add_field(field_name, static_cast<const TypeType*>(field_type)->get_type());
			}
			return create<TypeLiteral>(TypeInterner::intern(&new_struct_type));
		}
		else if (intrinsic.name_equals("tupleType")) {
			TupleType tuple_type;
			for (const Expression* argument: intrinsic.get_arguments()) {
				if (argument->get_type_id() != TypeId::TYPE) {
					error(intrinsic, "arguments of tupleType must be types");
				}
				tuple_type.add_element_type(static_cast<const TypeType*>(argument->get_type())->get_type());
			}
			return create<TypeLiteral>(TypeInterner::intern(&tuple_type));
		}
		if (intrinsic.name_equals("putChar")) {
			ensure_argument_types(intrinsic, {TypeInterner::get_int_type()});
			return create_intrinsic(intrinsic, TypeInterner::get_void_type());
		}
		else if (intrinsic.name_equals("putStr")) {
			ensure_argument_types(intrinsic, {TypeInterner::get_string_type()});
			return create_intrinsic(intrinsic, TypeInterner::get_void_type());
		}
		else if (intrinsic.name_equals("getChar")) {
			ensure_argument_types(intrinsic, {});
			return create_intrinsic(intrinsic, TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("arrayGet")) {
			ensure_argument_count(intrinsic, 2);
			const Type* array_type = expression_table[intrinsic.get_arguments()[0]]->get_type();
			const Type* element_type = get_element_type(intrinsic, array_type);
			if (expression_table[intrinsic.get_arguments()[1]]->get_type() != TypeInterner::get_int_type()) {
				error(intrinsic, "second argument of arrayGet must be a number");
			}
			return create_intrinsic(intrinsic, element_type);
		}
		else if (intrinsic.name_equals("arrayLength")) {
			ensure_argument_count(intrinsic, 1);
			const Type* array_type = expression_table[intrinsic.get_arguments()[0]]->get_type();
			get_element_type(intrinsic, array_type);
			return create_intrinsic(intrinsic, TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("arraySplice")) {
			if (intrinsic.get_arguments().size() < 3) {
				error(intrinsic, "arraySplice takes at least 3 arguments");
			}
			const Type* array_type = expression_table[intrinsic.get_arguments()[0]]->get_type();
			const Type* element_type = get_element_type(intrinsic, array_type);
			if (expression_table[intrinsic.get_arguments()[1]]->get_type() != TypeInterner::get_int_type()) {
				error(intrinsic, "second argument of arraySplice must be a number");
			}
			if (expression_table[intrinsic.get_arguments()[2]]->get_type() != TypeInterner::get_int_type()) {
				error(intrinsic, "third argument of arraySplice must be a number");
			}
			if (intrinsic.get_arguments().size() == 4) {
				const Expression* argument = expression_table[intrinsic.get_arguments()[3]];
				if (!(argument->get_type() == element_type || argument->get_type() == array_type)) {
					error(intrinsic, format("argument 4 of arraySplice must be of type % or %", print_type(element_type), print_type(array_type)));
				}
			}
			else {
				for (std::size_t i = 3; i < intrinsic.get_arguments().size(); ++i) {
					const Expression* argument = expression_table[intrinsic.get_arguments()[i]];
					if (argument->get_type() != element_type) {
						error(intrinsic, format("argument % of arraySplice must be of type %", print_number(i + 1), print_type(element_type)));
					}
				}
			}
			return create_intrinsic(intrinsic, array_type);
		}
		else if (intrinsic.name_equals("stringPush")) {
			ensure_argument_count(intrinsic, 2);
			if (expression_table[intrinsic.get_arguments()[0]]->get_type() != TypeInterner::get_string_type()) {
				error(intrinsic, "first argument of stringPush must be a string");
			}
			const Type* argument_type = expression_table[intrinsic.get_arguments()[1]]->get_type();
			if (!(argument_type == TypeInterner::get_int_type() || argument_type == TypeInterner::get_string_type())) {
				error(intrinsic, "second argument of stringPush must be a number or a string");
			}
			return create_intrinsic(intrinsic, TypeInterner::get_string_type());
		}
		else if (intrinsic.name_equals("stringIterator")) {
			ensure_argument_types(intrinsic, {TypeInterner::get_string_type()});
			return create_intrinsic(intrinsic, TypeInterner::get_string_iterator_type());
		}
		else if (intrinsic.name_equals("stringIteratorIsValid")) {
			ensure_argument_types(intrinsic, {TypeInterner::get_string_iterator_type()});
			return create_intrinsic(intrinsic, TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("stringIteratorGet")) {
			ensure_argument_types(intrinsic, {TypeInterner::get_string_iterator_type()});
			return create_intrinsic(intrinsic, TypeInterner::get_int_type());
		}
		else if (intrinsic.name_equals("stringIteratorNext")) {
			ensure_argument_types(intrinsic, {TypeInterner::get_string_iterator_type()});
			return create_intrinsic(intrinsic, TypeInterner::get_string_iterator_type());
		}
		else if (intrinsic.name_equals("copy")) {
			const Type* type = expression_table[intrinsic.get_arguments()[0]]->get_type();
			return create_intrinsic(intrinsic, type);
		}
		else if (intrinsic.name_equals("free")) {
			return create_intrinsic(intrinsic, TypeInterner::get_void_type());
		}
		else {
			return create_intrinsic(intrinsic, TypeInterner::get_void_type());
		}
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
		result = expression_table[return_.get_expression()];
		if (omit_return) {
			return result;
		}
		else {
			return create<Return>(result);
		}
	}
	const Expression* visit_type_literal(const TypeLiteral& type_literal) override {
		const Type* type = static_cast<const TypeType*>(type_literal.get_type())->get_type();
		return create<TypeLiteral>(type);
	}
	const Expression* visit_type_assert(const TypeAssert& type_assert) override {
		const Expression* expression = expression_table[type_assert.get_expression()];
		const Expression* type_expression = expression_table[type_assert.get_type()];
		if (type_expression->get_type_id() != TypeId::TYPE) {
			error(type_assert, "expression is not a type");
		}
		const Type* type = static_cast<const TypeType*>(type_expression->get_type())->get_type();
		if (expression->get_type() != type) {
			error(type_assert, format("expression does not have the declared type %", print_type(type)));
		}
		return nullptr;
	}
	const Expression* visit_return_type(const ReturnType& return_type) override {
		const Expression* type_expression = expression_table[return_type.get_type()];
		if (type_expression->get_type_id() != TypeId::TYPE) {
			error(return_type, "return type must be a type");
		}
		const Type* type = static_cast<const TypeType*>(type_expression->get_type())->get_type();
		function_table[key]->set_return_type(type);
		return nullptr;
	}
	static std::unique_ptr<Program> run(const Program& program) {
		const Function* main_function = program.get_main_function();
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		Function* new_function = new Function(TypeInterner::get_void_type());
		new_program->add_function(new_function);
		evaluate(new_program.get(), function_table, FunctionTableKey(main_function), new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// lower closures and structs to tuples
class Lowering: public Visitor<const Expression*> {
	using FunctionTable = std::map<const Function*, Function*>;
	FunctionTable& function_table;
	using ExpressionTable = std::map<const Expression*, const Expression*>;
	ExpressionTable& expression_table;
	Block* destination_block;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		destination_block->add_expression(expression);
		return expression;
	}
	static const Type* transform_type(const Type* type) {
		if (type->get_id() == TypeId::CLOSURE) {
			TupleType tuple_type;
			for (const Type* environment_type: static_cast<const ClosureType*>(type)->get_environment_types()) {
				tuple_type.add_element_type(transform_type(environment_type));
			}
			return TypeInterner::intern(&tuple_type);
		}
		if (type->get_id() == TypeId::STRUCT) {
			TupleType tuple_type;
			for (const auto& field: static_cast<const StructType*>(type)->get_fields()) {
				tuple_type.add_element_type(transform_type(field.second));
			}
			return TypeInterner::intern(&tuple_type);
		}
		if (type->get_id() == TypeId::ARRAY) {
			const Type* element_type = static_cast<const ArrayType*>(type)->get_element_type();
			return TypeInterner::get_array_type(transform_type(element_type));
		}
		return type;
	}
public:
	Lowering(FunctionTable& function_table, ExpressionTable& expression_table, Block* destination_block): function_table(function_table), expression_table(expression_table), destination_block(destination_block) {}
	static void evaluate(FunctionTable& function_table, ExpressionTable& expression_table, Block* destination_block, const Block& source_block) {
		Lowering lowering(function_table, expression_table, destination_block);
		for (const Expression* expression: source_block) {
			const Expression* new_expression = visit(lowering, expression);
			if (new_expression) {
				expression_table[expression] = new_expression;
			}
		}
	}
	static void evaluate(FunctionTable& function_table, Block* destination_block, const Block& source_block) {
		ExpressionTable expression_table;
		evaluate(function_table, expression_table, destination_block, source_block);
	}
	void evaluate(Block* destination_block, const Block& source_block) {
		evaluate(function_table, expression_table, destination_block, source_block);
	}
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	const Expression* visit_array_literal(const ArrayLiteral& array_literal) override {
		ArrayLiteral* new_array_literal = create<ArrayLiteral>(transform_type(array_literal.get_type()));
		for (const Expression* element: array_literal.get_elements()) {
			new_array_literal->add_element(expression_table[element]);
		}
		return new_array_literal;
	}
	const Expression* visit_string_literal(const StringLiteral& string_literal) override {
		return create<StringLiteral>(string_literal.get_value());
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, transform_type(if_.get_type()));
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		TupleLiteral* new_tuple_literal = create<TupleLiteral>(transform_type(tuple_literal.get_type()));
		for (const Expression* element: tuple_literal.get_elements()) {
			new_tuple_literal->add_element(expression_table[element]);
		}
		return new_tuple_literal;
	}
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		return create<TupleAccess>(tuple, tuple_access.get_index(), transform_type(tuple_access.get_type()));
	}
	const Expression* visit_struct_literal(const StructLiteral& struct_literal) override {
		TupleLiteral* tuple_literal = create<TupleLiteral>(transform_type(struct_literal.get_type()));
		for (const auto& field: struct_literal.get_fields()) {
			tuple_literal->add_element(expression_table[field.second]);
		}
		return tuple_literal;
	}
	const Expression* visit_struct_access(const StructAccess& struct_access) override {
		const Expression* tuple = expression_table[struct_access.get_struct()];
		const StructType* struct_type = static_cast<const StructType*>(struct_access.get_struct()->get_type());
		const std::size_t index = struct_type->get_index(struct_access.get_field_name());
		return create<TupleAccess>(tuple, index, transform_type(struct_access.get_type()));
	}
	const Expression* visit_closure(const Closure& closure) override {
		TupleLiteral* tuple_literal = create<TupleLiteral>(transform_type(closure.get_type()));
		for (const Expression* expression: closure.get_environment_expressions()) {
			tuple_literal->add_element(expression_table[expression]);
		}
		return tuple_literal;
	}
	const Expression* visit_closure_access(const ClosureAccess& closure_access) override {
		const Expression* tuple = expression_table[closure_access.get_closure()];
		return create<TupleAccess>(tuple, closure_access.get_index(), transform_type(closure_access.get_type()));
	}
	Expression* visit_argument(const Argument& argument) override {
		return create<Argument>(argument.get_index(), transform_type(argument.get_type()));
	}
	const Expression* visit_function_call(const FunctionCall& call) override {
		FunctionCall* new_call = create<FunctionCall>(transform_type(call.get_type()));
		for (const Expression* argument: call.get_arguments()) {
			new_call->add_argument(expression_table[argument]);
		}
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), transform_type(intrinsic.get_type()));
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
	const Expression* visit_type_literal(const TypeLiteral& type_literal) override {
		const Type* type = static_cast<const TypeType*>(type_literal.get_type())->get_type();
		return create<TypeLiteral>(type);
	}
	static std::unique_ptr<Program> run(const Program& program) {
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		for (const Function* function: program) {
			std::vector<const Type*> argument_types;
			for (const Type* type: function->get_argument_types()) {
				argument_types.push_back(transform_type(type));
			}
			Function* new_function = new Function(argument_types, transform_type(function->get_return_type()));
			new_program->add_function(new_function);
			function_table[function] = new_function;
		}
		for (const Function* function: program) {
			Function* new_function = function_table[function];
			evaluate(function_table, new_function->get_block(), function->get_block());
		}
		return new_program;
	}
};

// dead expression elimination
class DeadCodeElimination {
	using FunctionTable = std::map<const Function*, Function*>;
	using ExpressionTable = std::map<const Expression*, const Expression*>;
	using UsageTable = std::map<const Expression*, bool>;
	// TODO: remove unused arguments
	class IsArgument: public Visitor<bool> {
	public:
		bool visit_argument(const Argument& argument) override {
			return true;
		}
	};
	class Mark: public Visitor<void> {
		UsageTable& usage_table;
		void mark(const Expression* expression) {
			if (usage_table.count(expression) > 0) {
				return;
			}
			usage_table[expression] = true;
			visit(*this, expression);
		}
	public:
		Mark(UsageTable& usage_table): usage_table(usage_table) {}
		static void evaluate(UsageTable& usage_table, const Block& source_block) {
			Mark mark(usage_table);
			mark.mark(source_block.get_last());
		}
		void visit_binary_expression(const BinaryExpression& binary_expression) override {
			mark(binary_expression.get_left());
			mark(binary_expression.get_right());
		}
		void visit_array_literal(const ArrayLiteral& array_literal) override {
			for (const Expression* element: array_literal.get_elements()) {
				mark(element);
			}
		}
		void visit_if(const If& if_) override {
			mark(if_.get_condition());
			evaluate(usage_table, if_.get_then_block());
			evaluate(usage_table, if_.get_else_block());
		}
		void visit_tuple_literal(const TupleLiteral& tuple_literal) override {
			for (const Expression* element: tuple_literal.get_elements()) {
				mark(element);
			}
		}
		void visit_tuple_access(const TupleAccess& tuple_access) override {
			mark(tuple_access.get_tuple());
		}
		void visit_function_call(const FunctionCall& call) override {
			for (const Expression* argument: call.get_arguments()) {
				mark(argument);
			}
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {
			for (const Expression* argument: intrinsic.get_arguments()) {
				mark(argument);
			}
		}
		void visit_bind(const Bind& bind) override {
			mark(bind.get_left());
			mark(bind.get_right());
		}
		void visit_return(const Return& return_) override {
			mark(return_.get_expression());
		}
	};
	class Sweep: public Visitor<const Expression*> {
		FunctionTable& function_table;
		const UsageTable& usage_table;
		ExpressionTable& expression_table;
		Block* destination_block;
		template <class T, class... A> T* create(A&&... arguments) {
			T* expression = new T(std::forward<A>(arguments)...);
			destination_block->add_expression(expression);
			return expression;
		}
	public:
		Sweep(FunctionTable& function_table, const UsageTable& usage_table, ExpressionTable& expression_table, Block* destination_block): function_table(function_table), usage_table(usage_table), expression_table(expression_table), destination_block(destination_block) {}
		static void evaluate(FunctionTable& function_table, const UsageTable& usage_table, ExpressionTable& expression_table, Block* destination_block, const Block& source_block) {
			IsArgument is_argument;
			Sweep sweep(function_table, usage_table, expression_table, destination_block);
			for (const Expression* expression: source_block) {
				if (usage_table.count(expression) > 0 || visit(is_argument, expression)) {
					expression_table[expression] = visit(sweep, expression);
				}
			}
		}
		static void evaluate(FunctionTable& function_table, const UsageTable& usage_table, Block* destination_block, const Block& source_block) {
			ExpressionTable expression_table;
			evaluate(function_table, usage_table, expression_table, destination_block, source_block);
		}
		void evaluate(Block* destination_block, const Block& source_block) {
			evaluate(function_table, usage_table, expression_table, destination_block, source_block);
		}
		const Expression* visit_int_literal(const IntLiteral& int_literal) override {
			return create<IntLiteral>(int_literal.get_value());
		}
		const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
			const Expression* left = expression_table[binary_expression.get_left()];
			const Expression* right = expression_table[binary_expression.get_right()];
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		const Expression* visit_array_literal(const ArrayLiteral& array_literal) override {
			ArrayLiteral* new_array_literal = create<ArrayLiteral>(array_literal.get_type());
			for (const Expression* element: array_literal.get_elements()) {
				new_array_literal->add_element(expression_table[element]);
			}
			return new_array_literal;
		}
		const Expression* visit_string_literal(const StringLiteral& string_literal) override {
			return create<StringLiteral>(string_literal.get_value());
		}
		const Expression* visit_if(const If& if_) override {
			const Expression* condition = expression_table[if_.get_condition()];
			If* new_if = create<If>(condition, if_.get_type());
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			return new_if;
		}
		const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
			TupleLiteral* new_tuple_literal = create<TupleLiteral>(tuple_literal.get_type());
			for (const Expression* element: tuple_literal.get_elements()) {
				new_tuple_literal->add_element(expression_table[element]);
			}
			return new_tuple_literal;
		}
		const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
			const Expression* tuple = expression_table[tuple_access.get_tuple()];
			return create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
		}
		const Expression* visit_argument(const Argument& argument) override {
			return create<Argument>(argument.get_index(), argument.get_type());
		}
		const Expression* visit_function_call(const FunctionCall& call) override {
			FunctionCall* new_call = create<FunctionCall>(call.get_type());
			for (const Expression* argument: call.get_arguments()) {
				new_call->add_argument(expression_table[argument]);
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
	};
public:
	static std::unique_ptr<Program> run(const Program& program) {
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		for (const Function* function: program) {
			Function* new_function = new Function(function->get_argument_types(), function->get_return_type());
			new_program->add_function(new_function);
			function_table[function] = new_function;
		}
		for (const Function* function: program) {
			Function* new_function = function_table[function];
			UsageTable usage_table;
			Mark::evaluate(usage_table, function->get_block());
			Sweep::evaluate(function_table, usage_table, new_function->get_block(), function->get_block());
		}
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
	using ExpressionTable = std::map<const Expression*, const Expression*>;
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
		void visit_function_call(const FunctionCall& call) override {
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
		FunctionTable& function_table;
		const Function* function;
		const std::vector<const Expression*>& arguments;
		ExpressionTable& expression_table;
		Block* destination_block;
		bool omit_return;
		const Expression* result = nullptr;
		template <class T, class... A> T* create(A&&... arguments) {
			T* expression = new T(std::forward<A>(arguments)...);
			destination_block->add_expression(expression);
			return expression;
		}
	public:
		Replace(Program* program, FunctionTable& function_table, const Function* function, const std::vector<const Expression*>& arguments, ExpressionTable& expression_table, Block* destination_block, bool omit_return): program(program), function_table(function_table), function(function), arguments(arguments), expression_table(expression_table), destination_block(destination_block), omit_return(omit_return) {}
		static const Expression* evaluate(Program* program, FunctionTable& function_table, const Function* function, const std::vector<const Expression*>& arguments, ExpressionTable& expression_table, Block* destination_block, const Block& source_block, bool omit_return) {
			Replace replace(program, function_table, function, arguments, expression_table, destination_block, omit_return);
			for (const Expression* expression: source_block) {
				const Expression* new_expression = visit(replace, expression);
				if (new_expression) {
					expression_table[expression] = new_expression;
				}
			}
			return replace.result;
		}
		// main function
		static const Expression* evaluate(Program* program, FunctionTable& function_table, const Function* function, Block* destination_block, const Block& source_block) {
			std::vector<const Expression*> arguments;
			ExpressionTable expression_table;
			return evaluate(program, function_table, function, arguments, expression_table, destination_block, source_block, false);
		}
		// inlined functions
		const Expression* evaluate(const Function* function, const std::vector<const Expression*>& arguments, Block* destination_block, const Block& source_block) {
			ExpressionTable expression_table;
			return evaluate(program, function_table, function, arguments, expression_table, destination_block, source_block, true);
		}
		// non-inlined functions
		const Expression* evaluate(const Function* function, Block* destination_block, const Block& source_block) {
			std::vector<const Expression*> arguments;
			ExpressionTable expression_table;
			return evaluate(program, function_table, function, arguments, expression_table, destination_block, source_block, false);
		}
		// if blocks
		const Expression* evaluate(Block* destination_block, const Block& source_block) {
			return evaluate(program, function_table, function, arguments, expression_table, destination_block, source_block, false);
		}
		const Expression* visit_int_literal(const IntLiteral& int_literal) override {
			return create<IntLiteral>(int_literal.get_value());
		}
		const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
			const Expression* left = expression_table[binary_expression.get_left()];
			const Expression* right = expression_table[binary_expression.get_right()];
			return create<BinaryExpression>(binary_expression.get_operation(), left, right);
		}
		const Expression* visit_array_literal(const ArrayLiteral& array_literal) override {
			ArrayLiteral* new_array_literal = create<ArrayLiteral>(array_literal.get_type());
			for (const Expression* element: array_literal.get_elements()) {
				new_array_literal->add_element(expression_table[element]);
			}
			return new_array_literal;
		}
		const Expression* visit_string_literal(const StringLiteral& string_literal) override {
			return create<StringLiteral>(string_literal.get_value());
		}
		const Expression* visit_if(const If& if_) override {
			const Expression* condition = expression_table[if_.get_condition()];
			If* new_if = create<If>(condition, if_.get_type());
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			return new_if;
		}
		const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
			TupleLiteral* new_tuple_literal = create<TupleLiteral>(tuple_literal.get_type());
			for (const Expression* element: tuple_literal.get_elements()) {
				new_tuple_literal->add_element(expression_table[element]);
			}
			return new_tuple_literal;
		}
		const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
			const Expression* tuple = expression_table[tuple_access.get_tuple()];
			return create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
		}
		const Expression* visit_argument(const Argument& argument) override {
			if (function_table[function].should_inline()) {
				return arguments[argument.get_index()];
			}
			else {
				return create<Argument>(argument.get_index(), argument.get_type());
			}
		}
		const Expression* visit_function_call(const FunctionCall& call) override {
			if (function_table[call.get_function()].should_inline()) {
				std::vector<const Expression*> new_arguments;
				for (const Expression* argument: call.get_arguments()) {
					new_arguments.push_back(expression_table[argument]);
				}
				return evaluate(call.get_function(), new_arguments, destination_block, call.get_function()->get_block());
			}
			else {
				FunctionCall* new_call = create<FunctionCall>(call.get_type());
				for (const Expression* argument: call.get_arguments()) {
					new_call->add_argument(expression_table[argument]);
				}
				if (function_table[call.get_function()].new_function == nullptr) {
					Function* new_function = new Function(call.get_function()->get_argument_types(), call.get_function()->get_return_type());
					program->add_function(new_function);
					function_table[call.get_function()].new_function = new_function;
					evaluate(call.get_function(), new_function->get_block(), call.get_function()->get_block());
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
			result = expression_table[return_.get_expression()];
			if (omit_return) {
				return result;
			}
			else {
				return create<Return>(result);
			}
		}
		const Expression* visit_type_literal(const TypeLiteral& type_literal) override {
			const Type* type = static_cast<const TypeType*>(type_literal.get_type())->get_type();
			return create<TypeLiteral>(type);
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
		Function* new_function = new Function(main_function->get_return_type());
		new_program->add_function(new_function);
		Replace::evaluate(new_program.get(), function_table, main_function, new_function->get_block(), main_function->get_block());
		return new_program;
	}
};

// remove empty tuples
class Pass3: public Visitor<const Expression*> {
	using FunctionTable = std::map<const Function*, Function*>;
	FunctionTable& function_table;
	const Function* function;
	using ExpressionTable = std::map<const Expression*, const Expression*>;
	ExpressionTable& expression_table;
	Block* destination_block;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		destination_block->add_expression(expression);
		return expression;
	}
	static bool is_empty_tuple(const Type* type) {
		if (type->get_id() == TypeId::TUPLE) {
			for (const Type* element_type: static_cast<const TupleType*>(type)->get_element_types()) {
				if (!is_empty_tuple(element_type)) {
					return false;
				}
			}
			return true;
		}
		if (type->get_id() == TypeId::TYPE) {
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
	static const Type* transform_type(const Type* type) {
		if (type->get_id() == TypeId::TUPLE) {
			TupleType new_type;
			for (const Type* element_type: static_cast<const TupleType*>(type)->get_element_types()) {
				if (!is_empty_tuple(element_type)) {
					new_type.add_element_type(transform_type(element_type));
				}
			}
			return TypeInterner::intern(&new_type);
		}
		if (type->get_id() == TypeId::ARRAY) {
			const Type* element_type = static_cast<const ArrayType*>(type)->get_element_type();
			return TypeInterner::get_array_type(transform_type(element_type));
		}
		return type;
	}
public:
	Pass3(FunctionTable& function_table, const Function* function, ExpressionTable& expression_table, Block* destination_block): function_table(function_table), function(function),  expression_table(expression_table), destination_block(destination_block) {}
	static void evaluate(FunctionTable& function_table, const Function* function, ExpressionTable& expression_table, Block* destination_block, const Block& source_block) {
		Pass3 pass3(function_table, function, expression_table, destination_block);
		for (const Expression* expression: source_block) {
			if (!is_empty_tuple(expression)) {
				expression_table[expression] = visit(pass3, expression);
			}
		}
	}
	static void evaluate(FunctionTable& function_table, const Function* function, Block* destination_block, const Block& source_block) {
		ExpressionTable expression_table;
		evaluate(function_table, function, expression_table, destination_block, source_block);
	}
	void evaluate(Block* destination_block, const Block& source_block) {
		evaluate(function_table, function, expression_table, destination_block, source_block);
	}
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	const Expression* visit_array_literal(const ArrayLiteral& array_literal) override {
		ArrayLiteral* new_array_literal = create<ArrayLiteral>(transform_type(array_literal.get_type()));
		for (const Expression* element: array_literal.get_elements()) {
			new_array_literal->add_element(expression_table[element]);
		}
		return new_array_literal;
	}
	const Expression* visit_string_literal(const StringLiteral& string_literal) override {
		return create<StringLiteral>(string_literal.get_value());
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, transform_type(if_.get_type()));
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		TupleLiteral* new_tuple_literal = create<TupleLiteral>(transform_type(tuple_literal.get_type()));
		for (const Expression* element: tuple_literal.get_elements()) {
			if (!is_empty_tuple(element)) {
				new_tuple_literal->add_element(expression_table[element]);
			}
		}
		return new_tuple_literal;
	}
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		const std::vector<const Type*>& element_types = static_cast<const TupleType*>(tuple_access.get_tuple()->get_type())->get_element_types();
		const std::size_t index = adjust_index(element_types, tuple_access.get_index());
		return create<TupleAccess>(tuple, index, transform_type(tuple_access.get_type()));
	}
	Expression* visit_argument(const Argument& argument) override {
		const std::size_t index = adjust_index(function->get_argument_types(), argument.get_index());
		return create<Argument>(index, transform_type(argument.get_type()));
	}
	const Expression* visit_function_call(const FunctionCall& call) override {
		FunctionCall* new_call = create<FunctionCall>(transform_type(call.get_type()));
		for (const Expression* argument: call.get_arguments()) {
			if (!is_empty_tuple(argument)) {
				new_call->add_argument(expression_table[argument]);
			}
		}
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = create<Intrinsic>(intrinsic.get_name(), transform_type(intrinsic.get_type()));
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
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		for (const Function* function: program) {
			if (is_empty_tuple(function->get_return_type())) {
				continue;
			}
			std::vector<const Type*> argument_types;
			for (const Type* type: function->get_argument_types()) {
				if (!is_empty_tuple(type)) {
					argument_types.push_back(transform_type(type));
				}
			}
			Function* new_function = new Function(argument_types, transform_type(function->get_return_type()));
			new_program->add_function(new_function);
			function_table[function] = new_function;
		}
		for (const Function* function: program) {
			if (is_empty_tuple(function->get_return_type())) {
				continue;
			}
			Function* new_function = function_table[function];
			evaluate(function_table, function, new_function->get_block(), function->get_block());
		}
		return new_program;
	}
};

// memory management
class Pass4: public Visitor<const Expression*> {
	struct UsageTable {
		std::map<const Block*, std::map<const Expression*, std::pair<const Expression*, std::size_t>>> usages;
		std::map<const Block*, std::vector<const Expression*>> frees;
		std::map<const Expression*, std::size_t> levels;
	};
	static bool is_managed(const Expression* expression) {
		const TypeId type_id = expression->get_type_id();
		return type_id == TypeId::TUPLE || type_id == TypeId::ARRAY || type_id == TypeId::STRING || type_id == TypeId::STRING_ITERATOR;
	}
	class UsageAnalysis1: public Visitor<void> {
		UsageTable& usage_table;
		const Block* block;
		std::size_t level;
	public:
		UsageAnalysis1(UsageTable& usage_table, const Block* block, std::size_t level): usage_table(usage_table), block(block), level(level) {}
		void add_usage(const Expression* resource, const Expression* consumer, std::size_t argument_index) {
			usage_table.usages[block][resource] = std::make_pair(consumer, argument_index);
		}
		void propagate_usages(const Block* block, const Expression* consumer) {
			for (auto& entry: usage_table.usages[block]) {
				const Expression* resource = entry.first;
				if (usage_table.levels[resource] <= level) {
					add_usage(resource, consumer, 0);
				}
			}
		}
		static void evaluate(UsageTable& usage_table, const Block& block, std::size_t level = 1) {
			UsageAnalysis1 usage_analysis1(usage_table, &block, level);
			for (const Expression* expression: block) {
				if (is_managed(expression)) {
					usage_table.levels[expression] = level;
				}
				visit(usage_analysis1, expression);
			}
		}
		void evaluate(const Block& block) {
			evaluate(usage_table, block, level + 1);
		}
		void visit_array_literal(const ArrayLiteral& array_literal) override {
			for (std::size_t i = 0; i < array_literal.get_elements().size(); ++i) {
				const Expression* element = array_literal.get_elements()[i];
				if (is_managed(element)) {
					add_usage(element, &array_literal, i);
				}
			}
		}
		void visit_if(const If& if_) override {
			evaluate(if_.get_then_block());
			evaluate(if_.get_else_block());
			propagate_usages(&if_.get_then_block(), &if_);
			propagate_usages(&if_.get_else_block(), &if_);
		}
		void visit_tuple_literal(const TupleLiteral& tuple_literal) override {
			for (std::size_t i = 0; i < tuple_literal.get_elements().size(); ++i) {
				const Expression* element = tuple_literal.get_elements()[i];
				if (is_managed(element)) {
					add_usage(element, &tuple_literal, i);
				}
			}
		}
		void visit_tuple_access(const TupleAccess& tuple_access) override {
			add_usage(tuple_access.get_tuple(), &tuple_access, 0);
		}
		void visit_function_call(const FunctionCall& call) override {
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				const Expression* argument = call.get_arguments()[i];
				if (is_managed(argument)) {
					add_usage(argument, &call, i);
				}
			}
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {
			for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
				const Expression* argument = intrinsic.get_arguments()[i];
				if (is_managed(argument)) {
					add_usage(argument, &intrinsic, i);
				}
			}
		}
		void visit_return(const Return& return_) override {
			if (is_managed(return_.get_expression())) {
				add_usage(return_.get_expression(), &return_, 0);
			}
		}
	};
	class UsageAnalysis2: public Visitor<void> {
		UsageTable& usage_table;
		const Block* block;
		std::size_t level;
	public:
		UsageAnalysis2(UsageTable& usage_table, const Block* block, std::size_t level): usage_table(usage_table), block(block), level(level) {}
		bool is_last_use(const Expression* resource, const Expression* consumer, std::size_t argument_index) {
			return usage_table.usages[block][resource] == std::make_pair(consumer, argument_index);
		}
		void remove_invalid_usages(const Block* block, const Expression* consumer) {
			auto iterator = usage_table.usages[block].begin();
			while (iterator != usage_table.usages[block].end()) {
				const Expression* resource = iterator->first;
				if (usage_table.levels[resource] <= level && !is_last_use(resource, consumer, 0)) {
					iterator = usage_table.usages[block].erase(iterator);
				}
				else {
					++iterator;
				}
			}
		}
		void ensure_frees(const Block* source_block, const Block* target_block) {
			for (auto& entry: usage_table.usages[source_block]) {
				const Expression* resource = entry.first;
				if (usage_table.usages[target_block].count(resource) == 0 && usage_table.levels[resource] < level + 1) {
					// if a resource from the source block has no usage in the target block, add it to the free list
					usage_table.frees[target_block].push_back(resource);
				}
			}
		}
		static void evaluate(UsageTable& usage_table, const Block& block, std::size_t level = 1) {
			UsageAnalysis2 usage_analysis2(usage_table, &block, level);
			for (const Expression* expression: block) {
				visit(usage_analysis2, expression);
			}
		}
		void evaluate(const Block& block) {
			evaluate(usage_table, block, level + 1);
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
	using FunctionTable = std::map<const Function*, Function*>;
	FunctionTable& function_table;
	UsageTable& usage_table;
	using ExpressionTable = std::map<const Expression*, const Expression*>;
	ExpressionTable& expression_table;
	Block* destination_block;
	const Block* source_block;
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		destination_block->add_expression(expression);
		return expression;
	}
	bool is_last_use(const Expression* resource, const Expression* consumer, std::size_t argument_index) {
		return usage_table.usages[source_block][resource] == std::make_pair(consumer, argument_index);
	}
	bool is_unused(const Expression* resource) {
		return usage_table.usages[source_block].count(resource) == 0;
	}
	bool is_borrowed(const Intrinsic& intrinsic) {
		return intrinsic.name_equals("putStr") || intrinsic.name_equals("arrayGet") || intrinsic.name_equals("arrayLength") || intrinsic.name_equals("stringIteratorIsValid") || intrinsic.name_equals("stringIteratorGet");
	}
	const Expression* copy(const Expression* resource) {
		Intrinsic* copy_intrinsic = create<Intrinsic>("copy", resource->get_type());
		copy_intrinsic->add_argument(resource);
		return copy_intrinsic;
	}
	void free(const Expression* resource) {
		Intrinsic* free_intrinsic = create<Intrinsic>("free", TypeInterner::get_void_type());
		free_intrinsic->add_argument(resource);
	}
public:
	Pass4(FunctionTable& function_table, UsageTable& usage_table, ExpressionTable& expression_table, Block* destination_block, const Block* source_block): function_table(function_table), usage_table(usage_table), expression_table(expression_table), destination_block(destination_block), source_block(source_block) {}
	static void evaluate(FunctionTable& function_table, UsageTable& usage_table, ExpressionTable& expression_table, Block* destination_block, const Block& source_block) {
		Pass4 pass4(function_table, usage_table, expression_table, destination_block, &source_block);
		for (const Expression* expression: usage_table.frees[&source_block]) {
			pass4.free(expression_table[expression]);
		}
		for (const Expression* expression: source_block) {
			expression_table[expression] = visit(pass4, expression);
		}
	}
	static void evaluate(FunctionTable& function_table, UsageTable& usage_table, Block* destination_block, const Block& source_block) {
		ExpressionTable expression_table;
		evaluate(function_table, usage_table, expression_table, destination_block, source_block);
	}
	void evaluate(Block* destination_block, const Block& source_block) {
		evaluate(function_table, usage_table, expression_table, destination_block, source_block);
	}
	const Expression* visit_int_literal(const IntLiteral& int_literal) override {
		return create<IntLiteral>(int_literal.get_value());
	}
	const Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = expression_table[binary_expression.get_left()];
		const Expression* right = expression_table[binary_expression.get_right()];
		return create<BinaryExpression>(binary_expression.get_operation(), left, right);
	}
	const Expression* visit_array_literal(const ArrayLiteral& array_literal) override {
		ArrayLiteral* new_array_literal = new ArrayLiteral(array_literal.get_type());
		for (std::size_t i = 0; i < array_literal.get_elements().size(); ++i) {
			const Expression* element = array_literal.get_elements()[i];
			if (is_managed(element) && !is_last_use(element, &array_literal, i)) {
				new_array_literal->add_element(copy(expression_table[element]));
			}
			else {
				new_array_literal->add_element(expression_table[element]);
			}
		}
		destination_block->add_expression(new_array_literal);
		return new_array_literal;
	}
	const Expression* visit_string_literal(const StringLiteral& string_literal) override {
		return create<StringLiteral>(string_literal.get_value());
	}
	const Expression* visit_if(const If& if_) override {
		const Expression* condition = expression_table[if_.get_condition()];
		If* new_if = create<If>(condition, if_.get_type());
		evaluate(new_if->get_then_block(), if_.get_then_block());
		evaluate(new_if->get_else_block(), if_.get_else_block());
		return new_if;
	}
	const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		TupleLiteral* new_tuple_literal = new TupleLiteral(tuple_literal.get_type());
		for (std::size_t i = 0; i < tuple_literal.get_elements().size(); ++i) {
			const Expression* element = tuple_literal.get_elements()[i];
			if (is_managed(element) && !is_last_use(element, &tuple_literal, i)) {
				new_tuple_literal->add_element(copy(expression_table[element]));
			}
			else {
				new_tuple_literal->add_element(expression_table[element]);
			}
		}
		destination_block->add_expression(new_tuple_literal);
		return new_tuple_literal;
	}
	const Expression* visit_tuple_access(const TupleAccess& tuple_access) override {
		const Expression* tuple = expression_table[tuple_access.get_tuple()];
		const Expression* new_tuple_access = create<TupleAccess>(tuple, tuple_access.get_index(), tuple_access.get_type());
		if (is_managed(&tuple_access)) {
			new_tuple_access = copy(new_tuple_access);
		}
		if (is_last_use(tuple_access.get_tuple(), &tuple_access, 0)) {
			free(tuple);
		}
		return new_tuple_access;
	}
	const Expression* visit_argument(const Argument& argument) override {
		Argument* new_argument = create<Argument>(argument.get_index(), argument.get_type());
		if (is_managed(&argument) && is_unused(&argument)) {
			free(new_argument);
		}
		return new_argument;
	}
	const Expression* visit_function_call(const FunctionCall& call) override {
		FunctionCall* new_call = new FunctionCall(call.get_type());
		for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
			const Expression* argument = call.get_arguments()[i];
			if (is_managed(argument) && !is_last_use(argument, &call, i)) {
				new_call->add_argument(copy(expression_table[argument]));
			}
			else {
				new_call->add_argument(expression_table[argument]);
			}
		}
		destination_block->add_expression(new_call);
		new_call->set_function(function_table[call.get_function()]);
		return new_call;
	}
	const Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name(), intrinsic.get_type());
		for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
			const Expression* argument = intrinsic.get_arguments()[i];
			if (is_managed(argument) && !is_borrowed(intrinsic) && !is_last_use(argument, &intrinsic, i)) {
				new_intrinsic->add_argument(copy(expression_table[argument]));
			}
			else {
				new_intrinsic->add_argument(expression_table[argument]);
			}
		}
		destination_block->add_expression(new_intrinsic);
		const Expression* result = new_intrinsic;
		if (is_managed(&intrinsic) && intrinsic.name_equals("arrayGet")) {
			result = copy(result);
		}
		for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
			const Expression* argument = intrinsic.get_arguments()[i];
			if (is_managed(argument) && is_borrowed(intrinsic) && is_last_use(argument, &intrinsic, i)) {
				free(expression_table[argument]);
			}
		}
		return result;
	}
	const Expression* visit_bind(const Bind& bind) override {
		const Expression* left = expression_table[bind.get_left()];
		const Expression* right = expression_table[bind.get_right()];
		return create<Bind>(left, right);
	}
	const Expression* visit_return(const Return& return_) override {
		const Expression* expression = return_.get_expression();
		if (is_managed(expression) && !is_last_use(expression, &return_, 0)) {
			return create<Return>(copy(expression_table[expression]));
		}
		else {
			return create<Return>(expression_table[expression]);
		}
	}
	static std::unique_ptr<Program> run(const Program& program) {
		std::unique_ptr<Program> new_program = std::make_unique<Program>();
		FunctionTable function_table;
		for (const Function* function: program) {
			Function* new_function = new Function(function->get_argument_types(), function->get_return_type());
			new_program->add_function(new_function);
			function_table[function] = new_function;
		}
		for (const Function* function: program) {
			Function* new_function = function_table[function];
			UsageTable usage_table;
			UsageAnalysis1::evaluate(usage_table, function->get_block());
			UsageAnalysis2::evaluate(usage_table, function->get_block());
			evaluate(function_table, usage_table, new_function->get_block(), function->get_block());
		}
		return new_program;
	}
};

class TailCallData {
public:
	std::map<const Expression*, bool> tail_call_expressions;
	std::map<const Function*, bool> tail_call_functions;
	bool is_tail_call(const Expression* expression) const {
		return tail_call_expressions.count(expression) > 0;
	}
	bool has_tail_call(const Function* function) const {
		return tail_call_functions.count(function) > 0;
	}
};

// tail call optimization
class Pass5: public Visitor<void> {
	const Function* function;
	TailCallData& data;
public:
	Pass5(const Function* function, TailCallData& data): function(function), data(data) {}
	void evaluate(const Block& block) {
		visit(*this, block.get_last());
	}
	void visit_if(const If& if_) override {
		evaluate(if_.get_then_block());
		evaluate(if_.get_else_block());
	}
	void visit_function_call(const FunctionCall& call) override {
		if (call.get_function() == function) {
			data.tail_call_expressions[&call] = true;
			data.tail_call_functions[function] = true;
		}
	}
	void visit_bind(const Bind& bind) override {
		const Expression* right = bind.get_right();
		if (right->next_expression == &bind) {
			visit(*this, right);
		}
	}
	void visit_return(const Return& return_) override {
		const Expression* expression = return_.get_expression();
		if (expression->next_expression == &return_) {
			visit(*this, expression);
		}
	}
	static void run(const Program& program, TailCallData& data) {
		for (const Function* function: program) {
			Pass5 pass5(function, data);
			pass5.evaluate(function->get_block());
		}
	}
};
