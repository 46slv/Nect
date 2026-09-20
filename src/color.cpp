#include "nect/core.hpp"
#include <algorithm>

namespace nect {
std::string property_name(const Document& d,const Ref& ref) {
    if(const auto found=d.objects.find(ref.object);found!=d.objects.end())return found->second.name;
    if(const auto found=d.named_colors.find(ref.object);found!=d.named_colors.end())return found->second.name;
    throw Error("MISSING_REFERENCE",ref.object);
}
std::vector<Ref> color_properties(const Document& d) {
    std::vector<Ref> result;
    for(const auto& [id,color]:d.named_colors){(void)color;result.push_back({id,"","color"});}
    for(const auto& [id,object]:d.objects)for(const auto& op:object.stack) {
        if(op.type!="nect.paint.fill"&&op.type!="nect.paint.stroke")continue;
        result.push_back(operation_ref(id,op.id,"color"));
        if(op.gradient)for(const auto& stop:op.gradient->stops)
            result.push_back(gradient_ref(id,op.id,op.gradient->id,"stop."+stop.id+".color"));
    }
    return result;
}
std::array<Ref,4> color_channels(const Document& d,const Ref& ref) {
    const auto available=color_properties(d);
    if(std::find(available.begin(),available.end(),ref)==available.end())throw Error("MISSING_COLOR",ref.object+"/"+ref.field);
    const auto prefix=ref.field=="color"?"color.":ref.field.substr(0,ref.field.size()-5);
    std::array<Ref,4> result;const std::array<std::string,4> channels{"r","g","b","a"};
    for(std::size_t i=0;i<4;++i)result[i]={ref.object,"",prefix+channels[i]};
    return result;
}
ColorValue color_value(const Document& d,const Ref& ref,const std::map<Ref,double>& values) {
    ColorValue color;const auto channels=color_channels(d,ref);
    for(std::size_t i=0;i<4;++i)color.rgba[i]=values.at(channels[i]);
    return color;
}
std::optional<Ref> color_link(const Document& d,const Ref& ref) {
    const auto channels=color_channels(d,ref);const auto first=property(d,channels[0]);
    if(!first.binding)return {};
    const auto& source=first.binding->source;
    if(!source.point.empty()||!source.field.ends_with(".r"))return {};
    const Ref candidate{source.object,"",d.named_colors.contains(source.object)?"color":source.field.substr(0,source.field.size()-1)+"color"};
    std::array<Ref,4> source_channels;
    try{source_channels=color_channels(d,candidate);}catch(const Error&){return {};}
    for(std::size_t i=0;i<4;++i) {
        const auto scalar=property(d,channels[i]);
        if(!scalar.binding||scalar.binding->source!=source_channels[i]||scalar.binding->scale!=1||scalar.binding->offset!=0||scalar.binding->mode!="copy_local_value")return {};
    }
    return candidate;
}
bool color_is_used(const Document& d,const Ref& ref) {
    if(!ref.point.empty())return false;
    const auto object=d.objects.find(ref.object);if(object==d.objects.end())return false;
    for(const auto& op:object->second.stack) {
        if(!op.enabled||(op.type!="nect.paint.fill"&&op.type!="nect.paint.stroke"))continue;
        if(ref==operation_ref(ref.object,op.id,"color"))return !op.gradient||!op.gradient->enabled;
        if(op.gradient&&op.gradient->enabled)for(const auto& stop:op.gradient->stops)
            if(ref==gradient_ref(ref.object,op.id,op.gradient->id,"stop."+stop.id+".color"))return true;
    }
    return false;
}
}
