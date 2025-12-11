#pragma once

#include <source_location>
#include <type_traits>

// TODO: remove once cc is far enough again
#include <format>
#include <memory>
#include <string>

namespace nx::impl
{
// Check kind: soft (CHECK) vs require (REQUIRE)
enum class check_kind
{
    check,
    require
};

// Comparison operators
enum class cmp_op
{
    none,
    less,
    less_equal,
    greater,
    greater_equal,
    equal,
    not_equal
};

// Forward declarations
struct check_handle;
template <class L>
struct lhs_holder;
template <class L, class R>
struct binary_expr_capture;

// Tag for expression decomposition
struct lhs_grab
{
    // Overload operator<=> to capture lhs, hidden friend for less namespace pollution
    template <class L>
    friend auto operator<=>(lhs_grab, L const& lhs)
    {
        return lhs_holder<L>{lhs};
    }
};

// Intermediate expression holder that captures lhs and waits for comparison
template <class L>
struct lhs_holder
{
    // note: this only lives during the eval of CHECK/REQUIRE and is thus safe
    L const& lhs;

    explicit lhs_holder(L const& lhs) : lhs(lhs) {}

    // Comparison operators that build binary_expr_capture
    template <class R>
    auto operator<(R const& rhs) const
    {
        static_assert(requires { bool(lhs < rhs); }, "lhs < rhs must be a valid expression");
        return binary_expr_capture<L, R>{lhs, rhs, cmp_op::less, bool(lhs < rhs)};
    }

    template <class R>
    auto operator<=(R const& rhs) const
    {
        static_assert(requires { bool(lhs <= rhs); }, "lhs <= rhs must be a valid expression");
        return binary_expr_capture<L, R>{lhs, rhs, cmp_op::less_equal, bool(lhs <= rhs)};
    }

    template <class R>
    auto operator>(R const& rhs) const
    {
        static_assert(requires { bool(lhs > rhs); }, "lhs > rhs must be a valid expression");
        return binary_expr_capture<L, R>{lhs, rhs, cmp_op::greater, bool(lhs > rhs)};
    }

    template <class R>
    auto operator>=(R const& rhs) const
    {
        static_assert(requires { bool(lhs >= rhs); }, "lhs >= rhs must be a valid expression");
        return binary_expr_capture<L, R>{lhs, rhs, cmp_op::greater_equal, bool(lhs >= rhs)};
    }

    template <class R>
    auto operator==(R const& rhs) const
    {
        static_assert(requires { bool(lhs == rhs); }, "lhs == rhs must be a valid expression");
        return binary_expr_capture<L, R>{lhs, rhs, cmp_op::equal, bool(lhs == rhs)};
    }

    template <class R>
    auto operator!=(R const& rhs) const
    {
        static_assert(requires { bool(lhs != rhs); }, "lhs != rhs must be a valid expression");
        return binary_expr_capture<L, R>{lhs, rhs, cmp_op::not_equal, bool(lhs != rhs)};
    }

    // TODO: && and || guardrails
};

// Binary expression capture: stores lhs, rhs, operator, and passed
template <class L, class R>
struct binary_expr_capture
{
    // note: this only lives during the eval of CHECK/REQUIRE and is thus safe
    L const& lhs;
    R const& rhs;
    cmp_op op;
    bool passed;
};

// Check handle for chaining and deferred failure reporting
struct check_handle final
{
    struct impl_context;
    std::unique_ptr<impl_context> ctx;

    check_handle() = default;
    check_handle(check_handle&&) = default;
    check_handle(check_handle const&) = delete;
    check_handle& operator=(check_handle&&) = default;
    check_handle& operator=(check_handle const&) = delete;

    ~check_handle() noexcept(false);

    check_handle context(char const* msg) &&;
    check_handle note(char const* msg) &&;

    template <class T>
    check_handle dump(char const* label, T const& value) &&
    {
        return std::move(*this).add_extra_line(std::format("{}: {}", label, value));
    }

    // 2 dumps is used for CHECK(lhs op rhs)
    // 1 dump for CHECK(value)
    template <class T>
    check_handle dump(T const& value) &&
    {
        return std::move(*this).add_extra_line(std::format("{}", value));
    }

    static check_handle make(check_kind kind, cmp_op op, char const* expr_text, bool passed, std::source_location loc);

private:
    check_handle add_extra_line(std::string line) &&;
};

// Factory function for check_handle
template <class L, class R>
check_handle make_check_handle(check_kind kind, char const* expr_text, binary_expr_capture<L, R> const& expr, std::source_location loc)
{
    return check_handle::make(kind, expr.op, expr_text, expr.passed, loc) //
        .dump(expr.lhs)                                                   //
        .dump(expr.rhs);
}

template <class T>
check_handle make_check_handle(check_kind kind, char const* expr_text, lhs_holder<T> const& expr, std::source_location loc)
{
    static_assert(requires(T const& v) { bool(v); }, "type must be castable to bool in CHECK/REQUIRE(v)");

    return check_handle::make(kind, cmp_op::none, expr_text, bool(expr.lhs), loc) //
        .dump(expr.lhs);
}

} // namespace nx::impl

// CHECK macro
#define CHECK(Expr) \
    ::nx::impl::make_check_handle(::nx::impl::check_kind::check, #Expr, ::nx::impl::lhs_grab{} <=> Expr, std::source_location::current())

// REQUIRE macro
#define REQUIRE(Expr) \
    ::nx::impl::make_check_handle(::nx::impl::check_kind::require, #Expr, ::nx::impl::lhs_grab{} <=> Expr, std::source_location::current())
