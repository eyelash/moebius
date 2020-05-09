#pragma once

#include "printer.hpp"
#include "ast.hpp"
#include <vector>
#include <fstream>
#include <iterator>
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

static constexpr BinaryOperator operators[][5] = {
	{
		BinaryOperator("==", BinaryExpression::create<BinaryOperation::EQ>),
		BinaryOperator("!=", BinaryExpression::create<BinaryOperation::NE>)
	},
	{
		BinaryOperator("<=", BinaryExpression::create<BinaryOperation::LE>),
		BinaryOperator("<", BinaryExpression::create<BinaryOperation::LT>),
		BinaryOperator(">=", BinaryExpression::create<BinaryOperation::GE>),
		BinaryOperator(">", BinaryExpression::create<BinaryOperation::GT>)
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

class Scope {
	Scope* parent;
	std::map<StringView, const Expression*> variables;
	Function* function;
public:
	Scope(Scope* parent, Function* function = nullptr): parent(parent), function(function) {}
	Scope* get_parent() const {
		return parent;
	}
	void add_variable(const StringView& name, const Expression* value) {
		variables[name] = value;
	}
	const Expression* look_up(const StringView& name) {
		auto iterator = variables.find(name);
		if (iterator != variables.end()) {
			return iterator->second;
		}
		if (function) {
			if (parent) {
				if (const Expression* expression = parent->look_up(name)) {
					const std::size_t index = function->add_environment_expression(expression);
					Argument* argument = new Argument(index);
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
};

class SourceFile {
	const char* file_name;
	std::vector<char> content;
public:
	SourceFile(const char* file_name): file_name(file_name) {
		std::ifstream file(file_name);
		content.insert(content.end(), std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
	const char* get_name() const {
		return file_name;
	}
	const char* begin() const {
		return content.data();
	}
	const char* end() const {
		return content.data() + content.size();
	}
};

class SourcePosition {
	const char* file_name;
	std::size_t position;
public:
	SourcePosition(const char* file_name, std::size_t position): file_name(file_name), position(position) {}
	template <class T> void print_error(Printer& printer, const T& t) const {
		SourceFile file(file_name);
		unsigned int line_number = 1;
		const char* c = file.begin();
		const char* end = file.end();
		const char* position = std::min(c + this->position, end);
		const char* line_start = c;
		while (c < position) {
			if (*c == '\n') {
				++c;
				++line_number;
				line_start = c;
			}
			else {
				++c;
			}
		}

		printer.print(bold(format("%:%: ", file_name, print_number(line_number))));
		printer.print(bold(red("error: ")));
		printer.print(t);
		printer.print('\n');

		c = line_start;
		while (c < end && *c != '\n') {
			printer.print(*c);
			++c;
		}
		printer.print('\n');

		c = line_start;
		while (c < position) {
			printer.print(*c == '\t' ? '\t' : ' ');
			++c;
		}
		printer.print('^');
		printer.print('\n');
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
	bool parse(const StringView& s) {
		const Cursor copy = cursor;
		for (char c: s) {
			if (!(cursor && *cursor == c)) {
				cursor = copy;
				return false;
			}
			++cursor;
		}
		return true;
	}
	bool parse(const char* s) {
		return parse(StringView(s));
	}
	template <class T> void parse_all(T&& t) {
		while (parse(std::forward<T>(t))) {}
	}
	Parser(const SourceFile* file): cursor(file) {}
	Parser(const Cursor& cursor): cursor(cursor) {}
	Parser copy() const {
		return Parser(cursor);
	}
};

class MoebiusParser: private Parser {
	Scope* current_scope;
	template <class T> [[noreturn]] void error(const T& t) {
		Printer printer(stderr);
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
		return new Number(number);
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
			if (parse(operators[level][i].string)) {
				return operators[level][i];
			}
		}
		return BinaryOperator();
	}
	const Expression* parse_expression_last() {
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
		else if (parse("if")) {
			parse_white_space();
			expect("(");
			parse_white_space();
			const Expression* condition = parse_expression();
			parse_white_space();
			expect(")");
			parse_white_space();
			const Expression* then_expression = parse_expression();
			parse_white_space();
			expect("else");
			parse_white_space();
			const Expression* else_expression = parse_expression();
			return new If(condition, then_expression, else_expression);
		}
		else if (parse("fn")) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Function* function = new Function();
			Scope scope(current_scope, function);
			current_scope = &scope;
			while (cursor && *cursor != ')') {
				const StringView name = parse_identifier();
				const std::size_t index = function->add_argument();
				scope.add_variable(name, new Argument(index));
				parse_white_space();
				if (parse(",")) {
					parse_white_space();
				}
			}
			expect(")");
			parse_white_space();
			const Expression* expression = parse_expression();
			function->set_expression(expression);
			current_scope = scope.get_parent();
			return function;
		}
		else if (parse("'")) {
			if (!cursor) {
				error("unexpected end");
			}
			const char c = *cursor;
			++cursor;
			expect("'");
			return new Number(c);
		}
		else if (copy().parse(numeric)) {
			return parse_number();
		}
		else if (copy().parse(alphabetic)) {
			return parse_variable();
		}
		else {
			error("unexpected character");
		}
	}
	const Expression* parse_expression(int level = 0) {
		if (level == 4) {
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (parse("(")) {
				parse_white_space();
				Call* call = new Call(expression);
				while (cursor && *cursor != ')') {
					call->add_argument(parse_expression());
					parse_white_space();
					if (parse(",")) {
						parse_white_space();
					}
				}
				expect(")");
				expression = call;
				parse_white_space();
			}
			return expression;
		}
		const Expression* left = parse_expression(level + 1);
		parse_white_space();
		while (BinaryOperator op = parse_operator(level)) {
			parse_white_space();
			const Expression* right = parse_expression(level + 1);
			left = op.create(left, right);
			parse_white_space();
		}
		return left;
	}
	const Expression* parse_scope() {
		Scope scope(current_scope);
		current_scope = &scope;
		const Expression* result = nullptr;
		while (true) {
			if (parse("let")) {
				parse_white_space();
				StringView name = parse_identifier();
				parse_white_space();
				expect("=");
				parse_white_space();
				const Expression* expression = parse_expression();
				scope.add_variable(name, expression);
			}
			else if (parse("return")) {
				parse_white_space();
				result = parse_expression();
				break;
			}
			else {
				error("expected \"let\" or \"return\"");
				break;
			}
		}
		current_scope = scope.get_parent();
		return result;
	}
public:
	MoebiusParser(const SourceFile* file): Parser(file), current_scope(nullptr) {}
	const Expression* create_putChar() {
		Function* function = new Function();
		const std::size_t index = function->add_argument();
		Intrinsic* intrinsic = new Intrinsic("putChar", new VoidType());
		intrinsic->add_argument(index);
		function->set_expression(intrinsic);
		return function;
	}
	const Expression* create_getChar() {
		Function* function = new Function();
		Intrinsic* intrinsic = new Intrinsic("getChar", new NumberType());
		function->set_expression(intrinsic);
		return function;
	}
	const Expression* parse_program() {
		parse_white_space();
		Scope scope(nullptr);
		scope.add_variable("putChar", create_putChar());
		scope.add_variable("getChar", create_getChar());
		current_scope = &scope;
		return parse_scope();
	}
};

const Expression* parse(const char* file_name) {
	SourceFile file(file_name);
	MoebiusParser parser(&file);
	return parser.parse_program();
}
