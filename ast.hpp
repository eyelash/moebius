#pragma once

#include "printer.hpp"
#include <cstdint>
#include <vector>

class Number;
class BinaryExpression;
class If;
class Function;
class Closure;
class ClosureAccess;
class Argument;
class Call;
class Intrinsic;
class Bind;

class Type {
public:
	virtual int get_id() const = 0;
};

class NumberType: public Type {
public:
	NumberType() {}
	static constexpr int id = 2;
	int get_id() const override {
		return id;
	}
};

class ClosureType: public Type {
	const Function* function;
	std::vector<const Type*> environment_types;
public:
	ClosureType(const Function* function): function(function) {}
	static constexpr int id = 3;
	int get_id() const override {
		return id;
	}
	void add_environment_type(const Type* type) {
		environment_types.push_back(type);
	}
	const Function* get_function() const {
		return function;
	}
	const std::vector<const Type*>& get_environment_types() const {
		return environment_types;
	}
};

class NeverType: public Type {
public:
	static constexpr int id = 6;
	int get_id() const override {
		return id;
	}
};

class VoidType: public Type {
public:
	static constexpr int id = 5;
	int get_id() const override {
		return id;
	}
};

class Visitor {
public:
	virtual void visit_number(const Number* number) = 0;
	virtual void visit_binary_expression(const BinaryExpression* binary_expression) = 0;
	virtual void visit_if(const If* if_) = 0;
	virtual void visit_closure(const Closure* closure) = 0;
	virtual void visit_closure_access(const ClosureAccess* closure_access) = 0;
	virtual void visit_argument(const Argument* argument) = 0;
	virtual void visit_call(const Call* call) = 0;
	virtual void visit_intrinsic(const Intrinsic* intrinsic) = 0;
	virtual void visit_bind(const Bind* bind) = 0;
};

class Expression {
	const Type* type;
	SourcePosition position;
public:
	Expression(const Type* type = nullptr): type(type) {}
	virtual void accept(Visitor* visitor) const = 0;
	const Type* get_type() const {
		return type;
	}
	void set_type(const Type* type) {
		this->type = type;
	}
	int get_type_id() const {
		return get_type()->get_id();
	}
	void set_position(const SourcePosition& position) {
		this->position = position;
	}
	const SourcePosition& get_position() const {
		return position;
	}
};

class Number: public Expression {
	std::int32_t value;
public:
	Number(std::int32_t value): Expression(new NumberType()), value(value) {}
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
	LE,
	GT,
	GE
};

class BinaryExpression: public Expression {
	BinaryOperation operation;
	const Expression* left;
	const Expression* right;
public:
	BinaryExpression(BinaryOperation operation, const Expression* left, const Expression* right): Expression(new NumberType()), operation(operation), left(left), right(right) {}
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
	If(const Expression* condition, const Expression* then_expression, const Expression* else_expression, const Type* type = nullptr): Expression(type), condition(condition), then_expression(then_expression), else_expression(else_expression) {}
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
	std::size_t arguments = 1;
public:
	Function(const Expression* expression, const Type* type = nullptr): Expression(type), expression(expression) {}
	Function(): expression(nullptr) {}
	void accept(Visitor* visitor) const override {}
	void set_expression(const Expression* expression) {
		this->expression = expression;
	}
	std::size_t add_argument() {
		return arguments++;
	}
	const Expression* get_expression() const {
		return expression;
	}
	std::size_t get_arguments() const {
		return arguments;
	}
};

class Closure: public Expression {
	const Function* function;
	std::vector<const Expression*> environment_expressions;
public:
	Closure(const Function* function, const Type* type = nullptr): Expression(type), function(function) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_closure(this);
	}
	std::size_t add_environment_expression(const Expression* expression) {
		const std::size_t index = environment_expressions.size();
		environment_expressions.push_back(expression);
		return index;
	}
	const Function* get_function() const {
		return function;
	}
	const std::vector<const Expression*>& get_environment_expressions() const {
		return environment_expressions;
	}
};

class ClosureAccess: public Expression {
	const Expression* closure;
	std::size_t index;
public:
	ClosureAccess(const Expression* closure, std::size_t index, const Type* type = nullptr): Expression(type), closure(closure), index(index) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_closure_access(this);
	}
	const Expression* get_closure() const {
		return closure;
	}
	std::size_t get_index() const {
		return index;
	}
};

class Argument: public Expression {
	std::size_t index;
public:
	Argument(std::size_t index, const Type* type = nullptr): Expression(type), index(index) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_argument(this);
	}
	std::size_t get_index() const {
		return index;
	}
};

class Call: public Expression {
	std::vector<const Expression*> arguments;
	const Function* function = nullptr;
public:
	Call() {}
	void accept(Visitor* visitor) const override {
		visitor->visit_call(this);
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	void set_function(const Function* function) {
		this->function = function;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
	const Expression* get_object() const {
		return arguments[0];
	}
	const Function* get_function() const {
		return function;
	}
};

class Intrinsic: public Expression {
	const char* name;
	std::vector<const Expression*> arguments;
public:
	Intrinsic(const char* name, const Type* type): Expression(type), name(name) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_intrinsic(this);
	}
	void add_argument(std::size_t index) {
		arguments.push_back(new Argument(index));
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	const char* get_name() const {
		return name;
	}
	bool name_equals(const StringView& name) const {
		return StringView(this->name) == name;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};

class Bind: public Expression {
	const Expression* left;
	const Expression* right;
public:
	Bind(const Expression* left, const Expression* right): Expression(new VoidType()), left(left), right(right) {}
	void accept(Visitor* visitor) const override {
		visitor->visit_bind(this);
	}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
	static Expression* create(const Expression* left, const Expression* right) {
		return new Bind(left, right);
	}
};
