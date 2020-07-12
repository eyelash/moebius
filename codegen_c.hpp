#pragma once

#include "ast.hpp"

class CodegenC: public Visitor<std::size_t> {
	static StringView print_operator(BinaryOperation operation) {
		switch (operation) {
			case BinaryOperation::ADD: return "+";
			case BinaryOperation::SUB: return "-";
			case BinaryOperation::MUL: return "*";
			case BinaryOperation::DIV: return "/";
			case BinaryOperation::REM: return "%";
			case BinaryOperation::EQ: return "==";
			case BinaryOperation::NE: return "!=";
			case BinaryOperation::LT: return "<";
			case BinaryOperation::LE: return "<=";
			case BinaryOperation::GT: return ">";
			case BinaryOperation::GE: return ">=";
			default: return StringView();
		}
	}
	class FunctionTable {
		std::map<const Type*, std::size_t> types;
		std::map<const Function*, std::size_t> functions;
	public:
		MemoryPrinter& declarations;
		FunctionTable(MemoryPrinter& declarations): declarations(declarations) {}
		std::size_t look_up(const Function* function) {
			auto iterator = functions.find(function);
			if (iterator != functions.end()) {
				return iterator->second;
			}
			const std::size_t index = functions.size();
			functions[function] = index;
			return index;
		}
		std::size_t get_type(const Type* type) {
			auto iterator = types.find(type);
			if (iterator != types.end()) {
				return iterator->second;
			}
			switch (type->get_id()) {
				case VoidType::id: {
					const std::size_t index = types.size();
					declarations.println(format("typedef void t%;", print_number(index)));
					types[type] = index;
					return index;
				}
				case NumberType::id: {
					const std::size_t index = types.size();
					declarations.println(format("typedef int32_t t%;", print_number(index)));
					types[type] = index;
					return index;
				}
				case ClosureType::id: {
					std::vector<std::size_t> environment_types;
					for (const Type* environment_type: static_cast<const ClosureType*>(type)->get_environment_types()) {
						environment_types.push_back(get_type(environment_type));
					}
					const std::size_t index = types.size();
					declarations.println("typedef struct {");
					for (std::size_t i = 0; i < environment_types.size(); ++i) {
						declarations.println(format("  t% v%;", print_number(environment_types[i]), print_number(i)));
					}
					declarations.println(format("} t%;", print_number(index)));
					types[type] = index;
					return index;
				}
				default: {
					return 0;
				}
			}
		}
	};
	FunctionTable& function_table;
	const std::size_t index;
	MemoryPrinter& printer;
	std::size_t variable = 1;
	std::map<const Expression*, std::size_t> cache;
	std::size_t indentation = 1;
	template <class T> Indent<T> indent(const T& t) {
		return Indent<T>(t, indentation);
	}
	CodegenC(FunctionTable& function_table, std::size_t index, MemoryPrinter& printer): function_table(function_table), index(index), printer(printer) {}
	std::size_t evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
		return cache[block.get_result()];
	}
public:
	std::size_t visit_number(const Number& number) override {
		const std::size_t result = variable++;
		printer.println(indent(format("int32_t v% = %;", print_number(result), print_number(number.get_value()))));
		return result;
	}
	std::size_t visit_binary_expression(const BinaryExpression& binary_expression) override {
		const std::size_t left = cache[binary_expression.get_left()];
		const std::size_t right = cache[binary_expression.get_right()];
		const std::size_t result = variable++;
		printer.println(indent(format("int32_t v% = v% % v%;", print_number(result), print_number(left), print_operator(binary_expression.get_operation()), print_number(right))));
		return result;
	}
	std::size_t visit_if(const If& if_) override {
		const std::size_t condition = cache[if_.get_condition()];
		const std::size_t result = variable++;
		if (if_.get_type()->get_id() == VoidType::id) {
			printer.println(indent(format("if (v%) {", print_number(condition))));
			++indentation;
			evaluate(if_.get_then_block());
			--indentation;
			printer.println(indent("} else {"));
			++indentation;
			evaluate(if_.get_else_block());
			--indentation;
			printer.println(indent("}"));
		}
		else {
			const std::size_t result_type = function_table.get_type(if_.get_type());
			printer.println(indent(format("t% v%;", print_number(result_type), print_number(result))));
			printer.println(indent(format("if (v%) {", print_number(condition))));
			++indentation;
			const std::size_t then_result = evaluate(if_.get_then_block());
			printer.println(indent(format("v% = v%;", print_number(result), print_number(then_result))));
			--indentation;
			printer.println(indent("} else {"));
			++indentation;
			const std::size_t else_result = evaluate(if_.get_else_block());
			printer.println(indent(format("v% = v%;", print_number(result), print_number(else_result))));
			--indentation;
			printer.println(indent("}"));
		}
		return result;
	}
	std::size_t visit_closure(const Closure& closure) override {
		std::vector<std::size_t> elements;
		for (const Expression* element: closure.get_environment_expressions()) {
			elements.push_back(cache[element]);
		}
		const std::size_t result = variable++;
		const std::size_t type = function_table.get_type(closure.get_type());
		printer.print(indent(format("t% v% = {", print_number(type), print_number(result))));
		for (const std::size_t element: elements) {
			printer.print(format("v%,", print_number(element)));
		}
		printer.println("};");
		return result;
	}
	std::size_t visit_closure_access(const ClosureAccess& closure_access) override {
		const std::size_t closure = cache[closure_access.get_closure()];
		const std::size_t result = variable++;
		const std::size_t result_type = function_table.get_type(closure_access.get_type());
		printer.println(indent(format("t% v% = v%.v%;", print_number(result_type), print_number(result), print_number(closure), print_number(closure_access.get_index()))));
		return result;
	}
	std::size_t visit_argument(const Argument& argument) override {
		return argument.get_index();
	}
	std::size_t visit_call(const Call& call) override {
		std::vector<std::size_t> arguments;
		for (const Expression* argument: call.get_arguments()) {
			arguments.push_back(cache[argument]);
		}
		const std::size_t new_index = function_table.look_up(call.get_function());
		const std::size_t result = variable++;
		if (call.get_function()->get_return_type()->get_id() == VoidType::id) {
			printer.print(indent(format("f%(", print_number(new_index))));
		}
		else {
			const std::size_t result_type = function_table.get_type(call.get_function()->get_return_type());
			printer.print(indent(format("t% v% = f%(", print_number(result_type), print_number(result), print_number(new_index))));
		}
		for (std::size_t i = 0; i < arguments.size(); ++i) {
			if (i > 0) printer.print(", ");
			printer.print(format("v%", print_number(arguments[i])));
		}
		printer.println(");");
		return result;
	}
	std::size_t visit_intrinsic(const Intrinsic& intrinsic) override {
		const std::size_t result = variable++;
		if (intrinsic.name_equals("putChar")) {
			const std::size_t argument = cache[intrinsic.get_arguments()[0]];
			printer.println(indent(format("putchar(v%);", print_number(argument))));
		}
		else if (intrinsic.name_equals("getChar")) {
			printer.println(indent(format("uint32_t v% = getchar();", print_number(result))));
		}
		return result;
	}
	std::size_t visit_bind(const Bind& bind) override {
		return variable++;
	}
	static void codegen(const Program& program, const char* path) {
		MemoryPrinter type_declarations;
		MemoryPrinter function_declarations;
		MemoryPrinter printer;
		FunctionTable function_table(type_declarations);
		type_declarations.println("#include <stdio.h>");
		type_declarations.println("#include <stdint.h>");
		{
			printer.println("int main(int argc, char** argv) {");
			const std::size_t index = function_table.look_up(program.get_main_function());
			printer.println(format("  f%();", print_number(index)));
			printer.println("  return 0;");
			printer.println("}");
		}
		for (const Function* function: program) {
			const std::size_t return_type = function_table.get_type(function->get_return_type());
			const std::size_t index = function_table.look_up(function);
			const std::size_t arguments = function->get_argument_types().size();
			function_declarations.print(format("t% f%(", print_number(return_type), print_number(index)));
			for (std::size_t i = 0; i < arguments; ++i) {
				if (i > 0) function_declarations.print(", ");
				const std::size_t argument_type = function_table.get_type(function->get_argument_types()[i]);
				function_declarations.print(format("t% v%", print_number(argument_type), print_number(i)));
			}
			function_declarations.println(");");
			printer.print(format("t% f%(", print_number(return_type), print_number(index)));
			for (std::size_t i = 0; i < arguments; ++i) {
				if (i > 0) printer.print(", ");
				const std::size_t argument_type = function_table.get_type(function->get_argument_types()[i]);
				printer.print(format("t% v%", print_number(argument_type), print_number(i)));
			}
			printer.println(") {");
			CodegenC codegen(function_table, index, printer);
			codegen.variable = arguments;
			const std::size_t result = codegen.evaluate(function->get_block());
			if (function->get_return_type()->get_id() != VoidType::id) {
				printer.println(format("  return v%;", print_number(result)));
			}
			printer.println("}");
		}
		type_declarations.print_to_file(stdout);
		function_declarations.print_to_file(stdout);
		printer.print_to_file(stdout);
	}
};
