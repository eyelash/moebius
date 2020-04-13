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
	const Expression* look_up(const StringView& name) const {
		auto iterator = variables.find(name);
		if (iterator != variables.end()) {
			return iterator->second;
		}
		if (function) {
			for (const StringView& argument_name: function->get_argument_names()) {
				if (argument_name == name) {
					return new Argument(name);
				}
			}
			for (const StringView& environment_name: function->get_environment_names()) {
				if (environment_name == name) {
					return new Argument(name);
				}
			}
			if (parent) {
				if (const Expression* expression = parent->look_up(name)) {
					function->add_environment_expression(name, expression);
					return new Argument(name);
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
	StringView get_name() const {
		return StringView(file_name);
	}
	const char* begin() const {
		return content.data();
	}
	const char* end() const {
		return content.data() + content.size();
	}
};

class SourcePosition {
	const SourceFile* file;
	const char* position;
public:
	SourcePosition(const SourceFile* file): file(file), position(file->begin()) {}
	constexpr SourcePosition(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr bool operator <(const SourcePosition& rhs) const {
		return position < rhs.position;
	}
	constexpr char operator *() const {
		return *position;
	}
	SourcePosition& operator ++() {
		++position;
		return *this;
	}
	constexpr StringView operator -(const SourcePosition& start) const {
		return StringView(start.position, position - start.position);
	}
	std::tuple<StringView, unsigned int, SourcePosition> get_info() const {
		unsigned int line_number = 1;
		const char* c = file->begin();
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
		return std::make_tuple(file->get_name(), line_number, SourcePosition(file, line_start));
	}
};

template <class T> [[noreturn]] void error(const SourcePosition& position, const T& t) {
	Printer printer(stderr);
	const auto info = position.get_info();
	const StringView& file_name = std::get<0>(info);
	const unsigned int line_number = std::get<1>(info);
	const SourcePosition& line_start = std::get<2>(info);

	printer.print(format("%:%: ", file_name, print_number(line_number)));
	printer.print(bold(red("error: ")));
	printer.print(t);
	printer.print("\n");

	for (SourcePosition c = line_start; c && *c != '\n'; ++c) {
		printer.print(*c);
	}
	printer.print("\n");

	for (SourcePosition c = line_start; c < position; ++c) {
		printer.print(*c == '\t' ? '\t' : ' ');
	}
	printer.print("^\n");

	std::exit(EXIT_FAILURE);
}

class Parser {
public:
	SourcePosition position;
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
		if (position && f(*position)) {
			++position;
			return true;
		}
		return false;
	}
	bool parse(char c) {
		if (position && *position == c) {
			++position;
			return true;
		}
		return false;
	}
	bool parse(const StringView& s) {
		const SourcePosition copy = position;
		for (char c: s) {
			if (!(position && *position == c)) {
				position = copy;
				return false;
			}
			++position;
		}
		return true;
	}
	bool parse(const char* s) {
		return parse(StringView(s));
	}
	template <class T> void parse_all(T&& t) {
		while (parse(std::forward<T>(t))) {}
	}
	Parser(const SourceFile* file): position(file) {}
	Parser(const SourcePosition& position): position(position) {}
	Parser copy() const {
		return Parser(position);
	}
};

class MoebiusParser: private Parser {
	Scope* current_scope;
	void expect(const StringView& s) {
		if (!parse(s)) {
			error(position, format("expected \"%\"", s));
		}
	}
	bool parse_comment() {
		if (parse("//")) {
			parse_all([](char c) { return c != '\n'; });
			parse("\n");
			return true;
		}
		if (parse("/*")) {
			while (position && !copy().parse("*/")) {
				++position;
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
			number += *position - '0';
			++position;
		}
		return new Number(number);
	}
	StringView parse_identifier() {
		if (!copy().parse(alphabetic)) {
			error(position, "expected alphabetic character");
		}
		const SourcePosition start = position;
		parse_all(alphanumeric);
		return position - start;
	}
	const Expression* parse_variable() {
		StringView identifier = parse_identifier();
		const Expression* expression = current_scope->look_up(identifier);
		if (expression == nullptr) {
			error(position, format("undefined variable \"%\"", identifier));
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
			while (position && *position != ')') {
				const StringView name = parse_identifier();
				function->add_argument_name(name);
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
			if (!position) {
				error(position, "unexpected end");
			}
			const char c = *position;
			++position;
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
			error(position, "unexpected character");
		}
	}
	const Expression* parse_expression(int level = 0) {
		if (level == 4) {
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (parse("(")) {
				parse_white_space();
				Call* call = new Call(expression);
				while (position && *position != ')') {
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
				error(position, "expected \"let\" or \"return\"");
				break;
			}
		}
		current_scope = scope.get_parent();
		return result;
	}
public:
	MoebiusParser(const SourceFile* file): Parser(file), current_scope(nullptr) {}
	const Expression* create_putChar() {
		Intrinsic* intrinsic = new Intrinsic("putChar", new VoidType());
		intrinsic->add_argument("c");
		Function* function = new Function(intrinsic);
		function->add_argument_name("c");
		return function;
	}
	const Expression* parse_program() {
		parse_white_space();
		Scope scope(nullptr);
		scope.add_variable("putChar", create_putChar());
		current_scope = &scope;
		return parse_scope();
	}
};

const Expression* parse(const SourceFile& file) {
	MoebiusParser parser(&file);
	return parser.parse_program();
}
