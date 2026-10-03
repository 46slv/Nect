#include "window.hpp"
#include "semantic_control.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
const Ref amount=operation_ref("built","offset","amount");
const Ref angle=operation_ref("built","repeater","rotation");
const Ref public_amount=macro_parameter_ref("macro","instance","macro.offset.amount");
constexpr double exact=12.34567890123456,turns=-725.1234567890123;
struct Snapshot {
    Document doc;std::uint64_t revision;HistoryInfo history;std::string native;
    explicit Snapshot(const Session& s):doc(s.document()),revision(s.revision()),history(s.history()),native(encode(s.document())){}
    bool unchanged(const Session& s)const{return doc==s.document()&&revision==s.revision()&&history==s.history()&&native==encode(s.document());}
};
template<class F>void invalid(F f){try{f();}catch(const Error& e){check(e.code=="INVALID_CONTROL_DESCRIPTOR","Invalid metadata is refused explicitly");return;}throw std::runtime_error("Descriptor accepted invalid metadata");}
Document fixture() {
    Session s(empty_document("semantic-document","composition","artboard"));
    auto source=default_primitive("source-built","nect.shape.rectangle");
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=exact;
    auto repeater=default_operation("repeater","nect.shape.repeater");repeater.parameters.at("rotation").literal=turns;
    auto macro_offset=offset;macro_offset.id="node-offset";
    auto macro_repeater=repeater;macro_repeater.id="node-repeater";
    MacroDefinitionRevision r;r.revision=1;r.input={"input","local_paths_and_paint"};r.output={"output","local_paths_and_paint"};
    r.nodes={{macro_offset,"offset-in","offset-out"},{macro_repeater,"repeater-in","repeater-out"}};
    r.edges={{{"","input"},{"node-offset","offset-in"}},{{"node-offset","offset-out"},{"node-repeater","repeater-in"}},{{"node-repeater","repeater-out"},{"","output"}}};
    r.output_mapping={"node-repeater","repeater-out"};
    r.public_parameters.push_back({"macro.offset.amount","Expansion distance","node-offset","amount","number","du","local_paths_and_paint"});
    MacroDefinition definition;definition.id="definition";definition.label="Offset Repeat";definition.latest_revision=1;definition.revisions.emplace(1,r);
    auto source_macro=source;source_macro.id="source-macro";
    auto source_second=source;source_second.id="source-second";
    auto second_offset=offset;second_offset.id="second-offset";second_offset.parameters.at("amount").literal=exact+5;
    auto second_repeater=repeater;second_repeater.id="second-repeater";second_repeater.parameters.at("rotation").literal=turns+15;
    s.apply({CreatePrimitive{"composition","","built","Built",source},AddOperation{"built",offset,1},AddOperation{"built",repeater,2},
        CreatePrimitive{"composition","","macro","Macro",source_macro},MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"macro","definition","instance",1,1}},
        CreatePrimitive{"composition","","second","Second",source_second},AddOperation{"second",second_offset,1},AddOperation{"second",second_repeater,2}},s.revision());
    return s.document();
}
QByteArray reference(const Ref& r){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},
    {"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);}
QLineEdit* field(Window& w,const Ref& ref){for(auto* input:w.findChildren<QLineEdit*>())if(input->isVisible()&&input->property("nect-reference").toByteArray()==reference(ref))return input;throw std::runtime_error("Generated field missing");}
SemanticScrub* scrub(Window& w,const Ref& ref){for(auto* control:w.findChildren<QToolButton*>())if(auto* result=dynamic_cast<SemanticScrub*>(control);result&&result->isVisible()&&result->property("nect-reference").toByteArray()==reference(ref))return result;throw std::runtime_error("Generated scrub missing");}
void focus(Window& w,QWidget* control){w.findChild<QScrollArea*>("inspector-scroll")->ensureWidgetVisible(control);control->setFocus(Qt::OtherFocusReason);events();}
void load(Window& w,const Id& selected="built"){w.host.session=Session(fixture());w.host.session_id+="-new";w.host.edited();w.canvas->set_selection(selected);events();}
void type(Window& w,const Ref& ref,const QString& text){auto* input=field(w,ref);focus(w,input);input->selectAll();QTest::keyClick(input,Qt::Key_Backspace);QTest::keyClicks(input,text);}
void edit(Window& w,const Ref& ref,const QString& text){type(w,ref,text);QTest::keyClick(field(w,ref),Qt::Key_Return);events();}
void mouse(QWidget* control,QEvent::Type type,double x,Qt::MouseButton button,Qt::MouseButtons buttons){
    const QPointF point(x,12);QMouseEvent event(type,point,QPointF(control->mapToGlobal(QPoint{}))+point,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(control,&event);events();
}
void save_reopen(const Document& d,const Ref& ref,double value){
    QTemporaryDir files;Host writer(files.path()+"/writer");writer.session=Session(d);const auto path=files.path()+"/semantic.nect";writer.save(path);writer.flush();
    Host cold(files.path()+"/cold");cold.open(path);
    check(cold.session.document()==d&&encode(cold.session.document())==encode(d),"Cold native save/reopen preserves all canonical values and stable identities");
    const auto result=ref.point.empty()?evaluate(cold.session.document()).at(ref):macro_parameter_value(cold.session.document(),ref.object,ref.point,ref.field);
    check(result==value,"Cold readback retains the exact authored number");
    const auto response=QJsonDocument::fromJson(QByteArray::fromStdString(request(cold.session,R"({"op":"properties"})"))).object();
    check(response.value("ok").toBool(),"Cold canonical properties API succeeds");
    bool found=false;
    for(const auto& entry:response.value("result").toArray()) {
        const auto property=entry.toObject();
        if(QJsonDocument(property.value("ref").toObject()).toJson(QJsonDocument::Compact)!=reference(ref))continue;
        found=true;check(property.value("evaluated").toDouble()==value,"Cold canonical API returns the exact edited numeric/angle/Macro value");
    }
    check(found,"Cold API retains the same stable property Ref");
}
void descriptor_contract(){
    auto n=*builtin_semantic_descriptor("nect.shape.offset","amount");auto a=*builtin_semantic_descriptor("nect.shape.repeater","rotation");
    check(n.default_value==builtin_operation_type("nect.shape.offset")->parameter_defaults.at("amount")&&n.unit=="du","Numeric metadata comes from the real built-in descriptor");
    check(validate_semantic_descriptor(a).widget==SemanticWidget::angle&&a.unit=="degree","Rotation selects the angle family by validated metadata");
    check(!builtin_semantic_descriptor("nect.paint.fill","r"),"Other widget families remain explicit successors");
    auto bad=n;bad.value_type="mystery";invalid([&]{validate_semantic_descriptor(bad);});
    bad=n;bad.widget_hint="toggle";invalid([&]{validate_semantic_descriptor(bad);});
    bad=n;bad.widget_hint="angle";invalid([&]{validate_semantic_descriptor(bad);});
    bad=a;bad.unit="du";invalid([&]{validate_semantic_descriptor(bad);});
    bad=n;bad.minimum=2;bad.maximum=1;invalid([&]{validate_semantic_descriptor(bad);});
    bad=n;bad.step=0;invalid([&]{validate_semantic_descriptor(bad);});
    bad=n;bad.default_value=INFINITY;invalid([&]{validate_semantic_descriptor(bad);});
    bad=n;bad.widget_hint="future-number-v2";const auto resolution=validate_semantic_descriptor(bad);
    check(resolution.fallback&&resolution.widget==SemanticWidget::numeric&&resolution.status.find("UNKNOWN_WIDGET_HINT")!=std::string::npos,"Unknown hint reports same-known-type numeric fallback");
    auto* fallback=semantic_number_input(bad,"12");check(fallback->property("nect-control-fallback").toBool()&&fallback->toolTip().contains("UNKNOWN_WIDGET_HINT"),"Generated fallback exposes its status");delete fallback;
    bad.value_type="mystery";invalid([&]{semantic_number_input(bad,"12");});
    const auto d=fixture();const auto m=macro_semantic_descriptor(d,public_amount);
    check(m.key==public_amount.field&&m.label=="Expansion distance"&&m.unit==n.unit&&m.widget_hint==n.widget_hint&&m.minimum==n.minimum&&m.maximum==n.maximum&&m.step==n.step,"Macro projects target semantics while keeping PublicParamID and renamed label");
    auto corrupt=d;corrupt.macro_definitions.at("definition").revisions.at(1).public_parameters.front().unit="degree";
    invalid([&]{macro_semantic_descriptor(corrupt,public_amount);});
}
void direct_contract(Window& w,const Ref& ref){
    load(w,ref.object);auto& s=w.host.session;const Snapshot before(s);auto* input=field(w,ref);
    check(dynamic_cast<SemanticNumberInput*>(input)&&input->property("nect-control-status")=="SUPPORTED"&&input->text()==QString::number(exact,'g',17),"Built-in and Macro use the same generated exact numeric field");
    focus(w,input);input->clearFocus();events();check(before.unchanged(s),"Focus/blur is byte-exact and adds no history");
    edit(w,ref,QString::number(exact,'g',17));check(before.unchanged(s),"Retyping the exact value is one canonical no-op");
    type(w,ref,"98.76543210987654");QTest::keyClick(field(w,ref),Qt::Key_Escape);events();field(w,ref)->clearFocus();events();
    check(before.unchanged(s)&&field(w,ref)->text()==QString::number(exact,'g',17),"Escape cancels the direct draft before blur");
    for(const auto& text:{QString("nan"),QString("1e309"),QString("1e-999"),QString("1000000.0000001")}){
        edit(w,ref,text);check(before.unchanged(s)&&!w.statusBar()->currentMessage().isEmpty(),"Invalid finite/range draft is atomic and explicit");
    }
    constexpr double next=23.4567890123456;edit(w,ref,QString::number(next,'g',17));
    Session expected(before.doc);
    if(ref.point.empty())expected.apply({EditProperties{{ref},next,false}},0);
    else expected.apply({MacroCommand{SetMacroOverride{ref.object,ref.point,ref.field,next}}},0);
    check(s.document()==expected.document()&&s.revision()==before.revision+1&&s.history()==expected.history(),"Direct generated edit equals one canonical transaction");
    const auto after=s.document();s.undo(s.revision());w.host.edited();events();check(s.document()==before.doc,"One Undo restores the exact source");
    s.redo(s.revision());w.host.edited();events();check(s.document()==after,"One Redo restores the generated edit");save_reopen(after,ref,next);
}
void scrub_contract(Window& w,const Ref& ref){
    load(w,ref.object);auto& s=w.host.session;const Snapshot before(s);auto* control=scrub(w,ref);
    mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseMove,13.25,Qt::NoButton,Qt::LeftButton);
    check(s.gesture_active()&&before.unchanged(s)&&field(w,ref)->text()==QString::number(exact+1.25,'g',17),"Scrub previews the exact shared value without authored mutation");
    QTest::keyClick(control,Qt::Key_Escape);events();check(!s.gesture_active()&&before.unchanged(s),"Escape cancels the owned scrub transaction");
    mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseMove,16.125,Qt::NoButton,Qt::LeftButton);mouse(control,QEvent::MouseButtonRelease,16.125,Qt::LeftButton,Qt::NoButton);
    const double next=exact+4.125;Session expected(before.doc);
    if(ref.point.empty())expected.apply({EditProperties{{ref},next,false}},0);else expected.apply({MacroCommand{SetMacroOverride{ref.object,ref.point,ref.field,next}}},0);
    check(s.document()==expected.document()&&s.history()==expected.history()&&s.revision()==before.revision+1,"One scrub commits one canonical Undo transaction for either owner");
    const auto after=s.document();s.undo(s.revision());w.host.edited();events();check(s.document()==before.doc,"Scrub Undo restores exact source and override presence");
    s.redo(s.revision());w.host.edited();events();check(s.document()==after,"Scrub Redo restores exact result");save_reopen(after,ref,next);
    control=scrub(w,ref);const Snapshot dirty(s);type(w,ref,"111");mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);
    check(dirty.unchanged(s)&&!s.gesture_active()&&field(w,ref)->isModified()&&w.statusBar()->currentMessage().contains("UNCOMMITTED_INPUT"),"Scrub refuses an uncommitted numeric draft");
    QTest::keyClick(field(w,ref),Qt::Key_Escape);events();
    const auto retained=scrub(w,ref);s.apply({Rename{ref.object,"Renamed"}},s.revision());const Snapshot stale(s);
    mouse(retained,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);check(stale.unchanged(s)&&w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Stale scrub refuses atomically");
}
void scrub_guards(Window& w,const Ref& ref){
    load(w,ref.object);auto& s=w.host.session;const Snapshot original(s);auto* control=scrub(w,ref);
    mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseButtonRelease,12,Qt::LeftButton,Qt::NoButton);
    check(original.unchanged(s),"A no-motion scrub creates neither history nor a Macro override");
    control=scrub(w,ref);s.begin_gesture(s.revision());const auto foreign=s.gesture_generation();
    mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);
    check(s.gesture_active()&&s.gesture_generation()==foreign&&original.unchanged(s),"Scrub refuses and preserves another owner's active Session gesture");s.cancel_gesture();
    control=scrub(w,ref);mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseMove,14,Qt::NoButton,Qt::LeftButton);
    s.cancel_gesture();s.begin_gesture(s.revision());const auto replacement=s.gesture_generation();
    mouse(control,QEvent::MouseButtonRelease,14,Qt::LeftButton,Qt::NoButton);
    check(s.gesture_active()&&s.gesture_generation()==replacement&&original.unchanged(s),"Old scrub cannot commit or cancel a replacement gesture at the same revision");s.cancel_gesture();
    load(w,ref.object);control=scrub(w,ref);mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseMove,14,Qt::NoButton,Qt::LeftButton);
    w.refresh(false);events();check(!s.gesture_active()&&s.document()==original.doc,"Inspector rebuild cancels its owned scrub preview synchronously");
    load(w,ref.object);edit(w,ref,"999999.75");const Snapshot range(s);control=scrub(w,ref);
    mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseMove,12.125,Qt::NoButton,Qt::LeftButton);
    mouse(control,QEvent::MouseMove,13,Qt::NoButton,Qt::LeftButton);
    check(range.unchanged(s)&&s.gesture_active()&&field(w,ref)->text()=="999999.875"&&w.statusBar()->currentMessage().contains("OUT_OF_RANGE"),"Out-of-range sample retains the last valid exact preview");
    mouse(control,QEvent::MouseButtonRelease,13,Qt::LeftButton,Qt::NoButton);
    const auto value=ref.point.empty()?evaluate(s.document()).at(ref):macro_parameter_value(s.document(),ref.object,ref.point,ref.field);
    check(value==999999.875&&s.revision()==range.revision+1,"Range refusal commits only the valid sample in one transaction");
    load(w,ref.object);control=scrub(w,ref);s=Session(fixture());w.host.session_id+="-replacement";const Snapshot swapped(s);
    mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);
    check(swapped.unchanged(s)&&w.statusBar()->currentMessage().contains("SESSION_CONFLICT"),"Retained scrub cannot edit a replacement Session with the same IDs");w.host.edited();events();
    load(w,ref.object);control=scrub(w,ref);focus(w,control);const Snapshot keyboard(s);QTest::keyClick(control,Qt::Key_Right);events();
    const auto stepped=ref.point.empty()?evaluate(s.document()).at(ref):macro_parameter_value(s.document(),ref.object,ref.point,ref.field);
    check(stepped==exact+1&&s.revision()==keyboard.revision+1,"Keyboard scrub commits the declared step as one canonical edit");
}
Bounds bounds(const Document& d,const Id& id){auto shape=evaluate_shape(d,id,evaluate(d));Bounds result{INFINITY,INFINITY,-INFINITY,-INFINITY};
    for(const auto& path:shape.paths)for(const auto& contour:*path.contours)for(const auto& point:contour.points){auto p=map_point(path.transform,point.anchor);result.left=std::min(result.left,p.x);result.top=std::min(result.top,p.y);result.right=std::max(result.right,p.x);result.bottom=std::max(result.bottom,p.y);}return result;}
bool same_bounds(const Bounds& a,const Bounds& b){return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom;}
void parity_and_angle(Window& w){
    load(w);auto& s=w.host.session;const auto original_bounds=bounds(s.document(),"built");
    edit(w,amount,"25.1234567890123");w.canvas->set_selection("macro");events();edit(w,public_amount,"25.1234567890123");
    check(same_bounds(bounds(s.document(),"built"),bounds(s.document(),"macro"))&&!same_bounds(bounds(s.document(),"built"),original_bounds),"Generated built-in/Macro amount edits produce identical real evaluated geometry");
    w.canvas->set_selection("built");events();auto* input=field(w,angle);
    check(dynamic_cast<SemanticNumberInput*>(input)&&input->property("nect-widget-hint")=="angle"&&input->text()==QString::number(turns,'g',17),"Angle exact field retains signed turns and precision");
    auto* dial=w.findChild<QWidget*>("repeater-angle-knob-repeater");check(dial&&dial->property("nect-widget-hint")=="angle","Metadata generates the real angle dial");
    const Snapshot before(s);type(w,angle,"987.1234567890123");QTest::keyClick(field(w,angle),Qt::Key_Escape);events();check(before.unchanged(s),"Angle numeric draft cancellation preserves canonical values");
    edit(w,angle,"-1080.1234567890123");check(evaluate(s.document()).at(angle)==-1080.1234567890123,"Angle numeric field authors exact signed degrees");save_reopen(s.document(),angle,-1080.1234567890123);
}
void mixed_and_driven(Window& w){
    load(w);auto& s=w.host.session;w.canvas->set_selections({{"built",""},{"second",""}});events();
    auto* input=field(w,amount);check(input->text().isEmpty()&&input->placeholderText()=="Mixed"&&input->property("nect-mixed").toBool(),"Generated multi-selection preserves mixed state without averaging");
    const Snapshot before(s);type(w,amount,"100");QTest::keyClick(field(w,amount),Qt::Key_Escape);events();check(before.unchanged(s)&&field(w,amount)->text().isEmpty(),"Mixed draft Cancel preserves all differences");
    auto* control=scrub(w,amount);mouse(control,QEvent::MouseButtonPress,12,Qt::LeftButton,Qt::LeftButton);mouse(control,QEvent::MouseMove,14.5,Qt::NoButton,Qt::LeftButton);mouse(control,QEvent::MouseButtonRelease,14.5,Qt::LeftButton,Qt::NoButton);
    const auto second=operation_ref("second","second-offset","amount");
    check(evaluate(s.document()).at(amount)==exact+2.5&&evaluate(s.document()).at(second)==exact+7.5&&s.revision()==before.revision+1&&field(w,amount)->text().isEmpty(),"Mixed scrub preserves each target difference in one transaction");
    s.undo(s.revision());w.host.edited();events();check(s.document()==before.doc,"One mixed scrub Undo restores every target");
    for(bool expression:{false,true}) {
        load(w);if(expression)s.apply({SetExpression{{amount},Expression{"15",1}}},s.revision());
        else s.apply({LinkProperties{{amount},operation_ref("second","second-offset","amount"),false}},s.revision());
        w.host.edited();events();const Snapshot driven(s);check(field(w,amount)->isReadOnly()&&!scrub(w,amount)->isEnabled(),"Driven generated field is visibly read-only and scrub is disabled");
        edit(w,amount,"22");check(driven.unchanged(s),"Generated direct input cannot silently remove a link or expression");
    }
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
    descriptor_contract();QTemporaryDir dir;QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
    Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.resize(1200,900);w.show();events();
    direct_contract(w,amount);direct_contract(w,public_amount);scrub_contract(w,amount);scrub_contract(w,public_amount);
    scrub_guards(w,amount);scrub_guards(w,public_amount);parity_and_angle(w);mixed_and_driven(w);
    if(!qEnvironmentVariableIsEmpty("NECT_SEMANTIC_RECEIPT_DIR")) {
        load(w);focus(w,field(w,angle));events();
        check(w.grab().save(qEnvironmentVariable("NECT_SEMANTIC_RECEIPT_DIR")+"/semantic-inspector.png"),"Source-rendered Qt Inspector receipt saved");
    }
    std::cout<<"PASS "<<checks<<" semantic numeric/angle Qt contract checks (physical OS input NOT_RUN)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
