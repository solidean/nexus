#include <nexus/check.hh>
#include <nexus/tests/execute.hh>

// TODO: remove me
#include <string>
#include <vector>

struct nx::impl::check_handle::impl_context
{
    check_kind kind;
    cmp_op op;
    std::string expr_text;
    bool passed;
    std::source_location location;
    std::vector<std::string> extra_lines;
};

nx::impl::check_handle nx::impl::check_handle::make(check_kind kind, cmp_op op, char const* expr_text, bool passed, std::source_location loc)
{
    check_handle handle;
    handle.ctx = std::make_unique<impl_context>(impl_context{
        .kind = kind,
        .op = op,
        .expr_text = expr_text,
        .passed = passed,
        .location = loc,
    });
    return handle;
}

nx::impl::check_handle::~check_handle() noexcept(false)
{
    if (ctx)
    {
        nx::impl::report_check_result(ctx->kind, ctx->op, std::move(ctx->expr_text), ctx->passed, std::move(ctx->extra_lines), ctx->location);
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
