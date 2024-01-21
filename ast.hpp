#pragma once

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

class IntLiteral final: public Expression {
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

class BinaryExpression final: public Expression {
	BinaryOperation operation;
	Reference<Expression> left;
	Reference<Expression> right;
public:
	BinaryExpression(BinaryOperation operation, Reference<Expression>&& left, Reference<Expression>&& right): operation(operation), left(std::move(left)), right(std::move(right)) {}
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
	template <BinaryOperation operation> static Reference<Expression> create(Reference<Expression>&& left, Reference<Expression>&& right) {
		return new BinaryExpression(operation, std::move(left), std::move(right));
	}
};

class If final: public Expression {
	Reference<Expression> condition;
	Reference<Expression> then_expression;
	Reference<Expression> else_expression;
public:
	If(Reference<Expression>&& condition, Reference<Expression>&& then_expression, Reference<Expression>&& else_expression): condition(std::move(condition)), then_expression(std::move(then_expression)), else_expression(std::move(else_expression)) {}
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
