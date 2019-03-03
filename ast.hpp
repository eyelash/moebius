#pragma once

#include <cstdint>
#include <map>
#include <vector>

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

template <class T> class Table {
	std::map<StringView, T*> table;
public:
	void insert(const StringView& name, T* value) {
		table[name] = value;
	}
	T* look_up(const StringView& name) const {
		auto iterator = table.find(name);
		return iterator != table.end() ? iterator->second : nullptr;
	}
};

class Value;

class Callable {
public:
	virtual Value* call(const std::vector<Value*>& arguments) = 0;
};

class Value {
public:
	virtual std::int32_t get_int() {
		return 0;
	}
	virtual Callable* get_callable() {
		return nullptr;
	}
};

class Expression {
public:
	using Environment = Table<Value>;
	virtual Value* evaluate(const Environment& environment) = 0;
	Value* evaluate() {
		return evaluate(Environment());
	}
};

class Number: public Expression, public Value {
	std::int32_t value;
public:
	Number(std::int32_t value): value(value) {}
	Value* evaluate(const Environment& environment) override {
		return new Number(value);
	}
	std::int32_t get_int() override {
		return value;
	}
};

class Addition: public Expression {
	Expression* left;
	Expression* right;
public:
	Addition(Expression* left, Expression* right): left(left), right(right) {}
	Value* evaluate(const Environment& environment) override {
		return new Number(left->evaluate(environment)->get_int() + right->evaluate(environment)->get_int());
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
	Value* evaluate(const Environment& environment) override {
		return new Number(left->evaluate(environment)->get_int() - right->evaluate(environment)->get_int());
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
	Value* evaluate(const Environment& environment) override {
		return new Number(left->evaluate(environment)->get_int() * right->evaluate(environment)->get_int());
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
	Value* evaluate(const Environment& environment) override {
		return new Number(left->evaluate(environment)->get_int() / right->evaluate(environment)->get_int());
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
	Value* evaluate(const Environment& environment) override {
		return new Number(left->evaluate(environment)->get_int() % right->evaluate(environment)->get_int());
	}
	static Expression* create(Expression* left, Expression* right) {
		return new Remainder(left, right);
	}
};

class Function: public Expression {
	Expression* expression;
	std::vector<StringView> argument_names;
public:
	Function(Expression* expression, const std::vector<StringView>& argument_names): expression(expression), argument_names(argument_names) {}
	Value* evaluate(const Environment& environment) override {
		class Closure: public Value, public Callable {
			Function* function;
			Environment environment;
		public:
			Closure(Function* function, const Environment& environment): function(function), environment(environment) {}
			Callable* get_callable() override {
				return this;
			}
			Value* call(const std::vector<Value*>& arguments) override {
				const std::vector<StringView>& argument_names = function->argument_names;
				for (std::size_t i = 0; i < argument_names.size() && i < arguments.size(); ++i) {
					environment.insert(argument_names[i], arguments[i]);
				}
				return function->expression->evaluate(environment);
			}
		};
		return new Closure(this, environment);
	}
};

class Argument: public Expression {
	StringView name;
public:
	Argument(const StringView& name): name(name) {}
	Value* evaluate(const Environment& environment) override {
		return environment.look_up(name);
	}
};

class Call: public Expression {
	Expression* expression;
	std::vector<Expression*> arguments;
public:
	Call(Expression* expression, const std::vector<Expression*>& arguments): expression(expression), arguments(arguments) {}
	Value* evaluate(const Environment& environment) override {
		Callable* callable = expression->evaluate(environment)->get_callable();
		std::vector<Value*> argument_values;
		for (Expression* argument: arguments) {
			argument_values.push_back(argument->evaluate(environment));
		}
		return callable->call(argument_values);
	}
};
