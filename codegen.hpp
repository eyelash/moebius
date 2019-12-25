#pragma once

#include "ast.hpp"
#include "assembler.hpp"

class Value {
public:
	virtual int get_type() = 0;
	virtual std::uint32_t get_size() = 0;
	template <class T> T* get() {
		return static_cast<T*>(this);
	}
};

class CompiletimeNumber: public Value {
	std::int32_t value;
public:
	CompiletimeNumber(std::int32_t value): value(value) {}
	static constexpr int type = 1;
	int get_type() override {
		return type;
	}
	std::uint32_t get_size() override {
		return 0;
	}
	std::int32_t get_int() const {
		return value;
	}
};

class RuntimeNumber: public Value {
public:
	RuntimeNumber() {}
	static constexpr int type = 2;
	int get_type() override {
		return type;
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
	static constexpr int type = 3;
	int get_type() override {
		return type;
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

class CompiletimeBuiltin: public Value {
	StringView name;
public:
	CompiletimeBuiltin(const StringView& name): name(name) {}
	static constexpr int type = 4;
	int get_type() override {
		return type;
	}
	std::uint32_t get_size() override {
		return 0;
	}
	const StringView& get_name() const {
		return name;
	}
};

class Void: public Value {
public:
	static constexpr int type = 5;
	int get_type() override {
		return type;
	}
	std::uint32_t get_size() override {
		return 0;
	}
};

class FunctionTable {
	struct Entry {
		const Function* function;
		std::vector<Value*> environment_values;
		std::vector<Value*> argument_values;
		Value* return_value;
		std::size_t position;
		std::uint32_t input_size;
		std::uint32_t output_size;
		Entry(const Function* function, const std::vector<Value*>& environment_values, const std::vector<Value*>& argument_values): function(function), environment_values(environment_values), argument_values(argument_values), return_value(nullptr), position(0) {
			input_size = 0;
			for (Value* value: environment_values) {
				input_size += value->get_size();
			}
			for (Value* value: argument_values) {
				input_size += value->get_size();
			}
		}
		void set_return_value(Value* return_value) {
			this->return_value = return_value;
			output_size = return_value->get_size();
		}
		Value* look_up(const StringView& name, std::uint32_t& location) const {
			const std::vector<StringView>& environment_names = function->get_environment_names();
			const std::vector<StringView>& argument_names = function->get_argument_names();
			location = std::max(input_size, output_size);
			for (std::size_t i = 0; i < environment_names.size(); ++i) {
				location -= environment_values[i]->get_size();
				if (environment_names[i] == name) {
					return environment_values[i];
				}
			}
			for (std::size_t i = 0; i < argument_names.size(); ++i) {
				location -= argument_values[i]->get_size();
				if (argument_names[i] == name) {
					return argument_values[i];
				}
			}
			return nullptr;
		}
	};
	std::vector<Entry> functions;
	static bool equals(Value* value1, Value* value2) {
		const int type1 = value1->get_type();
		const int type2 = value2->get_type();
		if (type1 != type2) {
			return false;
		}
		if (type1 == CompiletimeNumber::type) {
			return value1->get<CompiletimeNumber>()->get_int() == value2->get<CompiletimeNumber>()->get_int();
		}
		if (type1 == RuntimeNumber::type) {
			return true;
		}
		if (type1 == Closure::type) {
			const std::vector<Value*>& values1 = value1->get<Closure>()->get_environment_values();
			const std::vector<Value*>& values2 = value2->get<Closure>()->get_environment_values();
			return equals(values1, values2);
		}
		if (type1 == CompiletimeBuiltin::type) {
			return value1->get<CompiletimeBuiltin>()->get_name() == value2->get<CompiletimeBuiltin>()->get_name();
		}
		if (type1 == Void::type) {
			return true;
		}
		return false;
	}
	static bool equals(const std::vector<Value*>& values1, const std::vector<Value*>& values2) {
		if (values1.size() != values2.size()) {
			return false;
		}
		for (std::size_t i = 0; i < values1.size(); ++i) {
			if (!equals(values1[i], values2[i])) {
				return false;
			}
		}
		return true;
	}
public:
	std::size_t look_up(const Closure* closure, const std::vector<Value*>& argument_values) const {
		std::size_t index;
		for (index = 0; index < functions.size(); ++index) {
			const Entry& entry = functions[index];
			if (entry.function == closure->get_function() && equals(entry.environment_values, closure->get_environment_values()) && equals(entry.argument_values, argument_values)) {
				return index;
			}
		}
		return index;
	}
	void add_entry(const Closure* closure, const std::vector<Value*>& argument_values) {
		functions.emplace_back(closure->get_function(), closure->get_environment_values(), argument_values);
	}
	Entry& operator [](std::size_t index) {
		return functions[index];
	}
	std::size_t size() const {
		return functions.size();
	}
};

class Pass1: public Visitor {
	Value* value;
	FunctionTable& function_table;
	std::size_t index;
public:
	Pass1(FunctionTable& function_table, std::size_t index): value(nullptr), function_table(function_table), index(index) {}
	Value* evaluate(const Expression* expression) {
		value = nullptr;
		expression->accept(this);
		return value;
	}
	void visit_number(const Number* number) override {
		//value = new CompiletimeNumber(number->get_value());
		value = new RuntimeNumber();
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		Value* left = evaluate(binary_expression->get_left());
		Value* right = evaluate(binary_expression->get_right());
		if (left->get_type() == CompiletimeNumber::type && right->get_type() == CompiletimeNumber::type) {
			const std::int32_t left_int = left->get<CompiletimeNumber>()->get_int();
			const std::int32_t right_int = right->get<CompiletimeNumber>()->get_int();
			switch (binary_expression->get_type()) {
				case BinaryExpressionType::ADD:
					value = new CompiletimeNumber(left_int + right_int);
					break;
				case BinaryExpressionType::SUB:
					value = new CompiletimeNumber(left_int - right_int);
					break;
				case BinaryExpressionType::MUL:
					value = new CompiletimeNumber(left_int * right_int);
					break;
				case BinaryExpressionType::DIV:
					value = new CompiletimeNumber(left_int / right_int);
					break;
				case BinaryExpressionType::REM:
					value = new CompiletimeNumber(left_int % right_int);
					break;
				case BinaryExpressionType::LT:
					value = new CompiletimeNumber(left_int < right_int);
					break;
			}
		}
		else {
			value = new RuntimeNumber();
		}
	}
	void visit_function(const Function* function) override {
		Closure* closure = new Closure(function);
		for (const StringView& name: function->get_environment_names()) {
			std::uint32_t location;
			Value* value = function_table[index].look_up(name, location);
			closure->add_environment_value(value);
		}
		value = closure;
	}
	void visit_argument(const Argument* argument) override {
		std::uint32_t location;
		value = function_table[index].look_up(argument->get_name(), location);
	}
	void visit_call(const Call* call) override {
		Value* function = evaluate(call->get_expression());
		if (function->get_type() == Closure::type) {
			Closure* closure = function->get<Closure>();
			std::vector<Value*> argument_values;
			for (const Expression* argument: call->get_arguments()) {
				argument_values.push_back(evaluate(argument));
			}

			const std::size_t new_index = function_table.look_up(closure, argument_values);
			if (new_index == function_table.size()) {
				function_table.add_entry(closure, argument_values);
				Pass1 pass1(function_table, new_index);
				value = pass1.evaluate(closure->get_expression());
				function_table[new_index].set_return_value(value);
			}
			else {
				value = function_table[new_index].return_value;
			}
		}
		else if (function->get_type() == CompiletimeBuiltin::type) {
			if (function->get<CompiletimeBuiltin>()->get_name() == "putChar") {
				if (call->get_arguments().size() != 1) {
					printf("error: call->get_arguments().size() != 1\n");
				}
				Value* argument_value = evaluate(call->get_arguments()[0]);
				if (argument_value->get_type() != RuntimeNumber::type) {
					printf("error: value->get_type != RuntimeNumber::type\n");
				}
				value = new Void();
			}
		}
	}
	void visit_builtin(const Builtin* builtin) override {
		value = new CompiletimeBuiltin(builtin->get_name());
	}
};

struct DeferredCall {
	Assembler::Jump jump;
	std::size_t function_index;
	DeferredCall(const Assembler::Jump& jump, std::size_t function_index): jump(jump), function_index(function_index) {}
};

class Pass2: public Visitor {
	Value* value;
	FunctionTable& function_table;
	std::vector<DeferredCall>& deferred_calls;
	std::size_t index;
	Assembler& assembler;
public:
	Pass2(FunctionTable& function_table, std::vector<DeferredCall>& deferred_calls, std::size_t index, Assembler& assembler): value(nullptr), function_table(function_table), deferred_calls(deferred_calls), index(index), assembler(assembler) {}
	Value* evaluate(const Expression* expression) {
		value = nullptr;
		expression->accept(this);
		return value;
	}
	void visit_number(const Number* number) override {
		printf("  PUSH %d\n", number->get_value());
		assembler.PUSH(number->get_value());
		//value = new CompiletimeNumber(number->get_value());
		value = new RuntimeNumber();
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		Value* left = evaluate(binary_expression->get_left());
		Value* right = evaluate(binary_expression->get_right());
		if (left->get_type() == CompiletimeNumber::type && right->get_type() == CompiletimeNumber::type) {
			const std::int32_t left_int = left->get<CompiletimeNumber>()->get_int();
			const std::int32_t right_int = right->get<CompiletimeNumber>()->get_int();
			switch (binary_expression->get_type()) {
				case BinaryExpressionType::ADD:
					value = new CompiletimeNumber(left_int + right_int);
					break;
				case BinaryExpressionType::SUB:
					value = new CompiletimeNumber(left_int - right_int);
					break;
				case BinaryExpressionType::MUL:
					value = new CompiletimeNumber(left_int * right_int);
					break;
				case BinaryExpressionType::DIV:
					value = new CompiletimeNumber(left_int / right_int);
					break;
				case BinaryExpressionType::REM:
					value = new CompiletimeNumber(left_int % right_int);
					break;
				case BinaryExpressionType::LT:
					value = new CompiletimeNumber(left_int < right_int);
					break;
			}
		}
		else {
			switch (binary_expression->get_type()) {
				case BinaryExpressionType::ADD:
					printf("  ADD\n");
					assembler.POP(EBX);
					assembler.POP(EAX);
					assembler.ADD(EAX, EBX);
					assembler.PUSH(EAX);
					value = new RuntimeNumber();
					break;
				case BinaryExpressionType::SUB:
					printf("  SUB\n");
					assembler.POP(EBX);
					assembler.POP(EAX);
					assembler.SUB(EAX, EBX);
					assembler.PUSH(EAX);
					break;
				case BinaryExpressionType::MUL:
					printf("  IMUL\n");
					assembler.POP(EBX);
					assembler.POP(EAX);
					assembler.IMUL(EBX);
					assembler.PUSH(EAX);
					break;
				case BinaryExpressionType::DIV:
					printf("  IDIV\n");
					assembler.POP(EBX);
					assembler.POP(EAX);
					assembler.CDQ();
					assembler.IDIV(EBX);
					assembler.PUSH(EAX);
					break;
				case BinaryExpressionType::REM:
					printf("  IDIV\n");
					assembler.POP(EBX);
					assembler.POP(EAX);
					assembler.CDQ();
					assembler.IDIV(EBX);
					assembler.PUSH(EDX);
					break;
				case BinaryExpressionType::LT:
					printf("  CMP\n");
					printf("  SETL\n");
					assembler.POP(EBX);
					assembler.POP(EAX);
					assembler.CMP(EAX, EBX);
					assembler.SETL(EAX);
					assembler.PUSH(EAX);
					break;
			}
		}
	}
	void visit_function(const Function* function) override {
		Closure* closure = new Closure(function);
		for (const StringView& name: function->get_environment_names()) {
			std::uint32_t location;
			Value* value = function_table[index].look_up(name, location);
			closure->add_environment_value(value);
			const std::uint32_t size = value->get_size();
			for (std::uint32_t i = 0; i < size; i += 4) {
				printf("  MOV EAX, [EBP + %d]\n", 8 + location + i);
				assembler.MOV(EAX, PTR(EBP, 8 + location + i));
				printf("  PUSH EAX\n");
				assembler.PUSH(EAX);
			}
		}
		value = closure;
	}
	void visit_argument(const Argument* argument) override {
		std::uint32_t location;
		value = function_table[index].look_up(argument->get_name(), location);
		const std::uint32_t size = value->get_size();
		for (std::uint32_t i = 0; i < size; i += 4) {
			printf("  MOV EAX, [EBP + %d]\n", 8 + location + i);
			assembler.MOV(EAX, PTR(EBP, 8 + location + i));
			printf("  PUSH EAX\n");
			assembler.PUSH(EAX);
		}
	}
	void visit_call(const Call* call) override {
		Value* function = evaluate(call->get_expression());
		if (function->get_type() == Closure::type) {
			Closure* closure = function->get<Closure>();
			std::vector<Value*> argument_values;
			for (const Expression* argument: call->get_arguments()) {
				argument_values.push_back(evaluate(argument));
			}
			const std::size_t new_index = function_table.look_up(closure, argument_values);
			printf("  CALL\n");
			Assembler::Jump jump = assembler.CALL();
			deferred_calls.emplace_back(jump, new_index);
			value = function_table[new_index].return_value;
			const std::uint32_t diff = function_table[new_index].input_size - function_table[new_index].output_size;
			if (diff != 0) {
				printf("  ADD ESP, %d\n", diff);
				assembler.ADD(ESP, diff);
			}
		}
		else if (function->get_type() == CompiletimeBuiltin::type) {
			if (function->get<CompiletimeBuiltin>()->get_name() == "putChar") {
				evaluate(call->get_arguments()[0]);
				printf("  WRITE\n");
				assembler.MOV(EAX, 0x04);
				assembler.MOV(EBX, 1); // stdout
				assembler.MOV(ECX, ESP);
				assembler.MOV(EDX, 1);
				assembler.INT(0x80);
				assembler.POP(EAX);
				value = new Void();
			}
		}
	}
	void visit_builtin(const Builtin* builtin) override {
		value = new CompiletimeBuiltin(builtin->get_name());
	}
};

void codegen(const Expression* expression, const char* path) {
	Assembler assembler;
	FunctionTable function_table;
	std::vector<DeferredCall> deferred_calls;
	Pass1 pass1(function_table, 0);
	pass1.evaluate(expression);
	{
		// the main function
		Pass2 pass2(function_table, deferred_calls, 0, assembler);
		pass2.evaluate(expression);
		printf("  EXIT\n");
		assembler.MOV(EAX, 0x01);
		assembler.MOV(EBX, 0);
		assembler.INT(0x80);
	}
	for (std::size_t index = 0; index < function_table.size(); ++index) {
		printf("function:\n");
		function_table[index].position = assembler.get_position();
		printf("  PUSH EBP\n");
		assembler.PUSH(EBP);
		printf("  MOV EBP, ESP\n");
		assembler.MOV(EBP, ESP);
		printf("  --\n");
		const Expression* expression = function_table[index].function->get_expression();
		Pass2 pass2(function_table, deferred_calls, index, assembler);
		Value* value = pass2.evaluate(expression);
		printf("  --\n");
		const std::uint32_t output_size = value->get_size();
		if (value->get_size() != function_table[index].output_size) printf("error: output_size\n");
		const std::uint32_t size = std::max(function_table[index].input_size, function_table[index].output_size);
		for (std::uint32_t i = 0; i < output_size; i += 4) {
			printf("  MOV EAX, [ESP + %u]\n", i);
			assembler.MOV(EAX, PTR(ESP, i));
			printf("  MOV [EBP + %u], EAX\n", 8 + size - output_size + i);
			assembler.MOV(PTR(EBP, 8 + size - output_size + i), EAX);
		}
		//printf("  ADD ESP, %d\n", output_size);
		//assembler.ADD(ESP, output_size);
		printf("  MOV ESP, EBP\n");
		assembler.MOV(ESP, EBP);
		printf("  POP EBP\n");
		assembler.POP(EBP);
		printf("  RET\n");
		assembler.RET();
	}
	for (const DeferredCall& deferred_call: deferred_calls) {
		const std::size_t target = function_table[deferred_call.function_index].position;
		deferred_call.jump.set_target(target);
	}
	assembler.write_file(path);
}
