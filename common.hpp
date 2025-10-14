#pragma once

#include "parsley/common.hpp"

template <class... T> class Tuple;
template <> class Tuple<> {
public:
	constexpr Tuple() {}
};
template <class T0, class... T> class Tuple<T0, T...> {
public:
	T0 head;
	Tuple<T...> tail;
	constexpr Tuple(T0 head, T... tail): head(head), tail(tail...) {}
};

#define TRY(x) ({ auto&& v = (x); if (v.index() != 0) return std::get<1>(std::forward<decltype(v)>(v)); std::get<0>(std::forward<decltype(v)>(v)); })
