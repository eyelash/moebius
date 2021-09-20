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
	constexpr BinaryOperator(): string(nullptr), create(nullptr) {}
	constexpr operator bool() const {
		return string != nullptr;
	}
};

constexpr BinaryOperator operators[][5] = {
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

constexpr const char* intrinsics[] = {
	"putChar",
	"getChar",
	"arrayNew",
	"arrayGet",
	"arrayLength",
	"arraySplice",
	"typeOf"
};

class Scope {
	Scope*& current_scope;
	Scope* parent;
	std::map<StringView, const Expression*> variables;
	Closure* closure;
	Block* block;
public:
	Scope(Scope*& current_scope, Closure* closure, Block* block): current_scope(current_scope), closure(closure), block(block) {
		parent = current_scope;
		current_scope = this;
	}
	Scope(Scope*& current_scope, Closure* closure): Scope(current_scope, closure, nullptr) {}
	Scope(Scope*& current_scope, Block* block): Scope(current_scope, nullptr, block) {}
	Scope(Scope*& current_scope): Scope(current_scope, nullptr, nullptr) {}
	~Scope() {
		current_scope = parent;
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
					const Expression* argument = create<ClosureAccess>(create<Argument>(0), index);
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
	SourcePosition get_position() const {
		return SourcePosition(file->get_name(), static_cast<std::size_t>(position - file->begin()));
	}
};

class Parser {
public:
	Cursor cursor;
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
			if (!copy || *copy != c) {
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
			if (!copy || *copy != c) {
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
	template <class F> void parse_all(F f) {
		while (parse(f)) {}
	}
	Parser(const SourceFile* file): cursor(file) {}
	Parser(const Cursor& cursor): cursor(cursor) {}
	Parser copy() const {
		return Parser(cursor);
	}
};

class MoebiusParser: private Parser {
	std::unique_ptr<Program> program;
	Scope* current_scope = nullptr;
	template <class T> [[noreturn]] void error(const SourcePosition& position, const T& t) {
		print_error(Printer(std::cerr), position, t);
		std::exit(EXIT_FAILURE);
	}
	template <class T> [[noreturn]] void error(const T& t) {
		error(cursor.get_position(), t);
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
			while (cursor && !copy().parse("*/")) {
				++cursor;
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
	IntLiteral* parse_character() {
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
			else if (c == '\'' || c == '\"' || c == '\\') c = c;
			else error("invalid escape");
		}
		return current_scope->create<IntLiteral>(c);
	}
	StringView parse_identifier() {
		if (!copy().parse(alphabetic)) {
			error("expected alphabetic character");
		}
		const Cursor start = cursor;
		parse_all(alphanumeric);
		return cursor - start;
	}
	BinaryOperator parse_operator(int level) {
		for (int i = 0; operators[level][i]; ++i) {
			if (parse(operators[level][i].string, operator_char)) {
				return operators[level][i];
			}
		}
		return BinaryOperator();
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
		const SourcePosition position = cursor.get_position();
		if (parse("{")) {
			parse_white_space();
			const Expression* expression = parse_scope();
			parse_white_space();
			expect("}");
			return expression;
		}
		else if (parse("(")) {
			parse_white_space();
			const Expression* expression = parse_expression();
			parse_white_space();
			expect(")");
			return expression;
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
		else if (parse("func", alphanumeric)) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Function* function = new Function();
			program->add_function(function);
			Closure* closure = new Closure(function);
			closure->set_position(position);
			{
				Scope scope(current_scope, closure, function->get_block());
				while (cursor && *cursor != ')') {
					const StringView argument_name = parse_identifier();
					const std::size_t index = function->add_argument();
					current_scope->add_variable(argument_name, current_scope->create<Argument>(index));
					parse_white_space();
					if (parse(",")) {
						parse_white_space();
					}
				}
				expect(")");
				parse_white_space();
				const Expression* expression = parse_expression();
				current_scope->create<Return>(expression);
			}
			current_scope->add_expression(closure);
			return closure;
		}
		else if (parse("\"")) {
			Intrinsic* intrinsic = new Intrinsic("arrayNew");
			intrinsic->set_position(position);
			while (cursor && *cursor != '"') {
				intrinsic->add_argument(parse_character());
			}
			expect("\"");
			current_scope->add_expression(intrinsic);
			return intrinsic;
		}
		else if (parse("'")) {
			if (!cursor) {
				error("unexpected end");
			}
			IntLiteral* int_literal = parse_character();
			int_literal->set_position(position);
			expect("'");
			return int_literal;
		}
		else if (parse("[")) {
			parse_white_space();
			Intrinsic* intrinsic = new Intrinsic("arrayNew");
			intrinsic->set_position(position);
			while (cursor && *cursor != ']') {
				intrinsic->add_argument(parse_expression());
				parse_white_space();
				if (parse(",")) {
					parse_white_space();
				}
			}
			expect("]");
			current_scope->add_expression(intrinsic);
			return intrinsic;
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
		else if (parse("Int", alphanumeric)) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_int_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse("Array", alphanumeric)) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_array_type());
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
			while (copy().parse(numeric)) {
				number *= 10;
				number += *cursor - '0';
				++cursor;
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
			while (cursor && *cursor != ')') {
				intrinsic->add_argument(parse_expression());
				parse_white_space();
				if (parse(",")) {
					parse_white_space();
				}
			}
			expect(")");
			current_scope->add_expression(intrinsic);
			return intrinsic;
		}
		else {
			error("unexpected character");
		}
	}
	const Expression* parse_expression(int level = 0) {
		if (level == 5) {
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (true) {
				SourcePosition position = cursor.get_position();
				if (parse("(")) {
					parse_white_space();
					Call* call = new Call();
					call->set_position(position);
					call->add_argument(expression);
					while (cursor && *cursor != ')') {
						call->add_argument(parse_expression());
						parse_white_space();
						if (parse(",")) {
							parse_white_space();
						}
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
					// method call syntax
					if (parse("(")) {
						const Expression* function = current_scope->look_up(name);
						if (function == nullptr) {
							error(format("undefined variable \"%\"", name));
						}
						parse_white_space();
						Call* call = new Call();
						call->set_position(position);
						call->add_argument(function);
						call->add_argument(expression);
						while (cursor && *cursor != ')') {
							call->add_argument(parse_expression());
							parse_white_space();
							if (parse(",")) {
								parse_white_space();
							}
						}
						expect(")");
						current_scope->add_expression(call);
						expression = call;
						parse_white_space();
					}
					else {
						StructAccess* struct_access = new StructAccess(expression, name);
						struct_access->set_position(position);
						current_scope->add_expression(struct_access);
						expression = struct_access;
					}
				}
				else if (parse("{")) {
					parse_white_space();
					StructInstantiation* struct_instantiation = new StructInstantiation(expression);
					struct_instantiation->set_position(position);
					while (cursor && *cursor != '}') {
						const StringView field_name = parse_identifier();
						parse_white_space();
						expect(":");
						parse_white_space();
						const Expression* field_expression = parse_expression();
						struct_instantiation->add_field(field_name, field_expression);
						parse_white_space();
						if (parse(",")) {
							parse_white_space();
						}
					}
					expect("}");
					current_scope->add_expression(struct_instantiation);
					expression = struct_instantiation;
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
		SourcePosition position = cursor.get_position();
		while (BinaryOperator op = parse_operator(level)) {
			parse_white_space();
			const Expression* right = parse_expression(level + 1);
			Expression* expression = op.create(left, right);
			expression->set_position(position);
			current_scope->add_expression(expression);
			left = expression;
			parse_white_space();
			position = cursor.get_position();
		}
		return left;
	}
	const Expression* parse_scope() {
		Scope scope(current_scope);
		while (true) {
			const SourcePosition position = cursor.get_position();
			if (parse("let", alphanumeric)) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				const Expression* type = nullptr;
				if (parse(":")) {
					parse_white_space();
					type = parse_expression();
					parse_white_space();
				}
				expect("=");
				parse_white_space();
				const Expression* expression = parse_expression();
				if (type) {
					current_scope->create<TypeAssert>(expression, type);
				}
				current_scope->add_variable(name, expression);
				parse_white_space();
			}
			else if (parse("func", alphanumeric)) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("(");
				parse_white_space();
				Function* function = new Function();
				program->add_function(function);
				Closure* closure = new Closure(function);
				closure->set_position(position);
				{
					Scope scope(current_scope, closure, function->get_block());
					current_scope->add_variable(name, current_scope->create<Argument>(0));
					while (cursor && *cursor != ')') {
						const StringView argument_name = parse_identifier();
						const std::size_t index = function->add_argument();
						const Expression* argument = current_scope->create<Argument>(index);
						current_scope->add_variable(argument_name, argument);
						parse_white_space();
						if (parse(":")) {
							parse_white_space();
							const Expression* argument_type = parse_expression();
							// TODO: set_position
							current_scope->create<TypeAssert>(argument, argument_type);
							parse_white_space();
						}
						if (parse(",")) {
							parse_white_space();
						}
					}
					expect(")");
					parse_white_space();
					if (parse(":")) {
						parse_white_space();
						const Expression* return_type = parse_expression();
						// TODO: set_position
						current_scope->create<ReturnType>(return_type);
						parse_white_space();
					}
					expect("=");
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
				StructDefinition* struct_definition = new StructDefinition();
				struct_definition->set_position(position);
				if (parse("{")) {
					parse_white_space();
					while (cursor && *cursor != '}') {
						const StringView field_name = parse_identifier();
						parse_white_space();
						expect(":");
						parse_white_space();
						const Expression* field_type = parse_expression();
						struct_definition->add_field(field_name, field_type);
						parse_white_space();
						if (parse(",")) {
							parse_white_space();
						}
					}
					expect("}");
					parse_white_space();
				}
				current_scope->add_expression(struct_definition);
				current_scope->add_variable(name, struct_definition);
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
	MoebiusParser(const SourceFile* file): Parser(file) {}
	std::unique_ptr<Program> parse_program() {
		program = std::make_unique<Program>();
		parse_white_space();
		Function* main_function = new Function();
		program->add_function(main_function);
		Scope scope(current_scope, main_function->get_block());
		const Expression* expression = parse_scope();
		current_scope->create<Return>(expression);
		parse_white_space();
		if (cursor) {
			error("unexpected character at end of program");
		}
		return std::move(program);
	}
};

std::unique_ptr<Program> parse(const char* file_name) {
	SourceFile file(file_name);
	MoebiusParser parser(&file);
	return parser.parse_program();
}
