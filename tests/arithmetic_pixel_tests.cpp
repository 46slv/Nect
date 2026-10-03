#include "nect/blend.hpp"
#include "fixtures/arithmetic-fixed-pixels.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace nect;
using Pixel=PremultipliedSrgb8;
namespace {
int checks=0;
void check(bool v,const std::string& why){if(!v)throw std::runtime_error(why);++checks;}
Pixel pixel(const std::array<unsigned,4>& a){return {static_cast<std::uint8_t>(a[0]),static_cast<std::uint8_t>(a[1]),static_cast<std::uint8_t>(a[2]),static_cast<std::uint8_t>(a[3])};}
void exact(std::string_view mode,Pixel b,Pixel s,double opacity,Pixel expected,const std::string& label){
 const auto v=composite_deterministic_srgb8_opacity(mode,b,s,opacity);
 check(v.status==BlendKernelStatus::ok,std::string(mode)+" status "+label);
 if(v.value!=expected){std::cerr<<mode<<" "<<label<<" opacity="<<opacity<<" actual="<<int(v.value.r)<<","<<int(v.value.g)<<","<<int(v.value.b)<<","<<int(v.value.a)<<" expected="<<int(expected.r)<<","<<int(expected.g)<<","<<int(expected.b)<<","<<int(expected.a)<<"\n";}
 check(v.value==expected,std::string(mode)+" fixed byte oracle "+label);
}
}
int main(){try {
 for(const auto& f:nect_r09_reference::pixel_fixtures)exact(f.mode,pixel(f.backdrop),pixel(f.source),f.opacity,pixel(f.binary64_expected),std::string(f.name));
        exact("darker-color",Pixel{50,0,0,70},Pixel{10,40,0,70},.5,Pixel{54,15,0,95},"exact original-byte total tie");
        exact("darker-color",Pixel{50,0,0,70},Pixel{10,40,0,70},1.,Pixel{57,29,0,121},"exact original-byte total tie");
        exact("lighter-color",Pixel{50,0,0,70},Pixel{10,40,0,70},.5,Pixel{54,15,0,95},"exact original-byte total tie");
        exact("lighter-color",Pixel{50,0,0,70},Pixel{10,40,0,70},1.,Pixel{57,29,0,121},"exact original-byte total tie");
        exact("darker-color",Pixel{25,50,0,255},Pixel{65,10,0,255},.5,Pixel{25,50,0,255},"exact original-byte total tie");
        exact("darker-color",Pixel{25,50,0,255},Pixel{65,10,0,255},1.,Pixel{25,50,0,255},"exact original-byte total tie");
        exact("lighter-color",Pixel{25,50,0,255},Pixel{65,10,0,255},.5,Pixel{25,50,0,255},"exact original-byte total tie");
        exact("lighter-color",Pixel{25,50,0,255},Pixel{65,10,0,255},1.,Pixel{25,50,0,255},"exact original-byte total tie");
        exact("darker-color",Pixel{5,0,0,7},Pixel{1,4,0,7},.5,Pixel{5,2,0,10},"exact original-byte total tie");
        exact("darker-color",Pixel{5,0,0,7},Pixel{1,4,0,7},1.,Pixel{6,4,0,14},"exact original-byte total tie");
        exact("lighter-color",Pixel{5,0,0,7},Pixel{1,4,0,7},.5,Pixel{5,2,0,10},"exact original-byte total tie");
        exact("lighter-color",Pixel{5,0,0,7},Pixel{1,4,0,7},1.,Pixel{6,4,0,14},"exact original-byte total tie");
 for(const auto mode:{"linear-burn","linear-dodge","linear-light","vivid-light","pin-light","hard-mix","subtract","divide","darker-color","lighter-color"}){
  for(const double p:{-1.,1.1,std::numeric_limits<double>::quiet_NaN()}){
   const auto v=composite_deterministic_srgb8_opacity(mode,{}, {},p);check(v.status==BlendKernelStatus::invalid_opacity&&v.value==Pixel{},"Opacity refusal precedes transparent shortcut");
  }
  const auto wrong_profile=composite_deterministic_srgb8_opacity(mode,{}, {},0.,"linear-srgb16");check(wrong_profile.status==BlendKernelStatus::unsupported_profile&&wrong_profile.value==Pixel{},"Profile refusal precedes transparent shortcut");
  const auto bad=composite_deterministic_srgb8_opacity(mode,{1,0,0,0},{},0.);check(bad.status==BlendKernelStatus::invalid_premultiplied_pixel&&bad.value==Pixel{},"Malformed premultiplied pixel cannot be hidden by opacity0");
  const auto legacy=composite_nonseparable_srgb8(mode,{},{});check(legacy.status==BlendKernelStatus::unsupported_blend,"Legacy four-mode adapter stays four-mode");
 }
 for(const auto mode:{"normal","classic-color-burn","classic-color-dodge","classic-difference","unknown"}){const auto v=composite_deterministic_srgb8_opacity(mode,{}, {},0.);check(v.status==BlendKernelStatus::unsupported_blend&&v.value==Pixel{},"Unsupported class/ID before endpoint shortcut");}
 std::cout<<"PASS "<<checks<<" deterministic byte/opacity/status checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
