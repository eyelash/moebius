#pragma once

#include "ast.hpp"
#include "assembler.hpp"

class FunctionTable {
	struct Entry {
		const Function* function;
		std::vector<Value*> environment_values;
		std::vector<Value*> argument_values;
		const Expression* expression;
		std::size_t position;
		std::uint32_t input_size;
		std::uint32_t output_size;
		Entry(const Function* function, const std::vector<Value*>& environment_values, const std::vector<Value*>& argument_values): function(function), environment_values(environment_values), argument_values(argument_values), expression(nullptr), position(0) {
			input_size = 0;
			for (Value* value: environment_values) {
				input_size += value->get_size();
			}
			for (Value* value: argument_values) {
				input_size += value->get_size();
			}
		}
		void set_expression(const Expression* expression) {
			this->expression = expression;
			output_size = expression->get_type()->get_size();
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
		const int type1 = value1->get_id();
		const int type2 = value2->get_id();
		if (type1 != type2) {
			return false;
		}
		if (type1 == Closure::id) {
			if (value1->get<Closure>()->get_function() != value2->get<Closure>()->get_function()) {
				return false;
			}
			const std::vector<Value*>& values1 = value1->get<Closure>()->get_environment_values();
			const std::vector<Value*>& values2 = value2->get<Closure>()->get_environment_values();
			return equals(values1, values2);
		}
		return true;
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
	std::size_t recursion = -1;
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
	const Entry& operator [](std::size_t index) const {
		return functions[index];
	}
	std::size_t size() const {
		return functions.size();
	}
	void clear(std::size_t index) {
		functions.erase(functions.begin() + index + 1, functions.end());
	}
};

// type checking and monomorphization
class Pass1: public Visitor {
	const Expression* expression;
	FunctionTable& function_table;
	std::size_t index;
public:
	Pass1(FunctionTable& function_table, std::size_t index): expression(nullptr), function_table(function_table), index(index) {}
	const Expression* evaluate(const Expression* expression) {
		this->expression = nullptr;
		expression->accept(this);
		const Expression* result = this->expression;
		this->expression = nullptr;
		return result;
	}
	void visit_number(const Number* number) override {
		expression = number;
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const Expression* left = evaluate(binary_expression->get_left());
		const Expression* right = evaluate(binary_expression->get_right());
		if (left->has_type<RuntimeNumber, Never>() && right->has_type<RuntimeNumber, Never>()) {
			expression = new BinaryExpression(binary_expression->get_operation(), left, right);
		}
		else {
			printf("error: binary expression of types %d and %d\n", left->get_type_id(), right->get_type_id());
		}
	}
	void visit_if(const If* if_) override {
		const Expression* condition = evaluate(if_->get_condition());
		if (condition->has_type<RuntimeNumber, Never>()) {
			const Expression* then_value = evaluate(if_->get_then_expression());
			const Expression* else_value = evaluate(if_->get_else_expression());
			// TODO: compare types properly
			if (then_value->get_type_id() == else_value->get_type_id()) {
				expression = new If(condition, then_value, else_value, then_value->get_type());
			}
			else if (then_value->has_type<Never>()) {
				expression = new If(condition, then_value, else_value, else_value->get_type());
			}
			else if (else_value->has_type<Never>()) {
				expression = new If(condition, then_value, else_value, then_value->get_type());
			}
			else {
				printf("error: if and else branches must return values of the same type\n");
			}
		}
		else {
			printf("error: type of condition must be a number\n");
		}
	}
	void visit_function(const Function* function) override {
		Closure* closure = new Closure(function);
		Function* new_function = new Function(nullptr, closure);
		for (const Expression* expression: function->get_environment_expressions()) {
			const Expression* new_expression = evaluate(expression);
			closure->add_environment_value(new_expression->get_type());
			new_function->add_environment_expression(new_expression);
		}
		expression = new_function;
	}
	void visit_argument(const Argument* argument) override {
		std::uint32_t location;
		Value* value = function_table[index].look_up(argument->get_name(), location);
		expression = new Argument(argument->get_name(), value);
	}
	void visit_call(const Call* call) override {
		const Expression* function = evaluate(call->get_expression());
		if (function->has_type<Closure>()) {
			Closure* closure = function->get_type()->get<Closure>();
			if (call->get_arguments().size() != closure->get_function()->get_argument_names().size()) {
				printf("error: call with %lu arguments to a function that accepts %lu arguments\n", call->get_arguments().size(), closure->get_function()->get_argument_names().size());
			}
			std::vector<Value*> argument_values;
			Call* new_call = new Call(function);
			for (const Expression* argument: call->get_arguments()) {
				const Expression* new_argument = evaluate(argument);
				argument_values.push_back(new_argument->get_type());
				new_call->add_argument(new_argument);
			}

			const std::size_t new_index = function_table.look_up(closure, argument_values);
			if (new_index == function_table.size()) {
				function_table.add_entry(closure, argument_values);
				Pass1 pass1(function_table, new_index);
				const Expression* e = pass1.evaluate(closure->get_function()->get_expression());
				function_table[new_index].set_expression(e);
				if (function_table.recursion == new_index) {
					// reevaluate the expression in case of recursion
					function_table.recursion = -1;
					function_table.clear(new_index);
					e = pass1.evaluate(closure->get_function()->get_expression());
					function_table[new_index].set_expression(e);
				}
				new_call->set_type(e->get_type());
				expression = new_call;
			}
			else {
				// detect recursion
				if (function_table[new_index].expression == nullptr) {
					if (new_index < function_table.recursion) {
						function_table.recursion = new_index;
					}
					new_call->set_type(new Never());
				}
				else {
					new_call->set_type(function_table[new_index].expression->get_type());
				}
				expression = new_call;
			}
		}
		else {
			printf("error: call to a value that is not a function\n");
		}
	}
	void visit_intrinsic(const Intrinsic* intrinsic) override {
		if (intrinsic->get_name() == "putChar") {
			const Expression* argument_value = evaluate(intrinsic->get_arguments()[0]);
			if (argument_value->get_type_id() != RuntimeNumber::id) {
				printf("error: argument of putChar must be a number\n");
			}
			expression = intrinsic;
		}
	}
};

struct DeferredCall {
	Assembler::Jump jump;
	std::size_t function_index;
	DeferredCall(const Assembler::Jump& jump, std::size_t function_index): jump(jump), function_index(function_index) {}
};

// code generation
class Pass2: public Visitor {
	const FunctionTable& function_table;
	std::vector<DeferredCall>& deferred_calls;
	std::size_t index;
	Assembler& assembler;
public:
	Pass2(const FunctionTable& function_table, std::vector<DeferredCall>& deferred_calls, std::size_t index, Assembler& assembler): function_table(function_table), deferred_calls(deferred_calls), index(index), assembler(assembler) {}
	void evaluate(const Expression* expression) {
		expression->accept(this);
	}
	void visit_number(const Number* number) override {
		printf("  PUSH %d\n", number->get_value());
		assembler.PUSH(number->get_value());
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const Expression* left = binary_expression->get_left();
		const Expression* right = binary_expression->get_right();
		evaluate(left);
		evaluate(right);
		if (left->has_type<RuntimeNumber, Never>() && right->has_type<RuntimeNumber, Never>()) {
			printf("  POP EBX\n");
			assembler.POP(EBX);
			printf("  POP EAX\n");
			assembler.POP(EAX);
			switch (binary_expression->get_operation()) {
				case BinaryOperation::ADD:
					printf("  ADD EAX, EBX\n");
					assembler.ADD(EAX, EBX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::SUB:
					printf("  SUB EAX, EBX\n");
					assembler.SUB(EAX, EBX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::MUL:
					printf("  IMUL EBX\n");
					assembler.IMUL(EBX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::DIV:
					printf("  CDQ\n");
					assembler.CDQ();
					printf("  IDIV EBX\n");
					assembler.IDIV(EBX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::REM:
					printf("  CDQ\n");
					assembler.CDQ();
					printf("  IDIV EBX\n");
					assembler.IDIV(EBX);
					printf("  PUSH EDX\n");
					assembler.PUSH(EDX);
					break;
				case BinaryOperation::EQ:
					printf("  CMP EAX, EBX\n");
					assembler.CMP(EAX, EBX);
					printf("  SETE EAX\n");
					assembler.SETE(EAX);
					printf("  MOVZX EAX, EAX\n");
					assembler.MOVZX(EAX, EAX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::NE:
					printf("  CMP EAX, EBX\n");
					assembler.CMP(EAX, EBX);
					printf("  SETNE EAX\n");
					assembler.SETNE(EAX);
					printf("  MOVZX EAX, EAX\n");
					assembler.MOVZX(EAX, EAX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::LT:
					printf("  CMP EAX, EBX\n");
					assembler.CMP(EAX, EBX);
					printf("  SETL EAX\n");
					assembler.SETL(EAX);
					printf("  MOVZX EAX, EAX\n");
					assembler.MOVZX(EAX, EAX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
				case BinaryOperation::LE:
					printf("  CMP EAX, EBX\n");
					assembler.CMP(EAX, EBX);
					printf("  SETLE EAX\n");
					assembler.SETLE(EAX);
					printf("  MOVZX EAX, EAX\n");
					assembler.MOVZX(EAX, EAX);
					printf("  PUSH EAX\n");
					assembler.PUSH(EAX);
					break;
			}
		}
	}
	void visit_if(const If* if_) override {
		const Expression* condition = if_->get_condition();
		evaluate(condition);
		if (condition->has_type<RuntimeNumber, Never>()) {
			printf("  POP EAX\n");
			assembler.POP(EAX);
			printf("  CMP EAX, 0\n");
			assembler.CMP(EAX, 0);
			printf("  JE\n;if\n");
			const Assembler::Jump jump_else = assembler.JE();
			evaluate(if_->get_then_expression());
			printf("  JMP\n;else\n");
			const Assembler::Jump jump_end = assembler.JMP();
			jump_else.set_target(assembler.get_position());
			evaluate(if_->get_else_expression());
			printf(";end\n");
			jump_end.set_target(assembler.get_position());
		}
	}
	void visit_function(const Function* function) override {
		for (const Expression* expression: function->get_environment_expressions()) {
			evaluate(expression);
		}
	}
	void visit_argument(const Argument* argument) override {
		std::uint32_t location;
		Value* value = function_table[index].look_up(argument->get_name(), location);
		const std::uint32_t size = value->get_size();
		for (std::uint32_t i = 0; i < size; i += 4) {
			printf("  PUSH [EBP + %d]\n", 8 + location + size - 4 - i);
			assembler.MOV(EAX, PTR(EBP, 8 + location + size - 4 - i));
			assembler.PUSH(EAX);
		}
	}
	void visit_call(const Call* call) override {
		const Expression* function = call->get_expression();
		evaluate(function);
		if (function->get_type_id() == Closure::id) {
			Closure* closure = function->get_type()->get<Closure>();
			std::vector<Value*> argument_values;
			for (const Expression* argument: call->get_arguments()) {
				evaluate(argument);
				argument_values.push_back(argument->get_type());
			}
			const std::size_t new_index = function_table.look_up(closure, argument_values);
			const std::uint32_t input_size = function_table[new_index].input_size;
			const std::uint32_t output_size = function_table[new_index].output_size;
			if (output_size > input_size) {
				// negative in order to grow the stack
				printf("  ADD ESP, %d\n", input_size - output_size);
				assembler.ADD(ESP, input_size - output_size);
			}
			printf("  CALL\n");
			Assembler::Jump jump = assembler.CALL();
			deferred_calls.emplace_back(jump, new_index);
			if (output_size < input_size) {
				printf("  ADD ESP, %d\n", input_size - output_size);
				assembler.ADD(ESP, input_size - output_size);
			}
		}
	}
	void visit_intrinsic(const Intrinsic* intrinsic) override {
		if (intrinsic->get_name() == "putChar") {
			evaluate(intrinsic->get_arguments()[0]);
			printf("  WRITE\n");
			assembler.MOV(EAX, 0x04);
			assembler.MOV(EBX, 1); // stdout
			assembler.MOV(ECX, ESP);
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.POP(EAX);
		}
	}
};

void codegen(const Expression* expression, const char* path) {
	Assembler assembler;
	FunctionTable function_table;
	std::vector<DeferredCall> deferred_calls;
	Pass1 pass1(function_table, 0);
	expression = pass1.evaluate(expression);
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
		const Expression* expression = function_table[index].expression;
		Pass2 pass2(function_table, deferred_calls, index, assembler);
		pass2.evaluate(expression);
		printf("  --\n");
		const std::uint32_t output_size = expression->get_type()->get_size();
		if (output_size != function_table[index].output_size) printf("error: output_size\n");
		const std::uint32_t size = std::max(function_table[index].input_size, function_table[index].output_size);
		for (std::uint32_t i = 0; i < output_size; i += 4) {
			printf("  POP [EBP + %d]\n", 8 + size - output_size + i);
			assembler.POP(EAX);
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
