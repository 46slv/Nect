#include "nect/io.hpp"
#include <iostream>
#include <stdexcept>
using namespace nect;
namespace {int checks=0;void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void apply(Session& s,ArtboardTemplateMutation mutation){s.apply({ArtboardTemplateCommand{std::move(mutation)}},s.revision());}
ColorValue color(double r,double g,double b,double a){ColorValue c;c.rgba={r,g,b,a};return c;}
std::optional<ColorValue> effective(const Session& s,const Id& id){return evaluate_artboard(s.document().compositions.front(),id).background;}
}
int main(){try{
 auto d=empty_document("doc","comp","source");d.compositions.front().artboards.push_back(Artboard{"target","Target",100,0,64,64});Session s(d);
 const auto red=color(1,0,0,.5),blue=color(0,0,1,.5),clear=color(.2,.3,.4,0);
 apply(s,SetArtboardBackground{"comp","source",red});
 apply(s,CreateArtboardTemplate{"comp",{"template","Shared","source",{}}});
 apply(s,AssignArtboardTemplate{"comp","target","template",{}});check(effective(s,"target")==red,"Background inherits exact source value");
 check(artboard_background_state(s.document().compositions.front(),"target").inherited,"Derived provenance identifies inherited background");
 apply(s,SetArtboardBackground{"comp","target",std::nullopt});check(!effective(s,"target"),"Explicit none suppresses inherited value");
 apply(s,SetArtboardBackground{"comp","source",blue});check(!effective(s,"target"),"Source edit cannot replace local none");
 apply(s,ResetArtboardTemplateOverride{"comp","target","background"});check(effective(s,"target")==blue,"Reset resumes current source");
 apply(s,SetArtboardBackground{"comp","source",clear});check(effective(s,"target")==clear,"Present alpha-zero is not none");
 const auto linked=s.document();apply(s,DetachArtboardTemplate{"comp","target","freeze"});const auto detached=s.document();
 check(!detached.compositions.front().artboards[1].template_assignment&&effective(s,"target")==clear,"Detach freezes exact optional ColorValue");
 s.undo(s.revision());check(s.document()==linked,"One Undo restores relationship and value");s.redo(s.revision());check(s.document()==detached,"Redo restores frozen value");
 apply(s,SetArtboardBackground{"comp","source",red});check(effective(s,"target")==clear,"Detached value ignores later source edits");
 const auto saved=encode(s.document());check(decode(saved)==s.document(),"Native0.81 exact roundtrip");
 auto lie=saved;const auto marker=lie.find("\"version\":\"0.81\"");check(marker!=std::string::npos,"Writer0.81");lie.replace(marker,16,"\"version\":\"0.80\"");
 bool refused=false;try{(void)decode(lie);}catch(const Error& e){refused=e.code=="NATIVE_VERSION_MISMATCH";}check(refused,"Old-version background lie refused");
 auto old=encode(d);const auto at=old.find("\"version\":\"0.81\"");old.replace(at,16,"\"version\":\"0.80\"");check(decode(old)==d,"Existing0.80 document still readable");
 const auto before=s.document();const auto revision=s.revision();const auto history=s.history();auto invalid=red;invalid.rgba[3]=2;
 refused=false;try{s.apply({ArtboardTemplateCommand{SetArtboardBackground{"comp","source",blue}},ArtboardTemplateCommand{SetArtboardBackground{"comp","target",invalid}}},revision);}catch(const Error& e){refused=e.code=="INVALID_COLOR";}
 check(refused&&s.document()==before&&s.revision()==revision&&s.history()==history,"Failed-later background edit is atomic");
 const auto reply=request(s,R"({"op":"get","ref":{"object":"target","point":"","field":"artboard.background"}})");check(reply.find("\"ok\":true")!=std::string::npos&&reply.find("optional_color")!=std::string::npos,"Typed optional Color readback");
 const auto svg=export_svg(s.document(),"comp","source");check(svg.find("<rect ")!=std::string::npos&&svg.find("fill-opacity=\"0.5\"")!=std::string::npos&&svg.find("isolation:isolate")!=std::string::npos,"SVG output underlay isolates artwork");
 auto update=s.document().compositions.front().artboards[1];update.background.reset();update.name="Renamed";s.apply({UpdateArtboard{"comp",update}},s.revision());check(effective(s,"target")==clear,"Frame-only update preserves background");
 std::cout<<"PASS "<<checks<<" background inheritance/native/Undo/API/SVG smoke checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<checks<<": "<<e.what()<<'\n';return 1;}}
