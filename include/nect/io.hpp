#pragma once
#include "core.hpp"
#include <string_view>

namespace nect {
inline constexpr const char* native_version="0.15";
inline constexpr std::size_t native_size_limit=64*1024*1024;
std::string base64_encode(const std::vector<unsigned char>&);
std::vector<unsigned char> base64_decode(std::string_view);
// Asset mutations preflight native serialization before changing the live Session.
void apply_serializable(Session&,const std::vector<Command>&,std::uint64_t expected_revision);
Document decode(std::string_view input);
void validate_json(std::string_view input);
std::string encode(const Document& document);
std::string export_svg(const Document& document, const Id& composition, const Id& artboard);
// JSON-lines adapter, deliberately not an MCP implementation.
std::string request(Session& session, std::string_view input);
}
