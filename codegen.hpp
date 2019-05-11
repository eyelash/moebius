#pragma once

#include "ast.hpp"
#include <map>

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

class Closure;

class Value {
public:
	virtual std::int32_t get_int() = 0;
	virtual Closure* get_closure() = 0;
};

using Environment = Table<Value>;

class CompiletimeNumber: public Value {
	std::int32_t value;
public:
	CompiletimeNumber(std::int32_t value): value(value) {}
	std::int32_t get_int() override {
		return value;
	}
	Closure* get_closure() override {
		return nullptr;
	}
};

class Closure: public Value {
	const Function* function;
	std::vector<Value*> environment_values;
public:
	Closure(const Function* function): function(function) {}
	std::int32_t get_int() override {
		return 0;
	}
	Closure* get_closure() override {
		return this;
	}
	void add_environment_value(Value* value) {
		environment_values.push_back(value);
	}
	const Expression* get_expression() const {
		return function->get_expression();
	}
	const std::vector<StringView>& get_argument_names() const {
		return function->get_argument_names();
	}
	const std::vector<StringView>& get_environment_names() const {
		return function->get_environment_names();
	}
	const std::vector<Value*>& get_environment_values() const {
		return environment_values;
	}
};

class CodegenVisitor: public Visitor {
	Value* value;
	const Environment& environment;
public:
	CodegenVisitor(const Environment& environment): value(nullptr), environment(environment) {}
	Value* evaluate(const Expression* expression) {
		value = nullptr;
		expression->accept(this);
		return value;
	}
	void visit_number(const Number* number) override {
		value = new CompiletimeNumber(number->get_value());
	}
	void visit_addition(const Addition* addition) override {
		Value* left = evaluate(addition->get_left());
		Value* right = evaluate(addition->get_right());
		value = new CompiletimeNumber(left->get_int() + right->get_int());
	}
	void visit_subtraction(const Subtraction* subtraction) override {
		Value* left = evaluate(subtraction->get_left());
		Value* right = evaluate(subtraction->get_right());
		value = new CompiletimeNumber(left->get_int() - right->get_int());
	}
	void visit_multiplication(const Multiplication* multiplication) override {
		Value* left = evaluate(multiplication->get_left());
		Value* right = evaluate(multiplication->get_right());
		value = new CompiletimeNumber(left->get_int() * right->get_int());
	}
	void visit_division(const Division* division) override {
		Value* left = evaluate(division->get_left());
		Value* right = evaluate(division->get_right());
		value = new CompiletimeNumber(left->get_int() / right->get_int());
	}
	void visit_remainder(const Remainder* remainder) override {
		Value* left = evaluate(remainder->get_left());
		Value* right = evaluate(remainder->get_right());
		value = new CompiletimeNumber(left->get_int() % right->get_int());
	}
	void visit_function(const Function* function) override {
		Closure* closure = new Closure(function);
		for (const StringView& name: function->get_environment_names()) {
			closure->add_environment_value(environment.look_up(name));
		}
		value = closure;
	}
	void visit_argument(const Argument* argument) override {
		value = environment.look_up(argument->get_name());
	}
	void visit_call(const Call* call) override {
		Closure* closure = evaluate(call->get_expression())->get_closure();
		const std::vector<StringView>& environment_names = closure->get_environment_names();
		const std::vector<StringView>& argument_names = closure->get_argument_names();
		const std::vector<Value*>& environment_values = closure->get_environment_values();
		std::vector<Value*> argument_values;
		for (const Expression* argument: call->get_arguments()) {
			argument_values.push_back(evaluate(argument));
		}
		Environment new_environment;
		for (std::size_t i = 0; i < environment_names.size() && i < environment_values.size(); ++i) {
			new_environment.insert(environment_names[i], environment_values[i]);
		}
		for (std::size_t i = 0; i < argument_names.size() && i < argument_values.size(); ++i) {
			new_environment.insert(argument_names[i], argument_values[i]);
		}
		CodegenVisitor visitor(new_environment);
		value = visitor.evaluate(closure->get_expression());
	}
};

Value* evaluate(const Expression* expression) {
	Environment environment;
	CodegenVisitor visitor(environment);
	return visitor.evaluate(expression);
}
