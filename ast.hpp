#pragma once

class IntLiteral;

template <class T> class Visitor {
public:
	virtual T visit_int_literal(const IntLiteral&) {
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
