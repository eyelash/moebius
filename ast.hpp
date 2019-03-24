#pragma once

#include "printer.hpp"
#include <cstdint>
#include <vector>

class Number;
class Addition;
class Subtraction;
class Multiplication;
class Division;
class Remainder;
class Function;
class Argument;
class Call;

class Visitor {
public:
	virtual void visit_number(const Number* number) = 0;
	virtual void visit_addition(const Addition* addition) = 0;
	virtual void visit_subtraction(const Subtraction* subtraction) = 0;
	virtual void visit_multiplication(const Multiplication* multiplication) = 0;
	virtual void visit_division(const Division* division) = 0;
	virtual void visit_remainder(const Remainder* remainder) = 0;
	virtual void visit_function(const Function* function) = 0;
	virtual void visit_argument(const Argument* argument) = 0;
	virtual void visit_call(const Call* call) = 0;
};

class Expression {
public:
	virtual void accept(Visitor* visitor) const = 0;
};

class BinaryExpression: public Expression {
	const Expression* left;
	const Expression* right;
public:
	BinaryExpression(const Expression* left, const Expression* right): left(left), right(right) {}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
	template <class T> static Expression* create(Expression* left, Expression* right) {
		return new T(left, right);
	}
};

class Number: public Expression {
	std::int32_t value;
public:
	Number(std::int32_t value): value(value) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_number(this);
	}
	std::int32_t get_value() const {
		return value;
	}
};

class Addition: public BinaryExpression {
public:
	Addition(const Expression* left, const Expression* right): BinaryExpression(left, right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_addition(this);
	}
};

class Subtraction: public BinaryExpression {
public:
	Subtraction(const Expression* left, const Expression* right): BinaryExpression(left, right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_subtraction(this);
	}
};

class Multiplication: public BinaryExpression {
public:
	Multiplication(const Expression* left, const Expression* right): BinaryExpression(left, right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_multiplication(this);
	}
};

class Division: public BinaryExpression {
public:
	Division(const Expression* left, const Expression* right): BinaryExpression(left, right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_division(this);
	}
};

class Remainder: public BinaryExpression {
public:
	Remainder(const Expression* left, const Expression* right): BinaryExpression(left, right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_remainder(this);
	}
};

class Function: public Expression {
	const Expression* expression;
	std::vector<StringView> argument_names;
public:
	Function(const Expression* expression, const std::vector<StringView>& argument_names): expression(expression), argument_names(argument_names) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_function(this);
	}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<StringView>& get_argument_names() const {
		return argument_names;
	}
};

class Argument: public Expression {
	StringView name;
public:
	Argument(const StringView& name): name(name) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_argument(this);
	}
	StringView get_name() const {
		return name;
	}
};

class Call: public Expression {
	const Expression* expression;
	std::vector<const Expression*> arguments;
public:
	Call(const Expression* expression, const std::vector<const Expression*>& arguments): expression(expression), arguments(arguments) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_call(this);
	}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};
