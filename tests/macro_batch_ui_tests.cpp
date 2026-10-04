#include "macro_batch_control.hpp"
#include "host.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
MacroDefinition definition() {
    // Canonical fixed-pair graph used by macro_tests, with retained numeric/
    // boolean public interfaces as in macro_boolean_interface_tests.
    MacroDefinitionRevision first;first.input={"input","local_paths_and_paint"};
    first.output={"output","local_paths_and_paint"};
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=5;
    auto repeat=default_operation("repeat","nect.shape.repeater");
    repeat.parameters.at("copies").literal=2;repeat.parameters.at("position_x").literal=125;
    first.nodes={{offset,"offset-in","offset-out"},{repeat,"repeat-in","repeat-out"}};
    first.edges={{{"repeat","repeat-out"},{"","output"}},{{"","input"},{"offset","offset-in"}},
        {{"offset","offset-out"},{"repeat","repeat-in"}}};
    first.output_mapping={"repeat","repeat-out"};
    first.public_parameters={{"macro.offset.amount","Amount","offset","amount","number","du","local_paths_and_paint"}};
    auto second=first;second.revision=2;second.interface_version=3;
    second.public_parameters.push_back({"macro.copies","Copies","repeat","copies","number","scalar","local_paths_and_paint"});
    second.public_parameters.push_back({"macro.offset.enabled","Use Offset","offset","enabled","boolean","boolean","local_paths_and_paint"});
    second.nodes[0].operation.parameters.at("amount").literal=18;
    second.nodes[1].operation.parameters.at("copies").literal=3;
    MacroDefinition result;result.id="document-macro";result.label="Offset Repeat";result.latest_revision=2;
    result.revisions={{1,std::move(first)},{2,std::move(second)}};return result;
}
Document fixture(bool first_text=false,bool second_text=false) {
    auto document=empty_document("batch-macro-document","composition","artboard");
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
    const auto source=definition();document.macro_definitions.emplace(source.id,source);return document;
}
bool near(Vec2 a,Vec2 b){return std::abs(a.x-b.x)<1e-7&&std::abs(a.y-b.y)<1e-7;}
bool same_paths(const std::vector<PathInstance>& a,const std::vector<PathInstance>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].contours->size()!=b[i].contours->size())return false;
        for(std::size_t c=0;c<a[i].contours->size();++c) {
            const auto& x=(*a[i].contours)[c];const auto& y=(*b[i].contours)[c];
            if(x.closed!=y.closed||x.points.size()!=y.points.size())return false;
            for(std::size_t p=0;p<x.points.size();++p) {
                const auto& xp=x.points[p];const auto& yp=y.points[p];
                if(!near(map_point(a[i].transform,xp.anchor),map_point(b[i].transform,yp.anchor))||
                    !near(map_point(a[i].transform,xp.incoming),map_point(b[i].transform,yp.incoming))||
                    !near(map_point(a[i].transform,xp.outgoing),map_point(b[i].transform,yp.outgoing)))return false;
            }
        }
    }
    return true;
}
void same_shape(const EvaluatedShape& actual,const EvaluatedShape& expected) {
    check(same_paths(actual.paths,expected.paths),"Macro evaluated cubic output equals the explicit pinned operator sequence");
    check(actual.paints.size()==expected.paints.size(),"Pinned Macro preserves expected paint count");
    for(std::size_t i=0;i<actual.paints.size();++i) {
        const auto& a=actual.paints[i];const auto& b=expected.paints[i];
        check(a.type==b.type&&a.rgba==b.rgba&&a.fill_rule==b.fill_rule&&a.transform==b.transform&&same_paths(a.paths,b.paths),
            "Pinned Macro paint geometry equals the ordinary operator oracle");
    }
}
struct Controls {
    QTemporaryDir directory;Host host;QWidget parent;QPointer<QWidget> box;
    std::vector<Id> targets{"first","second"};
    Controls():host(directory.path()) {
        host.session=Session(fixture());host.changed=[this]{rebuild();};rebuild();
    }
    ~Controls(){host.changed={};}
    void rebuild(){delete box.data();box=make_macro_batch_controls(host,targets,&parent);}
    void reset(Document document){host.session=Session(std::move(document));rebuild();}
    QComboBox* catalog(){return box->findChild<QComboBox*>("macro-batch-catalog");}
    QComboBox* pins(){return box->findChild<QComboBox*>("macro-batch-revision");}
    QPushButton* apply(){return box->findChild<QPushButton*>("macro-batch-apply");}
    QString error(){return box->findChild<QLabel*>("macro-batch-status")->text();}
    void choose(std::uint64_t pin=2){
        const auto index=catalog()->findData("document-macro");check(index>0,"Catalog resolves exact stable Definition ID");
        catalog()->setCurrentIndex(index);
        const auto selected=pins()->findData(QVariant::fromValue(static_cast<qulonglong>(pin)));
        check(selected>=0,"Chooser retains exact old and latest revision pins");pins()->setCurrentIndex(selected);
    }
    template<class Action> void refusal(const char* code,Action action) {
        const auto document=host.session.document();const auto bytes=encode(document);
        const auto revision=host.session.revision();const auto history=host.session.history();action();
        check(error().startsWith(code),"UI displays exact refusal code");
        check(host.session.document()==document&&encode(host.session.document())==bytes&&
            host.session.revision()==revision&&host.session.history()==history,
            "Refusal preserves authored/native snapshot, revision and full history");
    }
};
void apply_contract(Controls& c,bool first_text,bool second_text,std::uint64_t pin) {
    c.reset(fixture(first_text,second_text));const auto before=c.host.session.document();
    const auto bytes=encode(before);const auto history=c.host.session.history();
    check(c.catalog()->currentIndex()==0&&!c.apply()->isEnabled(),"Definition browsing starts as an uncommitted draft");
    c.catalog()->setCurrentIndex(c.catalog()->findData("document-macro"));
    check(c.pins()->currentData().toULongLong()==2,"New draft explicitly defaults to latest retained pin");
    c.choose(pin);check(c.apply()->isEnabled()&&encode(c.host.session.document())==bytes,"Browsing revisions never applies");
    check(c.catalog()->currentText().contains("document-macro"),"Definition label disambiguates stable ID");
    c.box->findChild<QPushButton*>("macro-batch-cancel")->click();
    check(c.catalog()->currentIndex()==0&&c.pins()->count()==0&&!c.apply()->isEnabled()&&
        encode(c.host.session.document())==bytes&&c.host.session.history()==history,"Cancel clears only draft state");
    c.choose(pin);QPointer<QWidget> old_box=c.box;c.apply()->click();
    check(!old_box,"edited callback may synchronously delete Macro controls");
    const auto applied=c.host.session.document();
    check(c.host.session.revision()==1&&c.host.session.history().states.size()==history.states.size()+1,
        "All targets append in one revision and one Undo state");
    auto unchanged=applied;std::set<Id> ids;auto ordinary=before;
    for(const auto* id:{"first","second"}) {
        const auto& stack=applied.objects.at(id).stack;
        check(stack.size()==2&&stack[0]==before.objects.at(id).stack[0],"Existing stack stays ordered and intact");
        const auto& entry=stack.back();check(entry.macro&&entry.macro->definition=="document-macro"&&
            entry.macro->pinned_revision==pin&&entry.macro->overrides.empty()&&entry.macro->boolean_overrides.empty(),
            "Every target receives the exact pin with fresh default numeric/boolean interface");
        check(ids.insert(entry.id).second&&!before.objects.contains(entry.id)&&entry.id!=stack[0].id,
            "Every appended instance has a distinct fresh identity");
        check(macro_parameter_value(applied,id,entry.id,"macro.offset.amount")== (pin==1?5:18),
            "Exact pinned Amount reads back on both targets");
        if(pin==2)check(macro_parameter_value(applied,id,entry.id,"macro.copies")==3&&
            macro_parameter_boolean_value(applied,id,entry.id,"macro.offset.enabled"),
            "Published Copies and boolean enabled defaults use the retained newer interface");
        for(const auto* node:macro_execution_order(before.macro_definitions.at("document-macro").revisions.at(pin))) {
            auto operation=node->operation;operation.id=std::string(id)+"-oracle-"+operation.id;
            ordinary.objects.at(id).stack.push_back(operation);
        }
        unchanged.objects.at(id).stack=before.objects.at(id).stack;
    }
    check(unchanged==before,"Only appended stack entries change; all sources and retained Definition revisions stay exact");
    const auto actual_values=evaluate(applied);const auto expected_values=evaluate(ordinary);
    for(const auto* id:{"first","second"}) {
        const auto actual=evaluate_shape(applied,id,actual_values);
        check(actual.paths.size()==(pin==1?2u:3u)&&!actual.paths.front().contours->empty(),
            "Pinned old/new Copies produce two/three nonempty Path or Text output instances");
        same_shape(actual,evaluate_shape(ordinary,id,expected_values));
    }
    check(decode(encode(applied))==applied,"Native roundtrip preserves all instance IDs and exact old/latest pins");
    c.host.session.undo(1);check(c.host.session.document()==before,"One Undo restores every target and source");
    c.host.session.redo(c.host.session.revision());check(c.host.session.document()==applied,"One Redo restores exact fresh IDs");
    c.rebuild();c.choose(pin);c.apply()->click();
    check(c.host.session.document().objects.at("first").stack.size()==3&&c.host.session.document().objects.at("second").stack.size()==3,
        "Applying the same Macro again intentionally appends another instance to each target");
    for(const auto* id:{"first","second"})check(ids.insert(c.host.session.document().objects.at(id).stack.back().id).second,
        "Repeated Apply allocates fresh identities instead of reusing or replacing instances");
}
}
int main(int argc,char** argv){
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);try {
        Controls c;apply_contract(c,false,false,1);apply_contract(c,false,false,2);
        apply_contract(c,true,true,1);apply_contract(c,true,true,2);apply_contract(c,false,true,2);
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
        check(!c.apply()->isEnabled(),"Incompatible second target refuses the entire batch");
        c.targets={"first","missing"};c.reset(fixture());c.refusal("MISSING_OBJECT",[]{});
        c.targets={"first","first"};c.reset(fixture());c.refusal("INVALID_SELECTION",[]{});
        c.targets={"first","second"};c.reset(fixture());c.choose();
        auto missing=fixture();missing.objects.erase("second");
        auto& roots=missing.compositions[0].roots;roots.erase(std::find(roots.begin(),roots.end(),"second"));
        c.host.session=Session(missing);c.refusal("MISSING_OBJECT",[&]{c.apply()->click();});
        c.reset(fixture());c.choose();auto no_macro=fixture();no_macro.macro_definitions.clear();c.host.session=Session(no_macro);
        c.refusal("MISSING_MACRO_DEFINITION",[&]{c.apply()->click();});
        c.reset(fixture());c.choose(1);auto no_pin=fixture();no_pin.macro_definitions.at("document-macro").revisions.erase(1);
        c.host.session=Session(no_pin);c.refusal("MISSING_MACRO_REVISION",[&]{c.apply()->click();});
        c.reset(fixture());c.choose();c.pins()->addItem("Unavailable revision",QVariant::fromValue(qulonglong{999}));
        c.pins()->setCurrentIndex(c.pins()->count()-1);c.refusal("MISSING_MACRO_REVISION",[]{});
        check(!c.apply()->isEnabled(),"Unknown revision is unavailable without a fallback");
        auto full=fixture();auto& stack=full.objects.at("second").stack;
        for(int i=1;i<128;++i)stack.push_back(default_operation("existing-fill-"+std::to_string(i),"nect.paint.fill"));
        c.reset(full);c.choose();c.refusal("LIMIT",[&]{c.apply()->click();});
        // Unsupported definitions cannot enter a valid Session at all. The same
        // validator is used by the UI before any batch command is admitted.
        c.reset(fixture());const auto before=c.host.session.document();const auto history=c.host.session.history();
        auto unsupported=fixture();unsupported.macro_definitions.at("document-macro").revisions.at(2).graph_version=99;
        bool rejected=false;try {Session invalid(unsupported);}catch(const Error& error){rejected=error.code=="UNSUPPORTED_MACRO_GRAPH_VERSION";}
        check(rejected&&c.host.session.document()==before&&c.host.session.history()==history&&c.host.session.revision()==0,
            "Unsupported Definition is canonically rejected without altering the live targets or history");
        std::cout<<checks<<" Macro batch UI checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
