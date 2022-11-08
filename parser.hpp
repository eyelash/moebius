#pragma once

#include "printer.hpp"
#include "ast.hpp"
#include <map>
#include <cstdlib>

struct BinaryOperator {
	const char* string;
	using Create = Expression* (*)(const Expression* left, const Expression* right);
	Create create;
	constexpr BinaryOperator(const char* string, Create create): string(string), create(create) {}
};

using OperatorLevel = std::initializer_list<BinaryOperator>;

constexpr std::initializer_list<OperatorLevel> operators = {
	{
		BinaryOperator(">>", Bind::create)
	},
	{
		BinaryOperator("==", BinaryExpression::create<BinaryOperation::EQ>),
		BinaryOperator("!=", BinaryExpression::create<BinaryOperation::NE>)
	},
	{
		BinaryOperator("<", BinaryExpression::create<BinaryOperation::LT>),
		BinaryOperator("<=", BinaryExpression::create<BinaryOperation::LE>),
		BinaryOperator(">", BinaryExpression::create<BinaryOperation::GT>),
		BinaryOperator(">=", BinaryExpression::create<BinaryOperation::GE>)
	},
	{
		BinaryOperator("+", BinaryExpression::create<BinaryOperation::ADD>),
		BinaryOperator("-", BinaryExpression::create<BinaryOperation::SUB>)
	},
	{
		BinaryOperator("*", BinaryExpression::create<BinaryOperation::MUL>),
		BinaryOperator("/", BinaryExpression::create<BinaryOperation::DIV>),
		BinaryOperator("%", BinaryExpression::create<BinaryOperation::REM>)
	}
};

struct UnaryOperator {
	const char* string;
	using Create = Expression* (*)(const Expression* expression);
	Create create;
	constexpr UnaryOperator(const char* string, Create create): string(string), create(create) {}
};

constexpr std::initializer_list<UnaryOperator> unary_operators = {};

constexpr const char* intrinsics[] = {
	"putChar",
	"putStr",
	"getChar",
	"arrayGet",
	"arrayLength",
	"arraySplice",
	"stringPush",
	"stringIterator",
	"stringIteratorIsValid",
	"stringIteratorGet",
	"stringIteratorNext",
	"reference",
	"typeOf",
	"arrayType",
	"tupleType",
	"referenceType",
	"import"
};

class Scope {
	Scope*& current_scope;
	Scope* parent;
	std::map<StringView, const Expression*> variables;
	Closure* closure;
	const Expression* self = nullptr;
	Block* block;
public:
	Scope(Scope*& current_scope, Closure* closure, Block* block): current_scope(current_scope), closure(closure), block(block) {
		parent = current_scope;
		current_scope = this;
	}
	Scope(Scope*& current_scope, Block* block): Scope(current_scope, nullptr, block) {}
	Scope(Scope*& current_scope): Scope(current_scope, nullptr, nullptr) {}
	~Scope() {
		current_scope = parent;
	}
	void set_self(const Expression* self) {
		this->self = self;
	}
	void add_variable(const StringView& name, const Expression* value) {
		variables[name] = value;
	}
	const Expression* look_up(const StringView& name) {
		auto iterator = variables.find(name);
		if (iterator != variables.end()) {
			return iterator->second;
		}
		if (closure) {
			if (parent) {
				if (const Expression* expression = parent->look_up(name)) {
					const std::size_t index = closure->add_environment_expression(expression);
					const Expression* argument = create<ClosureAccess>(self, index);
					add_variable(name, argument);
					return argument;
				}
			}
		}
		else if (parent) {
			return parent->look_up(name);
		}
		return nullptr;
	}
	void add_expression(Expression* expression) {
		if (block) {
			block->add_expression(expression);
		}
		else {
			parent->add_expression(expression);
		}
	}
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		add_expression(expression);
		return expression;
	}
};

class Cursor {
	const SourceFile* file;
	const char* position;
public:
	Cursor(const SourceFile* file): file(file), position(file->begin()) {}
	constexpr Cursor(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr bool operator <(const Cursor& rhs) const {
		return position < rhs.position;
	}
	constexpr char operator *() const {
		return *position;
	}
	Cursor& operator ++() {
		++position;
		return *this;
	}
	constexpr StringView operator -(const Cursor& start) const {
		return StringView(start.position, position - start.position);
	}
	const char* get_path() const {
		return file->get_path();
	}
	std::size_t get_position() const {
		return position - file->begin();
	}
};

class Parser {
public:
	Cursor cursor;
	static constexpr bool any_char(char c) {
		return true;
	}
	static constexpr bool white_space(char c) {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r';
	}
	static constexpr bool numeric(char c) {
		return c >= '0' && c <= '9';
	}
	static constexpr bool alphabetic(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}
	static constexpr bool alphanumeric(char c) {
		return alphabetic(c) || numeric(c);
	}
	static constexpr bool operator_char(char c) {
		return StringView("+-*/%=<>!&|~^?:").contains(c);
	}
	template <class F> bool parse(F f) {
		if (cursor && f(*cursor)) {
			++cursor;
			return true;
		}
		return false;
	}
	bool parse(char c) {
		if (cursor && *cursor == c) {
			++cursor;
			return true;
		}
		return false;
	}
	template <class F> bool parse(const StringView& s, F f) {
		Cursor copy = cursor;
		for (char c: s) {
			if (!(copy && *copy == c)) {
				return false;
			}
			++copy;
		}
		if (copy && f(*copy)) {
			return false;
		}
		cursor = copy;
		return true;
	}
	bool parse(const StringView& s) {
		Cursor copy = cursor;
		for (char c: s) {
			if (!(copy && *copy == c)) {
				return false;
			}
			++copy;
		}
		cursor = copy;
		return true;
	}
	bool parse(const char* s) {
		return parse(StringView(s));
	}
	template <class F> bool parse_not(F f) {
		Cursor copy = cursor;
		if (parse(f)) {
			cursor = copy;
			return false;
		}
		if (cursor) {
			return true;
		}
		return false;
	}
	template <class F> StringView parse_all(F f) {
		const Cursor start = cursor;
		while (parse(f)) {}
		return cursor - start;
	}
	Parser(const SourceFile* file): cursor(file) {}
	Parser(const Cursor& cursor): cursor(cursor) {}
	Parser copy() const {
		return Parser(cursor);
	}
	std::size_t get_position() const {
		return cursor.get_position();
	}
};

class MoebiusParser: private Parser {
	using SourcePosition = std::size_t;
	Program* program;
	Scope* current_scope = nullptr;
	template <class T> [[noreturn]] void error(std::size_t position, const T& t) {
		print_error(Printer(std::cerr), cursor.get_path(), position, t);
		std::exit(EXIT_FAILURE);
	}
	template <class T> [[noreturn]] void error(const T& t) {
		error(get_position(), t);
	}
	template <class F> void expect(const StringView& s, F f) {
		if (!parse(s, f)) {
			error(format("expected \"%\"", s));
		}
	}
	void expect(const StringView& s) {
		if (!parse(s)) {
			error(format("expected \"%\"", s));
		}
	}
	bool parse_comment() {
		if (parse("//")) {
			parse_all([](char c) { return c != '\n'; });
			parse("\n");
			return true;
		}
		if (parse("/*")) {
			while (parse_not("*/")) {
				parse(any_char);
			}
			expect("*/");
			return true;
		}
		return false;
	}
	void parse_white_space() {
		parse_all(white_space);
		while (parse_comment()) {
			parse_all(white_space);
		}
	}
	char parse_character() {
		char c = *cursor;
		++cursor;
		if (c == '\\') {
			if (!cursor) {
				error("unexpected end");
			}
			c = *cursor;
			++cursor;
			if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
			else if (c == 't') c = '\t';
			else if (c == 'v') c = '\v';
			else if (c == '\'' || c == '\"' || c == '\\' || c == '$') c = c;
			else error("invalid escape");
		}
		return c;
	}
	const Expression* parse_string_segment() {
		const SourcePosition position = get_position();
		constexpr auto string_segment_end_char = [](char c) constexpr {
			return c == '"' || c == '$';
		};
		if (parse("$")) {
			const Expression* expression;
			if (parse("{")) {
				parse_white_space();
				expression = parse_expression();
				parse_white_space();
				expect("}");
			}
			else {
				StringView identifier = parse_identifier();
				expression = current_scope->look_up(identifier);
				if (expression == nullptr) {
					error(position, format("undefined variable \"%\"", identifier));
				}
			}
			const Expression* function = current_scope->look_up("toString");
			if (function == nullptr) {
				error(position, "function toString not defined");
			}
			ClosureCall* call = current_scope->create<ClosureCall>(function);
			call->set_position(position);
			call->add_argument(expression);
			return call;
		}
		else {
			std::string string;
			while (parse_not(string_segment_end_char)) {
				string.push_back(parse_character());
			}
			StringLiteral* string_literal = current_scope->create<StringLiteral>(string);
			//string_literal->set_position(position);
			return string_literal;
		}
	}
	StringView parse_identifier() {
		if (!copy().parse(alphabetic)) {
			error("expected alphabetic character");
		}
		return parse_all(alphanumeric);
	}
	const BinaryOperator* parse_binary_operator(const OperatorLevel* level) {
		for (const BinaryOperator& op: *level) {
			if (parse(op.string, operator_char)) {
				return &op;
			}
		}
		return nullptr;
	}
	const UnaryOperator* parse_unary_operator() {
		for (const UnaryOperator& op: unary_operators) {
			if (parse(op.string, operator_char)) {
				return &op;
			}
		}
		return nullptr;
	}
	const char* parse_intrinsic_name() {
		StringView name = parse_identifier();
		for (const char* intrinsic_name: intrinsics) {
			if (name == intrinsic_name) {
				return intrinsic_name;
			}
		}
		error(format("unknown intrinsic \"%\"", name));
	}
	const Expression* parse_expression_last() {
		const SourcePosition position = get_position();
		if (parse("{")) {
			parse_white_space();
			if (copy().parse("let", alphanumeric) || copy().parse("return", alphanumeric) || copy().parse("func", alphanumeric) || copy().parse("struct", alphanumeric) || copy().parse("enum", alphanumeric)) {
				const Expression* expression = parse_scope();
				parse_white_space();
				expect("}");
				return expression;
			}
			else {
				StructTypeLiteral* struct_type_literal = new StructTypeLiteral();
				StructLiteral* struct_literal = new StructLiteral(struct_type_literal);
				struct_type_literal->set_position(position);
				struct_literal->set_position(position);
				while (parse_not("}")) {
					const StringView field_name = parse_identifier();
					parse_white_space();
					const Expression* field;
					if (parse(":")) {
						parse_white_space();
						field = parse_expression();
						parse_white_space();
					}
					else {
						field = current_scope->look_up(field_name);
						if (field == nullptr) {
							error(format("undefined variable \"%\"", field_name));
						}
					}
					Intrinsic* field_type = current_scope->create<Intrinsic>("typeOf");
					field_type->add_argument(field);
					struct_type_literal->add_field(field_name, field_type);
					struct_literal->add_field(field_name, field);
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect("}");
				current_scope->add_expression(struct_type_literal);
				current_scope->add_expression(struct_literal);
				return struct_literal;
			}
		}
		else if (parse("(")) {
			parse_white_space();
			std::vector<const Expression*> elements;
			while (parse_not(")")) {
				elements.push_back(parse_expression());
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect(")");
			if (elements.size() == 1) {
				return elements[0];
			}
			else {
				TupleLiteral* tuple = current_scope->create<TupleLiteral>();
				tuple->set_position(position);
				for (const Expression* element: elements) {
					tuple->add_element(element);
				}
				return tuple;
			}
		}
		else if (parse("if", alphanumeric)) {
			parse_white_space();
			expect("(");
			parse_white_space();
			const Expression* condition = parse_expression();
			parse_white_space();
			expect(")");
			parse_white_space();
			If* if_ = new If(condition);
			if_->set_position(position);
			{
				Scope scope(current_scope, if_->get_then_block());
				const Expression* then_expression = parse_expression();
				current_scope->create<Return>(then_expression);
			}
			parse_white_space();
			expect("else", alphanumeric);
			parse_white_space();
			{
				Scope scope(current_scope, if_->get_else_block());
				const Expression* else_expression = parse_expression();
				current_scope->create<Return>(else_expression);
			}
			current_scope->add_expression(if_);
			return if_;
		}
		else if (parse("switch", alphanumeric)) {
			parse_white_space();
			expect("(");
			parse_white_space();
			const Expression* enum_ = parse_expression();
			parse_white_space();
			expect(")");
			parse_white_space();
			expect("{");
			parse_white_space();
			Switch* switch_ = new Switch(enum_);
			switch_->set_position(position);
			while (parse_not("}")) {
				const StringView case_name = parse_identifier();
				parse_white_space();
				expect(":");
				parse_white_space();
				Scope scope(current_scope, switch_->add_case(case_name));
				current_scope->add_variable(case_name, current_scope->create<CaseVariable>());
				const Expression* case_expression = parse_expression();
				current_scope->create<Return>(case_expression);
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("}");
			current_scope->add_expression(switch_);
			return switch_;
		}
		else if (parse("func", alphanumeric)) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Function* function = new Function();
			function->set_path(cursor.get_path());
			program->add_function(function);
			Closure* closure = new Closure(function);
			closure->set_position(position);
			{
				Scope scope(current_scope, closure, function->get_block());
				current_scope->set_self(current_scope->create<Argument>(function->add_argument()));
				while (parse_not(")")) {
					auto [argument_name, type_assert_position, argument_type] = parse_name();
					const Expression* argument = current_scope->create<Argument>(function->add_argument());
					current_scope->add_variable(argument_name, argument);
					if (argument_type) {
						TypeAssert* type_assert = current_scope->create<TypeAssert>(argument, argument_type);
						type_assert->set_position(type_assert_position);
					}
					parse_white_space();
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect(")");
				parse_white_space();
				const SourcePosition return_type_position = get_position();
				if (parse(":")) {
					parse_white_space();
					const Expression* type = parse_expression();
					ReturnType* return_type = current_scope->create<ReturnType>(type);
					return_type->set_position(return_type_position);
					parse_white_space();
				}
				expect("=>");
				parse_white_space();
				const Expression* expression = parse_expression();
				current_scope->create<Return>(expression);
			}
			current_scope->add_expression(closure);
			return closure;
		}
		else if (parse("struct", alphanumeric)) {
			parse_white_space();
			expect("{");
			parse_white_space();
			StructTypeLiteral* struct_type_literal = new StructTypeLiteral();
			struct_type_literal->set_position(position);
			while (parse_not("}")) {
				const StringView field_name = parse_identifier();
				parse_white_space();
				expect(":");
				parse_white_space();
				const Expression* field_type = parse_expression();
				struct_type_literal->add_field(field_name, field_type);
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("}");
			current_scope->add_expression(struct_type_literal);
			return struct_type_literal;
		}
		else if (parse("enum", alphanumeric)) {
			parse_white_space();
			expect("{");
			parse_white_space();
			EnumTypeLiteral* enum_type_literal = new EnumTypeLiteral();
			enum_type_literal->set_position(position);
			while (parse_not("}")) {
				const StringView case_name = parse_identifier();
				parse_white_space();
				if (parse(":")) {
					parse_white_space();
					const Expression* case_type = parse_expression();
					parse_white_space();
					enum_type_literal->add_case(case_name, case_type);
				}
				else {
					const Expression* case_type = current_scope->create<TypeLiteral>(TypeInterner::get_void_type());
					enum_type_literal->add_case(case_name, case_type);
				}
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("}");
			current_scope->add_expression(enum_type_literal);
			return enum_type_literal;
		}
		else if (parse("\"")) {
			const Expression* left = parse_string_segment();
			while (parse_not("\"")) {
				const Expression* right = parse_string_segment();
				Intrinsic* intrinsic = current_scope->create<Intrinsic>("stringPush");
				intrinsic->add_argument(left);
				intrinsic->add_argument(right);
				left = intrinsic;
			}
			expect("\"");
			return left;
		}
		else if (parse("'")) {
			if (!copy().parse(any_char)) {
				error("unexpected end");
			}
			IntLiteral* int_literal = current_scope->create<IntLiteral>(parse_character());
			int_literal->set_position(position);
			expect("'");
			return int_literal;
		}
		else if (parse("[")) {
			parse_white_space();
			ArrayLiteral* array_literal = new ArrayLiteral();
			array_literal->set_position(position);
			while (parse_not("]")) {
				array_literal->add_element(parse_expression());
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("]");
			current_scope->add_expression(array_literal);
			return array_literal;
		}
		else if (parse("false", alphanumeric)) {
			Expression* expression = current_scope->create<IntLiteral>(0);
			expression->set_position(position);
			return expression;
		}
		else if (parse("true", alphanumeric)) {
			Expression* expression = current_scope->create<IntLiteral>(1);
			expression->set_position(position);
			return expression;
		}
		else if (parse("void", alphanumeric)) {
			Expression* expression = current_scope->create<VoidLiteral>();
			expression->set_position(position);
			return expression;
		}
		else if (parse("Int", alphanumeric)) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_int_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse("String", alphanumeric)) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_string_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse("StringIterator", alphanumeric)) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_string_iterator_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse("Void", alphanumeric)) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_void_type());
			expression->set_position(position);
			return expression;
		}
		else if (copy().parse(numeric)) {
			std::int32_t number = 0;
			for (char c: parse_all(numeric)) {
				number *= 10;
				number += c - '0';
			}
			Expression* expression = current_scope->create<IntLiteral>(number);
			expression->set_position(position);
			return expression;
		}
		else if (copy().parse(alphabetic)) {
			StringView identifier = parse_identifier();
			const Expression* expression = current_scope->look_up(identifier);
			if (expression == nullptr) {
				error(position, format("undefined variable \"%\"", identifier));
			}
			return expression;
		}
		else if (parse("@")) {
			const char* name = parse_intrinsic_name();
			parse_white_space();
			expect("(");
			parse_white_space();
			Intrinsic* intrinsic = new Intrinsic(name);
			intrinsic->set_position(position);
			while (parse_not(")")) {
				intrinsic->add_argument(parse_expression());
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect(")");
			current_scope->add_expression(intrinsic);
			return intrinsic;
		}
		else {
			error("unexpected character");
		}
	}
	const Expression* parse_expression(const OperatorLevel* level = operators.begin()) {
		if (level == operators.end()) {
			const SourcePosition position = get_position();
			if (const UnaryOperator* op = parse_unary_operator()) {
				parse_white_space();
				Expression* expression = op->create(parse_expression(level));
				expression->set_position(position);
				current_scope->add_expression(expression);
				return expression;
			}
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (true) {
				const SourcePosition position = get_position();
				if (parse("(")) {
					parse_white_space();
					ClosureCall* call = new ClosureCall(expression);
					call->set_position(position);
					while (parse_not(")")) {
						call->add_argument(parse_expression());
						parse_white_space();
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect(")");
					current_scope->add_expression(call);
					expression = call;
					parse_white_space();
				}
				else if (parse(".")) {
					parse_white_space();
					StringView name = parse_identifier();
					parse_white_space();
					const SourcePosition method_call_position = get_position();
					if (parse("(")) {
						parse_white_space();
						const Expression* method = current_scope->look_up(name);
						MethodCall* call = new MethodCall(expression, name, method);
						call->set_position(method_call_position);
						while (parse_not(")")) {
							call->add_argument(parse_expression());
							parse_white_space();
							if (!parse(",")) {
								break;
							}
							parse_white_space();
						}
						expect(")");
						current_scope->add_expression(call);
						expression = call;
						parse_white_space();
					}
					else {
						StructAccess* struct_access = current_scope->create<StructAccess>(expression, name);
						struct_access->set_position(position);
						expression = struct_access;
					}
				}
				else if (parse("{")) {
					parse_white_space();
					StructLiteral* struct_literal = new StructLiteral(expression);
					struct_literal->set_position(position);
					while (parse_not("}")) {
						const StringView field_name = parse_identifier();
						parse_white_space();
						const Expression* field;
						if (parse(":")) {
							parse_white_space();
							field = parse_expression();
							parse_white_space();
						}
						else {
							field = current_scope->look_up(field_name);
							if (field == nullptr) {
								error(format("undefined variable \"%\"", field_name));
							}
						}
						struct_literal->add_field(field_name, field);
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect("}");
					current_scope->add_expression(struct_literal);
					TypeAssert* type_assert = current_scope->create<TypeAssert>(struct_literal, expression);
					type_assert->set_position(position);
					expression = struct_literal;
					parse_white_space();
				}
				else {
					break;
				}
			}
			return expression;
		}
		const Expression* left = parse_expression(level + 1);
		parse_white_space();
		SourcePosition position = get_position();
		while (const BinaryOperator* op = parse_binary_operator(level)) {
			parse_white_space();
			const Expression* right = parse_expression(level + 1);
			Expression* expression = op->create(left, right);
			expression->set_position(position);
			current_scope->add_expression(expression);
			left = expression;
			parse_white_space();
			position = get_position();
		}
		return left;
	}
	std::tuple<StringView, SourcePosition, const Expression*> parse_name() {
		const StringView name = parse_identifier();
		parse_white_space();
		const SourcePosition type_assert_position = get_position();
		if (parse(":")) {
			parse_white_space();
			const Expression* type = parse_expression();
			return std::make_tuple(name, type_assert_position, type);
		}
		else {
			return std::make_tuple(name, type_assert_position, nullptr);
		}
	}
	const Expression* parse_scope() {
		Scope scope(current_scope);
		while (true) {
			const SourcePosition position = get_position();
			if (parse("let", alphanumeric)) {
				parse_white_space();
				std::vector<std::tuple<StringView, SourcePosition, const Expression*>> element_names;
				if (parse("(")) {
					parse_white_space();
					while (parse_not(")")) {
						element_names.push_back(parse_name());
						parse_white_space();
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect(")");
				}
				else {
					element_names.push_back(parse_name());
				}
				parse_white_space();
				expect("=");
				parse_white_space();
				const Expression* expression = parse_expression();
				if (element_names.size() == 1) {
					auto [name, type_assert_position, type] = element_names[0];
					if (type) {
						TypeAssert* type_assert = current_scope->create<TypeAssert>(expression, type);
						type_assert->set_position(type_assert_position);
					}
					current_scope->add_variable(name, expression);
				}
				else for (std::size_t i = 0; i < element_names.size(); ++i) {
					auto [name, type_assert_position, type] = element_names[i];
					TupleAccess* tuple_access = current_scope->create<TupleAccess>(expression, i);
					tuple_access->set_position(position);
					if (type) {
						TypeAssert* type_assert = current_scope->create<TypeAssert>(tuple_access, type);
						type_assert->set_position(type_assert_position);
					}
					current_scope->add_variable(name, tuple_access);
				}
				parse_white_space();
			}
			else if (parse("func", alphanumeric)) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("(");
				parse_white_space();
				Function* function = new Function();
				function->set_path(cursor.get_path());
				program->add_function(function);
				Closure* closure = new Closure(function);
				closure->set_position(position);
				{
					Scope scope(current_scope, closure, function->get_block());
					const Expression* self = current_scope->create<Argument>(function->add_argument());
					current_scope->set_self(self);
					current_scope->add_variable(name, self);
					while (parse_not(")")) {
						auto [argument_name, type_assert_position, argument_type] = parse_name();
						const Expression* argument = current_scope->create<Argument>(function->add_argument());
						current_scope->add_variable(argument_name, argument);
						if (argument_type) {
							TypeAssert* type_assert = current_scope->create<TypeAssert>(argument, argument_type);
							type_assert->set_position(type_assert_position);
						}
						parse_white_space();
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect(")");
					parse_white_space();
					const SourcePosition return_type_position = get_position();
					if (parse(":")) {
						parse_white_space();
						const Expression* type = parse_expression();
						ReturnType* return_type = current_scope->create<ReturnType>(type);
						return_type->set_position(return_type_position);
						parse_white_space();
					}
					expect("=>");
					parse_white_space();
					const Expression* expression = parse_expression();
					current_scope->create<Return>(expression);
				}
				current_scope->add_expression(closure);
				current_scope->add_variable(name, closure);
				parse_white_space();
			}
			else if (parse("struct", alphanumeric)) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("{");
				parse_white_space();
				StructTypeLiteral* struct_type_literal = new StructTypeLiteral();
				struct_type_literal->set_position(position);
				while (parse_not("}")) {
					const StringView field_name = parse_identifier();
					parse_white_space();
					expect(":");
					parse_white_space();
					const Expression* field_type = parse_expression();
					struct_type_literal->add_field(field_name, field_type);
					parse_white_space();
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect("}");
				parse_white_space();
				current_scope->add_expression(struct_type_literal);
				current_scope->add_variable(name, struct_type_literal);
			}
			else if (parse("enum", alphanumeric)) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("{");
				parse_white_space();
				EnumTypeLiteral* enum_type_literal = new EnumTypeLiteral();
				enum_type_literal->set_position(position);
				while (parse_not("}")) {
					const StringView case_name = parse_identifier();
					parse_white_space();
					if (parse(":")) {
						parse_white_space();
						const Expression* case_type = parse_expression();
						parse_white_space();
						enum_type_literal->add_case(case_name, case_type);
					}
					else {
						const Expression* case_type = current_scope->create<TypeLiteral>(TypeInterner::get_void_type());
						enum_type_literal->add_case(case_name, case_type);
					}
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect("}");
				parse_white_space();
				current_scope->add_expression(enum_type_literal);
				current_scope->add_variable(name, enum_type_literal);
			}
			else {
				break;
			}
		}
		expect("return", alphanumeric);
		parse_white_space();
		return parse_expression();
	}
public:
	MoebiusParser(const SourceFile* file, Program* program): Parser(file), program(program) {}
	const Function* parse_program() {
		parse_white_space();
		Function* main_function = new Function();
		main_function->set_path(cursor.get_path());
		program->add_function(main_function);
		Scope scope(current_scope, main_function->get_block());
		const Expression* expression = parse_scope();
		current_scope->create<Return>(expression);
		parse_white_space();
		if (copy().parse(any_char)) {
			error("unexpected character at end of program");
		}
		return main_function;
	}
	static const Function* parse_program(const char* path, Program* program) {
		SourceFile file(path);
		MoebiusParser parser(&file, program);
		return parser.parse_program();
	}
};
