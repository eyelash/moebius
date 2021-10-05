#pragma once

#include "ast.hpp"
#include <sstream>

class CodegenC: public Visitor<Variable> {
	class Type {
		std::size_t index;
	public:
		constexpr Type(std::size_t index): index(index) {}
		Type() {}
		void print(const Printer& printer) const {
			printer.print(format("t%", print_number(index)));
		}
	};
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
		std::map<const ::Type*, std::size_t> types;
		IndentPrinter& type_declaration_printer;
	public:
		FunctionTable(IndentPrinter& type_declaration_printer): type_declaration_printer(type_declaration_printer) {}
		std::size_t look_up(const Function* function) {
			auto iterator = functions.find(function);
			if (iterator != functions.end()) {
				return iterator->second;
			}
			const std::size_t index = functions.size();
			functions[function] = index;
			return index;
		}
		Type get_type(const ::Type* type) {
			auto iterator = types.find(type);
			if (iterator != types.end()) {
				return iterator->second;
			}
			switch (type->get_id()) {
				case TypeId::VOID: {
					const std::size_t index = types.size();
					type_declaration_printer.println(format("typedef void %;", Type(index)));
					types[type] = index;
					return index;
				}
				case TypeId::INT: {
					const std::size_t index = types.size();
					type_declaration_printer.println(format("typedef int32_t %;", Type(index)));
					types[type] = index;
					return index;
				}
				case TypeId::TUPLE: {
					std::vector<Type> element_types;
					for (const ::Type* element_type: static_cast<const TupleType*>(type)->get_types()) {
						element_types.push_back(get_type(element_type));
					}
					const std::size_t index = types.size();
					type_declaration_printer.println_increasing("typedef struct {");
					for (std::size_t i = 0; i < element_types.size(); ++i) {
						type_declaration_printer.println(format("% v%;", element_types[i], print_number(i)));
					}
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type] = index;
					return index;
				}
				case TypeId::ARRAY: {
					const Type element_type = get_type(TypeInterner::get_int_type());
					const Type number_type = get_type(TypeInterner::get_int_type());
					const std::size_t index = types.size();
					type_declaration_printer.println_increasing("typedef struct {");
					type_declaration_printer.println(format("%* elements;", element_type));
					type_declaration_printer.println(format("% length;", number_type));
					type_declaration_printer.println(format("% capacity;", number_type));
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type] = index;
					return index;
				}
				case TypeId::STRING: {
					const Type number_type = get_type(TypeInterner::get_int_type());
					const std::size_t index = types.size();
					type_declaration_printer.println_increasing("typedef struct {");
					type_declaration_printer.println("char* elements;");
					type_declaration_printer.println(format("% length;", number_type));
					type_declaration_printer.println(format("% capacity;", number_type));
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
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
	std::map<const Expression*, Variable> expression_table;
	const TailCallData& tail_call_data;
	Variable next_variable() {
		return Variable(variable++);
	}
	CodegenC(FunctionTable& function_table, IndentPrinter& printer, const TailCallData& tail_call_data): function_table(function_table), printer(printer), tail_call_data(tail_call_data) {}
	Variable evaluate(const Block& block) {
		for (const Expression* expression: block) {
			expression_table[expression] = visit(*this, expression);
		}
		return expression_table[block.get_result()];
	}
	static const char* getenv(const char* variable, const char* default_value) {
		const char* result = std::getenv(variable);
		return result ? result : default_value;
	}
public:
	Variable visit_int_literal(const IntLiteral& int_literal) override {
		const Variable result = next_variable();
		const Type type = function_table.get_type(int_literal.get_type());
		printer.println(format("% % = %;", type, result, print_number(int_literal.get_value())));
		return result;
	}
	Variable visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Variable left = expression_table[binary_expression.get_left()];
		const Variable right = expression_table[binary_expression.get_right()];
		const Variable result = next_variable();
		const Type type = function_table.get_type(binary_expression.get_type());
		printer.println(format("% % = % % %;", type, result, left, print_operator(binary_expression.get_operation()), right));
		return result;
	}
	Variable visit_string_literal(const StringLiteral& string_literal) override {
		const Variable result = next_variable();
		const Type type = function_table.get_type(string_literal.get_type());
		const std::size_t size = string_literal.get_value().size();
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("% % = string_new((char[]){", type, result));
			for (std::size_t i = 0; i < size; ++i) {
				if (i > 0) printer.print(", ");
				printer.print(print_number(string_literal.get_value()[i]));
			}
			printer.print(format("}, %);", print_number(size)));
		}));
		return result;
	}
	Variable visit_if(const If& if_) override {
		const Variable condition = expression_table[if_.get_condition()];
		const Variable result = next_variable();
		if (if_.get_type_id() == TypeId::VOID) {
			printer.println_increasing(format("if (%) {", condition));
			evaluate(if_.get_then_block());
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			evaluate(if_.get_else_block());
			printer.println_decreasing("}");
		}
		else {
			const Type result_type = function_table.get_type(if_.get_type());
			printer.println(format("% %;", result_type, result));
			printer.println_increasing(format("if (%) {", condition));
			const Variable then_result = evaluate(if_.get_then_block());
			printer.println(format("% = %;", result, then_result));
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			const Variable else_result = evaluate(if_.get_else_block());
			printer.println(format("% = %;", result, else_result));
			printer.println_decreasing("}");
		}
		return result;
	}
	Variable visit_tuple(const Tuple& tuple) override {
		const Variable result = next_variable();
		printer.println(print_functor([&](auto& printer) {
			const Type type = function_table.get_type(tuple.get_type());
			printer.print(format("% % = {", type, result));
			for (std::size_t i = 0; i < tuple.get_expressions().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(expression_table[tuple.get_expressions()[i]]);
			}
			printer.print("};");
		}));
		return result;
	}
	Variable visit_tuple_access(const TupleAccess& tuple_access) override {
		const Variable tuple = expression_table[tuple_access.get_tuple()];
		const Variable result = next_variable();
		const Type result_type = function_table.get_type(tuple_access.get_type());
		printer.println(format("% % = %.v%;", result_type, result, tuple, print_number(tuple_access.get_index())));
		return result;
	}
	Variable visit_argument(const Argument& argument) override {
		return Variable(argument.get_index());
	}
	Variable visit_call(const Call& call) override {
		const std::size_t new_index = function_table.look_up(call.get_function());
		const Variable result = next_variable();
		if (tail_call_data.is_tail_call(call)) {
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				const Variable argument = expression_table[call.get_arguments()[i]];
				printer.println(format("% = %;", Variable(i), argument));
			}
			printer.println("continue;");
			if (call.get_function()->get_return_type()->get_id() != TypeId::VOID) {
				const Type result_type = function_table.get_type(call.get_function()->get_return_type());
				printer.println(format("% %;", result_type, result));
			}
		}
		else printer.println(print_functor([&](auto& printer) {
			if (call.get_function()->get_return_type()->get_id() != TypeId::VOID) {
				const Type result_type = function_table.get_type(call.get_function()->get_return_type());
				printer.print(format("% % = ", result_type, result));
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
			printer.println(format("fputc(%, stdout);", argument));
		}
		else if (intrinsic.name_equals("putStr")) {
			const Variable argument = expression_table[intrinsic.get_arguments()[0]];
			printer.println(format("fputs(%.elements, stdout);", argument));
		}
		else if (intrinsic.name_equals("getChar")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = getchar();", type, result));
		}
		else if (intrinsic.name_equals("arrayNew")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			const Type element_type = function_table.get_type(TypeInterner::get_int_type());
			const std::size_t size = intrinsic.get_arguments().size();
			printer.println(print_functor([&](auto& printer) {
				printer.print(format("% % = array_new((%[]){", type, result, element_type));
				for (std::size_t i = 0; i < size; ++i) {
					if (i > 0) printer.print(", ");
					printer.print(expression_table[intrinsic.get_arguments()[i]]);
				}
				printer.print(format("}, %);", print_number(size)));
			}));
		}
		else if (intrinsic.name_equals("arrayGet") || intrinsic.name_equals("stringGet")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = %.elements[%];", type, result, array, index));
		}
		else if (intrinsic.name_equals("arrayLength") || intrinsic.name_equals("stringLength")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = %.length;", type, result, array));
		}
		else if (intrinsic.name_equals("arraySplice")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			const Type element_type = function_table.get_type(TypeInterner::get_int_type());
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Variable remove = expression_table[intrinsic.get_arguments()[2]];
			if (intrinsic.get_arguments().size() == 4 && intrinsic.get_arguments()[3]->get_type_id() == TypeId::ARRAY) {
				const Variable insert = expression_table[intrinsic.get_arguments()[3]];
				printer.println(format("% % = array_splice(%, %, %, %.elements, %.length);", type, result, array, index, remove, insert, insert));
			}
			else {
				const std::size_t insert = intrinsic.get_arguments().size() - 3;
				printer.println(print_functor([&](auto& printer) {
					printer.print(format("% % = array_splice(%, %, %, (%[]){", type, result, array, index, remove, element_type));
					for (std::size_t i = 0; i < insert; ++i) {
						if (i > 0) printer.print(", ");
						printer.print(expression_table[intrinsic.get_arguments()[i + 3]]);
					}
					printer.print(format("}, %);", print_number(insert)));
				}));
			}
		}
		else if (intrinsic.name_equals("stringSplice")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			const Variable string = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Variable remove = expression_table[intrinsic.get_arguments()[2]];
			if (intrinsic.get_arguments().size() == 4 && intrinsic.get_arguments()[3]->get_type_id() == TypeId::STRING) {
				const Variable insert = expression_table[intrinsic.get_arguments()[3]];
				printer.println(format("% % = string_splice(%, %, %, %.elements, %.length);", type, result, string, index, remove, insert, insert));
			}
			else {
				const std::size_t insert = intrinsic.get_arguments().size() - 3;
				printer.println(print_functor([&](auto& printer) {
					printer.print(format("% % = string_splice(%, %, %, (char[]){", type, result, string, index, remove));
					for (std::size_t i = 0; i < insert; ++i) {
						if (i > 0) printer.print(", ");
						printer.print(expression_table[intrinsic.get_arguments()[i + 3]]);
					}
					printer.print(format("}, %);", print_number(insert)));
				}));
			}
		}
		else if (intrinsic.name_equals("copy")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			if (intrinsic.get_type_id() == TypeId::STRING) {
				printer.println(format("% % = string_new(%.elements, %.length);", type, result, array, array));
			}
			else {
				printer.println(format("% % = array_new(%.elements, %.length);", type, result, array, array));
			}
		}
		else if (intrinsic.name_equals("free")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			printer.println(format("free(%.elements);", array));
		}
		return result;
	}
	Variable visit_bind(const Bind& bind) override {
		return next_variable();
	}
	Variable visit_return(const Return& return_) override {
		return next_variable();
	}
	static void codegen(const Program& program, const char* source_path, const TailCallData& tail_call_data) {
		std::ostringstream type_declarations;
		std::ostringstream function_declarations;
		std::ostringstream functions;
		IndentPrinter type_declaration_printer(type_declarations);
		IndentPrinter function_declaration_printer(function_declarations);
		IndentPrinter printer(functions);
		FunctionTable function_table(type_declaration_printer);
		type_declaration_printer.println("#include <stdlib.h>");
		type_declaration_printer.println("#include <stdint.h>");
		type_declaration_printer.println("#include <stdio.h>");
		{
			printer.println_increasing("int main(int argc, char** argv) {");
			const std::size_t index = function_table.look_up(program.get_main_function());
			printer.println(format("f%();", print_number(index)));
			printer.println("return 0;");
			printer.println_decreasing("}");
		}
		for (const Function* function: program) {
			const Type return_type = function_table.get_type(function->get_return_type());
			const std::size_t index = function_table.look_up(function);
			const std::size_t arguments = function->get_argument_types().size();
			function_declaration_printer.println(print_functor([&](auto& printer) {
				printer.print(format("static % f%(", return_type, print_number(index)));
				for (std::size_t i = 0; i < arguments; ++i) {
					if (i > 0) printer.print(", ");
					const Type argument_type = function_table.get_type(function->get_argument_types()[i]);
					printer.print(format("% %", argument_type, Variable(i)));
				}
				printer.print(");");
			}));
			printer.println_increasing(print_functor([&](auto& printer) {
				printer.print(format("static % f%(", return_type, print_number(index)));
				for (std::size_t i = 0; i < arguments; ++i) {
					if (i > 0) printer.print(", ");
					const Type argument_type = function_table.get_type(function->get_argument_types()[i]);
					printer.print(format("% %", argument_type, Variable(i)));
				}
				printer.print(") {");
			}));
			if (tail_call_data.has_tail_call(function)) {
				printer.println_increasing("while (1) {");
			}
			CodegenC codegen(function_table, printer, tail_call_data);
			codegen.variable = arguments;
			const Variable result = codegen.evaluate(function->get_block());
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
		{
			const Type array_type = function_table.get_type(TypeInterner::get_array_type());
			const Type element_type = function_table.get_type(TypeInterner::get_int_type());
			const Type number_type = function_table.get_type(TypeInterner::get_int_type());
			function_declaration_printer.println(format("static % array_new(%* elements, % length);", array_type, element_type, number_type));
			printer.println_increasing(format("static % array_new(%* elements, % length) {", array_type, element_type, number_type));
			printer.println(format("% array;", array_type));
			printer.println(format("array.elements = malloc(length * sizeof(%));", element_type));
			printer.println_increasing(format("for (% i = 0; i < length; i++) {", number_type));
			printer.println("array.elements[i] = elements[i];");
			printer.println_decreasing("}");
			printer.println("array.length = length;");
			printer.println("array.capacity = length;");
			printer.println("return array;");
			printer.println_decreasing("}");
		}
		{
			const Type array_type = function_table.get_type(TypeInterner::get_array_type());
			const Type element_type = function_table.get_type(TypeInterner::get_int_type());
			const Type number_type = function_table.get_type(TypeInterner::get_int_type());
			function_declaration_printer.println(format("static % array_splice(% array, % index, % remove, %* insert_elements, % insert_length);", array_type, array_type, number_type, number_type, element_type, number_type));
			printer.println_increasing(format("static % array_splice(% array, % index, % remove, %* insert_elements, % insert_length) {", array_type, array_type, number_type, number_type, element_type, number_type));
			printer.println(format("% new_length = array.length - remove + insert_length;", number_type));
			printer.println_increasing("if (new_length > array.capacity) {");
			printer.println(format("% new_capacity = array.capacity * 2;", number_type));
			printer.println("if (new_capacity < new_length) new_capacity = new_length;");
			printer.println(format("%* new_elements = malloc(new_capacity * sizeof(%));", element_type, element_type));
			printer.println_increasing(format("for (% i = 0; i < index; i++) {", number_type));
			printer.println("new_elements[i] = array.elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("new_elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = index + remove; i < array.length; i++) {", number_type));
			printer.println("new_elements[i - remove + insert_length] = array.elements[i];");
			printer.println_decreasing("}");
			printer.println("free(array.elements);");
			printer.println("array.elements = new_elements;");
			printer.println("array.length = new_length;");
			printer.println("array.capacity = new_capacity;");
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			printer.println_increasing("if (remove > insert_length) {");
			printer.println_increasing(format("for (% i = index + remove; i < array.length; i++) {", number_type));
			printer.println("array.elements[i - remove + insert_length] = array.elements[i];");
			printer.println_decreasing("}");
			printer.println_decreasing("}");
			printer.println_increasing("else if (insert_length > remove) {");
			printer.println_increasing(format("for (% i = array.length - 1; i >= index + remove; i--) {", number_type));
			printer.println("array.elements[i - remove + insert_length] = array.elements[i];");
			printer.println_decreasing("}");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("array.elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println("array.length = new_length;");
			printer.println_decreasing("}");
			printer.println("return array;");
			printer.println_decreasing("}");
		}
		{
			const Type string_type = function_table.get_type(TypeInterner::get_string_type());
			const Type number_type = function_table.get_type(TypeInterner::get_int_type());
			function_declaration_printer.println(format("static % string_new(char* elements, % length);", string_type, number_type));
			printer.println_increasing(format("static % string_new(char* elements, % length) {", string_type, number_type));
			printer.println(format("% string;", string_type));
			printer.println("string.elements = malloc(length + 1);");
			printer.println_increasing(format("for (% i = 0; i < length; i++) {", number_type));
			printer.println("string.elements[i] = elements[i];");
			printer.println_decreasing("}");
			printer.println("string.elements[length] = '\\0';");
			printer.println("string.length = length;");
			printer.println("string.capacity = length;");
			printer.println("return string;");
			printer.println_decreasing("}");
		}
		{
			const Type array_type = function_table.get_type(TypeInterner::get_string_type());
			const Type number_type = function_table.get_type(TypeInterner::get_int_type());
			function_declaration_printer.println(format("static % string_splice(% string, % index, % remove, char* insert_elements, % insert_length);", array_type, array_type, number_type, number_type, number_type));
			printer.println_increasing(format("static % string_splice(% string, % index, % remove, char* insert_elements, % insert_length) {", array_type, array_type, number_type, number_type, number_type));
			printer.println(format("% new_length = string.length - remove + insert_length;", number_type));
			printer.println_increasing("if (new_length > string.capacity) {");
			printer.println(format("% new_capacity = string.capacity * 2;", number_type));
			printer.println("if (new_capacity < new_length) new_capacity = new_length;");
			printer.println("char* new_elements = malloc(new_capacity + 1);");
			printer.println_increasing(format("for (% i = 0; i < index; i++) {", number_type));
			printer.println("new_elements[i] = string.elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("new_elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = index + remove; i < string.length; i++) {", number_type));
			printer.println("new_elements[i - remove + insert_length] = string.elements[i];");
			printer.println_decreasing("}");
			printer.println("new_elements[new_length] = '\\0';");
			printer.println("free(string.elements);");
			printer.println("string.elements = new_elements;");
			printer.println("string.length = new_length;");
			printer.println("string.capacity = new_capacity;");
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			printer.println_increasing("if (remove > insert_length) {");
			printer.println_increasing(format("for (% i = index + remove; i < string.length; i++) {", number_type));
			printer.println("string.elements[i - remove + insert_length] = string.elements[i];");
			printer.println_decreasing("}");
			printer.println("string.elements[new_length] = '\\0';");
			printer.println_decreasing("}");
			printer.println_increasing("else if (insert_length > remove) {");
			printer.println("string.elements[new_length] = '\\0';");
			printer.println_increasing(format("for (% i = string.length - 1; i >= index + remove; i--) {", number_type));
			printer.println("string.elements[i - remove + insert_length] = string.elements[i];");
			printer.println_decreasing("}");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("string.elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println("string.length = new_length;");
			printer.println_decreasing("}");
			printer.println("return string;");
			printer.println_decreasing("}");
		}
		std::string c_path = std::string(source_path) + ".c";
		std::ofstream file(c_path);
		file << type_declarations.str();
		file << function_declarations.str();
		file << functions.str();
		Printer status_printer(std::cerr);
		status_printer.print(bold(c_path));
		status_printer.print(bold(green(" successfully generated")));
		status_printer.print('\n');
		std::string executable_path = std::string(source_path) + ".exe";
		const char* c_compiler = getenv("CC", "cc");
		const char* compiler_arguments = getenv("CFLAGS", "");
		std::string command = std::string(c_compiler) + " " + compiler_arguments + " -o " + executable_path + " " + c_path;
		if (std::system(command.c_str()) == 0) {
			status_printer.print(bold(executable_path));
			status_printer.print(bold(green(" successfully generated")));
			status_printer.print('\n');
		}
	}
};
