#pragma once

#include "printer.hpp"
#include <cstdint>
#include <vector>
#include <set>
#include <memory>

class IntLiteral;
class BinaryExpression;
class StringLiteral;
class If;
class TupleLiteral;
class TupleAccess;
class StructLiteral;
class StructAccess;
class Function;
class Closure;
class ClosureAccess;
class Argument;
class Call;
class Intrinsic;
class Bind;
class Return;
class TypeLiteral;
class StructDefinition;
class TypeAssert;
class ReturnType;

enum class TypeId {
	INT,
	CHAR,
	CLOSURE,
	STRUCT,
	TUPLE,
	ARRAY,
	STRING,
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
	std::vector<std::string> field_names;
	std::vector<const Type*> field_types;
public:
	TypeId get_id() const override {
		return TypeId::STRUCT;
	}
	void add_field(const std::string& field_name, const Type* field_type) {
		field_names.push_back(field_name);
		field_types.push_back(field_type);
	}
	const std::vector<std::string>& get_field_names() const {
		return field_names;
	}
	const std::vector<const Type*>& get_field_types() const {
		return field_types;
	}
	bool has_field(const std::string& field_name) const {
		for (std::size_t i = 0; i < field_names.size(); ++i) {
			if (field_names[i] == field_name) {
				return true;
			}
		}
		return false;
	}
	std::size_t get_index(const std::string& field_name) const {
		for (std::size_t i = 0; i < field_names.size(); ++i) {
			if (field_names[i] == field_name) {
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
	const Type* get_type() const {
		return type;
	}
};

class TypeCompare {
	template <class T> static constexpr int compare_(const T& t1, const T& t2) {
		return std::less<T>()(t2, t1) - std::less<T>()(t1, t2);
	}
public:
	static int compare(const Type* type1, const Type* type2) {
		if (type1 == type2) {
			return 0;
		}
		const TypeId id1 = type1->get_id();
		const TypeId id2 = type2->get_id();
		if (id1 != id2) {
			return compare_(id1, id2);
		}
		if (id1 == TypeId::CLOSURE) {
			const ClosureType* closure_type1 = static_cast<const ClosureType*>(type1);
			const ClosureType* closure_type2 = static_cast<const ClosureType*>(type2);
			if (closure_type1->get_function() != closure_type2->get_function()) {
				return compare_(closure_type1->get_function(), closure_type2->get_function());
			}
			return compare(closure_type1->get_environment_types(), closure_type2->get_environment_types());
		}
		if (id1 == TypeId::STRUCT) {
			const StructType* struct_type1 = static_cast<const StructType*>(type1);
			const StructType* struct_type2 = static_cast<const StructType*>(type2);
			if (int diff = compare(struct_type1->get_field_names(), struct_type2->get_field_names())) {
				return diff;
			}
			return compare(struct_type1->get_field_types(), struct_type2->get_field_types());
		}
		if (id1 == TypeId::TUPLE) {
			const TupleType* tuple_type1 = static_cast<const TupleType*>(type1);
			const TupleType* tuple_type2 = static_cast<const TupleType*>(type2);
			return compare(tuple_type1->get_element_types(), tuple_type2->get_element_types());
		}
		if (id1 == TypeId::ARRAY) {
			const ArrayType* array_type1 = static_cast<const ArrayType*>(type1);
			const ArrayType* array_type2 = static_cast<const ArrayType*>(type2);
			return compare(array_type1->get_element_type(), array_type2->get_element_type());
		}
		if (id1 == TypeId::TYPE) {
			const TypeType* type_type1 = static_cast<const TypeType*>(type1);
			const TypeType* type_type2 = static_cast<const TypeType*>(type2);
			return compare(type_type1->get_type(), type_type2->get_type());
		}
		return 0;
	}
	static int compare(const std::vector<const Type*>& types1, const std::vector<const Type*>& types2) {
		if (types1.size() != types2.size()) {
			return compare_(types1.size(), types2.size());
		}
		for (std::size_t i = 0; i < types1.size(); ++i) {
			if (int diff = compare(types1[i], types2[i])) {
				return diff;
			}
		}
		return 0;
	}
	static int compare(const std::vector<std::string>& strings1, const std::vector<std::string>& strings2) {
		if (strings1.size() != strings2.size()) {
			return compare_(strings1.size(), strings2.size());
		}
		for (std::size_t i = 0; i < strings1.size(); ++i) {
			if (int diff = strings1[i].compare(strings2[i])) {
				return diff;
			}
		}
		return 0;
	}
	bool operator ()(const Type* type1, const Type* type2) const {
		return compare(type1, type2) < 0;
	}
	bool operator ()(const Type* type1, const std::unique_ptr<Type>& type2) const {
		return compare(type1, type2.get()) < 0;
	}
	bool operator ()(const std::unique_ptr<Type>& type1, const Type* type2) const {
		return compare(type1.get(), type2) < 0;
	}
	bool operator ()(const std::unique_ptr<Type>& type1, const std::unique_ptr<Type>& type2) const {
		return compare(type1.get(), type2.get()) < 0;
	}
	using is_transparent = std::true_type;
};

class TypeInterner {
	static inline std::set<std::unique_ptr<Type>, TypeCompare> types;
public:
	static Type* copy(const Type* type) {
		switch (type->get_id()) {
			case TypeId::INT: return new IntType();
			case TypeId::CHAR: return new CharType();
			case TypeId::CLOSURE: {
				const ClosureType* closure_type = static_cast<const ClosureType*>(type);
				ClosureType* new_closure_type = new ClosureType(closure_type->get_function());
				for (const Type* environment_type: closure_type->get_environment_types()) {
					new_closure_type->add_environment_type(environment_type);
				}
				return new_closure_type;
			}
			case TypeId::STRUCT: {
				const StructType* struct_type = static_cast<const StructType*>(type);
				StructType* new_struct_type = new StructType();
				for (std::size_t i = 0; i < struct_type->get_field_types().size(); ++i) {
					const std::string& name = struct_type->get_field_names()[i];
					const Type* type = struct_type->get_field_types()[i];
					new_struct_type->add_field(name, type);
				}
				return new_struct_type;
			}
			case TypeId::TUPLE: {
				const TupleType* tuple_type = static_cast<const TupleType*>(type);
				TupleType* new_tuple_type = new TupleType();
				for (const Type* element_type: tuple_type->get_element_types()) {
					new_tuple_type->add_element_type(element_type);
				}
				return new_tuple_type;
			}
			case TypeId::ARRAY: {
				const ArrayType* array_type = static_cast<const ArrayType*>(type);
				return new ArrayType(array_type->get_element_type());
			}
			case TypeId::STRING: return new StringType();
			case TypeId::VOID: return new VoidType();
			case TypeId::TYPE: {
				const TypeType* type_type = static_cast<const TypeType*>(type);
				return new TypeType(type_type->get_type());
			}
			default: return nullptr;
		}
	}
	static const Type* intern(const Type* type) {
		auto iterator = types.find(type);
		if (iterator != types.end()) {
			return iterator->get();
		}
		Type* interned_type = copy(type);
		types.emplace(interned_type);
		return interned_type;
	}
	static const Type* get_int_type() {
		static const Type* int_type = nullptr;
		if (int_type == nullptr) {
			IntType type;
			int_type = intern(&type);
		}
		return int_type;
	}
	static const Type* get_char_type() {
		static const Type* char_type = nullptr;
		if (char_type == nullptr) {
			CharType type;
			char_type = intern(&type);
		}
		return char_type;
	}
	static const Type* get_array_type(const Type* element_type) {
		ArrayType type(element_type);
		return intern(&type);
	}
	static const Type* get_string_type() {
		static const Type* string_type = nullptr;
		if (string_type == nullptr) {
			StringType type;
			string_type = intern(&type);
		}
		return string_type;
	}
	static const Type* get_void_type() {
		static const Type* void_type = nullptr;
		if (void_type == nullptr) {
			VoidType type;
			void_type = intern(&type);
		}
		return void_type;
	}
	static const Type* get_type_type(const Type* type) {
		TypeType type_type(type);
		return intern(&type_type);
	}
};

template <class T> class Visitor {
public:
	virtual T visit_int_literal(const IntLiteral& int_literal) {
		return T();
	}
	virtual T visit_binary_expression(const BinaryExpression& binary_expression) {
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
	virtual T visit_closure(const Closure& closure) {
		return T();
	}
	virtual T visit_closure_access(const ClosureAccess& closure_access) {
		return T();
	}
	virtual T visit_argument(const Argument& argument) {
		return T();
	}
	virtual T visit_call(const Call& call) {
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
	virtual T visit_struct_definition(const StructDefinition& struct_definition) {
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
		void visit_closure(const Closure& closure) override {
			result = visitor.visit_closure(closure);
		}
		void visit_closure_access(const ClosureAccess& closure_access) override {
			result = visitor.visit_closure_access(closure_access);
		}
		void visit_argument(const Argument& argument) override {
			result = visitor.visit_argument(argument);
		}
		void visit_call(const Call& call) override {
			result = visitor.visit_call(call);
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
		void visit_struct_definition(const StructDefinition& struct_definition) override {
			result = visitor.visit_struct_definition(struct_definition);
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
	~Block() {
		const Expression* expression = first;
		while (expression) {
			const Expression* next = expression->next_expression;
			delete expression;
			expression = next;
		}
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
	void clear() {
		first = nullptr;
		last = nullptr;
	}
	const Expression* get_last() const {
		return last;
	}
	const Expression* get_result() const {
		return static_cast<Return*>(last)->get_expression();
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
	std::vector<std::string> field_names;
	std::vector<const Expression*> fields;
public:
	StructLiteral(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_struct_literal(*this);
	}
	void add_field(const std::string& field_name, const Expression* field) {
		field_names.push_back(field_name);
		fields.push_back(field);
	}
	void add_field(const StringView& field_name, const Expression* field) {
		field_names.emplace_back(field_name.begin(), field_name.end());
		fields.push_back(field);
	}
	const std::vector<std::string>& get_field_names() const {
		return field_names;
	}
	const std::vector<const Expression*>& get_fields() const {
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

class Function {
	Block block;
	std::size_t arguments;
	std::vector<const Type*> argument_types;
	const Type* return_type;
public:
	const Function* next_function = nullptr;
	Function(const Type* return_type = nullptr): arguments(1), return_type(return_type) {}
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

class Call: public Expression {
	std::vector<const Expression*> arguments;
	const Function* function = nullptr;
public:
	Call(const Type* type = nullptr): Expression(type) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_call(*this);
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
	const Expression* get_object() const {
		return arguments[0];
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

class StructDefinition: public Expression {
	std::vector<std::string> field_names;
	std::vector<const Expression*> field_types;
public:
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_struct_definition(*this);
	}
	void add_field(const std::string& field_name, const Expression* field_type) {
		field_names.push_back(field_name);
		field_types.push_back(field_type);
	}
	void add_field(const StringView& field_name, const Expression* field_type) {
		field_names.emplace_back(field_name.begin(), field_name.end());
		field_types.push_back(field_type);
	}
	const std::vector<std::string>& get_field_names() const {
		return field_names;
	}
	const std::vector<const Expression*>& get_field_types() const {
		return field_types;
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

class GetInt: public Visitor<bool> {
public:
	std::int32_t value;
	bool visit_int_literal(const IntLiteral& int_literal) override {
		value = int_literal.get_value();
		return true;
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
		return struct_literal.get_fields()[index];
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
				for (std::size_t i = 0; i < struct_type->get_field_types().size(); ++i) {
					const std::string& field_name = struct_type->get_field_names()[i];
					const Type* field_type = struct_type->get_field_types()[i];
					if (i > 0) p.print(",");
					p.print(format("%:%", field_name, PrintType(field_type)));
				}
				p.print("})");
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
