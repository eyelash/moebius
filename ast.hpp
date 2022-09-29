#pragma once

#include "printer.hpp"
#include <cstdint>
#include <vector>
#include <set>
#include <memory>

class Function;

enum class TypeId {
	INT,
	CHAR,
	CLOSURE,
	STRUCT,
	ENUM,
	TUPLE,
	ARRAY,
	STRING,
	STRING_ITERATOR,
	VOID,
	TYPE
};

class Type {
public:
	virtual ~Type() = default;
	virtual TypeId get_id() const = 0;
};

class IntType: public Type {
public:
	TypeId get_id() const override {
		return TypeId::INT;
	}
};

class CharType: public Type {
public:
	TypeId get_id() const override {
		return TypeId::CHAR;
	}
};

class ClosureType: public Type {
	const Function* function;
	std::vector<const Type*> environment_types;
public:
	ClosureType(const Function* function): function(function) {}
	TypeId get_id() const override {
		return TypeId::CLOSURE;
	}
	bool operator <(const ClosureType& rhs) const {
		return std::make_pair(function, std::ref(environment_types)) < std::make_pair(rhs.function, std::ref(rhs.environment_types));
	}
	void add_environment_type(const Type* type) {
		environment_types.push_back(type);
	}
	const Function* get_function() const {
		return function;
	}
	const std::vector<const Type*>& get_environment_types() const {
		return environment_types;
	}
};

class StructType: public Type {
	std::vector<std::pair<std::string, const Type*>> fields;
public:
	TypeId get_id() const override {
		return TypeId::STRUCT;
	}
	bool operator <(const StructType& rhs) const {
		return fields < rhs.fields;
	}
	void add_field(const std::string& field_name, const Type* field_type) {
		fields.emplace_back(field_name, field_type);
	}
	const std::vector<std::pair<std::string, const Type*>>& get_fields() const {
		return fields;
	}
	bool has_field(const std::string& field_name) const {
		for (std::size_t i = 0; i < fields.size(); ++i) {
			if (fields[i].first == field_name) {
				return true;
			}
		}
		return false;
	}
	std::size_t get_index(const std::string& field_name) const {
		for (std::size_t i = 0; i < fields.size(); ++i) {
			if (fields[i].first == field_name) {
				return i;
			}
		}
		return 0;
	}
};

class EnumType: public Type {
	std::vector<std::pair<std::string, const Type*>> cases;
public:
	TypeId get_id() const override {
		return TypeId::ENUM;
	}
	bool operator <(const EnumType& rhs) const {
		return cases < rhs.cases;
	}
	void add_case(const std::string& case_name, const Type* case_type) {
		cases.emplace_back(case_name, case_type);
	}
	const std::vector<std::pair<std::string, const Type*>>& get_cases() const {
		return cases;
	}
	bool has_case(const std::string& case_name) const {
		for (std::size_t i = 0; i < cases.size(); ++i) {
			if (cases[i].first == case_name) {
				return true;
			}
		}
		return false;
	}
	std::size_t get_index(const std::string& case_name) const {
		for (std::size_t i = 0; i < cases.size(); ++i) {
			if (cases[i].first == case_name) {
				return i;
			}
		}
		return 0;
	}
};

class TupleType: public Type {
	std::vector<const Type*> element_types;
public:
	TypeId get_id() const override {
		return TypeId::TUPLE;
	}
	bool operator <(const TupleType& rhs) const {
		return element_types < rhs.element_types;
	}
	void add_element_type(const Type* type) {
		element_types.push_back(type);
	}
	const std::vector<const Type*>& get_element_types() const {
		return element_types;
	}
};

class ArrayType: public Type {
	const Type* element_type;
public:
	ArrayType(const Type* element_type): element_type(element_type) {}
	TypeId get_id() const override {
		return TypeId::ARRAY;
	}
	bool operator <(const ArrayType& rhs) const {
		return element_type < rhs.element_type;
	}
	const Type* get_element_type() const {
		return element_type;
	}
};

class StringType: public Type {
public:
	TypeId get_id() const override {
		return TypeId::STRING;
	}
};

class StringIteratorType: public Type {
public:
	TypeId get_id() const override {
		return TypeId::STRING_ITERATOR;
	}
};

class VoidType: public Type {
public:
	TypeId get_id() const override {
		return TypeId::VOID;
	}
};

class TypeType: public Type {
	const Type* type;
public:
	TypeType(const Type* type): type(type) {}
	TypeId get_id() const override {
		return TypeId::TYPE;
	}
	bool operator <(const TypeType& rhs) const {
		return type < rhs.type;
	}
	const Type* get_type() const {
		return type;
	}
};

template <class T> class TypeCompare {
public:
	bool operator ()(const T* type1, const T* type2) const {
		return *type1 < *type2;
	}
	bool operator ()(const T* type1, const std::unique_ptr<T>& type2) const {
		return *type1 < *type2;
	}
	bool operator ()(const std::unique_ptr<T>& type1, const T* type2) const {
		return *type1 < *type2;
	}
	bool operator ()(const std::unique_ptr<T>& type1, const std::unique_ptr<T>& type2) const {
		return *type1 < *type2;
	}
	using is_transparent = std::true_type;
};

class TypeInterner {
	static inline std::unique_ptr<IntType> int_type;
	static inline std::unique_ptr<CharType> char_type;
	static inline std::set<std::unique_ptr<ClosureType>, TypeCompare<ClosureType>> closure_types;
	static inline std::set<std::unique_ptr<StructType>, TypeCompare<StructType>> struct_types;
	static inline std::set<std::unique_ptr<EnumType>, TypeCompare<EnumType>> enum_types;
	static inline std::set<std::unique_ptr<TupleType>, TypeCompare<TupleType>> tuple_types;
	static inline std::set<std::unique_ptr<ArrayType>, TypeCompare<ArrayType>> array_types;
	static inline std::unique_ptr<StringType> string_type;
	static inline std::unique_ptr<StringIteratorType> string_iterator_type;
	static inline std::unique_ptr<VoidType> void_type;
	static inline std::set<std::unique_ptr<TypeType>, TypeCompare<TypeType>> type_types;
	template <class T> static T* get_or_set(std::unique_ptr<T>& type) {
		if (type) {
			return type.get();
		}
		T* interned_type = new T();
		type = std::unique_ptr<T>(interned_type);
		return interned_type;
	}
	template <class T> static T* get_or_insert(std::set<std::unique_ptr<T>, TypeCompare<T>>& types, const T* type) {
		auto iterator = types.find(type);
		if (iterator != types.end()) {
			return iterator->get();
		}
		T* interned_type = new T(*type);
		types.emplace(interned_type);
		return interned_type;
	}
public:
	static const Type* get_int_type() {
		return get_or_set(int_type);
	}
	static const Type* get_char_type() {
		return get_or_set(char_type);
	}
	static const Type* intern(const ClosureType* closure_type) {
		return get_or_insert(closure_types, closure_type);
	}
	static const Type* intern(const StructType* struct_type) {
		return get_or_insert(struct_types, struct_type);
	}
	static const Type* intern(const EnumType* enum_type) {
		return get_or_insert(enum_types, enum_type);
	}
	static const Type* intern(const TupleType* tuple_type) {
		return get_or_insert(tuple_types, tuple_type);
	}
	static const Type* get_array_type(const Type* element_type) {
		ArrayType array_type(element_type);
		return get_or_insert(array_types, &array_type);
	}
	static const Type* get_string_type() {
		return get_or_set(string_type);
	}
	static const Type* get_string_iterator_type() {
		return get_or_set(string_iterator_type);
	}
	static const Type* get_void_type() {
		return get_or_set(void_type);
	}
	static const Type* get_type_type(const Type* type) {
		TypeType type_type(type);
		return get_or_insert(type_types, &type_type);
	}
};

class IntLiteral;
class BinaryExpression;
class ArrayLiteral;
class StringLiteral;
class If;
class TupleLiteral;
class TupleAccess;
class StructLiteral;
class StructAccess;
class EnumLiteral;
class Switch;
class CaseVariable;
class Closure;
class ClosureAccess;
class Argument;
class ClosureCall;
class MethodCall;
class FunctionCall;
class Intrinsic;
class Bind;
class Return;
class TypeLiteral;
class TypeAssert;
class ReturnType;

template <class T> class Visitor {
public:
	virtual T visit_int_literal(const IntLiteral& int_literal) {
		return T();
	}
	virtual T visit_binary_expression(const BinaryExpression& binary_expression) {
		return T();
	}
	virtual T visit_array_literal(const ArrayLiteral& array_literal) {
		return T();
	}
	virtual T visit_string_literal(const StringLiteral& string_literal) {
		return T();
	}
	virtual T visit_if(const If& if_) {
		return T();
	}
	virtual T visit_tuple_literal(const TupleLiteral& tuple_literal) {
		return T();
	}
	virtual T visit_tuple_access(const TupleAccess& tuple_access) {
		return T();
	}
	virtual T visit_struct_literal(const StructLiteral& struct_literal) {
		return T();
	}
	virtual T visit_struct_access(const StructAccess& struct_access) {
		return T();
	}
	virtual T visit_enum_literal(const EnumLiteral& enum_literal) {
		return T();
	}
	virtual T visit_switch(const Switch& switch_) {
		return T();
	}
	virtual T visit_case_variable(const CaseVariable& case_variable) {
		return T();
	}
	virtual T visit_closure(const Closure& closure) {
		return T();
	}
	virtual T visit_closure_access(const ClosureAccess& closure_access) {
		return T();
	}
	virtual T visit_argument(const Argument& argument) {
		return T();
	}
	virtual T visit_closure_call(const ClosureCall& call) {
		return T();
	}
	virtual T visit_method_call(const MethodCall& call) {
		return T();
	}
	virtual T visit_function_call(const FunctionCall& call) {
		return T();
	}
	virtual T visit_intrinsic(const Intrinsic& intrinsic) {
		return T();
	}
	virtual T visit_bind(const Bind& bind) {
		return T();
	}
	virtual T visit_return(const Return& return_) {
		return T();
	}
	virtual T visit_type_literal(const TypeLiteral& type_literal) {
		return T();
	}
	virtual T visit_type_assert(const TypeAssert& type_assert) {
		return T();
	}
	virtual T visit_return_type(const ReturnType& return_type) {
		return T();
	}
};

class Expression {
	const Type* type;
	SourcePosition position;
public:
	const Expression* next_expression = nullptr;
	Expression(const Type* type = nullptr): type(type) {}
	virtual ~Expression() = default;
	virtual void accept(Visitor<void>& visitor) const = 0;
	const Type* get_type() const {
		return type;
	}
	void set_type(const Type* type) {
		this->type = type;
	}
	TypeId get_type_id() const {
		return get_type()->get_id();
	}
	void set_position(const SourcePosition& position) {
		this->position = position;
	}
	const SourcePosition& get_position() const {
		return position;
	}
};

template <class T> T visit(Visitor<T>& visitor, const Expression* expression) {
	class VoidVisitor: public Visitor<void> {
		Visitor<T>& visitor;
	public:
		T result;
		VoidVisitor(Visitor<T>& visitor): visitor(visitor) {}
		void visit_int_literal(const IntLiteral& int_literal) override {
			result = visitor.visit_int_literal(int_literal);
		}
		void visit_binary_expression(const BinaryExpression& binary_expression) override {
			result = visitor.visit_binary_expression(binary_expression);
		}
		void visit_array_literal(const ArrayLiteral& array_literal) override {
			result = visitor.visit_array_literal(array_literal);
		}
		void visit_string_literal(const StringLiteral& string_literal) override {
			result = visitor.visit_string_literal(string_literal);
		}
		void visit_if(const If& if_) override {
			result = visitor.visit_if(if_);
		}
		void visit_tuple_literal(const TupleLiteral& tuple_literal) override {
			result = visitor.visit_tuple_literal(tuple_literal);
		}
		void visit_tuple_access(const TupleAccess& tuple_access) override {
			result = visitor.visit_tuple_access(tuple_access);
		}
		void visit_struct_literal(const StructLiteral& struct_literal) override {
			result = visitor.visit_struct_literal(struct_literal);
		}
		void visit_struct_access(const StructAccess& struct_access) override {
			result = visitor.visit_struct_access(struct_access);
		}
		void visit_enum_literal(const EnumLiteral& enum_literal) override {
			result = visitor.visit_enum_literal(enum_literal);
		}
		void visit_switch(const Switch& switch_) override {
			result = visitor.visit_switch(switch_);
		}
		void visit_case_variable(const CaseVariable& case_variable) override {
			result = visitor.visit_case_variable(case_variable);
		}
		void visit_closure(const Closure& closure) override {
			result = visitor.visit_closure(closure);
		}
		void visit_closure_access(const ClosureAccess& closure_access) override {
			result = visitor.visit_closure_access(closure_access);
		}
		void visit_argument(const Argument& argument) override {
			result = visitor.visit_argument(argument);
		}
		void visit_closure_call(const ClosureCall& call) override {
			result = visitor.visit_closure_call(call);
		}
		void visit_method_call(const MethodCall& call) override {
			result = visitor.visit_method_call(call);
		}
		void visit_function_call(const FunctionCall& call) override {
			result = visitor.visit_function_call(call);
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {
			result = visitor.visit_intrinsic(intrinsic);
		}
		void visit_bind(const Bind& bind) override {
			result = visitor.visit_bind(bind);
		}
		void visit_return(const Return& return_) override {
			result = visitor.visit_return(return_);
		}
		void visit_type_literal(const TypeLiteral& type_literal) override {
			result = visitor.visit_type_literal(type_literal);
		}
		void visit_type_assert(const TypeAssert& type_assert) override {
			result = visitor.visit_type_assert(type_assert);
		}
		void visit_return_type(const ReturnType& return_type) override {
			result = visitor.visit_return_type(return_type);
		}
	};
	VoidVisitor void_visitor(visitor);
	expression->accept(void_visitor);
	return void_visitor.result;
}

inline void visit(Visitor<void>& visitor, const Expression* expression) {
	expression->accept(visitor);
}

class Return: public Expression {
	const Expression* expression;
public:
	Return(const Expression* expression): Expression(TypeInterner::get_void_type()), expression(expression) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_return(*this);
	}
	const Expression* get_expression() const {
		return expression;
	}
};

class Block {
	const Expression* first = nullptr;
	Expression* last = nullptr;
public:
	Block() = default;
	Block(const Block&) = delete;
	Block(Block&& block): first(std::exchange(block.first, nullptr)), last(std::exchange(block.last, nullptr)) {}
	~Block() {
		const Expression* expression = first;
		while (expression) {
			const Expression* next = expression->next_expression;
			delete expression;
			expression = next;
		}
	}
	Block& operator =(const Block&) = delete;
	Block& operator =(Block&& block) {
		std::swap(first, block.first);
		std::swap(last, block.last);
		return *this;
	}
	void add_expression(Expression* expression) {
		if (first == nullptr) {
			first = expression;
		}
		if (last) {
			last->next_expression = expression;
		}
		last = expression;
	}
	const Expression* get_last() const {
		return last;
	}
	class Iterator {
		const Expression* expression;
	public:
		constexpr Iterator(const Expression* expression): expression(expression) {}
		constexpr bool operator !=(const Iterator& rhs) const {
			return expression != rhs.expression;
		}
		Iterator& operator ++() {
			expression = expression->next_expression;
			return *this;
		}
		constexpr const Expression* operator *() const {
			return expression;
		}
	};
	constexpr Iterator begin() const {
		return Iterator(first);
	}
	constexpr Iterator end() const {
		return Iterator(nullptr);
	}
};

class IntLiteral: public Expression {
	std::int32_t value;
public:
	IntLiteral(std::int32_t value): Expression(TypeInterner::get_int_type()), value(value) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_int_literal(*this);
	}
	std::int32_t get_value() const {
		return value;
	}
};

enum class BinaryOperation {
	ADD,
	SUB,
	MUL,
	DIV,
	REM,
	EQ,
	NE,
	LT,
	LE,
	GT,
	GE
};

class BinaryExpression: public Expression {
	BinaryOperation operation;
	const Expression* left;
	const Expression* right;
public:
	BinaryExpression(BinaryOperation operation, const Expression* left, const Expression* right): Expression(TypeInterner::get_int_type()), operation(operation), left(left), right(right) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_binary_expression(*this);
	}
	BinaryOperation get_operation() const {
		return operation;
	}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
	template <BinaryOperation operation> static Expression* create(const Expression* left, const Expression* right) {
		return new BinaryExpression(operation, left, right);
	}
};

class ArrayLiteral: public Expression {
	std::vector<const Expression*> elements;
public:
	ArrayLiteral(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_array_literal(*this);
	}
	void add_element(const Expression* element) {
		elements.push_back(element);
	}
	const std::vector<const Expression*>& get_elements() const {
		return elements;
	}
};

class StringLiteral: public Expression {
	std::string value;
public:
	StringLiteral(const std::string& value): Expression(TypeInterner::get_string_type()), value(value) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_string_literal(*this);
	}
	const std::string& get_value() const {
		return value;
	}
};

class If: public Expression {
	const Expression* condition;
	Block then_block;
	Block else_block;
public:
	If(const Expression* condition, const Type* type = nullptr): Expression(type), condition(condition) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_if(*this);
	}
	const Expression* get_condition() const {
		return condition;
	}
	Block* get_then_block() {
		return &then_block;
	}
	const Block& get_then_block() const {
		return then_block;
	}
	Block* get_else_block() {
		return &else_block;
	}
	const Block& get_else_block() const {
		return else_block;
	}
};

class TupleLiteral: public Expression {
	std::vector<const Expression*> elements;
public:
	TupleLiteral(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_tuple_literal(*this);
	}
	void add_element(const Expression* element) {
		elements.push_back(element);
	}
	const std::vector<const Expression*>& get_elements() const {
		return elements;
	}
};

class TupleAccess: public Expression {
	const Expression* tuple;
	std::size_t index;
public:
	TupleAccess(const Expression* tuple, std::size_t index, const Type* type = nullptr): Expression(type), tuple(tuple), index(index) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_tuple_access(*this);
	}
	const Expression* get_tuple() const {
		return tuple;
	}
	std::size_t get_index() const {
		return index;
	}
};

class StructLiteral: public Expression {
	std::vector<std::pair<std::string, const Expression*>> fields;
public:
	StructLiteral(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_struct_literal(*this);
	}
	void add_field(const std::string& field_name, const Expression* field) {
		fields.emplace_back(field_name, field);
	}
	void add_field(const StringView& field_name, const Expression* field) {
		fields.emplace_back(std::string(field_name.begin(), field_name.end()), field);
	}
	const std::vector<std::pair<std::string, const Expression*>>& get_fields() const {
		return fields;
	}
};

class StructAccess: public Expression {
	const Expression* struct_;
	std::string field_name;
public:
	StructAccess(const Expression* struct_, const std::string& field_name, const Type* type = nullptr): Expression(type), struct_(struct_), field_name(field_name) {}
	StructAccess(const Expression* struct_, const StringView& field_name, const Type* type = nullptr): Expression(type), struct_(struct_), field_name(field_name.begin(), field_name.end()) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_struct_access(*this);
	}
	const Expression* get_struct() const {
		return struct_;
	}
	const std::string& get_field_name() const {
		return field_name;
	}
};

class EnumLiteral: public Expression {
	const Expression* expression;
	std::size_t index;
public:
	EnumLiteral(const Expression* expression, std::size_t index, const Type* type = nullptr): Expression(type), expression(expression), index(index) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_enum_literal(*this);
	}
	const Expression* get_expression() const {
		return expression;
	}
	std::size_t get_index() const {
		return index;
	}
};

class Switch: public Expression {
	const Expression* enum_;
	std::vector<std::pair<std::string, Block>> cases;
public:
	Switch(const Expression* enum_, const Type* type = nullptr): Expression(type), enum_(enum_) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_switch(*this);
	}
	Block* add_case(const std::string& field_name) {
		cases.emplace_back(std::piecewise_construct, std::forward_as_tuple(field_name), std::forward_as_tuple());
		return &cases.back().second;
	}
	Block* add_case(const StringView& field_name) {
		cases.emplace_back(std::piecewise_construct, std::forward_as_tuple(field_name.begin(), field_name.end()), std::forward_as_tuple());
		return &cases.back().second;
	}
	const Expression* get_enum() const {
		return enum_;
	}
	const std::vector<std::pair<std::string, Block>>& get_cases() const {
		return cases;
	}
};

class CaseVariable: public Expression {
public:
	CaseVariable(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_case_variable(*this);
	}
};

class Function {
	Block block;
	std::size_t arguments;
	std::vector<const Type*> argument_types;
	const Type* return_type;
public:
	const Function* next_function = nullptr;
	Function(const Type* return_type = nullptr): arguments(0), return_type(return_type) {}
	Function(const std::vector<const Type*>& argument_types, const Type* return_type = nullptr): arguments(argument_types.size()), argument_types(argument_types), return_type(return_type) {}
	std::size_t add_argument() {
		return arguments++;
	}
	void set_return_type(const Type* type) {
		this->return_type = type;
	}
	std::size_t get_arguments() const {
		return arguments;
	}
	const std::vector<const Type*>& get_argument_types() const {
		return argument_types;
	}
	const Type* get_return_type() const {
		return return_type;
	}
	Block* get_block() {
		return &block;
	}
	const Block& get_block() const {
		return block;
	}
};

class Closure: public Expression {
	const Function* function;
	std::vector<const Expression*> environment_expressions;
public:
	Closure(const Function* function, const Type* type = nullptr): Expression(type), function(function) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_closure(*this);
	}
	std::size_t add_environment_expression(const Expression* expression) {
		const std::size_t index = environment_expressions.size();
		environment_expressions.push_back(expression);
		return index;
	}
	const Function* get_function() const {
		return function;
	}
	const std::vector<const Expression*>& get_environment_expressions() const {
		return environment_expressions;
	}
};

class ClosureAccess: public Expression {
	const Expression* closure;
	std::size_t index;
public:
	ClosureAccess(const Expression* closure, std::size_t index, const Type* type = nullptr): Expression(type), closure(closure), index(index) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_closure_access(*this);
	}
	const Expression* get_closure() const {
		return closure;
	}
	std::size_t get_index() const {
		return index;
	}
};

class Argument: public Expression {
	std::size_t index;
public:
	Argument(std::size_t index, const Type* type = nullptr): Expression(type), index(index) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_argument(*this);
	}
	std::size_t get_index() const {
		return index;
	}
};

class ClosureCall: public Expression {
	const Expression* closure;
	std::vector<const Expression*> arguments;
public:
	ClosureCall(const Expression* closure): Expression(nullptr), closure(closure) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_closure_call(*this);
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	const Expression* get_closure() const {
		return closure;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};

class MethodCall: public Expression {
	const Expression* object;
	std::string method_name;
	const Expression* method;
	std::vector<const Expression*> arguments;
public:
	MethodCall(const Expression* object, const StringView& method_name, const Expression* method): Expression(nullptr), object(object), method_name(method_name.begin(), method_name.end()), method(method) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_method_call(*this);
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	const Expression* get_object() const {
		return object;
	}
	const std::string& get_method_name() const {
		return method_name;
	}
	const Expression* get_method() const {
		return method;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};

class FunctionCall: public Expression {
	std::vector<const Expression*> arguments;
	const Function* function = nullptr;
public:
	FunctionCall(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_function_call(*this);
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	void set_function(const Function* function) {
		this->function = function;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
	const Function* get_function() const {
		return function;
	}
};

class Intrinsic: public Expression {
	const char* name;
	std::vector<const Expression*> arguments;
public:
	Intrinsic(const char* name, const Type* type = nullptr): Expression(type), name(name) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_intrinsic(*this);
	}
	void add_argument(const Expression* expression) {
		arguments.push_back(expression);
	}
	const char* get_name() const {
		return name;
	}
	bool name_equals(const StringView& name) const {
		return StringView(this->name) == name;
	}
	const std::vector<const Expression*>& get_arguments() const {
		return arguments;
	}
};

class Bind: public Expression {
	const Expression* left;
	const Expression* right;
public:
	Bind(const Expression* left, const Expression* right): Expression(TypeInterner::get_void_type()), left(left), right(right) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_bind(*this);
	}
	const Expression* get_left() const {
		return left;
	}
	const Expression* get_right() const {
		return right;
	}
	static Expression* create(const Expression* left, const Expression* right) {
		return new Bind(left, right);
	}
};

class TypeLiteral: public Expression {
public:
	TypeLiteral(const Type* type): Expression(TypeInterner::get_type_type(type)) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_type_literal(*this);
	}
};

class TypeAssert: public Expression {
	const Expression* expression;
	const Expression* type;
public:
	TypeAssert(const Expression* expression, const Expression* type): Expression(TypeInterner::get_void_type()), expression(expression), type(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_type_assert(*this);
	}
	const Expression* get_expression() const {
		return expression;
	}
	const Expression* get_type() const {
		return type;
	}
};

class ReturnType: public Expression {
	const Expression* type;
public:
	ReturnType(const Expression* type): Expression(TypeInterner::get_void_type()), type(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_return_type(*this);
	}
	const Expression* get_type() const {
		return type;
	}
};

class Program {
	const Function* first = nullptr;
	Function* last = nullptr;
public:
	~Program() {
		const Function* function = first;
		while (function) {
			const Function* next = function->next_function;
			delete function;
			function = next;
		}
	}
	void add_function(Function* function) {
		if (first == nullptr) {
			first = function;
		}
		if (last) {
			last->next_function = function;
		}
		last = function;
	}
	const Function* get_main_function() const {
		return first;
	}
	class Iterator {
		const Function* function;
	public:
		constexpr Iterator(const Function* function): function(function) {}
		constexpr bool operator !=(const Iterator& rhs) const {
			return function != rhs.function;
		}
		Iterator& operator ++() {
			function = function->next_function;
			return *this;
		}
		constexpr const Function* operator *() const {
			return function;
		}
	};
	constexpr Iterator begin() const {
		return Iterator(first);
	}
	constexpr Iterator end() const {
		return Iterator(nullptr);
	}
};

class GetInt: public Visitor<const IntLiteral*> {
public:
	const IntLiteral* visit_int_literal(const IntLiteral& int_literal) override {
		return &int_literal;
	}
};

class GetString: public Visitor<const StringLiteral*> {
public:
	const StringLiteral* visit_string_literal(const StringLiteral& string_literal) override {
		return &string_literal;
	}
};

class GetTupleElement: public Visitor<const Expression*> {
	std::size_t index;
public:
	GetTupleElement(std::size_t index): index(index) {}
	const Expression* visit_tuple_literal(const TupleLiteral& tuple_literal) override {
		return tuple_literal.get_elements()[index];
	}
	const Expression* visit_struct_literal(const StructLiteral& struct_literal) override {
		return struct_literal.get_fields()[index].second;
	}
};

class GetEnum: public Visitor<const EnumLiteral*> {
public:
	const EnumLiteral* visit_enum_literal(const EnumLiteral& enum_literal) override {
		return &enum_literal;
	}
};

class PrintType {
	const Type* type;
public:
	PrintType(const Type* type): type(type) {}
	void print(const Printer& p) const {
		switch (type->get_id()) {
			case TypeId::INT: {
				p.print("Int");
				break;
			}
			case TypeId::CLOSURE: {
				p.print("Function");
				break;
			}
			case TypeId::STRUCT: {
				const StructType* struct_type = static_cast<const StructType*>(type);
				p.print("Struct({");
				for (std::size_t i = 0; i < struct_type->get_fields().size(); ++i) {
					const std::string& field_name = struct_type->get_fields()[i].first;
					const Type* field_type = struct_type->get_fields()[i].second;
					if (i > 0) p.print(",");
					p.print(format("%:%", field_name, PrintType(field_type)));
				}
				p.print("})");
				break;
			}
			case TypeId::ENUM: {
				const EnumType* enum_type = static_cast<const EnumType*>(type);
				p.print("Enum({");
				for (std::size_t i = 0; i < enum_type->get_cases().size(); ++i) {
					const std::string& case_name = enum_type->get_cases()[i].first;
					const Type* case_type = enum_type->get_cases()[i].second;
					if (i > 0) p.print(",");
					p.print(format("%:%", case_name, PrintType(case_type)));
				}
				p.print("})");
				break;
			}
			case TypeId::TUPLE: {
				const TupleType* tuple_type = static_cast<const TupleType*>(type);
				p.print("Tuple((");
				for (std::size_t i = 0; i < tuple_type->get_element_types().size(); ++i) {
					const Type* element_type = tuple_type->get_element_types()[i];
					if (i > 0) p.print(",");
					p.print(PrintType(element_type));
				}
				p.print("))");
				break;
			}
			case TypeId::ARRAY: {
				const Type* element_type = static_cast<const ArrayType*>(type)->get_element_type();
				p.print(format("Array(%)", PrintType(element_type)));
				break;
			}
			case TypeId::STRING: {
				p.print("String");
				break;
			}
			case TypeId::STRING_ITERATOR: {
				p.print("StringIterator");
				break;
			}
			case TypeId::VOID: {
				p.print("Void");
				break;
			}
			case TypeId::TYPE: {
				const Type* type_type = static_cast<const TypeType*>(type)->get_type();
				p.print(format("Type(%)", PrintType(type_type)));
				break;
			}
			default: {
				break;
			}
		}
	}
};
inline PrintType print_type(const Type* type) {
	return PrintType(type);
}
