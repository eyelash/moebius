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

class NumberValue: public Value {
	std::int32_t value;
public:
	NumberValue(std::int32_t value): value(value) {}
	std::int32_t get_int() override {
		return value;
	}
	Closure* get_closure() override {
		return nullptr;
	}
};

Value* evaluate(const Expression* expression, const Environment& environment);

class Closure: public Value {
	const Function* function;
	Environment environment;
public:
	Closure(const Function* function, const Environment& environment): function(function), environment(environment) {}
	std::int32_t get_int() override {
		return 0;
	}
	Closure* get_closure() override {
		return this;
	}
	const Function* get_function() const {
		return function;
	}
	Value* call(const std::vector<Value*>& arguments) const {
		const std::vector<StringView>& argument_names = function->get_argument_names();
		Environment new_environment(environment);
		for (std::size_t i = 0; i < argument_names.size() && i < arguments.size(); ++i) {
			new_environment.insert(argument_names[i], arguments[i]);
		}
		return evaluate(function->get_expression(), new_environment);
	}
};

Value* evaluate(const Expression* expression, const Environment& environment) {
	class CodegenVisitor: public Visitor {
		Value* value;
		const Environment environment;
	public:
		CodegenVisitor(const Environment& environment): value(nullptr), environment(environment) {}
		void visit_number(const Number* number) override {
			value = new NumberValue(number->get_value());
		}
		void visit_addition(const Addition* addition) override {
			Value* left = evaluate(addition->get_left(), environment);
			Value* right = evaluate(addition->get_right(), environment);
			value = new NumberValue(left->get_int() + right->get_int());
		}
		void visit_subtraction(const Subtraction* subtraction) override {
			Value* left = evaluate(subtraction->get_left(), environment);
			Value* right = evaluate(subtraction->get_right(), environment);
			value = new NumberValue(left->get_int() - right->get_int());
		}
		void visit_multiplication(const Multiplication* multiplication) override {
			Value* left = evaluate(multiplication->get_left(), environment);
			Value* right = evaluate(multiplication->get_right(), environment);
			value = new NumberValue(left->get_int() * right->get_int());
		}
		void visit_division(const Division* division) override {
			Value* left = evaluate(division->get_left(), environment);
			Value* right = evaluate(division->get_right(), environment);
			value = new NumberValue(left->get_int() / right->get_int());
		}
		void visit_remainder(const Remainder* remainder) override {
			Value* left = evaluate(remainder->get_left(), environment);
			Value* right = evaluate(remainder->get_right(), environment);
			value = new NumberValue(left->get_int() % right->get_int());
		}
		void visit_function(const Function* function) override {
			value = new Closure(function, environment);
		}
		void visit_argument(const Argument* argument) override {
			value = environment.look_up(argument->get_name());
		}
		void visit_call(const Call* call) override {
			Closure* closure = evaluate(call->get_expression(), environment)->get_closure();
			std::vector<Value*> argument_values;
			for (const Expression* argument: call->get_arguments()) {
				argument_values.push_back(evaluate(argument, environment));
			}
			value = closure->call(argument_values);
		}
		Value* get_value() const {
			return value;
		}
	};
	CodegenVisitor visitor(environment);
	expression->accept(&visitor);
	return visitor.get_value();
}

Value* evaluate(const Expression* expression) {
	return evaluate(expression, Environment());
}
