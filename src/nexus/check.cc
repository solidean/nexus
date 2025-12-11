#include <nexus/check.hh>
#include <nexus/tests/execute.hh>

// TODO: remove me
#include <string>
#include <vector>

namespace nx::impl
{
// Operator to string conversion
static char const* op_to_string(cmp_op op)
{
    switch (op)
    {
    case cmp_op::less: return "<";
    case cmp_op::less_equal: return "<=";
    case cmp_op::greater: return ">";
    case cmp_op::greater_equal: return ">=";
    case cmp_op::equal: return "==";
    case cmp_op::not_equal: return "!=";
    }
    return "?";
}
} // namespace nx::impl

struct nx::impl::check_handle::impl_context
{
    check_kind kind;
    std::string expr_text;
    bool passed;
    std::source_location location;
    std::vector<std::string> extra_lines;
};

nx::impl::check_handle nx::impl::check_handle::make(check_kind kind, char const* expr_text, bool passed, std::source_location loc)
{
    check_handle handle;
    handle.ctx = std::make_unique<impl_context>(impl_context{
        .kind = kind,
        .expr_text = expr_text,
        .passed = passed,
        .location = loc,
    });
    return handle;
}

nx::impl::check_handle::~check_handle()
{
    if (ctx)
    {
        nx::impl::report_check_result(ctx->kind, std::move(ctx->expr_text), ctx->passed, std::move(ctx->extra_lines), ctx->location);
    }
}

nx::impl::check_handle nx::impl::check_handle::add_extra_line(std::string line) &&
{
    ctx->extra_lines.push_back(std::move(line));
    return std::move(*this);
}

nx::impl::check_handle nx::impl::check_handle::context(char const* msg) &&
{
    return std::move(*this).add_extra_line(std::format("context: {}", msg));
}

nx::impl::check_handle nx::impl::check_handle::note(char const* msg) && { return std::move(*this).add_extra_line(std::format("note: {}", msg)); }
