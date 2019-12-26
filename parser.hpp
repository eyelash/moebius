#pragma once

#include "printer.hpp"
#include "ast.hpp"
#include <map>

struct BinaryOperator {
	const char* string;
	BinaryExpressionType type;
	constexpr BinaryOperator(const char* string, BinaryExpressionType type): string(string), type(type) {}
	constexpr BinaryOperator(): string(nullptr), type(BinaryExpressionType::ADD) {}
	constexpr operator bool() const {
		return string != nullptr;
	}
};

static constexpr BinaryOperator operators[][4] = {
	{
		BinaryOperator("<", BinaryExpressionType::LT)
	},
	{
		BinaryOperator("+", BinaryExpressionType::ADD),
		BinaryOperator("-", BinaryExpressionType::SUB)
	},
	{
		BinaryOperator("*", BinaryExpressionType::MUL),
		BinaryOperator("/", BinaryExpressionType::DIV),
		BinaryOperator("%", BinaryExpressionType::REM)
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
	void visit_number(const Number* number) override {}
	void visit_binary_expression(const BinaryExpression* expression) override {
		expression->get_left()->accept(this);
		expression->get_right()->accept(this);
	}
	void visit_if(const If* if_) override {
		if_->get_condition()->accept(this);
		if_->get_then_expression()->accept(this);
		if_->get_else_expression()->accept(this);
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
	void visit_builtin(const Builtin* builtin) override {}
};

class Position {
	const char* position;
	const char* line_start;
public:
	constexpr Position(const char* position): position(position), line_start(position) {}
	constexpr bool operator <(const char* end) const {
		return position < end;
	}
	constexpr char operator *() const {
		return *position;
	}
	Position& operator ++() {
		if (*position == '\n') {
			++position;
			line_start = position;
		}
		else {
			++position;
		}
		return *this;
	}
	constexpr StringView operator -(const Position& start) const {
		return StringView(start.position, position - start.position);
	}
	void print(Printer& p, const char* end) const {
		for (const char* c = line_start; c < end && *c != '\n'; ++c) {
			p.print(*c);
		}
		p.print('\n');
		for (const char* c = line_start; c < position; ++c) {
			p.print(*c == '\t' ? '\t' : ' ');
		}
		p.print('^');
		p.print('\n');
	}
};

class Parser {
	Position position;
	const char* end;
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
	template <class F> bool parse(F f, bool advance = true) {
		if (position < end && f(*position)) {
			if (advance) {
				++position;
			}
			return true;
		}
		return false;
	}
	bool parse(char c) {
		if (position < end && *position == c) {
			++position;
			return true;
		}
		return false;
	}
	bool parse(const StringView& s, bool advance = true) {
		Position copy = position;
		for (char c: s) {
			if (!(copy < end && *copy == c)) {
				return false;
			}
			++copy;
		}
		if (advance) {
			position = copy;
		}
		return true;
	}
	bool parse(const char* s, bool advance = true) {
		return parse(StringView(s), advance);
	}
	template <class T> void parse_all(T&& t) {
		while (parse(std::forward<T>(t))) {}
	}
	template <class... T> void error(const char* s, const T&... t) {
		Printer printer(stderr);
		printer.print(bold(red("error: ")));
		printer.print(s, t...);
		printer.print("\n");
		position.print(printer, end);
	}
	void expect(const StringView& s) {
		if (!parse(s)) {
			error("expected \"%\"", s);
		}
	}
	void parse_white_space() {
		parse_all(white_space);
	}
	const Expression* parse_number() {
		std::int32_t number = 0;
		while (parse(numeric, false)) {
			number *= 10;
			number += *position - '0';
			++position;
		}
		return new Number(number);
	}
	StringView parse_identifier() {
		if (!parse(alphabetic, false)) {
			error("expected alphabetic character");
		}
		const Position start = position;
		parse_all(alphanumeric);
		return position - start;
	}
	const Expression* parse_variable() {
		StringView identifier = parse_identifier();
		const Expression* expression = current_scope->look_up(identifier);
		if (expression == nullptr) {
			error("undefined variable \"%\"", identifier);
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
			Scope scope(current_scope);
			current_scope = &scope;
			while (position < end && *position != ')') {
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
		else if (parse("'")) {
			if (!(position < end)) {
				error("unexpected end");
			}
			const char c = *position;
			++position;
			expect("'");
			return new Number(c);
		}
		else if (parse(numeric, false)) {
			return parse_number();
		}
		else if (parse(alphabetic, false)) {
			return parse_variable();
		}
		else {
			error("unexpected character");
			return nullptr;
		}
	}
	const Expression* parse_expression(int level = 0) {
		if (level == 3) {
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (parse("(")) {
				parse_white_space();
				Call* call = new Call(expression);
				while (position < end && *position != ')') {
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
			left = new BinaryExpression(op.type, left, right);
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
	Parser(const StringView& string): position(string.begin()), end(string.end()), current_scope(nullptr) {}
	const Expression* parse() {
		parse_white_space();
		Scope scope(nullptr);
		scope.add_variable("putChar", new Builtin("putChar"));
		current_scope = &scope;
		return parse_scope();
	}
};
