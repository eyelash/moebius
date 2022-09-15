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
	struct DeferredCall {
		Jump jump;
		const Function* function;
		DeferredCall(const Jump& jump, const Function* function): jump(jump), function(function) {}
	};
	std::vector<DeferredCall>& deferred_calls;
	const Function* function;
	A& assembler;
	using ExpressionTable = std::map<const Expression*,std::uint32_t>;
	ExpressionTable& expression_table;
	std::uint32_t variable = 0;
	std::uint32_t result;
	CodegenX86(std::vector<DeferredCall>& deferred_calls, const Function* function, A& assembler, ExpressionTable& expression_table, std::uint32_t result): deferred_calls(deferred_calls), function(function), assembler(assembler), expression_table(expression_table), result(result) {}
	static void evaluate(std::vector<DeferredCall>& deferred_calls, const Function* function, A& assembler, ExpressionTable& expression_table, std::uint32_t result, const Block& block) {
		CodegenX86 codegen(deferred_calls, function, assembler, expression_table, result);
		for (const Expression* expression: block) {
			expression_table[expression] = visit(codegen, expression);
		}
	}
	static void evaluate(std::vector<DeferredCall>& deferred_calls, const Function* function, A& assembler, std::uint32_t result, const Block& block) {
		ExpressionTable expression_table;
		evaluate(deferred_calls, function, assembler, expression_table, result, block);
	}
	void evaluate(std::uint32_t result, const Block& block) {
		evaluate(deferred_calls, function, assembler, expression_table, result, block);
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
		evaluate(result, if_.get_then_block());
		const Jump jump_end = assembler.JMP();
		assembler.comment("else");
		jump_else.set_target(assembler, assembler.get_position());
		evaluate(result, if_.get_else_block());
		assembler.comment("end");
		jump_end.set_target(assembler, assembler.get_position());
		return result;
	}
	std::uint32_t visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		const std::vector<const Expression*>& elements = tuple_literal.get_elements();
		if (elements.size() == 1) {
			return expression_table[elements[0]];
		}
		else {
			for (const Expression* element: elements) {
				const std::uint32_t element_size = get_type_size(element->get_type());
				const std::uint32_t element_destination = allocate(element_size);
				memcopy(element_destination, expression_table[element], element_size);
			}
			return variable;
		}
	}
	std::uint32_t visit_tuple_access(const TupleAccess& tuple_access) override {
		const std::uint32_t tuple = expression_table[tuple_access.get_tuple()];
		const std::vector<const Type*>& element_types = static_cast<const TupleType*>(tuple_access.get_tuple()->get_type())->get_element_types();
		std::uint32_t location = tuple + get_type_size(tuple_access.get_tuple()->get_type());
		for (std::size_t i = 0; i <= tuple_access.get_index(); ++i) {
			location -= get_type_size(element_types[i]);
		}
		return location;
	}
	std::uint32_t visit_argument(const Argument& argument) override {
		const std::vector<const Type*>& argument_types = function->get_argument_types();
		std::uint32_t location = 8 + std::max(get_input_size(function), get_output_size(function));
		for (std::size_t i = 0; i <= argument.get_index(); ++i) {
			location -= get_type_size(argument_types[i]);
		}
		return location;
	}
	std::uint32_t visit_function_call(const FunctionCall& call) override {
		const std::uint32_t input_size = get_input_size(call.get_function());
		const std::uint32_t output_size = get_output_size(call.get_function());
		for (const Expression* argument: call.get_arguments()) {
			const std::uint32_t argument_size = get_type_size(argument->get_type());
			const std::uint32_t argument_destination = allocate(argument_size);
			memcopy(argument_destination, expression_table[argument], argument_size);
		}
		if (output_size > input_size) {
			variable -= output_size - input_size;
		}
		assembler.LEA(ESP, PTR(EBP, variable));
		Jump jump = assembler.CALL();
		deferred_calls.emplace_back(jump, call.get_function());
		if (output_size < input_size) {
			variable += input_size - output_size;
		}
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
		assembler.comment("return");
		const std::uint32_t size = get_type_size(return_.get_expression()->get_type());
		memcopy(result, expression_table[return_.get_expression()], size);
		return allocate(0);
	}
	static void codegen(const Program& program, const char* source_path, const TailCallData& tail_call_data) {
		std::vector<DeferredCall> deferred_calls;
		std::map<const Function*, std::size_t> function_locations;
		A assembler;
		assembler.write_headers();
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
			const std::uint32_t output_size = get_output_size(function);
			const std::uint32_t size = std::max(get_input_size(function), output_size);
			CodegenX86::evaluate(deferred_calls, function, assembler, 8 + size - output_size, function->get_block());
			assembler.comment("--");
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
