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
	static const ::Type* get_element_type(const ::Type* type) {
		if (type->get_id() == TypeId::STRING) {
			return TypeInterner::get_char_type();
		}
		return static_cast<const ArrayType*>(type)->get_element_type();
	}
	static bool is_managed(const ::Type* type) {
		return type->get_id() == TypeId::TUPLE || type->get_id() == TypeId::ARRAY || type->get_id() == TypeId::STRING;
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
				case TypeId::INT: {
					const std::size_t index = types.size();
					type_declaration_printer.println(format("typedef int32_t %;", Type(index)));
					types[type] = index;
					return index;
				}
				case TypeId::CHAR: {
					const std::size_t index = types.size();
					type_declaration_printer.println(format("typedef char %;", Type(index)));
					types[type] = index;
					return index;
				}
				case TypeId::TUPLE: {
					std::vector<Type> element_types;
					for (const ::Type* element_type: static_cast<const TupleType*>(type)->get_element_types()) {
						element_types.push_back(get_type(element_type));
					}
					const std::size_t index = types.size();
					type_declaration_printer.println_increasing("typedef struct {");
					for (std::size_t i = 0; i < element_types.size(); ++i) {
						type_declaration_printer.println(format("% v%;", element_types[i], print_number(i)));
					}
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type] = index;
					generate_tuple_functions(type);
					return index;
				}
				case TypeId::ARRAY:
				case TypeId::STRING: {
					const Type element_type = get_type(get_element_type(type));
					const Type number_type = get_type(TypeInterner::get_int_type());
					const std::size_t index = types.size();
					type_declaration_printer.println_increasing("typedef struct {");
					type_declaration_printer.println(format("%* elements;", element_type));
					type_declaration_printer.println(format("% length;", number_type));
					type_declaration_printer.println(format("% capacity;", number_type));
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type] = index;
					generate_array_functions(type, type->get_id() == TypeId::STRING);
					return index;
				}
				case TypeId::VOID: {
					const std::size_t index = types.size();
					type_declaration_printer.println(format("typedef void %;", Type(index)));
					types[type] = index;
					return index;
				}
				default: {
					return 0;
				}
			}
		}
		void generate_tuple_functions(const ::Type* type) {
			const Type tuple_type = get_type(type);
			const std::vector<const ::Type*>& types = static_cast<const TupleType*>(type)->get_element_types();
			const Type void_type = get_type(TypeInterner::get_void_type());
			IndentPrinter& printer = type_declaration_printer;

			// tuple_copy
			printer.println_increasing(format("static % %_copy(% tuple) {", tuple_type, tuple_type, tuple_type));
			printer.println(format("% new_tuple;", tuple_type));
			for (std::size_t i = 0; i < types.size(); ++i) {
				if (is_managed(types[i])) {
					printer.println(format("new_tuple.v% = %_copy(tuple.v%);", print_number(i), get_type(types[i]), print_number(i)));
				}
				else {
					printer.println(format("new_tuple.v% = tuple.v%;", print_number(i), print_number(i)));
				}
			}
			printer.println("return new_tuple;");
			printer.println_decreasing("}");

			// tuple_free
			printer.println_increasing(format("static % %_free(% tuple) {", void_type, tuple_type, tuple_type));
			for (std::size_t i = 0; i < types.size(); ++i) {
				if (is_managed(types[i])) {
					printer.println(format("%_free(tuple.v%);", get_type(types[i]), print_number(i)));
				}
			}
			printer.println_decreasing("}");
		}
		void generate_array_functions(const ::Type* type, bool null_terminated = false) {
			const Type array_type = get_type(type);
			const Type element_type = get_type(get_element_type(type));
			const Type number_type = get_type(TypeInterner::get_int_type());
			const Type void_type = get_type(TypeInterner::get_void_type());
			IndentPrinter& printer = type_declaration_printer;

			// array_new
			printer.println_increasing(format("static % %_new(%* elements, % length) {", array_type, array_type, element_type, number_type));
			printer.println(format("% array;", array_type));
			if (null_terminated) {
				printer.println(format("array.elements = malloc((length + 1) * sizeof(%));", element_type));
			}
			else {
				printer.println(format("array.elements = malloc(length * sizeof(%));", element_type));
			}
			printer.println_increasing(format("for (% i = 0; i < length; i++) {", number_type));
			printer.println("array.elements[i] = elements[i];");
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("array.elements[length] = 0;");
			}
			printer.println("array.length = length;");
			printer.println("array.capacity = length;");
			printer.println("return array;");
			printer.println_decreasing("}");

			// array_copy
			printer.println_increasing(format("static % %_copy(% array) {", array_type, array_type, array_type));
			printer.println(format("% new_array;", array_type));
			if (null_terminated) {
				printer.println(format("new_array.elements = malloc((array.length + 1) * sizeof(%));", element_type));
			}
			else {
				printer.println(format("new_array.elements = malloc(array.length * sizeof(%));", element_type));
			}
			printer.println_increasing(format("for (% i = 0; i < array.length; i++) {", number_type));
			if (is_managed(get_element_type(type))) {
				printer.println(format("new_array.elements[i] = %_copy(array.elements[i]);", element_type));
			}
			else {
				printer.println("new_array.elements[i] = array.elements[i];");
			}
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("new_array.elements[array.length] = 0;");
			}
			printer.println("new_array.length = array.length;");
			printer.println("new_array.capacity = array.length;");
			printer.println("return new_array;");
			printer.println_decreasing("}");

			// array_free
			printer.println_increasing(format("static % %_free(% array) {", void_type, array_type, array_type));
			if (is_managed(get_element_type(type))) {
				printer.println_increasing(format("for (% i = 0; i < array.length; i++) {", number_type));
				printer.println(format("%_free(array.elements[i]);", element_type));
				printer.println_decreasing("}");
			}
			printer.println("free(array.elements);");
			printer.println_decreasing("}");

			// array_splice
			printer.println_increasing(format("static % %_splice(% array, % index, % remove, %* insert_elements, % insert_length) {", array_type, array_type, array_type, number_type, number_type, element_type, number_type));
			if (is_managed(get_element_type(type))) {
				printer.println_increasing(format("for (% i = 0; i < remove; i++) {", number_type));
				printer.println(format("%_free(array.elements[index + i]);", element_type));
				printer.println_decreasing("}");
			}
			printer.println(format("% new_length = array.length - remove + insert_length;", number_type));
			printer.println_increasing("if (new_length > array.capacity) {");
			printer.println(format("% new_capacity = array.capacity * 2;", number_type));
			printer.println("if (new_capacity < new_length) new_capacity = new_length;");
			if (null_terminated) {
				printer.println(format("%* new_elements = malloc((new_capacity + 1) * sizeof(%));", element_type, element_type));
			}
			else {
				printer.println(format("%* new_elements = malloc(new_capacity * sizeof(%));", element_type, element_type));
			}
			printer.println_increasing(format("for (% i = 0; i < index; i++) {", number_type));
			printer.println("new_elements[i] = array.elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("new_elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = index + remove; i < array.length; i++) {", number_type));
			printer.println("new_elements[i - remove + insert_length] = array.elements[i];");
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("new_elements[new_length] = 0;");
			}
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
			if (null_terminated) {
				printer.println("array.elements[new_length] = 0;");
			}
			printer.println_decreasing("}");
			printer.println_increasing("else if (insert_length > remove) {");
			if (null_terminated) {
				printer.println("array.elements[new_length] = 0;");
			}
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
	CodegenC(FunctionTable& function_table, IndentPrinter& printer, ExpressionTable& expression_table, std::size_t variable, Variable result, const TailCallData& tail_call_data): function_table(function_table), printer(printer), expression_table(expression_table), variable(variable), result(result), tail_call_data(tail_call_data) {}
	static void evaluate(FunctionTable& function_table, IndentPrinter& printer, ExpressionTable& expression_table, std::size_t variable, Variable result, const TailCallData& tail_call_data, const Block& block) {
		CodegenC codegen(function_table, printer, expression_table, variable, result, tail_call_data);
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
	Variable visit_array_literal(const ArrayLiteral& array_literal) override {
		const Variable result = next_variable();
		const Type type = function_table.get_type(array_literal.get_type());
		const Type element_type = function_table.get_type(get_element_type(array_literal.get_type()));
		const std::size_t size = array_literal.get_elements().size();
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("% % = %_new((%[]){", type, result, type, element_type));
			for (std::size_t i = 0; i < size; ++i) {
				if (i > 0) printer.print(", ");
				printer.print(expression_table[array_literal.get_elements()[i]]);
			}
			printer.print(format("}, %);", print_number(size)));
		}));
		return result;
	}
	Variable visit_string_literal(const StringLiteral& string_literal) override {
		const Variable result = next_variable();
		const Type type = function_table.get_type(string_literal.get_type());
		const Type element_type = function_table.get_type(get_element_type(string_literal.get_type()));
		const std::size_t size = string_literal.get_value().size();
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("% % = %_new((%[]){", type, result, type, element_type));
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
		if (if_.get_type() != TypeInterner::get_void_type()) {
			const Type result_type = function_table.get_type(if_.get_type());
			printer.println(format("% %;", result_type, result));
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
		const Type type = function_table.get_type(tuple_literal.get_type());
		printer.println(print_functor([&](auto& printer) {
			printer.print(format("% % = {", type, result));
			for (std::size_t i = 0; i < tuple_literal.get_elements().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(expression_table[tuple_literal.get_elements()[i]]);
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
		if (tail_call_data.is_tail_call(&call)) {
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				const Variable argument = expression_table[call.get_arguments()[i]];
				printer.println(format("% = %;", Variable(i), argument));
			}
			printer.println("continue;");
		}
		else printer.println(print_functor([&](auto& printer) {
			if (call.get_type() != TypeInterner::get_void_type()) {
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
		else if (intrinsic.name_equals("arrayGet")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = %.elements[%];", type, result, array, index));
		}
		else if (intrinsic.name_equals("arrayLength")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = %.length;", type, result, array));
		}
		else if (intrinsic.name_equals("arraySplice")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			const Type element_type = function_table.get_type(get_element_type(intrinsic.get_type()));
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Variable remove = expression_table[intrinsic.get_arguments()[2]];
			if (intrinsic.get_arguments().size() == 4 && intrinsic.get_arguments()[3]->get_type() == intrinsic.get_type()) {
				const Variable insert = expression_table[intrinsic.get_arguments()[3]];
				printer.println(format("% % = %_splice(%, %, %, %.elements, %.length);", type, result, type, array, index, remove, insert, insert));
				printer.println(format("free(%.elements);", insert));
			}
			else {
				const std::size_t insert = intrinsic.get_arguments().size() - 3;
				printer.println(print_functor([&](auto& printer) {
					printer.print(format("% % = %_splice(%, %, %, (%[]){", type, result, type, array, index, remove, element_type));
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
			printer.println(format("% % = %_copy(%);", type, result, type, array));
		}
		else if (intrinsic.name_equals("free")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_arguments()[0]->get_type());
			printer.println(format("%_free(%);", type, array));
		}
		else {
			printer.println(print_functor([&](auto& printer) {
				printer.print(format("// %(", intrinsic.get_name()));
				for (std::size_t i = 0; i < intrinsic.get_arguments().size(); ++i) {
					if (i > 0) printer.print(", ");
					printer.print(expression_table[intrinsic.get_arguments()[i]]);
				}
				printer.print(")");
			}));
		}
		return result;
	}
	Variable visit_bind(const Bind& bind) override {
		return next_variable();
	}
	Variable visit_return(const Return& return_) override {
		const Expression* expression = return_.get_expression();
		if (expression->get_type() != TypeInterner::get_void_type() && !tail_call_data.is_tail_call(expression)) {
			printer.println(format("% = %;", result, expression_table[expression]));
		}
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
			const Variable result = Variable(arguments);
			if (function->get_return_type()->get_id() != TypeId::VOID) {
				const Type result_type = function_table.get_type(function->get_return_type());
				printer.println(format("% %;", result_type, result));
			}
			CodegenC::evaluate(function_table, printer, arguments + 1, result, tail_call_data, function->get_block());
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
		std::string c_path = std::string(source_path) + ".c";
		{
			std::ofstream file(c_path);
			file << type_declarations.str();
			file << function_declarations.str();
			file << functions.str();
		}
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
