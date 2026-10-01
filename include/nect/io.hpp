#pragma once
#include "core.hpp"
#include <string_view>

namespace nect {
inline constexpr const char* native_version="0.76";
inline constexpr std::size_t native_size_limit=64*1024*1024;
inline constexpr std::size_t portable_preset_payload_limit=256*1024;
inline constexpr std::size_t portable_macro_payload_limit=256*1024;
struct PortablePresetAssetEnvelope {
    std::uint64_t version=0;
    std::uint64_t accepted_revision=0;
    std::uint64_t payload_schema=0;
    std::string kind,asset_id,sha256,payload,label;
};
std::string base64_encode(const std::vector<unsigned char>&);
std::vector<unsigned char> base64_decode(std::string_view);
// Asset mutations preflight native serialization before changing the live Session.
void apply_serializable(Session&,const std::vector<Command>&,std::uint64_t expected_revision);
Document decode(std::string_view input);
void validate_json(std::string_view input);
std::string encode(const Document& document);
// Canonical standalone PresetDefinition payloads used by the workspace Library.
// These reuse the native v1/v2 codec and reject noncanonical or duplicate-key JSON.
std::string canonical_preset_payload(const PresetDefinition& definition);
PresetDefinition read_canonical_preset_payload(std::string_view input);
PortablePresetAssetEnvelope read_portable_preset_asset_envelope(std::string_view input);
// Canonical MacroDefinition bytes retain every graph revision and stable local ID.
std::string canonical_macro_payload(const MacroDefinition& definition);
MacroDefinition read_canonical_macro_payload(std::string_view input);
std::string export_svg(const Document& document, const Id& composition, const Id& artboard);
// JSON-lines adapter, deliberately not an MCP implementation.
std::string request(Session& session, std::string_view input);
}
