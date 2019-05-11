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
	std::map<StringView, const Expression*> variables;
public:
	Scope(Scope* parent = nullptr): parent(parent) {}
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
		if (parent) {
			return parent->look_up(name);
		}
		return nullptr;
	}
};

class CaptureAnalysis: public Visitor {
	Function* function;
	void add_name(const StringView& name) {
		for (const StringView& argument: function->get_argument_names()) {
			if (argument == name) {
				return;
			}
		}
		function->add_environment_name(name);
	}
public:
	CaptureAnalysis(Function* function): function(function) {}
	void visit_binary_expression(const BinaryExpression* expression) {
		expression->get_left()->accept(this);
		expression->get_right()->accept(this);
	}
	void visit_number(const Number* number) override {}
	void visit_addition(const Addition* addition) override {
		visit_binary_expression(addition);
	}
	void visit_subtraction(const Subtraction* subtraction) override {
		visit_binary_expression(subtraction);
	}
	void visit_multiplication(const Multiplication* multiplication) override {
		visit_binary_expression(multiplication);
	}
	void visit_division(const Division* division) override {
		visit_binary_expression(division);
	}
	void visit_remainder(const Remainder* remainder) override {
		visit_binary_expression(remainder);
	}
	void visit_function(const Function* function) override {
		for (const StringView& name: function->get_environment_names()) {
			add_name(name);
		}
	}
	void visit_argument(const Argument* argument) override {
		add_name(argument->get_name());
	}
	void visit_call(const Call* call) override {
		call->get_expression()->accept(this);
		for (const Expression* argument: call->get_arguments()) {
			argument->accept(this);
		}
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
		if (0 < string.size() && alphabetic(string[0])) {
			std::size_t i = 1;
			while (i < string.size() && alphanumeric(string[i])) {
				++i;
			}
			StringView result = string.substr(0, i);
			string = string.substr(i);
			return result;
		}
		return StringView();
	}
	const Expression* parse_variable() {
		StringView identifier = parse_identifier();
		const Expression* expression = current_scope->look_up(identifier);
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
		else if (parse("fn")) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Scope scope(current_scope);
			current_scope = &scope;
			Function* function = new Function();
			while (0 < string.size() && string[0] != ')') {
				const StringView name = parse_identifier();
				function->add_argument_name(name);
				scope.add_variable(name, new Argument(name));
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
			CaptureAnalysis capture_analysis(function);
			expression->accept(&capture_analysis);
			return function;
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
	const Expression* parse_expression(int level = 0) {
		if (level == 2) {
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (parse("(")) {
				parse_white_space();
				Call* call = new Call(expression);
				while (0 < string.size() && string[0] != ')') {
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
	Parser(const char* string): string(string), current_scope(nullptr) {}
	Parser(const char* string, std::size_t length): string(string, length), current_scope(nullptr) {}
	const Expression* parse() {
		parse_white_space();
		return parse_scope();
	}
};
