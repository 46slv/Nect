#pragma once
#include "nect/core.hpp"
#include <boost/json.hpp>
namespace nect {
// Derived analysis only. The legacy SVG projection is supplied by its existing
// owner so capability admission cannot drift between the two API surfaces.
boost::json::object compatibility_plan(const Document&,std::uint64_t,
    const Id& composition,const Id& artboard,const std::string& target,
    const boost::json::object& options,const boost::json::object& svg_evidence);
}
