#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace nect::test_support {
inline void require_native_065(const std::string& encoded) {
    const std::string current_version = "\"version\":\"0.65\"";
    if (encoded.find(current_version) == std::string::npos || encoded.empty() || encoded.back() != '}')
        throw std::runtime_error("Legacy fixture must start from native 0.65 output");
}
inline void remove_empty_terminal_array(std::string& encoded,const char* field) {
    const auto length=std::char_traits<char>::length(field);
    if(encoded.size()<=length||encoded.compare(encoded.size()-length-1,length,field)!=0)
        throw std::runtime_error(std::string("Legacy fixture requires an empty terminal top-level ")+field+" array");
    encoded.erase(encoded.size()-length-1,length);
}
inline std::string untag_ordinary_processing_entries(std::string encoded) {
    if(encoded.find("\"kind\":\"macro\"")!=std::string::npos)
        throw std::runtime_error("Cannot downgrade a native fixture containing Macro stack entries");
    const std::string wrapper="{\"kind\":\"operation\",\"operation\":";
    std::size_t search=0;
    while((search=encoded.find(wrapper,search))!=std::string::npos) {
        const auto operation_start=search+wrapper.size();
        if(operation_start>=encoded.size()||encoded[operation_start]!='{')
            throw std::runtime_error("Malformed tagged ordinary stack operation");
        std::size_t depth=0,operation_end=operation_start;
        bool in_string=false,escaped=false;
        for(;operation_end<encoded.size();++operation_end) {
            const auto ch=encoded[operation_end];
            if(in_string) {
                if(escaped)escaped=false;
                else if(ch=='\\')escaped=true;
                else if(ch=='"')in_string=false;
                continue;
            }
            if(ch=='"')in_string=true;
            else if(ch=='{')++depth;
            else if(ch=='}'&&--depth==0)break;
        }
        if(operation_end>=encoded.size()||operation_end+1>=encoded.size()||encoded[operation_end+1]!='}')
            throw std::runtime_error("Unterminated tagged ordinary stack operation");
        const auto operation_size=operation_end-operation_start+1;
        const auto operation=encoded.substr(operation_start,operation_size);
        encoded.replace(search,operation_end+2-search,operation);
        search+=operation_size;
    }
    return encoded;
}
inline std::string untag_ordinary_processing_entries_for_legacy_fixture(std::string encoded) {
    require_native_065(encoded);
    return untag_ordinary_processing_entries(std::move(encoded));
}
inline std::string without_empty_macro_and_definition_fields_for_legacy_fixture(std::string encoded) {
    require_native_065(encoded);
    remove_empty_terminal_array(encoded,",\"macros\":[]");
    remove_empty_terminal_array(encoded,",\"definitions\":[]");
    return untag_ordinary_processing_entries(std::move(encoded));
}
inline std::string without_empty_presets_for_legacy_fixture(std::string encoded) {
    encoded=without_empty_macro_and_definition_fields_for_legacy_fixture(std::move(encoded));
    remove_empty_terminal_array(encoded,",\"presets\":[]");
    return encoded;
}
}
