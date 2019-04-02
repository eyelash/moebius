#pragma once

#include "printer.hpp"
#include "ast.hpp"
#include <map>

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

static constexpr BinaryOperator operators[][4] = {
	{
		BinaryOperator("+", BinaryExpression::create<Addition>),
		BinaryOperator("-", BinaryExpression::create<Subtraction>)
	},
	{
		BinaryOperator("*", BinaryExpression::create<Multiplication>),
		BinaryOperator("/", BinaryExpression::create<Division>),
		BinaryOperator("%", BinaryExpression::create<Remainder>)
	}
};

class Scope {
	Scope* parent;
	std::map<StringView, Expression*> variables;
public:
	Scope(Scope* parent = nullptr): parent(parent) {}
	Scope* get_parent() const {
		return parent;
	}
	void add_variable(const StringView& name, Expression* value) {
		variables[name] = value;
	}
	Expression* look_up(const StringView& name) const {
		auto iterator = variables.find(name);
		if (iterator != variables.end()) {
			return iterator->second;
		}
		if (parent) {
			return parent->look_up(name);
		}
		return nullptr;
	}
};

class Parser {
	StringView string;
	Scope* current_scope;
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
	template <class F> void parse_all(F f) {
		std::size_t i = 0;
		while (i < string.size() && f(string[i])) {
			++i;
		}
		string = string.substr(i);
	}
	bool parse(char c) {
		if (0 < string.size() && string[0] == c) {
			string = string.substr(1);
			return true;
		}
		return false;
	}
	bool parse(const StringView& s) {
		if (string.starts_with(s)) {
			string = string.substr(s.size());
			return true;
		}
		return false;
	}
	void parse_white_space() {
		parse_all(white_space);
	}
	template <class... T> void error(const char* s, const T&... t) {
		Printer printer(stderr);
		printer.print("error: ");
		printer.print(s, t...);
		printer.print("\n");
	}
	void expect(const StringView& s) {
		if (!parse(s)) {
			error("expected \"%\"", s);
		}
	}
	Expression* parse_number() {
		std::int32_t number = 0;
		while (0 < string.size() && numeric(string[0])) {
			number *= 10;
			number += string[0] - '0';
			string = string.substr(1);
		}
		return new Number(number);
	}
	StringView parse_identifier() {
		std::size_t i = 0;
		if (i < string.size() && alphabetic(string[i])) {
			++i;
			while (i < string.size() && alphanumeric(string[i])) {
				++i;
			}
			StringView result = string.substr(0, i);
			string = string.substr(i);
			return result;
		}
		return StringView();
	}
	Expression* parse_variable() {
		StringView identifier = parse_identifier();
		Expression* expression = current_scope->look_up(identifier);
		if (expression == nullptr) {
			error("invalid identifier \"%\"", identifier);
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
	Expression* parse_expression_last() {
		if (parse("{")) {
			parse_white_space();
			Expression* expression = parse_scope();
			parse_white_space();
			expect("}");
			return expression;
		}
		else if (parse("(")) {
			parse_white_space();
			Expression* expression = parse_expression();
			parse_white_space();
			expect(")");
			return expression;
		}
		else if (parse("fn")) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Scope scope(current_scope);
			current_scope = &scope;
			std::vector<StringView> argument_names;
			while (0 < string.size() && string[0] != ')') {
				const StringView name = parse_identifier();
				argument_names.push_back(name);
				scope.add_variable(name, new Argument(name));
				parse_white_space();
				if (parse(",")) {
					parse_white_space();
				}
			}
			expect(")");
			parse_white_space();
			Expression* expression = parse_expression();
			current_scope = scope.get_parent();
			return new Function(expression, argument_names);
		}
		else if (0 < string.size() && numeric(string[0])) {
			return parse_number();
		}
		else if (0 < string.size() && alphabetic(string[0])) {
			return parse_variable();
		}
		else {
			error("unexpected character");
			return nullptr;
		}
	}
	Expression* parse_expression(int level = 0) {
		if (level == 2) {
			Expression* expression = parse_expression_last();
			parse_white_space();
			while (parse("(")) {
				std::vector<const Expression*> arguments;
				parse_white_space();
				while (0 < string.size() && string[0] != ')') {
					arguments.push_back(parse_expression());
					parse_white_space();
					if (parse(",")) {
						parse_white_space();
					}
				}
				expect(")");
				expression = new Call(expression, arguments);
				parse_white_space();
			}
			return expression;
		}
		Expression* left = parse_expression(level + 1);
		parse_white_space();
		while (BinaryOperator op = parse_operator(level)) {
			parse_white_space();
			Expression* right = parse_expression(level + 1);
			left = op.create(left, right);
			parse_white_space();
		}
		return left;
	}
	Expression* parse_scope() {
		Scope scope(current_scope);
		current_scope = &scope;
		Expression* result = nullptr;
		while (true) {
			if (parse("let")) {
				parse_white_space();
				StringView name = parse_identifier();
				parse_white_space();
				expect("=");
				parse_white_space();
				Expression* expression = parse_expression();
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
	Parser(const char* string): string(string), current_scope(nullptr) {}
	Parser(const char* string, std::size_t length): string(string, length), current_scope(nullptr) {}
	Expression* parse() {
		parse_white_space();
		return parse_scope();
	}
};
