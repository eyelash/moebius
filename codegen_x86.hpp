#pragma once

#include "ast.hpp"
#include "assembler.hpp"

class CodegenX86: public Visitor<std::uint32_t> {
	using A = Assembler;
	using Jump = typename A::Jump;
	static std::uint32_t get_type_size(const Type* type) {
		switch (type->get_id()) {
			case TypeId::INT:
				return 4;
			case TypeId::TUPLE: {
				std::uint32_t size = 0;
				for (const Type* element_type: static_cast<const TupleType*>(type)->get_element_types()) {
					size += get_type_size(element_type);
				}
				return size;
			}
			default:
				return 0;
		}
	}
	static std::uint32_t get_input_size(const Function* function) {
		std::uint32_t input_size = 0;
		for (const Type* type: function->get_argument_types()) {
			input_size += get_type_size(type);
		}
		return input_size;
	}
	static std::uint32_t get_output_size(const Function* function) {
		return get_type_size(function->get_return_type());
	}
	static std::uint32_t get_argument_location(const Function* function, std::size_t index) {
		const std::vector<const Type*>& argument_types = function->get_argument_types();
		std::uint32_t location = std::max(get_input_size(function), get_output_size(function));
		for (std::size_t i = 0; i < argument_types.size(); ++i) {
			location -= get_type_size(argument_types[i]);
			if (i == index) {
				return location;
			}
		}
		return location;
	}
	struct DeferredCall {
		Jump jump;
		const Function* function;
		DeferredCall(const Jump& jump, const Function* function): jump(jump), function(function) {}
	};
	std::vector<DeferredCall>& deferred_calls;
	const Function* function;
	A& assembler;
	std::uint32_t variable = 0;
	std::map<const Expression*,std::uint32_t> expression_table;
	CodegenX86(std::vector<DeferredCall>& deferred_calls, const Function* function, A& assembler): deferred_calls(deferred_calls), function(function), assembler(assembler) {}
	std::uint32_t evaluate(const Block& block) {
		for (const Expression* expression: block) {
			expression_table[expression] = visit(*this, expression);
		}
		return expression_table[block.get_result()];
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
	std::uint32_t visit_int_literal(const IntLiteral& int_literal) override {
		const std::uint32_t result = allocate(4);
		assembler.MOV(PTR(EBP, result), int_literal.get_value());
		return result;
	}
	std::uint32_t visit_binary_expression(const BinaryExpression& binary_expression) override {
		const std::uint32_t left = expression_table[binary_expression.get_left()];
		const std::uint32_t right = expression_table[binary_expression.get_right()];
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
		const std::uint32_t condition = expression_table[if_.get_condition()];
		assembler.MOV(EAX, PTR(EBP, condition));
		assembler.CMP(EAX, 0);
		const std::uint32_t size = get_type_size(if_.get_type());
		const std::uint32_t result = allocate(size);
		const Jump jump_else = assembler.JE();
		assembler.comment("if");
		const std::uint32_t then_result = evaluate(if_.get_then_block());
		memcopy(result, then_result, size);
		const Jump jump_end = assembler.JMP();
		assembler.comment("else");
		jump_else.set_target(assembler, assembler.get_position());
		const std::uint32_t else_result = evaluate(if_.get_else_block());
		memcopy(result, else_result, size);
		assembler.comment("end");
		jump_end.set_target(assembler, assembler.get_position());
		return result;
	}
	std::uint32_t visit_tuple(const Tuple& tuple) override {
		const std::uint32_t size = get_type_size(tuple.get_type());
		const std::vector<const Expression*>& expressions = tuple.get_elements();
		if (expressions.size() == 1) {
			return expression_table[expressions[0]];
		}
		else if (expressions.size() > 1) {
			std::uint32_t destination = allocate(size);
			const std::uint32_t result = destination;
			for (const Expression* expression: expressions) {
				const std::uint32_t element_size = get_type_size(expression->get_type());
				memcopy(destination, expression_table[expression], element_size);
				destination += element_size;
			}
			return result;
		}
	}
	std::uint32_t visit_tuple_access(const TupleAccess& tuple_access) override {
		const std::uint32_t tuple = expression_table[tuple_access.get_tuple()];
		const std::vector<const Type*>& types = static_cast<const TupleType*>(tuple_access.get_tuple()->get_type())->get_element_types();
		std::uint32_t before = 0;
		for (std::size_t i = 0; i < tuple_access.get_index(); ++i) {
			before += get_type_size(types[i]);
		}
		return tuple + before;
	}
	std::uint32_t visit_argument(const Argument& argument) override {
		const std::uint32_t location = get_argument_location(function, argument.get_index());
		return 8 + location;
	}
	std::uint32_t visit_call(const Call& call) override {
		const std::uint32_t input_size = get_input_size(call.get_function());
		const std::uint32_t output_size = get_output_size(call.get_function());
		std::uint32_t destination = variable;
		for (const Expression* argument: call.get_arguments()) {
			const std::uint32_t argument_size = get_type_size(argument->get_type());
			destination -= argument_size;
			memcopy(destination, expression_table[argument], argument_size);
		}
		assembler.LEA(ESP, PTR(EBP, destination));
		if (output_size > input_size) {
			// negative in order to grow the stack
			assembler.ADD(ESP, input_size - output_size);
		}
		Jump jump = assembler.CALL();
		deferred_calls.emplace_back(jump, call.get_function());
		if (output_size < input_size) {
			assembler.ADD(ESP, input_size - output_size);
		}
		variable -= output_size;
		return variable;
	}
	std::uint32_t visit_intrinsic(const Intrinsic& intrinsic) override {
		if (intrinsic.name_equals("putChar")) {
			const std::uint32_t argument = expression_table[intrinsic.get_arguments()[0]];
			assembler.comment("putChar");
			assembler.MOV(EAX, 0x04);
			assembler.MOV(EBX, 1); // stdout
			assembler.LEA(ECX, PTR(EBP, argument));
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.POP(EAX);
			return allocate(0);
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
		else if (intrinsic.name_equals("arrayNew")) {
			print_error(Printer(std::cerr), "the x86 codegen does not support arrays");
			std::exit(EXIT_FAILURE);
		}
		else {
			return allocate(0);
		}
	}
	std::uint32_t visit_bind(const Bind& bind) override {
		return allocate(0);
	}
	std::uint32_t visit_return(const Return& return_) override {
		return allocate(0);
	}
	static void codegen(const Program& program, const char* source_path, const TailCallData& tail_call_data) {
		std::vector<DeferredCall> deferred_calls;
		std::map<const Function*, std::size_t> function_locations;
		A assembler;
		assembler.write_elf_header();
		assembler.write_program_header();
		{
			Jump jump = assembler.CALL();
			deferred_calls.emplace_back(jump, program.get_main_function());
			assembler.comment("exit");
			assembler.MOV(EAX, 0x01);
			assembler.MOV(EBX, 0);
			assembler.INT(0x80);
		}
		for (const Function* function: program) {
			assembler.comment("function");
			function_locations[function] = assembler.get_position();
			assembler.PUSH(EBP);
			assembler.MOV(EBP, ESP);
			assembler.comment("--");
			CodegenX86 codegen(deferred_calls, function, assembler);
			const std::uint32_t result = codegen.evaluate(function->get_block());
			assembler.comment("--");
			assembler.MOV(ESP, EBP);
			assembler.ADD(ESP, result);
			const std::uint32_t output_size = get_output_size(function);
			const std::uint32_t size = std::max(get_input_size(function), output_size);
			for (std::uint32_t i = 0; i < output_size; i += 4) {
				assembler.POP(EAX);
				assembler.MOV(PTR(EBP, 8 + size - output_size + i), EAX);
			}
			//assembler.ADD(ESP, output_size);
			assembler.MOV(ESP, EBP);
			assembler.POP(EBP);
			assembler.RET();
		}
		for (const DeferredCall& deferred_call: deferred_calls) {
			const std::size_t target = function_locations[deferred_call.function];
			deferred_call.jump.set_target(assembler, target);
		}
		std::string path = std::string(source_path) + ".exe";
		assembler.write_file(path.c_str());
		Printer status_printer(std::cerr);
		status_printer.print(bold(path));
		status_printer.print(bold(green(" successfully generated")));
		status_printer.print('\n');
	}
};
