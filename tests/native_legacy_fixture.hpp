#pragma once

#include <stdexcept>
#include <string>

namespace nect::test_support {
inline std::string without_empty_presets_for_legacy_fixture(std::string encoded) {
    const std::string current_version = "\"version\":\"0.64\"";
    const std::string field_prefix = ",\"presets\":";
    const std::string empty_field = ",\"presets\":[]";
    if (encoded.find(current_version) == std::string::npos || encoded.empty() || encoded.back() != '}')
        throw std::runtime_error("Legacy fixture must start from native 0.64 output");
    const std::string definitions_field = ",\"definitions\":[]";
    if(encoded.size()<=definitions_field.size()||encoded.compare(encoded.size()-definitions_field.size()-1,
        definitions_field.size(),definitions_field)!=0)
        throw std::runtime_error("Legacy fixture requires an empty terminal top-level definitions array");
    encoded.erase(encoded.size()-definitions_field.size()-1,definitions_field.size());
    const auto field_at = encoded.rfind(field_prefix);
    if (field_at == std::string::npos || encoded.compare(field_at, empty_field.size(), empty_field) != 0 ||
        field_at + empty_field.size() + 1 != encoded.size())
        throw std::runtime_error("Legacy fixture requires an empty top-level presets array");
    encoded.erase(field_at, empty_field.size());
    return encoded;
}
}
