#pragma once

#include "printer.hpp"
#include <cstdint>
#include <vector>

class Number;
class BinaryExpression;
class If;
class Function;
class Argument;
class Call;
class Intrinsic;

class Value {
public:
	virtual int get_id() = 0;
	virtual std::uint32_t get_size() = 0;
	template <class T> T* get() {
		return static_cast<T*>(this);
	}
};

class RuntimeNumber: public Value {
public:
	RuntimeNumber() {}
	static constexpr int id = 2;
	int get_id() override {
		return id;
	}
	std::uint32_t get_size() override {
		return 4;
	}
};

class Closure: public Value {
	const Function* function;
	std::vector<Value*> environment_values;
public:
	Closure(const Function* function): function(function) {}
	static constexpr int id = 3;
	int get_id() override {
		return id;
	}
	std::uint32_t get_size() override {
		int size = 0;
		for (Value* value: environment_values) {
			size += value->get_size();
		}
		return size;
	}
	void add_environment_value(Value* value) {
		environment_values.push_back(value);
	}
	const Function* get_function() const {
		return function;
	}
	const std::vector<Value*>& get_environment_values() const {
		return environment_values;
	}
};

class Never: public Value {
public:
	static constexpr int id = 6;
	int get_id() override {
		return id;
	}
	std::uint32_t get_size() override {
		return 0;
	}
};

class Void: public Value {
public:
	static constexpr int id = 5;
	int get_id() override {
		return id;
	}
	std::uint32_t get_size() override {
		return 0;
	}
};

class Visitor {
public:
	virtual void visit_number(const Number* number) = 0;
	virtual void visit_binary_expression(const BinaryExpression* binary_expression) = 0;
	virtual void visit_if(const If* if_) = 0;
	virtual void visit_function(const Function* function) = 0;
	virtual void visit_argument(const Argument* argument) = 0;
	virtual void visit_call(const Call* call) = 0;
	virtual void visit_intrinsic(const Intrinsic* intrinsic) = 0;
};

class Expression {
	Value* type;
public:
	Expression(Value* type = nullptr): type(type) {}
	virtual void accept(Visitor* visitor) const = 0;
	Value* get_type() const {
		return type;
	}
	void set_type(Value* type) {
		this->type = type;
	}
	int get_type_id() const {
		return get_type()->get_id();
	}
	template <class T> bool has_type() const {
		return get_type_id() == T::id;
	}
	template <class T0, class T1, class... T> bool has_type() const {
		return has_type<T0>() || has_type<T1, T...>();
	}
};

class Number: public Expression {
	std::int32_t value;
public:
	Number(std::int32_t value): Expression(new RuntimeNumber()), value(value) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_number(this);
	}
	std::int32_t get_value() const {
		return value;
	}
};

enum class BinaryOperation {
	ADD,
	SUB,
	MUL,
	DIV,
	REM,
	EQ,
	NE,
	LT,
	LE
};

class BinaryExpression: public Expression {
	BinaryOperation operation;
	const Expression* left;
	const Expression* right;
public:
	BinaryExpression(BinaryOperation operation, const Expression* left, const Expression* right): Expression(new RuntimeNumber()), operation(operation), left(left), right(right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_binary_expression(this);
	}
	BinaryOperation get_operation() const {
		return operation;
	}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
	template <BinaryOperation operation> static Expression* create(const Expression* left, const Expression* right) {
		return new BinaryExpression(operation, left, right);
	}
};

class If: public Expression {
	const Expression* condition;
	const Expression* then_expression;
	const Expression* else_expression;
public:
	If(const Expression* condition, const Expression* then_expression, const Expression* else_expression, Value* type = nullptr): Expression(type), condition(condition), then_expression(then_expression), else_expression(else_expression) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_if(this);
	}
	const Expression* get_condition() const {
		return condition;
	}
	const Expression* get_then_expression() const {
		return then_expression;
	}
	const Expression* get_else_expression() const {
		return else_expression;
	}
};

class Function: public Expression {
	const Expression* expression;
	std::vector<StringView> environment_names;
	std::vector<const Expression*> environment_expressions;
	std::vector<StringView> argument_names;
public:
	Function(const Expression* expression, Value* value = nullptr): Expression(value), expression(expression) {}
	Function(): expression(nullptr) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_function(this);
	}
	void set_expression(const Expression* expression) {
		this->expression = expression;
	}
	void add_environment_expression(const StringView& name, const Expression* expression) {
		environment_names.push_back(name);
		environment_expressions.push_back(expression);
	}
	// TODO: remove this again
	void add_environment_expression(const Expression* expression) {
		environment_expressions.push_back(expression);
	}
	void add_argument_name(const StringView& name) {
		argument_names.push_back(name);
	}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<StringView>& get_environment_names() const {
		return environment_names;
	}
	const std::vector<const Expression*>& get_environment_expressions() const {
		return environment_expressions;
	}
	const std::vector<StringView>& get_argument_names() const {
		return argument_names;
	}
};

class Argument: public Expression {
	StringView name;
public:
	Argument(const StringView& name, Value* type = nullptr): Expression(type), name(name) {}
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
	Call(const Expression* expression): expression(expression) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_call(this);
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	const Expression* get_expression() const {
		return expression;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};

class Intrinsic: public Expression {
	StringView name;
	std::vector<const Expression*> arguments;
public:
	Intrinsic(const StringView& name, Value* value): Expression(value), name(name) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_intrinsic(this);
	}
	void add_argument(const StringView& argument_name) {
		arguments.push_back(new Argument(argument_name));
	}
	StringView get_name() const {
		return name;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};
