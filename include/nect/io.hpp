#pragma once
#include "core.hpp"
#include <string_view>

namespace nect {
Document decode(std::string_view input);
std::string encode(const Document& document);
std::string export_svg(const Document& document, const Id& composition, const Id& artboard);
// JSON-lines adapter, deliberately not an MCP implementation.
std::string request(Session& session, std::string_view input);
}
