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
	"getChar"
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
		return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
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
		if (f(*copy)) {
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
	Program* program;
	Scope* current_scope = nullptr;
	template <class T> [[noreturn]] void error(const T& t) {
		FilePrinter printer(stderr);
		cursor.get_position().print_error(printer, t);
		std::exit(EXIT_FAILURE);
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
	const Expression* parse_number() {
		std::int32_t number = 0;
		while (copy().parse(numeric)) {
			number *= 10;
			number += *cursor - '0';
			++cursor;
		}
		return current_scope->create<Number>(number);
	}
	StringView parse_identifier() {
		if (!copy().parse(alphabetic)) {
			error("expected alphabetic character");
		}
		const Cursor start = cursor;
		parse_all(alphanumeric);
		return cursor - start;
	}
	const Expression* parse_variable() {
		StringView identifier = parse_identifier();
		const Expression* expression = current_scope->look_up(identifier);
		if (expression == nullptr) {
			error(format("undefined variable \"%\"", identifier));
		}
		return expression;
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
				if_->set_then_expression(then_expression);
			}
			parse_white_space();
			expect("else");
			parse_white_space();
			{
				Scope scope(current_scope, if_->get_else_block());
				const Expression* else_expression = parse_expression();
				if_->set_else_expression(else_expression);
			}
			current_scope->add_expression(if_);
			return if_;
		}
		else if (parse("fn", alphanumeric)) {
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
					const StringView name = parse_identifier();
					const std::size_t index = function->add_argument();
					scope.add_variable(name, current_scope->create<Argument>(index));
					parse_white_space();
					if (parse(",")) {
						parse_white_space();
					}
				}
				expect(")");
				parse_white_space();
				const Expression* expression = parse_expression();
				function->set_expression(expression);
			}
			current_scope->add_expression(closure);
			return closure;
		}
		else if (parse("'")) {
			if (!cursor) {
				error("unexpected end");
			}
			const char c = *cursor;
			++cursor;
			expect("'");
			return current_scope->create<Number>(c);
		}
		else if (copy().parse(numeric)) {
			return parse_number();
		}
		else if (copy().parse(alphabetic)) {
			return parse_variable();
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
			SourcePosition position = cursor.get_position();
			while (parse("(")) {
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
				position = cursor.get_position();
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
		const Expression* result = nullptr;
		while (true) {
			if (parse("let", alphanumeric)) {
				parse_white_space();
				StringView name = parse_identifier();
				parse_white_space();
				expect("=");
				parse_white_space();
				const Expression* expression = parse_expression();
				scope.add_variable(name, expression);
			}
			else if (parse("return", alphanumeric)) {
				parse_white_space();
				result = parse_expression();
				break;
			}
			else {
				error("expected \"let\" or \"return\"");
				break;
			}
		}
		return result;
	}
public:
	MoebiusParser(const SourceFile* file): Parser(file) {}
	const Program* parse_program() {
		program = new Program();
		parse_white_space();
		Function* main_function = new Function();
		program->add_function(main_function);
		Scope scope(current_scope, main_function->get_block());
		main_function->set_expression(parse_scope());
		return program;
	}
};

const Program* parse(const char* file_name) {
	SourceFile file(file_name);
	MoebiusParser parser(&file);
	return parser.parse_program();
}
