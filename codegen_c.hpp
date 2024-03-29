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
		case BinaryOperation::ADD:
			return "+";
		case BinaryOperation::SUB:
			return "-";
		case BinaryOperation::MUL:
			return "*";
		case BinaryOperation::DIV:
			return "/";
		case BinaryOperation::REM:
			return "%";
		case BinaryOperation::EQ:
			return "==";
		case BinaryOperation::NE:
			return "!=";
		case BinaryOperation::LT:
			return "<";
		case BinaryOperation::LE:
			return "<=";
		case BinaryOperation::GT:
			return ">";
		case BinaryOperation::GE:
			return ">=";
		default:
			return StringView();
		}
	}
	static constexpr bool is_printable_character(std::int32_t c) {
		return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == ' ' || c == '-' || c == '.' || c == ',' || c == ':' || c == ';' || c == '!' || c == '?';
	}
	static const ::Type* get_element_type(const ::Type* type) {
		if (type->get_id() == TypeId::STRING) {
			return TypeInterner::get_char_type();
		}
		return static_cast<const ArrayType*>(type)->get_element_type();
	}
	static bool is_managed(const ::Type* type) {
		const TypeId type_id = type->get_id();
		return type_id == TypeId::STRUCT || type_id == TypeId::ENUM || type_id == TypeId::TUPLE || type_id == TypeId::ARRAY || type_id == TypeId::STRING || type_id == TypeId::STRING_ITERATOR || type_id == TypeId::REFERENCE;
	}
	struct TypeTableEntry {
		std::size_t index;
		bool is_declared = false;
		bool is_defined = false;
		bool functions_generated = false;
	};
	class FunctionTable {
		std::map<const Function*, std::size_t> functions;
		std::map<const ::Type*, TypeTableEntry> types;
		std::size_t next_type_index = 0;
		IndentPrinter& type_declaration_printer;
		IndentPrinter& function_declaration_printer;
		IndentPrinter& type_function_printer;
	public:
		FunctionTable(IndentPrinter& type_declaration_printer, IndentPrinter& function_declaration_printer, IndentPrinter& type_function_printer): type_declaration_printer(type_declaration_printer), function_declaration_printer(function_declaration_printer), type_function_printer(type_function_printer) {}
		std::size_t look_up(const Function* function) {
			auto iterator = functions.find(function);
			if (iterator != functions.end()) {
				return iterator->second;
			}
			const std::size_t index = functions.size();
			functions[function] = index;
			return index;
		}
		std::size_t declare_type(const ::Type* type) {
			if (types[type].is_declared) {
				return types[type].index;
			}
			switch (type->get_id()) {
			case TypeId::INT:
				{
					const std::size_t index = next_type_index++;
					type_declaration_printer.println(format("typedef int32_t %;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::CHAR:
				{
					const std::size_t index = next_type_index++;
					type_declaration_printer.println(format("typedef char %;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::STRUCT:
				{
					const std::vector<std::pair<std::string, const ::Type*>>& fields = static_cast<const StructType*>(type)->get_fields();
					for (const auto& field: fields) {
						declare_type(field.second);
					}
					const std::size_t index = next_type_index++;
					type_declaration_printer.println_increasing("typedef struct {");
					for (std::size_t i = 0; i < fields.size(); ++i) {
						if (fields[i].second != TypeInterner::get_void_type()) {
							const Type field_type = declare_type(fields[i].second);
							type_declaration_printer.println(format("% %;", field_type, fields[i].first));
						}
					}
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::ENUM:
				{
					const Type number_type = declare_type(TypeInterner::get_int_type());
					const std::vector<std::pair<std::string, const ::Type*>>& cases = static_cast<const EnumType*>(type)->get_cases();
					for (const auto& case_: cases) {
						declare_type(case_.second);
					}
					const std::size_t index = next_type_index++;
					type_declaration_printer.println_increasing("typedef struct {");
					type_declaration_printer.println(format("% tag;", number_type));
					type_declaration_printer.println_increasing("union {");
					for (std::size_t i = 0; i < cases.size(); ++i) {
						if (cases[i].second != TypeInterner::get_void_type()) {
							const Type case_type = declare_type(cases[i].second);
							type_declaration_printer.println(format("% v%;", case_type, print_number(i)));
						}
					}
					type_declaration_printer.println_decreasing("} value;");
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::TUPLE:
				{
					const std::vector<const ::Type*>& element_types = static_cast<const TupleType*>(type)->get_element_types();
					for (const ::Type* element_type: element_types) {
						declare_type(element_type);
					}
					const std::size_t index = next_type_index++;
					type_declaration_printer.println_increasing("typedef struct {");
					for (std::size_t i = 0; i < element_types.size(); ++i) {
						if (element_types[i] != TypeInterner::get_void_type()) {
							const Type element_type = declare_type(element_types[i]);
							type_declaration_printer.println(format("% v%;", element_type, print_number(i)));
						}
					}
					type_declaration_printer.println_decreasing(format("} %;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::ARRAY:
			case TypeId::STRING:
				{
					const Type element_type = declare_type(get_element_type(type));
					const Type number_type = declare_type(TypeInterner::get_int_type());
					const std::size_t index = next_type_index++;
					type_declaration_printer.println_increasing(format("typedef struct % {", Type(index)));
					type_declaration_printer.println(format("% length;", number_type));
					type_declaration_printer.println(format("% capacity;", number_type));
					type_declaration_printer.println(format("% elements[];", element_type));
					type_declaration_printer.println_decreasing(format("} *%;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::STRING_ITERATOR:
				{
					TupleType tuple_type;
					tuple_type.add_element_type(TypeInterner::get_string_type());
					tuple_type.add_element_type(TypeInterner::get_int_type());
					const ::Type* interned_tuple_type = TypeInterner::intern(&tuple_type);
					const std::size_t index = declare_type(interned_tuple_type);
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::VOID:
				{
					const std::size_t index = next_type_index++;
					type_declaration_printer.println(format("typedef void %;", Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			case TypeId::REFERENCE:
				{
					const std::size_t index = next_type_index++;
					type_declaration_printer.println(format("typedef struct % *%;", Type(index), Type(index)));
					types[type].index = index;
					types[type].is_declared = true;
					return index;
				}
			default:
				types[type].is_declared = true;
				return 0;
			}
		}
		std::size_t define_type(const ::Type* type) {
			if (types[type].is_defined) {
				return types[type].index;
			}
			const std::size_t index = declare_type(type);
			switch (type->get_id()) {
			case TypeId::REFERENCE:
				{
					const Type value_type = declare_type(static_cast<const ReferenceType*>(type)->get_type());
					type_declaration_printer.println_increasing(format("struct % {", Type(index)));
					type_declaration_printer.println(format("% value;", value_type));
					type_declaration_printer.println_decreasing(format("};"));
					types[type].is_defined = true;
					return index;
				}
			default:
				types[type].is_defined = true;
				return index;
			}
		}
		void generate_functions(const ::Type* type) {
			if (types[type].functions_generated) {
				return;
			}
			types[type].functions_generated = true;
			switch (type->get_id()) {
			case TypeId::STRUCT:
				generate_struct_functions(type);
				return;
			case TypeId::ENUM:
				generate_enum_functions(type);
				return;
			case TypeId::TUPLE:
				generate_tuple_functions(type);
				return;
			case TypeId::ARRAY:
			case TypeId::STRING:
				generate_array_functions(type, type->get_id() == TypeId::STRING);
				return;
			case TypeId::STRING_ITERATOR:
				{
					TupleType tuple_type;
					tuple_type.add_element_type(TypeInterner::get_string_type());
					tuple_type.add_element_type(TypeInterner::get_int_type());
					const ::Type* interned_tuple_type = TypeInterner::intern(&tuple_type);
					generate_functions(interned_tuple_type);
					generate_string_iterator_functions(type);
					return;
				}
			case TypeId::REFERENCE:
				generate_reference_functions(type);
				return;
			default:
				return;
			}
		}
		Type get_type(const ::Type* type) {
			const std::size_t index = define_type(type);
			generate_functions(type);
			return index;
		}
		void generate_struct_functions(const ::Type* type) {
			const Type struct_type = get_type(type);
			const auto& fields = static_cast<const StructType*>(type)->get_fields();
			const Type void_type = get_type(TypeInterner::get_void_type());
			for (const auto& field: fields) {
				get_type(field.second);
			}
			IndentPrinter& printer = type_function_printer;

			// struct_copy
			function_declaration_printer.println(format("static % %_copy(%);", struct_type, struct_type, struct_type));
			printer.println_increasing(format("static % %_copy(% struct_) {", struct_type, struct_type, struct_type));
			printer.println(format("% new_struct;", struct_type));
			for (std::size_t i = 0; i < fields.size(); ++i) {
				if (is_managed(fields[i].second)) {
					printer.println(format("new_struct.% = %_copy(struct_.%);", fields[i].first, get_type(fields[i].second), fields[i].first));
				}
				else if (fields[i].second != TypeInterner::get_void_type()) {
					printer.println(format("new_struct.% = struct_.%;", fields[i].first, fields[i].first));
				}
			}
			printer.println("return new_struct;");
			printer.println_decreasing("}");

			// struct_free
			function_declaration_printer.println(format("static % %_free(%);", void_type, struct_type, struct_type));
			printer.println_increasing(format("static % %_free(% struct_) {", void_type, struct_type, struct_type));
			for (std::size_t i = 0; i < fields.size(); ++i) {
				if (is_managed(fields[i].second)) {
					printer.println(format("%_free(struct_.%);", get_type(fields[i].second), fields[i].first));
				}
			}
			printer.println_decreasing("}");
		}
		void generate_enum_functions(const ::Type* type) {
			const Type enum_type = get_type(type);
			const auto& cases = static_cast<const EnumType*>(type)->get_cases();
			const Type void_type = get_type(TypeInterner::get_void_type());
			for (const auto& case_: cases) {
				get_type(case_.second);
			}
			IndentPrinter& printer = type_function_printer;

			// enum_copy
			function_declaration_printer.println(format("static % %_copy(%);", enum_type, enum_type, enum_type));
			printer.println_increasing(format("static % %_copy(% enum_) {", enum_type, enum_type, enum_type));
			printer.println(format("% new_enum;", enum_type));
			printer.println("new_enum.tag = enum_.tag;");
			printer.println_increasing("switch (enum_.tag) {");
			for (std::size_t i = 0; i < cases.size(); ++i) {
				printer.println_increasing(format("case %: {", print_number(i)));
				if (is_managed(cases[i].second)) {
					printer.println(format("new_enum.value.v% = %_copy(enum_.value.v%);", print_number(i), get_type(cases[i].second), print_number(i)));
				}
				else if (cases[i].second != TypeInterner::get_void_type()) {
					printer.println(format("new_enum.value.v% = enum_.value.v%;", print_number(i), print_number(i)));
				}
				printer.println("break;");
				printer.println_decreasing("}");
			}
			printer.println_decreasing("}");
			printer.println("return new_enum;");
			printer.println_decreasing("}");

			// enum_free
			function_declaration_printer.println(format("static % %_free(%);", void_type, enum_type, enum_type));
			printer.println_increasing(format("static % %_free(% enum_) {", void_type, enum_type, enum_type));
			printer.println_increasing("switch (enum_.tag) {");
			for (std::size_t i = 0; i < cases.size(); ++i) {
				printer.println_increasing(format("case %: {", print_number(i)));
				if (is_managed(cases[i].second)) {
					printer.println(format("%_free(enum_.value.v%);", get_type(cases[i].second), print_number(i)));
				}
				printer.println("break;");
				printer.println_decreasing("}");
			}
			printer.println_decreasing("}");
			printer.println_decreasing("}");
		}
		void generate_tuple_functions(const ::Type* type) {
			const Type tuple_type = get_type(type);
			const std::vector<const ::Type*>& element_types = static_cast<const TupleType*>(type)->get_element_types();
			const Type void_type = get_type(TypeInterner::get_void_type());
			IndentPrinter& printer = type_function_printer;

			// tuple_copy
			function_declaration_printer.println(format("static % %_copy(%);", tuple_type, tuple_type, tuple_type));
			printer.println_increasing(format("static % %_copy(% tuple) {", tuple_type, tuple_type, tuple_type));
			printer.println(format("% new_tuple;", tuple_type));
			for (std::size_t i = 0; i < element_types.size(); ++i) {
				if (is_managed(element_types[i])) {
					printer.println(format("new_tuple.v% = %_copy(tuple.v%);", print_number(i), get_type(element_types[i]), print_number(i)));
				}
				else if (element_types[i] != TypeInterner::get_void_type()) {
					printer.println(format("new_tuple.v% = tuple.v%;", print_number(i), print_number(i)));
				}
			}
			printer.println("return new_tuple;");
			printer.println_decreasing("}");

			// tuple_free
			function_declaration_printer.println(format("static % %_free(%);", void_type, tuple_type, tuple_type));
			printer.println_increasing(format("static % %_free(% tuple) {", void_type, tuple_type, tuple_type));
			for (std::size_t i = 0; i < element_types.size(); ++i) {
				if (is_managed(element_types[i])) {
					printer.println(format("%_free(tuple.v%);", get_type(element_types[i]), print_number(i)));
				}
			}
			printer.println_decreasing("}");
		}
		void generate_array_functions(const ::Type* type, bool null_terminated = false) {
			const Type array_type = get_type(type);
			const Type element_type = get_type(get_element_type(type));
			const Type number_type = get_type(TypeInterner::get_int_type());
			const Type void_type = get_type(TypeInterner::get_void_type());
			IndentPrinter& printer = type_function_printer;

			// array_new
			printer.println_increasing(format("static % %_new(%* elements, % length) {", array_type, array_type, element_type, number_type));
			if (null_terminated) {
				printer.println(format("% array = malloc(sizeof(struct %) + (length + 1) * sizeof(%));", array_type, array_type, element_type));
			}
			else {
				printer.println(format("% array = malloc(sizeof(struct %) + length * sizeof(%));", array_type, array_type, element_type));
			}
			printer.println("array->length = length;");
			printer.println("array->capacity = length;");
			printer.println_increasing(format("for (% i = 0; i < length; i++) {", number_type));
			printer.println("array->elements[i] = elements[i];");
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("array->elements[length] = 0;");
			}
			printer.println("return array;");
			printer.println_decreasing("}");

			// array_copy
			function_declaration_printer.println(format("static % %_copy(%);", array_type, array_type, array_type));
			printer.println_increasing(format("static % %_copy(% array) {", array_type, array_type, array_type));
			if (null_terminated) {
				printer.println(format("% new_array = malloc(sizeof(struct %) + (array->length + 1) * sizeof(%));", array_type, array_type, element_type));
			}
			else {
				printer.println(format("% new_array = malloc(sizeof(struct %) + array->length * sizeof(%));", array_type, array_type, element_type));
			}
			printer.println("new_array->length = array->length;");
			printer.println("new_array->capacity = array->length;");
			printer.println_increasing(format("for (% i = 0; i < array->length; i++) {", number_type));
			if (is_managed(get_element_type(type))) {
				printer.println(format("new_array->elements[i] = %_copy(array->elements[i]);", element_type));
			}
			else {
				printer.println("new_array->elements[i] = array->elements[i];");
			}
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("new_array->elements[array->length] = 0;");
			}
			printer.println("return new_array;");
			printer.println_decreasing("}");

			// array_free
			function_declaration_printer.println(format("static % %_free(%);", void_type, array_type, array_type));
			printer.println_increasing(format("static % %_free(% array) {", void_type, array_type, array_type));
			if (is_managed(get_element_type(type))) {
				printer.println_increasing(format("for (% i = 0; i < array->length; i++) {", number_type));
				printer.println(format("%_free(array->elements[i]);", element_type));
				printer.println_decreasing("}");
			}
			printer.println("free(array);");
			printer.println_decreasing("}");

			// array_splice
			printer.println_increasing(format("static % %_splice(% array, % index, % remove, %* insert_elements, % insert_length) {", array_type, array_type, array_type, number_type, number_type, element_type, number_type));
			if (is_managed(get_element_type(type))) {
				printer.println_increasing(format("for (% i = 0; i < remove; i++) {", number_type));
				printer.println(format("%_free(array->elements[index + i]);", element_type));
				printer.println_decreasing("}");
			}
			printer.println(format("% new_length = array->length - remove + insert_length;", number_type));
			printer.println_increasing("if (new_length > array->capacity) {");
			printer.println(format("% new_capacity = array->capacity * 2;", number_type));
			printer.println("if (new_capacity < new_length) new_capacity = new_length;");
			if (null_terminated) {
				printer.println(format("% new_array = malloc(sizeof(struct %) + (new_capacity + 1) * sizeof(%));", array_type, array_type, element_type));
			}
			else {
				printer.println(format("% new_array = malloc(sizeof(struct %) + new_capacity * sizeof(%));", array_type, array_type, element_type));
			}
			printer.println("new_array->length = new_length;");
			printer.println("new_array->capacity = new_capacity;");
			printer.println_increasing(format("for (% i = 0; i < index; i++) {", number_type));
			printer.println("new_array->elements[i] = array->elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("new_array->elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = index + remove; i < array->length; i++) {", number_type));
			printer.println("new_array->elements[i - remove + insert_length] = array->elements[i];");
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("new_array->elements[new_length] = 0;");
			}
			printer.println("free(array);");
			printer.println("return new_array;");
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			printer.println_increasing("if (remove > insert_length) {");
			printer.println_increasing(format("for (% i = index + remove; i < array->length; i++) {", number_type));
			printer.println("array->elements[i - remove + insert_length] = array->elements[i];");
			printer.println_decreasing("}");
			if (null_terminated) {
				printer.println("array->elements[new_length] = 0;");
			}
			printer.println_decreasing("}");
			printer.println_increasing("else if (insert_length > remove) {");
			if (null_terminated) {
				printer.println("array->elements[new_length] = 0;");
			}
			printer.println_increasing(format("for (% i = array->length - 1; i >= index + remove; i--) {", number_type));
			printer.println("array->elements[i - remove + insert_length] = array->elements[i];");
			printer.println_decreasing("}");
			printer.println_decreasing("}");
			printer.println_increasing(format("for (% i = 0; i < insert_length; i++) {", number_type));
			printer.println("array->elements[index + i] = insert_elements[i];");
			printer.println_decreasing("}");
			printer.println("array->length = new_length;");
			printer.println("return array;");
			printer.println_decreasing("}");
			printer.println_decreasing("}");

			// from_codepoint
			if (get_element_type(type) == TypeInterner::get_char_type()) {
				printer.println_increasing(format("static % from_codepoint(% codepoint, %* s) {", number_type, number_type, element_type));
				printer.println_increasing("if (codepoint < (1 << 7)) {");
				printer.println("s[0] = codepoint;");
				printer.println("return 1;");
				printer.println_decreasing("}");
				printer.println_increasing("else if (codepoint < (1 << 11)) {");
				printer.println("s[0] = 0xC0 | codepoint >> 6;");
				printer.println("s[1] = 0x80 | codepoint & 0x3F;");
				printer.println("return 2;");
				printer.println_decreasing("}");
				printer.println_increasing("else if (codepoint < (1 << 16)) {");
				printer.println("s[0] = 0xE0 | codepoint >> 12;");
				printer.println("s[1] = 0x80 | codepoint >> 6 & 0x3F;");
				printer.println("s[2] = 0x80 | codepoint & 0x3F;");
				printer.println("return 3;");
				printer.println_decreasing("}");
				printer.println_increasing("else if (codepoint < (1 << 21)) {");
				printer.println("s[0] = 0xF0 | codepoint >> 18;");
				printer.println("s[1] = 0x80 | codepoint >> 12 & 0x3F;");
				printer.println("s[2] = 0x80 | codepoint >> 6 & 0x3F;");
				printer.println("s[3] = 0x80 | codepoint & 0x3F;");
				printer.println("return 4;");
				printer.println_decreasing("}");
				printer.println_decreasing("}");
			}
		}
		void generate_string_iterator_functions(const ::Type* type) {
			const Type string_iterator_type = get_type(type);
			const Type number_type = get_type(TypeInterner::get_int_type());
			const Type char_type = get_type(TypeInterner::get_char_type());
			IndentPrinter& printer = type_function_printer;

			// string_iterator_get_next
			TupleType iteration_result_type_tuple;
			iteration_result_type_tuple.add_element_type(type);
			iteration_result_type_tuple.add_element_type(TypeInterner::get_int_type());
			iteration_result_type_tuple.add_element_type(TypeInterner::get_int_type());
			const Type iteration_result_type = get_type(TypeInterner::intern(&iteration_result_type_tuple));
			function_declaration_printer.println(format("static % string_iterator_get_next(%);", iteration_result_type, string_iterator_type));
			printer.println_increasing(format("static % string_iterator_get_next(% string_iterator) {", iteration_result_type, string_iterator_type));
			printer.println(format("%* s = string_iterator.v0->elements + string_iterator.v1;", char_type));
			printer.println(format("% size = string_iterator.v0->length - string_iterator.v1;", number_type));
			printer.println(format("% result;", iteration_result_type));
			printer.println("result.v0.v0 = string_iterator.v0;");
			printer.println_increasing("if (size >= 1 && (s[0] & 0x80) == 0x00) {");
			printer.println("result.v0.v1 = string_iterator.v1 + 1;");
			printer.println("result.v1 = 1;");
			printer.println("result.v2 = s[0];");
			printer.println_decreasing("}");
			printer.println_increasing("else if (size >= 2 && (s[0] & 0xE0) == 0xC0) {");
			printer.println("result.v0.v1 = string_iterator.v1 + 2;");
			printer.println("result.v1 = 1;");
			printer.println("result.v2 = 0;");
			printer.println("result.v2 |= (s[0] & 0x1F) << 6;");
			printer.println("result.v2 |= (s[1] & 0x3F);");
			printer.println_decreasing("}");
			printer.println_increasing("else if (size >= 3 && (s[0] & 0xF0) == 0xE0) {");
			printer.println("result.v0.v1 = string_iterator.v1 + 3;");
			printer.println("result.v1 = 1;");
			printer.println("result.v2 = 0;");
			printer.println("result.v2 |= (s[0] & 0x0F) << 12;");
			printer.println("result.v2 |= (s[1] & 0x3F) << 6;");
			printer.println("result.v2 |= (s[2] & 0x3F);");
			printer.println_decreasing("}");
			printer.println_increasing("else if (size >= 4 && (s[0] & 0xF8) == 0xF0) {");
			printer.println("result.v0.v1 = string_iterator.v1 + 4;");
			printer.println("result.v1 = 1;");
			printer.println("result.v2 = 0;");
			printer.println("result.v2 |= (s[0] & 0x07) << 18;");
			printer.println("result.v2 |= (s[1] & 0x3F) << 12;");
			printer.println("result.v2 |= (s[2] & 0x3F) << 6;");
			printer.println("result.v2 |= (s[3] & 0x3F);");
			printer.println_decreasing("}");
			printer.println_increasing("else {");
			printer.println("result.v0.v1 = string_iterator.v1;");
			printer.println("result.v1 = 0;");
			printer.println_decreasing("}");
			printer.println("return result;");
			printer.println_decreasing("}");
		}
		void generate_reference_functions(const ::Type* type) {
			const Type reference_type = get_type(type);
			const Type value_type = get_type(static_cast<const ReferenceType*>(type)->get_type());
			const Type void_type = get_type(TypeInterner::get_void_type());
			IndentPrinter& printer = type_function_printer;

			// reference_copy
			function_declaration_printer.println(format("static % %_copy(%);", reference_type, reference_type, reference_type));
			printer.println_increasing(format("static % %_copy(% reference) {", reference_type, reference_type, reference_type));
			printer.println(format("% new_reference = malloc(sizeof(struct %));", reference_type, reference_type));
			printer.println(format("new_reference->value = %_copy(reference->value);", value_type));
			printer.println("return new_reference;");
			printer.println_decreasing("}");

			// reference_free
			function_declaration_printer.println(format("static % %_free(%);", void_type, reference_type, reference_type));
			printer.println_increasing(format("static % %_free(% reference) {", void_type, reference_type, reference_type));
			printer.println(format("%_free(reference->value);", value_type));
			printer.println("free(reference);");
			printer.println_decreasing("}");
		}
	};
	FunctionTable& function_table;
	IndentPrinter& printer;
	using ExpressionTable = std::map<const Expression*, Variable>;
	ExpressionTable& expression_table;
	std::size_t variable;
	Variable case_variable;
	Variable result;
	const TailCallData& tail_call_data;
	Variable next_variable() {
		return Variable(variable++);
	}
	CodegenC(FunctionTable& function_table, IndentPrinter& printer, ExpressionTable& expression_table, std::size_t variable, Variable case_variable, Variable result, const TailCallData& tail_call_data): function_table(function_table), printer(printer), expression_table(expression_table), variable(variable), case_variable(case_variable), result(result), tail_call_data(tail_call_data) {}
	static void evaluate(FunctionTable& function_table, IndentPrinter& printer, ExpressionTable& expression_table, std::size_t variable, Variable case_variable, Variable result, const TailCallData& tail_call_data, const Block& block) {
		CodegenC codegen(function_table, printer, expression_table, variable, case_variable, result, tail_call_data);
		for (const Expression* expression: block) {
			expression_table[expression] = visit(codegen, expression);
		}
	}
	static void evaluate(FunctionTable& function_table, IndentPrinter& printer, std::size_t variable, Variable result, const TailCallData& tail_call_data, const Block& block) {
		ExpressionTable expression_table;
		evaluate(function_table, printer, expression_table, variable, 0, result, tail_call_data, block);
	}
	void evaluate(Variable result, const Block& block) {
		evaluate(function_table, printer, expression_table, variable, 0, result, tail_call_data, block);
	}
	void evaluate(Variable case_variable, Variable result, const Block& block) {
		evaluate(function_table, printer, expression_table, variable, case_variable, result, tail_call_data, block);
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
		printer.println(print_functor([&](auto& printer) {
			std::size_t size = 0;
			printer.print(format("% % = %_new(\"", type, result, type));
			for (std::int32_t codepoint: code_points(string_literal.get_value())) {
				if (is_printable_character(codepoint)) {
					printer.print(static_cast<char>(codepoint));
					++size;
				}
				else for (char c: from_codepoint(codepoint)) {
					printer.print(format("\\%", print_octal(static_cast<unsigned char>(c), 3)));
					++size;
				}
			}
			printer.print(format("\", %);", print_number(size)));
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
		printer.println(format("% %;", type, result));
		for (std::size_t i = 0; i < tuple_literal.get_elements().size(); ++i) {
			const Variable element = expression_table[tuple_literal.get_elements()[i]];
			if (tuple_literal.get_elements()[i]->get_type() != TypeInterner::get_void_type()) {
				printer.println(format("%.v% = %;", result, print_number(i), element));
			}
		}
		return result;
	}
	Variable visit_tuple_access(const TupleAccess& tuple_access) override {
		const Variable tuple = expression_table[tuple_access.get_tuple()];
		const Variable result = next_variable();
		if (tuple_access.get_type() != TypeInterner::get_void_type()) {
			const Type result_type = function_table.get_type(tuple_access.get_type());
			printer.println(format("% % = %.v%;", result_type, result, tuple, print_number(tuple_access.get_index())));
		}
		return result;
	}
	Variable visit_struct_literal(const StructLiteral& struct_literal) override {
		const Variable result = next_variable();
		const Type type = function_table.get_type(struct_literal.get_type());
		printer.println(format("% %;", type, result));
		for (std::size_t i = 0; i < struct_literal.get_fields().size(); ++i) {
			const auto& field = struct_literal.get_fields()[i];
			if (field.second->get_type() != TypeInterner::get_void_type()) {
				printer.println(format("%.% = %;", result, field.first, expression_table[field.second]));
			}
		}
		return result;
	}
	Variable visit_struct_access(const StructAccess& struct_access) override {
		const Variable struct_ = expression_table[struct_access.get_struct()];
		const Variable result = next_variable();
		if (struct_access.get_type() != TypeInterner::get_void_type()) {
			const Type result_type = function_table.get_type(struct_access.get_type());
			if (struct_access.get_struct()->get_type_id() == TypeId::REFERENCE) {
				printer.println(format("% % = %->value.%;", result_type, result, struct_, struct_access.get_field_name()));
			}
			else {
				printer.println(format("% % = %.%;", result_type, result, struct_, struct_access.get_field_name()));
			}
		}
		return result;
	}
	Variable visit_enum_literal(const EnumLiteral& enum_literal) override {
		const Variable expression = expression_table[enum_literal.get_expression()];
		const std::size_t index = enum_literal.get_index();
		const Variable result = next_variable();
		const Type result_type = function_table.get_type(enum_literal.get_type());
		printer.println(format("% %;", result_type, result));
		printer.println(format("%.tag = %;", result, print_number(index)));
		if (enum_literal.get_expression()->get_type() != TypeInterner::get_void_type()) {
			printer.println(format("%.value.v% = %;", result, print_number(index), expression));
		}
		return result;
	}
	static const EnumType* get_enum_type(const Expression* enum_) {
		if (enum_->get_type_id() == TypeId::ENUM) {
			return static_cast<const EnumType*>(enum_->get_type());
		}
		if (enum_->get_type_id() == TypeId::REFERENCE) {
			const ::Type* type = static_cast<const ReferenceType*>(enum_->get_type())->get_type();
			if (type->get_id() == TypeId::ENUM) {
				return static_cast<const EnumType*>(type);
			}
		}
		return nullptr;
	}
	Variable visit_switch(const Switch& switch_) override {
		const Variable enum_ = expression_table[switch_.get_enum()];
		const Variable result = next_variable();
		const Variable case_variable = next_variable();
		if (switch_.get_type() != TypeInterner::get_void_type()) {
			const Type result_type = function_table.get_type(switch_.get_type());
			printer.println(format("% %;", result_type, result));
		}
		if (switch_.get_enum()->get_type_id() == TypeId::REFERENCE) {
			printer.println_increasing(format("switch (%->value.tag) {", enum_));
		}
		else {
			printer.println_increasing(format("switch (%.tag) {", enum_));
		}
		for (std::size_t i = 0; i < switch_.get_cases().size(); ++i) {
			const Block& case_block = switch_.get_cases()[i].second;
			const ::Type* case_type = get_enum_type(switch_.get_enum())->get_cases()[i].second;
			printer.println_increasing(format("case %: {", print_number(i)));
			if (case_type != TypeInterner::get_void_type()) {
				if (switch_.get_enum()->get_type_id() == TypeId::REFERENCE) {
					printer.println(format("% % = %->value.value.v%;", function_table.get_type(case_type), case_variable, enum_, print_number(i)));
				}
				else {
					printer.println(format("% % = %.value.v%;", function_table.get_type(case_type), case_variable, enum_, print_number(i)));
				}
			}
			if (switch_.get_enum()->get_type_id() == TypeId::REFERENCE) {
				printer.println(format("free(%);", enum_));
			}
			evaluate(case_variable, result, case_block);
			printer.println("break;");
			printer.println_decreasing("}");
		}
		printer.println_decreasing("}");
		return result;
	}
	Variable visit_case_variable(const CaseVariable& case_variable) override {
		return this->case_variable;
	}
	Variable visit_argument(const Argument& argument) override {
		return Variable(argument.get_index());
	}
	Variable visit_function_call(const FunctionCall& call) override {
		const std::size_t new_index = function_table.look_up(call.get_function());
		const Variable result = next_variable();
		if (tail_call_data.is_tail_call(&call)) {
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				if (call.get_arguments()[i]->get_type() != TypeInterner::get_void_type()) {
					const Variable argument = expression_table[call.get_arguments()[i]];
					printer.println(format("% = %;", Variable(i), argument));
				}
			}
			printer.println("continue;");
		}
		else printer.println(print_functor([&](auto& printer) {
			if (call.get_type() != TypeInterner::get_void_type()) {
				const Type result_type = function_table.get_type(call.get_function()->get_return_type());
				printer.print(format("% % = ", result_type, result));
			}
			printer.print(format("f%(", print_number(new_index)));
			bool is_first_argument = true;
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				if (call.get_arguments()[i]->get_type() != TypeInterner::get_void_type()) {
					if (is_first_argument) is_first_argument = false;
					else printer.print(", ");
					printer.print(expression_table[call.get_arguments()[i]]);
				}
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
			printer.println(format("fputs(%->elements, stdout);", argument));
		}
		else if (intrinsic.name_equals("getChar")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = getchar();", type, result));
		}
		else if (intrinsic.name_equals("arrayGet")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = %->elements[%];", type, result, array, index));
		}
		else if (intrinsic.name_equals("arrayLength")) {
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = %->length;", type, result, array));
		}
		else if (intrinsic.name_equals("arraySplice")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			const Type element_type = function_table.get_type(get_element_type(intrinsic.get_type()));
			const Variable array = expression_table[intrinsic.get_arguments()[0]];
			const Variable index = expression_table[intrinsic.get_arguments()[1]];
			const Variable remove = expression_table[intrinsic.get_arguments()[2]];
			if (intrinsic.get_arguments().size() == 4 && intrinsic.get_arguments()[3]->get_type() == intrinsic.get_type()) {
				const Variable insert = expression_table[intrinsic.get_arguments()[3]];
				printer.println(format("% % = %_splice(%, %, %, %->elements, %->length);", type, result, type, array, index, remove, insert, insert));
				printer.println(format("free(%);", insert));
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
		else if (intrinsic.name_equals("stringPush")) {
			const Type type = function_table.get_type(intrinsic.get_type());
			const Type element_type = function_table.get_type(get_element_type(intrinsic.get_type()));
			const Type number_type = function_table.get_type(TypeInterner::get_int_type());
			const Variable string = expression_table[intrinsic.get_arguments()[0]];
			const Variable argument = expression_table[intrinsic.get_arguments()[1]];
			if (intrinsic.get_arguments()[1]->get_type() == intrinsic.get_type()) {
				printer.println(format("% % = %_splice(%, %->length, 0, %->elements, %->length);", type, result, type, string, string, argument, argument));
				printer.println(format("free(%);", argument));
			}
			else {
				const Variable elements = next_variable();
				const Variable length = next_variable();
				printer.println(format("% %[4];", element_type, elements));
				printer.println(format("% % = from_codepoint(%, %);", number_type, length, argument, elements));
				printer.println(format("% % = %_splice(%, %->length, 0, %, %);", type, result, type, string, string, elements, length));
			}
		}
		else if (intrinsic.name_equals("stringIterator")) {
			const Variable string = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% %;", type, result));
			printer.println(format("%.v0 = %;", result, string));
			printer.println(format("%.v1 = 0;", result));
		}
		else if (intrinsic.name_equals("stringIteratorGetNext")) {
			const Variable iterator = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = string_iterator_get_next(%);", type, result, iterator));
		}
		else if (intrinsic.name_equals("reference")) {
			const Variable value = expression_table[intrinsic.get_arguments()[0]];
			const Type type = function_table.get_type(intrinsic.get_type());
			printer.println(format("% % = malloc(sizeof(struct %));", type, result, type));
			printer.println(format("%->value = %;", result, value));
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
		const Variable right = expression_table[bind.get_right()];
		const Variable result = next_variable();
		if (bind.get_right()->get_type() != TypeInterner::get_void_type()) {
			const Type result_type = function_table.get_type(bind.get_right()->get_type());
			printer.println(format("% % = %;", result_type, result, right));
		}
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
		std::ostringstream type_declarations;
		std::ostringstream function_declarations;
		std::ostringstream type_functions;
		std::ostringstream functions;
		IndentPrinter type_declaration_printer(type_declarations);
		IndentPrinter function_declaration_printer(function_declarations);
		IndentPrinter type_function_printer(type_functions);
		IndentPrinter printer(functions);
		FunctionTable function_table(type_declaration_printer, function_declaration_printer, type_function_printer);
		type_declaration_printer.println("#include <stdlib.h>");
		type_declaration_printer.println("#include <stdint.h>");
		type_declaration_printer.println("#include <stdio.h>");
		{
			printer.println_increasing("int main(int argc, char **argv) {");
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
				bool is_first_argument = true;
				for (std::size_t i = 0; i < arguments; ++i) {
					if (function->get_argument_types()[i] != TypeInterner::get_void_type()) {
						if (is_first_argument) is_first_argument = false;
						else printer.print(", ");
						const Type argument_type = function_table.get_type(function->get_argument_types()[i]);
						printer.print(format("% %", argument_type, Variable(i)));
					}
				}
				printer.print(");");
			}));
			printer.println_increasing(print_functor([&](auto& printer) {
				printer.print(format("static % f%(", return_type, print_number(index)));
				bool is_first_argument = true;
				for (std::size_t i = 0; i < arguments; ++i) {
					if (function->get_argument_types()[i] != TypeInterner::get_void_type()) {
						if (is_first_argument) is_first_argument = false;
						else printer.print(", ");
						const Type argument_type = function_table.get_type(function->get_argument_types()[i]);
						printer.print(format("% %", argument_type, Variable(i)));
					}
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
			file << type_functions.str();
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
