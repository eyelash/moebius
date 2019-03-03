#pragma once

#include <cstdint>

class Type {
public:
};

class Int: public Type {
public:
	static const Type* get() {
		static Int type;
		return &type;
	}
};

class Value;

class Expression {
public:
	virtual Value* evaluate() = 0;
};

class Value: public Expression {
public:
	Value* evaluate() override {
		return this;
	}
	virtual std::int32_t get_int() {
		return 0;
	}
};

class Number: public Value {
	std::int32_t value;
public:
	Number(std::int32_t value): value(value) {}
	std::int32_t get_int() override {
		return value;
	}
};

class Addition: public Expression {
	Expression* left;
	Expression* right;
public:
	Addition(Expression* left, Expression* right): left(left), right(right) {}
	Value* evaluate() override {
		return new Number(left->evaluate()->get_int() + right->evaluate()->get_int());
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Addition(left, right);
	}
};

class Subtraction: public Expression {
	Expression* left;
	Expression* right;
public:
	Subtraction(Expression* left, Expression* right): left(left), right(right) {}
	Value* evaluate() override {
		return new Number(left->evaluate()->get_int() - right->evaluate()->get_int());
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Subtraction(left, right);
	}
};

class Multiplication: public Expression {
	Expression* left;
	Expression* right;
public:
	Multiplication(Expression* left, Expression* right): left(left), right(right) {}
	Value* evaluate() override {
		return new Number(left->evaluate()->get_int() * right->evaluate()->get_int());
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Multiplication(left, right);
	}
};

class Division: public Expression {
	Expression* left;
	Expression* right;
public:
	Division(Expression* left, Expression* right): left(left), right(right) {}
	Value* evaluate() override {
		return new Number(left->evaluate()->get_int() / right->evaluate()->get_int());
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Division(left, right);
	}
};

class Remainder: public Expression {
	Expression* left;
	Expression* right;
public:
	Remainder(Expression* left, Expression* right): left(left), right(right) {}
	Value* evaluate() override {
		return new Number(left->evaluate()->get_int() % right->evaluate()->get_int());
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Remainder(left, right);
	}
};
