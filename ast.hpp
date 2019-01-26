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

class Expression {
public:
	virtual const Type* get_type() = 0;
	virtual void evaluate(void* result) = 0;
};

class Addition: public Expression {
	Expression* left;
	Expression* right;
public:
	Addition(Expression* left, Expression* right): left(left), right(right) {}
	const Type* get_type() override {
		return Int::get();
	}
	void evaluate(void* result) override {
		std::int32_t left_result, right_result;
		left->evaluate(&left_result);
		right->evaluate(&right_result);
		*static_cast<std::int32_t*>(result) = left_result + right_result;
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
	const Type* get_type() override {
		return Int::get();
	}
	void evaluate(void* result) override {
		std::int32_t left_result, right_result;
		left->evaluate(&left_result);
		right->evaluate(&right_result);
		*static_cast<std::int32_t*>(result) = left_result - right_result;
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
	const Type* get_type() override {
		return Int::get();
	}
	void evaluate(void* result) override {
		std::int32_t left_result, right_result;
		left->evaluate(&left_result);
		right->evaluate(&right_result);
		*static_cast<std::int32_t*>(result) = left_result * right_result;
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
	const Type* get_type() override {
		return Int::get();
	}
	void evaluate(void* result) override {
		std::int32_t left_result, right_result;
		left->evaluate(&left_result);
		right->evaluate(&right_result);
		*static_cast<std::int32_t*>(result) = left_result / right_result;
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
	const Type* get_type() override {
		return Int::get();
	}
	void evaluate(void* result) override {
		std::int32_t left_result, right_result;
		left->evaluate(&left_result);
		right->evaluate(&right_result);
		*static_cast<std::int32_t*>(result) = left_result % right_result;
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Remainder(left, right);
	}
};

class Number: public Expression {
	std::int32_t value;
public:
	Number(std::int32_t value): value(value) {}
	const Type* get_type() override {
		return Int::get();
	}
	void evaluate(void* result) override {
		*static_cast<std::int32_t*>(result) = value;
	}
};
