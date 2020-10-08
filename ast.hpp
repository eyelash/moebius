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
	static const NumberType* get() {
		static NumberType* type = new NumberType();
		return type;
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
	static const NeverType* get() {
		static NeverType* type = new NeverType();
		return type;
	}
};

class VoidType: public Type {
public:
	static constexpr int id = 5;
	int get_id() const override {
		return id;
	}
	static const VoidType* get() {
		static VoidType* type = new VoidType();
		return type;
	}
};

template <class T> class Visitor {
public:
	virtual T visit_number(const Number& number) = 0;
	virtual T visit_binary_expression(const BinaryExpression& binary_expression) = 0;
	virtual T visit_if(const If& if_) = 0;
	virtual T visit_closure(const Closure& closure) = 0;
	virtual T visit_closure_access(const ClosureAccess& closure_access) = 0;
	virtual T visit_argument(const Argument& argument) = 0;
	virtual T visit_call(const Call& call) = 0;
	virtual T visit_intrinsic(const Intrinsic& intrinsic) = 0;
	virtual T visit_bind(const Bind& bind) = 0;
};

class Expression {
	const Type* type;
	SourcePosition position;
public:
	const Expression* next_expression = nullptr;
	Expression(const Type* type = nullptr): type(type) {}
	virtual void accept(Visitor<void>& visitor) const = 0;
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

template <class T> T visit(Visitor<T>& visitor, const Expression* expression) {
	class VoidVisitor: public Visitor<void> {
		Visitor<T>& visitor;
		T result;
	public:
		VoidVisitor(Visitor<T>& visitor): visitor(visitor) {}
		void visit_number(const Number& number) override {
			result = visitor.visit_number(number);
		}
		void visit_binary_expression(const BinaryExpression& binary_expression) override {
			result = visitor.visit_binary_expression(binary_expression);
		}
		void visit_if(const If& if_) override {
			result = visitor.visit_if(if_);
		}
		void visit_closure(const Closure& closure) override {
			result = visitor.visit_closure(closure);
		}
		void visit_closure_access(const ClosureAccess& closure_access) override {
			result = visitor.visit_closure_access(closure_access);
		}
		void visit_argument(const Argument& argument) override {
			result = visitor.visit_argument(argument);
		}
		void visit_call(const Call& call) override {
			result = visitor.visit_call(call);
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {
			result = visitor.visit_intrinsic(intrinsic);
		}
		void visit_bind(const Bind& bind) override {
			result = visitor.visit_bind(bind);
		}
		T get_result() const {
			return result;
		}
	};
	VoidVisitor void_visitor(visitor);
	expression->accept(void_visitor);
	return void_visitor.get_result();
}

inline void visit(Visitor<void>& visitor, const Expression* expression) {
	expression->accept(visitor);
}

class Block {
	const Expression* first = nullptr;
	Expression* last = nullptr;
	const Expression* result = nullptr;
public:
	void add_expression(Expression* expression) {
		if (first == nullptr) {
			first = expression;
		}
		if (last) {
			last->next_expression = expression;
		}
		last = expression;
	}
	void clear() {
		first = nullptr;
		last = nullptr;
	}
	void set_result(const Expression* expression) {
		result = expression;
	}
	const Expression* get_result() const {
		return result;
	}
	class Iterator {
		const Expression* expression;
	public:
		constexpr Iterator(const Expression* expression): expression(expression) {}
		constexpr bool operator !=(const Iterator& rhs) const {
			return expression != rhs.expression;
		}
		Iterator& operator ++() {
			expression = expression->next_expression;
			return *this;
		}
		constexpr const Expression* operator *() const {
			return expression;
		}
	};
	constexpr Iterator begin() const {
		return Iterator(first);
	}
	constexpr Iterator end() const {
		return Iterator(nullptr);
	}
};

class Number: public Expression {
	std::int32_t value;
public:
	Number(std::int32_t value): Expression(NumberType::get()), value(value) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_number(*this);
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
	BinaryExpression(BinaryOperation operation, const Expression* left, const Expression* right): Expression(NumberType::get()), operation(operation), left(left), right(right) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_binary_expression(*this);
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
	Block then_block;
	Block else_block;
public:
	If(const Expression* condition, const Type* type = nullptr): Expression(type), condition(condition) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_if(*this);
	}
	void set_then_expression(const Expression* then_expression) {
		then_block.set_result(then_expression);
	}
	void set_else_expression(const Expression* else_expression) {
		else_block.set_result(else_expression);
	}
	const Expression* get_condition() const {
		return condition;
	}
	const Expression* get_then_expression() const {
		return then_block.get_result();
	}
	const Expression* get_else_expression() const {
		return else_block.get_result();
	}
	Block* get_then_block() {
		return &then_block;
	}
	const Block& get_then_block() const {
		return then_block;
	}
	Block* get_else_block() {
		return &else_block;
	}
	const Block& get_else_block() const {
		return else_block;
	}
};

class Function {
	Block block;
	std::size_t arguments = 1;
	std::vector<const Type*> argument_types;
	const Type* return_type;
public:
	const Function* next_function = nullptr;
	Function(const Type* return_type = nullptr): return_type(return_type) {}
	void set_expression(const Expression* expression) {
		block.set_result(expression);
	}
	std::size_t add_argument() {
		return arguments++;
	}
	void add_argument_types(const std::vector<const Type*>& argument_types) {
		this->argument_types.insert(this->argument_types.end(), argument_types.begin(), argument_types.end());
	}
	void set_return_type(const Type* type) {
		this->return_type = type;
	}
	const Expression* get_expression() const {
		return block.get_result();
	}
	std::size_t get_arguments() const {
		return arguments;
	}
	const std::vector<const Type*>& get_argument_types() const {
		return argument_types;
	}
	const Type* get_return_type() const {
		return return_type;
	}
	Block* get_block() {
		return &block;
	}
	const Block& get_block() const {
		return block;
	}
};

class Closure: public Expression {
	const Function* function;
	std::vector<const Expression*> environment_expressions;
public:
	Closure(const Function* function, const Type* type = nullptr): Expression(type), function(function) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_closure(*this);
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
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_closure_access(*this);
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
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_argument(*this);
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
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_call(*this);
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
	Intrinsic(const char* name, const Type* type = nullptr): Expression(type), name(name) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_intrinsic(*this);
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
	Bind(const Expression* left, const Expression* right): Expression(VoidType::get()), left(left), right(right) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_bind(*this);
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

class Program {
	const Function* first = nullptr;
	Function* last = nullptr;
public:
	void add_function(Function* function) {
		if (first == nullptr) {
			first = function;
		}
		if (last) {
			last->next_function = function;
		}
		last = function;
	}
	const Function* get_main_function() const {
		return first;
	}
	class Iterator {
		const Function* function;
	public:
		constexpr Iterator(const Function* function): function(function) {}
		constexpr bool operator !=(const Iterator& rhs) const {
			return function != rhs.function;
		}
		Iterator& operator ++() {
			function = function->next_function;
			return *this;
		}
		constexpr const Function* operator *() const {
			return function;
		}
	};
	constexpr Iterator begin() const {
		return Iterator(first);
	}
	constexpr Iterator end() const {
		return Iterator(nullptr);
	}
};
