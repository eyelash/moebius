#pragma once

#include "ast.hpp"

class CodegenJS: public Visitor<Variable> {
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
	public:
		std::size_t look_up(const Function* function) {
			auto iterator = functions.find(function);
			if (iterator != functions.end()) {
				return iterator->second;
			}
			const std::size_t index = functions.size();
			functions[function] = index;
			return index;
		}
	};
	FunctionTable& function_table;
	IndentPrinter& printer;
	using ExpressionTable = std::map<const Expression*, Variable>;
	ExpressionTable& expression_table;
	std::size_t variable;
	Variable result;
	const TailCallData& tail_call_data;
	Variable next_variable() {
		return Variable(variable++);
	}
	CodegenJS(FunctionTable& function_table, IndentPrinter& printer, ExpressionTable& expression_table, std::size_t variable, Variable result, const TailCallData& tail_call_data): function_table(function_table), printer(printer), expression_table(expression_table), variable(variable), result(result), tail_call_data(tail_call_data) {}
	static void evaluate(FunctionTable& function_table, IndentPrinter& printer, ExpressionTable& expression_table, std::size_t variable, Variable result, const TailCallData& tail_call_data, const Block& block) {
		CodegenJS codegen(function_table, printer, expression_table, variable, result, tail_call_data);
		for (const Expression* expression: block) {
			expression_table[expression] = visit(codegen, expression);
		}
	}
	static void evaluate(FunctionTable& function_table, IndentPrinter& printer, std::size_t variable, Variable result, const TailCallData& tail_call_data, const Block& block) {
		ExpressionTable expression_table;
		evaluate(function_table, printer, expression_table, variable, result, tail_call_data, block);
	}
	void evaluate(Variable result, const Block& block) {
		evaluate(function_table, printer, expression_table, variable, result, tail_call_data, block);
	}
public:
	Variable visit_int_literal(const IntLiteral& int_literal) override {
		const Variable result = next_variable();
		printer.println(format("const % = %;", result, print_number(int_literal.get_value())));
		return result;
	}
	Variable visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Variable left = expression_table[binary_expression.get_left()];
		const Variable right = expression_table[binary_expression.get_right()];
		const Variable result = next_variable();
		printer.println(format("const % = (% % %) | 0;", result, left, print_operator(binary_expression.get_operation()), right));
		return result;
	}
	Variable visit_array_literal(const ArrayLiteral& array_literal) override {
		const Variable result = next_variable();
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("const % = [", result));
			for (std::size_t i = 0; i < array_literal.get_elements().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(expression_table[array_literal.get_elements()[i]]);
			}
			printer.print("];");
		}));
		return result;
	}
	Variable visit_string_literal(const StringLiteral& string_literal) override {
		const Variable result = next_variable();
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("const % = [", result));
			for (std::size_t i = 0; i < string_literal.get_value().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(print_number(string_literal.get_value()[i]));
			}
			printer.print("];");
		}));
		return result;
	}
	Variable visit_if(const If& if_) override {
		const Variable condition = expression_table[if_.get_condition()];
		const Variable result = next_variable();
		if (if_.get_type() != TypeInterner::get_void_type()) {
			printer.println(format("let %;", result));
		}
		printer.println_increasing(format("if (%) {", condition));
		evaluate(result, if_.get_then_block());
		printer.println_decreasing("}");
		printer.println_increasing("else {");
		evaluate(result, if_.get_else_block());
		printer.println_decreasing("}");
		return result;
	}
	Variable visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		const Variable result = next_variable();
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("const % = [", result));
			for (std::size_t i = 0; i < tuple_literal.get_elements().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(expression_table[tuple_literal.get_elements()[i]]);
			}
			printer.print("];");
		}));
		return result;
	}
	Variable visit_tuple_access(const TupleAccess& tuple_access) override {
		const Variable tuple = expression_table[tuple_access.get_tuple()];
		const Variable result = next_variable();
		printer.println(format("const % = %[%];", result, tuple, print_number(tuple_access.get_index())));
		return result;
	}
	Variable visit_argument(const Argument& argument) override {
		return Variable(argument.get_index());
	}
	Variable visit_call(const Call& call) override {
		const std::size_t new_index = function_table.look_up(call.get_function());
		const Variable result = next_variable();
		if (tail_call_data.is_tail_call(&call)) {
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				const Variable argument = expression_table[call.get_arguments()[i]];
				printer.println(format("% = %;", Variable(i), argument));
			}
			printer.println("continue;");
		}
		else printer.println(print_functor([&](auto& printer) {
			if (call.get_type() != TypeInterner::get_void_type()) {
				printer.print(format("const % = ", result));
			}
			printer.print(format("f%(", print_number(new_index)));
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(expression_table[call.get_arguments()[i]]);
			}
			printer.print(");");
		}));
		return result;
	}
	Variable visit_intrinsic(const Intrinsic& intrinsic) override {
		const Variable result = next_variable();
		if (intrinsic.name_equals("putChar")) {
			const Variable argument = expression_table[intrinsic.get_arguments()[0]];
			printer.println(format("putChar(%);", argument));
			printer.println(format("const % = null;", result));
		}
		else if (intrinsic.name_equals("putStr")) {
			const Variable argument = expression_table[intrinsic.get_arguments()[0]];
			printer.println(format("putStr(%);", argument));
			printer.println(format("const % = null;", result));
		}
		else if (intrinsic.name_equals("getChar")) {
			// TODO
		}
		else if (intrinsic.name_equals("arrayGet")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			printer.println(format("const % = %[%];", result, array, index));
		}
		else if (intrinsic.name_equals("arrayLength")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			printer.println(format("const % = %.length;", result, array));
		}
		else if (intrinsic.name_equals("arraySplice")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Variable remove = expression_table[intrinsic.get_arguments()[2]];
			printer.println(format("const % = %.slice();", result, array));
			if (intrinsic.get_arguments().size() == 4 && intrinsic.get_arguments()[3]->get_type() == intrinsic.get_type()) {
				const Variable insert = expression_table[intrinsic.get_arguments()[3]];
				printer.println(format("%.splice(%, %, ...%);", result, index, remove, insert));
			}
			else {
				const std::size_t insert = intrinsic.get_arguments().size() - 3;
				printer.println(print_functor([&](auto& printer) {
					printer.print(format("%.splice(%, %", result, index, remove));
					for (std::size_t i = 0; i < insert; ++i) {
						printer.print(", ");
						printer.print(expression_table[intrinsic.get_arguments()[i + 3]]);
					}
					printer.print(");");
				}));
			}
		}
		else if (intrinsic.name_equals("copy")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			printer.println(format("const % = %;", result, array));
		}
		return result;
	}
	Variable visit_bind(const Bind& bind) override {
		const Variable result = next_variable();
		printer.println(format("const % = null;", result));
		return result;
	}
	Variable visit_return(const Return& return_) override {
		const Expression* expression = return_.get_expression();
		if (expression->get_type() != TypeInterner::get_void_type() && !tail_call_data.is_tail_call(expression)) {
			printer.println(format("% = %;", result, expression_table[expression]));
		}
		return next_variable();
	}
	static void codegen(const Program& program, const char* source_path, const TailCallData& tail_call_data) {
		FunctionTable function_table;
		std::string path = std::string(source_path) + ".html";
		std::ofstream file(path);
		IndentPrinter printer(file);
		printer.println("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><script>");
		printer.println("window.addEventListener('load', main);");
		{
			printer.println_increasing("function main() {");
			const std::size_t index = function_table.look_up(program.get_main_function());
			printer.println(format("f%();", print_number(index)));
			printer.println("flushStdout();");
			printer.println_decreasing("}");
		}
		for (const Function* function: program) {
			const std::size_t index = function_table.look_up(function);
			const std::size_t arguments = function->get_argument_types().size();
			printer.println_increasing(print_functor([&](auto& printer) {
				printer.print(format("function f%(", print_number(index)));
				for (std::size_t i = 0; i < arguments; ++i) {
					if (i > 0) printer.print(", ");
					printer.print(format("v%", print_number(i)));
				}
				printer.print(") {");
			}));
			if (tail_call_data.has_tail_call(function)) {
				printer.println_increasing("while (true) {");
			}
			const Variable result = Variable(arguments);
			if (function->get_return_type()->get_id() != TypeId::VOID) {
				printer.println(format("let %;", result));
			}
			CodegenJS::evaluate(function_table, printer, arguments + 1, result, tail_call_data, function->get_block());
			if (function->get_return_type()->get_id() != TypeId::VOID) {
				printer.println(format("return %;", result));
			}
			else {
				printer.println("return;");
			}
			if (tail_call_data.has_tail_call(function)) {
				printer.println_decreasing("}");
			}
			printer.println_decreasing("}");
		}
		printer.println("const stdoutBuffer = [];");
		{
			printer.println_increasing("function flushStdout() {");
			printer.println("const textDecoder = new TextDecoder();");
			printer.println("const s = textDecoder.decode(new Int8Array(stdoutBuffer));");
			printer.println("document.body.appendChild(document.createTextNode(s));");
			printer.println("stdoutBuffer.splice(0);");
			printer.println_decreasing("}");
		}
		{
			printer.println_increasing("function putChar(c) {");
			printer.println_increasing("if (c === 0x0A) {");
			printer.println("flushStdout();");
			printer.println("document.body.appendChild(document.createElement('br'));");
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			printer.println("stdoutBuffer.push(c);");
			printer.println_decreasing("}");
			printer.println_decreasing("}");
		}
		{
			printer.println_increasing("function putStr(s) {");
			printer.println("s.forEach(putChar);");
			printer.println_decreasing("}");
		}
		printer.println("</script></head><body></body></html>");
		Printer status_printer(std::cerr);
		status_printer.print(bold(path));
		status_printer.print(bold(green(" successfully generated")));
		status_printer.print('\n');
	}
};
