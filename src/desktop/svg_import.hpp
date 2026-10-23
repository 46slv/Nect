#pragma once
#include "nect/core.hpp"
#include <string_view>

namespace nect::desktop {
// Memory-only interchange lowering. The caller applies this complete batch to
// its existing Session; the reader never owns or mutates a live document.
struct SvgImportPlan {
    std::vector<Command> commands;
    Id root;
    double width=0,height=0;
    std::size_t paths=0,points=0;
};
SvgImportPlan read_svg(std::string_view bytes,const Id& composition,const Id& prefix,
    const std::string& name,double x,double y);
}
