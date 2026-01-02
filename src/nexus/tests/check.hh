#pragma once

#include <clean-core/string.hh>
#include <clean-core/to_debug_string.hh>

#include <source_location>
#include <type_traits>

// TODO: remove once cc is far enough again
#include <memory>

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

    check_handle context(cc::string msg) &&;
    check_handle note(cc::string msg) &&;

    check_handle fail_note() &&;
    check_handle fail_note(cc::string msg) &&;
    check_handle succeed_note() &&;
    check_handle succeed_note(cc::string msg) &&;

    template <class T>
    check_handle dump(cc::string_view label, T const& value) &&
    {
        return std::move(*this).add_extra_line(std::format("{}: {}", label, cc::to_debug_string(value)));
    }

    // 2 dumps is used for CHECK(lhs op rhs)
    // 1 dump for CHECK(value)
    template <class T>
    check_handle dump(T const& value) &&
    {
        return std::move(*this).add_extra_line(cc::to_debug_string(value));
    }

    static check_handle make(check_kind kind, cmp_op op, char const* expr_text, bool passed, std::source_location loc);

private:
    check_handle add_extra_line(cc::string line) &&;
};

// Factory function for check_handle
template <class L, class R>
check_handle make_check_handle(check_kind kind,
                               char const* expr_text,
                               binary_expr_capture<L, R> const& expr,
                               std::source_location loc)
{
    return check_handle::make(kind, expr.op, expr_text, expr.passed, loc) //
        .dump(expr.lhs)                                                   //
        .dump(expr.rhs);
}

template <class T>
check_handle make_check_handle(check_kind kind, char const* expr_text, lhs_holder<T> const& expr, std::source_location loc)
{
    static_assert(requires(T const& v) { bool(v); }, "type must be castable to bool in CHECK/REQUIRE(v)");

    return check_handle::make(kind, cmp_op::none, expr_text, bool(expr.lhs), loc);
}

} // namespace nx::impl

// CHECK macro: soft assertion that continues test execution on failure
// Supports boolean expressions and comparisons, preserving lhs/rhs values for diagnostics
// Returns check_handle that can be chained with .note(), .context(), .dump(), etc.
//
// Examples:
//   CHECK(ptr != nullptr);
//   CHECK(value == 42);
//   CHECK(a < b).context("comparing sizes");
//   CHECK(result).note("expected truthy result").dump("result", result);
#define CHECK(Expr)                                                                                      \
    ::nx::impl::make_check_handle(::nx::impl::check_kind::check, #Expr, ::nx::impl::lhs_grab{} <=> Expr, \
                                  std::source_location::current())

// REQUIRE macro: hard assertion that stops test execution on failure
// Supports boolean expressions and comparisons, preserving lhs/rhs values for diagnostics
// Returns check_handle that can be chained with .note(), .context(), .dump(), etc.
//
// Examples:
//   REQUIRE(file.is_open());
//   REQUIRE(count > 0);
//   REQUIRE(ptr != nullptr).context("initialization phase");
#define REQUIRE(Expr)                                                                                      \
    ::nx::impl::make_check_handle(::nx::impl::check_kind::require, #Expr, ::nx::impl::lhs_grab{} <=> Expr, \
                                  std::source_location::current())

// FAIL macro: unconditional hard failure (like REQUIRE(false))
// Optionally takes a message argument
// Returns check_handle that can be chained with other helper functions
//
// Examples:
//   FAIL();                              // fails with default note
//   FAIL("unexpected code path");        // fails with custom message
//   FAIL("invalid state").context("cleanup phase");
#define FAIL(...)                                                                                            \
    ::nx::impl::check_handle::make(::nx::impl::check_kind::require, ::nx::impl::cmp_op::none, "FAIL", false, \
                                   std::source_location::current())                                          \
        .fail_note(__VA_ARGS__)

// SUCCEED macro: unconditional soft success (like CHECK(true))
// Optionally takes a message argument
// Returns check_handle that can be chained with other helper functions
//
// Examples:
//   SUCCEED();                           // succeeds with default note
//   SUCCEED("reached milestone");        // succeeds with custom message
//   SUCCEED("validation passed").dump("data", validated_data);
#define SUCCEED(...)                                                                                         \
    ::nx::impl::check_handle::make(::nx::impl::check_kind::check, ::nx::impl::cmp_op::none, "SUCCEED", true, \
                                   std::source_location::current())                                          \
        .succeed_note(__VA_ARGS__)
