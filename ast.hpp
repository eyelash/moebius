#pragma once

template <class T> class Reference {
	T* pointer;
public:
	Reference(T* pointer = nullptr): pointer(pointer) {}
	Reference(const Reference&) = delete;
	~Reference() {
		if (pointer) {
			delete pointer;
		}
	}
	Reference& operator =(const Reference&) = delete;
	operator T*() const {
		return pointer;
	}
};

class IntLiteral;
class BinaryExpression;
class If;

template <class T> class Visitor {
public:
	virtual T visit_int_literal(const IntLiteral&) {
		return T();
	}
	virtual T visit_binary_expression(const BinaryExpression&) {
		return T();
	}
	virtual T visit_if(const If&) {
		return T();
	}
};

class Expression {
public:
	virtual ~Expression() = default;
	virtual void accept(Visitor<void>& visitor) const = 0;
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
		void visit_if(const If& if_) override {
			result = visitor.visit_if(if_);
		}
	};
	VoidVisitor void_visitor(visitor);
	expression->accept(void_visitor);
	return void_visitor.result;
}

inline void visit(Visitor<void>& visitor, const Expression* expression) {
	expression->accept(visitor);
}

class IntLiteral: public Expression {
	std::int32_t value;
public:
	IntLiteral(std::int32_t value): value(value) {}
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
	Reference<const Expression> left;
	Reference<const Expression> right;
public:
	BinaryExpression(BinaryOperation operation, const Expression* left, const Expression* right): operation(operation), left(left), right(right) {}
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

class If: public Expression {
	Reference<const Expression> condition;
	Reference<const Expression> then_expression;
	Reference<const Expression> else_expression;
public:
	If(const Expression* condition, const Expression* then_expression, const Expression* else_expression): condition(condition), then_expression(then_expression), else_expression(else_expression) {}
	void accept(Visitor<void>& visitor) const override {
		visitor.visit_if(*this);
	}
	const Expression* get_condition() const {
		return condition;
	}
	const Expression* get_then_expression() const {
		return then_expression;
	}
	const Expression* get_else_expression() const {
		return else_expression;
	}
};
