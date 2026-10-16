#include "preset_batch_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <algorithm>
#include <iostream>
#include <set>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
Document fixture(bool first_text=false,bool second_text=true) {
    auto document=empty_document("batch-preset-document","composition","artboard");
    RasterAsset asset;asset.id="retained-asset";asset.name="Accepted original source";
    asset.payload=make_raster(encode_raster_png(RasterPixels{1,1,{25,50,75,128}}));
    document.raster_assets.emplace(asset.id,asset);
    for(const auto* id:{"first","second"}) {
        Object object;object.id=id;object.name=id;
        const bool is_text=std::string(id)=="first"?first_text:second_text;
        object.kind=is_text?Kind::text:Kind::path;
        if(is_text)object.text=default_text(std::string(id)+"-text","Retained source");
        else {
            Contour contour;contour.id=std::string(id)+"-contour";contour.closed=true;
            const std::array<Vec2,4> points{{{0,0},{80,0},{80,40},{0,40}}};
            for(std::size_t i=0;i<points.size();++i) {
                Point point;point.id=std::string(id)+"-point-"+std::to_string(i);
                point.x.literal=points[i].x;point.y.literal=points[i].y;contour.points.push_back(point);
            }
            object.contours.push_back(contour);
        }
        object.stack.push_back(default_operation(std::string(id)+"-fill","nect.paint.fill"));
        document.objects.emplace(id,object);document.compositions[0].roots.push_back(id);
    }
    Object group;group.id="group";group.name="Incompatible target";group.kind=Kind::group;
    document.objects.emplace(group.id,group);document.compositions[0].roots.push_back(group.id);
    PresetDefinition preset;preset.id="document-preset";preset.schema_version=2;preset.label="Offset and Stroke";
    auto offset=default_operation("unused-offset","nect.shape.offset");offset.parameters.at("amount").literal=18;
    auto stroke=default_operation("unused-stroke","nect.paint.stroke");stroke.parameters.at("width").literal=3;
    for(const auto& operation:{offset,stroke}) {
        PresetEntry entry;entry.type=operation.type;entry.version=operation.version;
        for(const auto& [name,value]:operation.parameters)entry.parameters[name]=value.literal;
        preset.entries.push_back(entry);
    }
    document.preset_definitions.emplace(preset.id,preset);return document;
}
struct Controls {
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> box;
    std::vector<Id> targets{"first","second"};bool rebuild_on_edit=true;
    Controls():host(directory.path()) {
        host.session=Session(fixture());host.changed=[this]{if(rebuild_on_edit)rebuild();};rebuild();
    }
    ~Controls(){host.changed={};}
    void rebuild(){delete box.data();box=make_preset_batch_controls(host,targets,&parent);}
    void reset(Document document){host.session=Session(std::move(document));rebuild();}
    QComboBox* catalog(){return box->findChild<QComboBox*>("preset-batch-catalog");}
    QPushButton* apply(){return box->findChild<QPushButton*>("preset-batch-apply");}
    QString error(){return box->findChild<QLabel*>("preset-batch-status")->text();}
    void choose(){const auto index=catalog()->findData("document-preset");check(index>0,"Catalog resolves exact document Preset ID");catalog()->setCurrentIndex(index);}
    template<class Action> void refusal(const char* code,Action action) {
        const auto document=host.session.document();const auto bytes=encode(document);
        const auto revision=host.session.revision();const auto history=host.session.history();
        action();
        check(error().startsWith(code),"UI displays exact refusal code");
        check(host.session.document()==document&&encode(host.session.document())==bytes&&
            host.session.revision()==revision&&host.session.history()==history,
            "Refusal preserves complete authored/native snapshot, revision and history");
    }
};
void apply_contract(Controls& c,bool first_text,bool second_text) {
    c.reset(fixture(first_text,second_text));const auto before=c.host.session.document();
    const auto bytes=encode(before);const auto history=c.host.session.history();
    check(c.catalog()->currentIndex()==0&&!c.apply()->isEnabled(),"Catalog starts as an uncommitted draft");
    c.choose();check(c.apply()->isEnabled()&&encode(c.host.session.document())==bytes,"Catalog browse never auto-applies");
    c.box->findChild<QPushButton*>("preset-batch-cancel")->click();
    check(c.catalog()->currentIndex()==0&&!c.apply()->isEnabled()&&encode(c.host.session.document())==bytes,
        "Cancel only clears draft");
    c.choose();QPointer<QWidget> old_box=c.box;c.apply()->click();
    check(!old_box,"Synchronous edited callback can delete controls safely");
    const auto applied=c.host.session.document();
    check(c.host.session.revision()==1&&c.host.session.history().states.size()==history.states.size()+1,
        "All targets are one revision and one history state");
    check(c.host.session.history().states.back().label=="Apply Preset to Selection: Offset and Stroke",
        "History identifies the selection action");
    auto unchanged=applied;std::set<Id> ids;
    for(const auto* id:{"first","second"}) {
        const auto& stack=applied.objects.at(id).stack;
        check(stack.size()==3&&stack[0]==before.objects.at(id).stack[0],"Existing stack is preserved in order");
        check(stack[1].type=="nect.shape.offset"&&stack[1].parameters.at("amount").literal==18&&
            stack[2].type=="nect.paint.stroke"&&stack[2].parameters.at("width").literal==3,
            "Actual Apply sets fixed Preset operations and values on every target");
        for(std::size_t i=1;i<stack.size();++i)check(ids.insert(stack[i].id).second,"Every processing entry has a distinct fresh ID");
        unchanged.objects.at(id).stack=before.objects.at(id).stack;
    }
    check(unchanged==before,"Only appended processing entries change; source and Preset snapshots remain intact");
    check(applied.raster_assets.at("retained-asset").payload==before.raster_assets.at("retained-asset").payload&&
        applied.raster_assets.at("retained-asset").payload->bytes()==before.raster_assets.at("retained-asset").payload->bytes(),
        "Accepted original source bytes and immutable payload identity remain intact");
    check(decode(encode(applied))==applied,"Native roundtrip preserves exact batch IDs and values");
    c.host.session.undo(1);check(c.host.session.document()==before,"One Undo restores every source and distinct stack");
    c.host.session.redo(c.host.session.revision());check(c.host.session.document()==applied,"One Redo restores exact generated IDs");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try {
    Controls c;apply_contract(c,false,false);apply_contract(c,true,true);apply_contract(c,false,true);
    c.reset(fixture());c.choose();c.host.session_id+="-new";
    c.refusal("SESSION_CONFLICT",[&]{c.apply()->click();});
    c.reset(fixture());c.choose();c.host.session.apply({Rename{"first","External edit"}},0);
    c.refusal("REVISION_CONFLICT",[&]{c.apply()->click();});
    c.reset(fixture());c.choose();c.host.session.begin_gesture(0);
    c.refusal("GESTURE_ACTIVE",[&]{c.apply()->click();});c.host.session.cancel_gesture();
    c.refusal("REVISION_CONFLICT",[&]{c.apply()->click();});
    c.reset(fixture());c.choose();auto other=fixture();other.id="other-document";c.host.session=Session(other);
    c.refusal("SESSION_CONFLICT",[&]{c.apply()->click();});
    c.targets={"first","group"};c.reset(fixture());c.refusal("INVALID_DOMAIN",[]{});
    check(!c.catalog()->isEnabled()&&!c.apply()->isEnabled(),"Mixed incompatible selection refuses all targets");
    c.targets={"first","missing"};c.reset(fixture());c.refusal("MISSING_OBJECT",[]{});
    c.targets={"first","second"};c.reset(fixture());c.choose();
    auto missing=fixture();missing.objects.erase("second");
    auto& roots=missing.compositions[0].roots;roots.erase(std::find(roots.begin(),roots.end(),"second"));
    c.host.session=Session(missing);c.refusal("MISSING_OBJECT",[&]{c.apply()->click();});
    c.reset(fixture());c.choose();auto no_preset=fixture();no_preset.preset_definitions.clear();c.host.session=Session(no_preset);
    c.refusal("MISSING_PRESET",[&]{c.apply()->click();});
    auto full=fixture();auto& stack=full.objects.at("second").stack;
    for(int i=1;i<128;++i)stack.push_back(default_operation("existing-fill-"+std::to_string(i),"nect.paint.fill"));
    c.reset(full);c.choose();c.refusal("LIMIT",[&]{c.apply()->click();});
    std::cout<<checks<<" Preset batch UI checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
