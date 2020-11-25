#pragma once

#include "ast.hpp"
#include <sstream>

class CodegenC: public Visitor<Variable> {
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
		std::map<const Function*, std::size_t> functions;
		std::map<const Type*, std::size_t> types;
		IndentPrinter& declarations;
	public:
		FunctionTable(IndentPrinter& declarations): declarations(declarations) {}
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
					declarations.println_indented(format("typedef void t%;", print_number(index)));
					types[type] = index;
					return index;
				}
				case NumberType::id: {
					const std::size_t index = types.size();
					declarations.println_indented(format("typedef int32_t t%;", print_number(index)));
					types[type] = index;
					return index;
				}
				case TupleType::id: {
					std::vector<std::size_t> element_types;
					for (const Type* element_type: static_cast<const TupleType*>(type)->get_types()) {
						element_types.push_back(get_type(element_type));
					}
					const std::size_t index = types.size();
					declarations.println_indented("typedef struct {");
					declarations.increase_indentation();
					for (std::size_t i = 0; i < element_types.size(); ++i) {
						declarations.println_indented(format("t% v%;", print_number(element_types[i]), print_number(i)));
					}
					declarations.decrease_indentation();
					declarations.println_indented(format("} t%;", print_number(index)));
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
	IndentPrinter& printer;
	std::size_t variable = 1;
	std::map<const Expression*, Variable> cache;
	Variable next_variable() {
		return Variable(variable++);
	}
	CodegenC(FunctionTable& function_table, IndentPrinter& printer): function_table(function_table), printer(printer) {}
	Variable evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
		return cache[block.get_result()];
	}
public:
	Variable visit_number(const Number& number) override {
		const Variable result = next_variable();
		const std::size_t type = function_table.get_type(number.get_type());
		printer.println_indented(format("t% % = %;", print_number(type), result, print_number(number.get_value())));
		return result;
	}
	Variable visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Variable left = cache[binary_expression.get_left()];
		const Variable right = cache[binary_expression.get_right()];
		const Variable result = next_variable();
		const std::size_t type = function_table.get_type(binary_expression.get_type());
		printer.println_indented(format("t% % = % % %;", print_number(type), result, left, print_operator(binary_expression.get_operation()), right));
		return result;
	}
	Variable visit_if(const If& if_) override {
		const Variable condition = cache[if_.get_condition()];
		const Variable result = next_variable();
		if (if_.get_type()->get_id() == VoidType::id) {
			printer.println_indented(format("if (%) {", condition));
			printer.increase_indentation();
			evaluate(if_.get_then_block());
			printer.decrease_indentation();
			printer.println_indented("} else {");
			printer.increase_indentation();
			evaluate(if_.get_else_block());
			printer.decrease_indentation();
			printer.println_indented("}");
		}
		else {
			const std::size_t result_type = function_table.get_type(if_.get_type());
			printer.println_indented(format("t% %;", print_number(result_type), result));
			printer.println_indented(format("if (%) {", condition));
			printer.increase_indentation();
			const Variable then_result = evaluate(if_.get_then_block());
			printer.println_indented(format("% = %;", result, then_result));
			printer.decrease_indentation();
			printer.println_indented("} else {");
			printer.increase_indentation();
			const Variable else_result = evaluate(if_.get_else_block());
			printer.println_indented(format("% = %;", result, else_result));
			printer.decrease_indentation();
			printer.println_indented("}");
		}
		return result;
	}
	Variable visit_tuple(const Tuple& tuple) override {
		const Variable result = next_variable();
		printer.println_indented(print_functor([&](auto& printer) {
			const std::size_t type = function_table.get_type(tuple.get_type());
			printer.print(format("t% % = {", print_number(type), result));
			for (std::size_t i = 0; i < tuple.get_expressions().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(cache[tuple.get_expressions()[i]]);
			}
			printer.print("};");
		}));
		return result;
	}
	Variable visit_tuple_access(const TupleAccess& tuple_access) override {
		const Variable tuple = cache[tuple_access.get_tuple()];
		const Variable result = next_variable();
		const std::size_t result_type = function_table.get_type(tuple_access.get_type());
		printer.println_indented(format("t% % = %.v%;", print_number(result_type), result, tuple, print_number(tuple_access.get_index())));
		return result;
	}
	Variable visit_struct(const Struct& struct_) override {
		return next_variable();
	}
	Variable visit_struct_access(const StructAccess& struct_access) override {
		return next_variable();
	}
	Variable visit_closure(const Closure& closure) override {
		return next_variable();
	}
	Variable visit_closure_access(const ClosureAccess& closure_access) override {
		return next_variable();
	}
	Variable visit_argument(const Argument& argument) override {
		return Variable(argument.get_index());
	}
	Variable visit_call(const Call& call) override {
		const std::size_t new_index = function_table.look_up(call.get_function());
		const Variable result = next_variable();
		printer.println_indented(print_functor([&](auto& printer) {
			if (call.get_function()->get_return_type()->get_id() != VoidType::id) {
				const std::size_t result_type = function_table.get_type(call.get_function()->get_return_type());
				printer.print(format("t% % = ", print_number(result_type), result));
			}
			printer.print(format("f%(", print_number(new_index)));
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(cache[call.get_arguments()[i]]);
			}
			printer.print(");");
		}));
		return result;
	}
	Variable visit_intrinsic(const Intrinsic& intrinsic) override {
		const Variable result = next_variable();
		if (intrinsic.name_equals("putChar")) {
			const Variable argument = cache[intrinsic.get_arguments()[0]];
			printer.println_indented(format("putchar(%);", argument));
		}
		else if (intrinsic.name_equals("getChar")) {
			const std::size_t type = function_table.get_type(intrinsic.get_type());
			printer.println_indented(format("t% % = getchar();", print_number(type), result));
		}
		return result;
	}
	Variable visit_bind(const Bind& bind) override {
		return next_variable();
	}
	static void codegen(const Program& program, const char* path) {
		std::ostringstream type_declarations;
		std::ostringstream function_declarations;
		std::ostringstream functions;
		IndentPrinter type_declaration_printer(type_declarations);
		IndentPrinter function_declaration_printer(function_declarations);
		IndentPrinter printer(functions);
		FunctionTable function_table(type_declaration_printer);
		type_declaration_printer.println_indented("#include <stdio.h>");
		type_declaration_printer.println_indented("#include <stdint.h>");
		{
			printer.println_indented("int main(int argc, char** argv) {");
			printer.increase_indentation();
			const std::size_t index = function_table.look_up(program.get_main_function());
			printer.println_indented(format("f%();", print_number(index)));
			printer.println_indented("return 0;");
			printer.decrease_indentation();
			printer.println_indented("}");
		}
		for (const Function* function: program) {
			const std::size_t return_type = function_table.get_type(function->get_return_type());
			const std::size_t index = function_table.look_up(function);
			const std::size_t arguments = function->get_argument_types().size();
			function_declaration_printer.println_indented(print_functor([&](auto& printer) {
				printer.print(format("t% f%(", print_number(return_type), print_number(index)));
				for (std::size_t i = 0; i < arguments; ++i) {
					if (i > 0) printer.print(", ");
					const std::size_t argument_type = function_table.get_type(function->get_argument_types()[i]);
					printer.print(format("t% v%", print_number(argument_type), print_number(i)));
				}
				printer.print(");");
			}));
			printer.println_indented(print_functor([&](auto& printer) {
				printer.print(format("t% f%(", print_number(return_type), print_number(index)));
				for (std::size_t i = 0; i < arguments; ++i) {
					if (i > 0) printer.print(", ");
					const std::size_t argument_type = function_table.get_type(function->get_argument_types()[i]);
					printer.print(format("t% v%", print_number(argument_type), print_number(i)));
				}
				printer.print(") {");
			}));
			printer.increase_indentation();
			CodegenC codegen(function_table, printer);
			codegen.variable = arguments;
			const Variable result = codegen.evaluate(function->get_block());
			if (function->get_return_type()->get_id() != VoidType::id) {
				printer.println_indented(format("return %;", result));
			}
			printer.decrease_indentation();
			printer.println_indented("}");
		}
		std::cout << type_declarations.str();
		std::cout << function_declarations.str();
		std::cout << functions.str();
	}
};
