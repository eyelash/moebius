#pragma once

#include "ast.hpp"
#include "assembler.hpp"

class CodegenX86: public Visitor<std::uint32_t> {
	using A = Assembler;
	using Jump = typename A::Jump;
	static std::uint32_t get_type_size(const Type* type) {
		switch (type->get_id()) {
			case NumberType::id:
				return 4;
			case ClosureType::id: {
				int size = 0;
				for (const Type* type: static_cast<const ClosureType*>(type)->get_environment_types()) {
					size += get_type_size(type);
				}
				return size;
			}
			default:
				return 0;
		}
	}
	struct DeferredCall {
		Jump jump;
		std::size_t function_index;
		DeferredCall(const Jump& jump, std::size_t function_index): jump(jump), function_index(function_index) {}
	};
	struct FunctionTableEntry {
		const Function* function;
		std::uint32_t input_size;
		std::uint32_t output_size;
		std::size_t position;
		FunctionTableEntry(const Function* function): function(function) {
			input_size = 0;
			for (const Type* type: function->get_argument_types()) {
				input_size += get_type_size(type);
			}
			output_size = get_type_size(function->get_expression()->get_type());
		}
		const Type* look_up(std::size_t index, std::uint32_t& location) const {
			const std::vector<const Type*>& argument_types = function->get_argument_types();
			location = std::max(input_size, output_size);
			for (std::size_t i = 0; i < argument_types.size(); ++i) {
				location -= get_type_size(argument_types[i]);
				if (i == index) {
					return argument_types[i];
				}
			}
			return nullptr;
		}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::vector<DeferredCall> deferred_calls;
		std::size_t done = 0;
		std::size_t look_up(const Function* function) {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				if (functions[index].function == function) {
					return index;
				}
			}
			functions.emplace_back(function);
			return index;
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
	};
	FunctionTable& function_table;
	const std::size_t index;
	A& assembler;
	std::uint32_t variable = 0;
	std::map<const Expression*,std::uint32_t> cache;
	CodegenX86(FunctionTable& function_table, std::size_t index, A& assembler): function_table(function_table), index(index), assembler(assembler) {}
	void evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
	}
	std::uint32_t allocate(std::uint32_t size) {
		variable -= size;
		return variable;
	}
	void memcopy(std::uint32_t destination, std::uint32_t source, std::uint32_t size) {
		for (std::uint32_t i = 0; i < size; i += 4) {
			assembler.MOV(EAX, PTR(EBP, source + i));
			assembler.MOV(PTR(EBP, destination + i), EAX);
		}
	}
public:
	std::uint32_t visit_number(const Number& number) override {
		const std::uint32_t result = allocate(4);
		assembler.MOV(EAX, number.get_value());
		assembler.MOV(PTR(EBP, result), EAX);
		return result;
	}
	std::uint32_t visit_binary_expression(const BinaryExpression& binary_expression) override {
		const std::uint32_t left = cache[binary_expression.get_left()];
		const std::uint32_t right = cache[binary_expression.get_right()];
		assembler.MOV(EAX, PTR(EBP, left));
		assembler.MOV(EBX, PTR(EBP, right));
		const std::uint32_t result = allocate(4);
		switch (binary_expression.get_operation()) {
			case BinaryOperation::ADD:
				assembler.ADD(EAX, EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::SUB:
				assembler.SUB(EAX, EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::MUL:
				assembler.IMUL(EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::DIV:
				assembler.CDQ();
				assembler.IDIV(EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::REM:
				assembler.CDQ();
				assembler.IDIV(EBX);
				assembler.MOV(PTR(EBP, result), EDX);
				break;
			case BinaryOperation::EQ:
				assembler.CMP(EAX, EBX);
				assembler.SETE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::NE:
				assembler.CMP(EAX, EBX);
				assembler.SETNE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::LT:
				assembler.CMP(EAX, EBX);
				assembler.SETL(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::LE:
				assembler.CMP(EAX, EBX);
				assembler.SETLE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::GT:
				assembler.CMP(EAX, EBX);
				assembler.SETG(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::GE:
				assembler.CMP(EAX, EBX);
				assembler.SETGE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
		}
		return result;
	}
	std::uint32_t visit_if(const If& if_) override {
		const std::uint32_t condition = cache[if_.get_condition()];
		assembler.MOV(EAX, PTR(EBP, condition));
		assembler.CMP(EAX, 0);
		const std::uint32_t size = get_type_size(if_.get_type());
		const std::uint32_t result = allocate(size);
		const Jump jump_else = assembler.JE();
		assembler.comment("if");
		evaluate(if_.get_then_block());
		const std::uint32_t then_result = cache[if_.get_then_expression()];
		memcopy(result, then_result, size);
		const Jump jump_end = assembler.JMP();
		assembler.comment("else");
		jump_else.set_target(assembler.get_position());
		evaluate(if_.get_else_block());
		const std::uint32_t else_result = cache[if_.get_else_expression()];
		memcopy(result, else_result, size);
		assembler.comment("end");
		jump_end.set_target(assembler.get_position());
		return result;
	}
	std::uint32_t visit_closure(const Closure& closure) override {
		const std::uint32_t size = get_type_size(closure.get_type());
		const std::vector<const Expression*>& environment = closure.get_environment_expressions();
		if (environment.size() == 1) {
			return cache[environment[0]];
		}
		else if (environment.size() > 1) {
			std::uint32_t destination = allocate(size);
			const std::uint32_t result = destination;
			for (const Expression* expression: environment) {
				const std::uint32_t element_size = get_type_size(expression->get_type());
				memcopy(destination, cache[expression], element_size);
				destination += element_size;
			}
			return result;
		}
	}
	std::uint32_t visit_closure_access(const ClosureAccess& closure_access) override {
		const std::uint32_t closure = cache[closure_access.get_closure()];
		// assert(closure_access.get_closure()->get_type_id() == ClosureType::id);
		const std::vector<const Type*>& types = static_cast<const ClosureType*>(closure_access.get_closure()->get_type())->get_environment_types();
		std::uint32_t before = 0;
		for (std::size_t i = 0; i < closure_access.get_index(); ++i) {
			before += get_type_size(types[i]);
		}
		return closure + before;
	}
	std::uint32_t visit_argument(const Argument& argument) override {
		std::uint32_t location;
		function_table[index].look_up(argument.get_index(), location);
		return 8 + location;
	}
	std::uint32_t visit_call(const Call& call) override {
		const std::size_t new_index = function_table.look_up(call.get_function());
		const std::uint32_t input_size = function_table[new_index].input_size;
		const std::uint32_t output_size = function_table[new_index].output_size;
		std::uint32_t destination = variable;
		for (const Expression* argument: call.get_arguments()) {
			const std::uint32_t argument_size = get_type_size(argument->get_type());
			destination -= argument_size;
			memcopy(destination, cache[argument], argument_size);
		}
		assembler.LEA(ESP, PTR(EBP, destination));
		if (output_size > input_size) {
			// negative in order to grow the stack
			assembler.ADD(ESP, input_size - output_size);
		}
		Jump jump = assembler.CALL();
		function_table.deferred_calls.emplace_back(jump, new_index);
		if (output_size < input_size) {
			assembler.ADD(ESP, input_size - output_size);
		}
		variable -= output_size;
		return variable;
	}
	std::uint32_t visit_intrinsic(const Intrinsic& intrinsic) override {
		if (intrinsic.name_equals("putChar")) {
			const std::uint32_t argument = cache[intrinsic.get_arguments()[0]];
			assembler.comment("putChar");
			assembler.MOV(EAX, 0x04);
			assembler.MOV(EBX, 1); // stdout
			assembler.LEA(ECX, PTR(EBP, argument));
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.POP(EAX);
		}
		else if (intrinsic.name_equals("getChar")) {
			const std::uint32_t result = allocate(4);
			assembler.comment("getChar");
			assembler.MOV(EAX, 0x03);
			assembler.MOV(EBX, 0); // stdin
			assembler.LEA(ECX, PTR(EBP, result));
			assembler.MOV(PTR(ECX), EBX);
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			return result;
		}
	}
	std::uint32_t visit_bind(const Bind& bind) override {}
	static void codegen(const Function* main_function, const char* path) {
		FunctionTable function_table;
		A assembler;
		{
			// the main function
			CodegenX86 codegen(function_table, 0, assembler);
			assembler.MOV(EBP, ESP);
			codegen.evaluate(main_function->get_block());
			assembler.comment("exit");
			assembler.MOV(EAX, 0x01);
			assembler.MOV(EBX, 0);
			assembler.INT(0x80);
		}
		while (function_table.done < function_table.size()) {
			const std::size_t index = function_table.done;
			assembler.comment("function");
			function_table[index].position = assembler.get_position();
			assembler.PUSH(EBP);
			assembler.MOV(EBP, ESP);
			assembler.comment("--");
			CodegenX86 codegen(function_table, index, assembler);
			codegen.evaluate(function_table[index].function->get_block());
			const std::uint32_t output_location = codegen.cache[function_table[index].function->get_expression()];
			assembler.comment("--");
			assembler.MOV(ESP, EBP);
			assembler.ADD(ESP, output_location);
			const std::uint32_t output_size = function_table[index].output_size;
			const std::uint32_t size = std::max(function_table[index].input_size, output_size);
			for (std::uint32_t i = 0; i < output_size; i += 4) {
				assembler.POP(EAX);
				assembler.MOV(PTR(EBP, 8 + size - output_size + i), EAX);
			}
			//assembler.ADD(ESP, output_size);
			assembler.MOV(ESP, EBP);
			assembler.POP(EBP);
			assembler.RET();
			function_table.done += 1;
		}
		for (const DeferredCall& deferred_call: function_table.deferred_calls) {
			const std::size_t target = function_table[deferred_call.function_index].position;
			deferred_call.jump.set_target(target);
		}
		assembler.write_file(path);
	}
};
