#include "window.hpp"
#include "visual_style.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDockWidget>
#include <QJsonDocument>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <cmath>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);++checks;}
void events() {QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* named(QObject& scope,const char* name) {
    for(auto* p:scope.findChildren<T*>(name))if(p->isVisible())return p;
    throw std::runtime_error(std::string("Missing visible control: ")+name);
}
bool same(const Session& a,const Session& b) {
    return a.document()==b.document()&&a.preview_document()==b.preview_document()&&
        encode(a.document())==encode(b.document())&&a.history()==b.history()&&
        a.revision()==b.revision()&&a.gesture_generation()==b.gesture_generation()&&
        a.gesture_active()==b.gesture_active()&&a.can_undo()==b.can_undo()&&a.can_redo()==b.can_redo();
}
Document fixture() {
    Session s(empty_document("primitive-context","comp","art"));
    auto star=default_primitive("star-source","nect.shape.star");
    star.parameters.at("center_x").literal=180;star.parameters.at("center_y").literal=180;
    star.parameters.at("outer_radius").literal=60;star.parameters.at("inner_radius").literal=25;
    star.parameters.at("rotation").literal=0;
    auto polygon=default_primitive("polygon-source","nect.shape.polygon");
    polygon.parameters.at("center_x").literal=440;polygon.parameters.at("center_y").literal=180;
    polygon.parameters.at("radius").literal=60;polygon.parameters.at("rotation").literal=0;
    auto other=default_primitive("other-source","nect.shape.rectangle");
    other.parameters.at("center_x").literal=520;other.parameters.at("center_y").literal=440;
    other.parameters.at("width").literal=30;other.parameters.at("height").literal=24;
    auto red=default_operation("star-fill","nect.paint.fill");red.parameters.at("r").literal=1;
    auto green=default_operation("polygon-fill","nect.paint.fill");green.parameters.at("g").literal=1;
    auto blue=default_operation("other-fill","nect.paint.fill");blue.parameters.at("b").literal=1;
    s.apply({CreatePrimitive{"comp","","star","Retained Star",star},AddOperation{"star",red,0},
        CreatePrimitive{"comp","","polygon","Retained Polygon",polygon},AddOperation{"polygon",green,0},
        CreatePrimitive{"comp","","other","Other artwork",other},AddOperation{"other",blue,0},
        Set{{"star","star-source-outer-1-5","x"},245},
        Set{{"polygon","polygon-source-outer-1-3","x"},390},
        Link{{"other","","generator.width"},{{"star","star-source-outer-1-5","x"},0.1,0,"copy_local_value"}}},0);
    auto d=s.document();auto& art=d.compositions.front().artboards.front();art.width=640;art.height=480;
    auto second=art;second.id="other-art";second.name="Other Artboard";second.x=700;
    d.compositions.front().artboards.push_back(second);return d;
}
void evidence(Window& w,const QString& suffix) {
    const auto prefix=qEnvironmentVariable("NECT_PRIMITIVE_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(w.grab().save(prefix+suffix+".png"),"Window evidence saved");
}
void select(Window& w,const Id& id,const Id& point={}) {
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();
    auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)
        if((*it)->data(0,Qt::UserRole).toString().toStdString()==id&&
            (*it)->data(0,Qt::UserRole+1).toString().toStdString()==point)row=*it;
    check(row,"Exact retained object row");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    check(w.canvas->selected_object==id,"Exact retained object selected");
    check(w.canvas->selected_point==point,"Exact stable generated point selection");
    dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void reveal(Window& w,QWidget* c) {
    auto* area=named<QScrollArea>(w,"inspector-scroll");check(c->isVisible()&&c->isEnabled(),"Actual enabled primitive control");
    area->verticalScrollBar()->setValue(c->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<c->objectName().toStdString()<<" viewport="<<area->viewport()->width()
        <<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()
        <<" hmax="<<area->horizontalScrollBar()->maximum()<<std::endl;
    const bool fits=area->horizontalScrollBar()->maximum()==0&&
        area->viewport()->rect().contains(QRect(c->mapTo(area->viewport(),QPoint()),c->size()))&&c->visibleRegion().contains(c->rect());
    if(!fits) {
        for(auto* x:area->widget()->findChildren<QWidget*>())
            if(x->minimumSizeHint().width()>area->viewport()->width()-60)
                std::cout<<"Minimum owner "<<x->metaObject()->className()<<" "<<x->objectName().toStdString()
                    <<" min="<<x->minimumSizeHint().width()<<std::endl;
        evidence(w,".failure");
    }
    check(fits,"Primitive control reachable with vertical scrolling only");
}
QLineEdit* field(Window& w,const Id& object,const char* name,const Id& point={}) {
    const auto key=QJsonDocument(QJsonObject{{"object",QString::fromStdString(object)},{"point",QString::fromStdString(point)},{"field",name}}).toJson(QJsonDocument::Compact);
    for(auto* p:w.findChildren<QLineEdit*>())if(p->isVisible()&&p->property("nect-reference").toByteArray()==key)return p;
    throw std::runtime_error("Missing visible primitive field");
}
void history(Window& w,const QString& name) {
    for(auto* a:w.findChildren<QAction*>())if(a->text()==name){a->trigger();events();return;}
    throw std::runtime_error("Missing history action");
}
void canonical(Window& w,Session& expected) {
    check(same(w.host.session,expected),"Action equals complete independent Session/native/history/preview/generation");
    history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected),"Undo full equality");
    history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Redo full equality");
}
void number(Window& w,Session& expected,const Id& id,const char* name,const char* text,double value,bool refused=false,const Id& point={}) {
    std::cout<<"Numeric "<<id<<" "<<name<<std::endl;
    auto* c=field(w,id,name,point);reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);
    QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);
    check(same(w.host.session,expected),"Numeric draft fully Session neutral");
    QTest::keyClick(c,Qt::Key_Return);events();
    if(refused) {
        bool rejected=false;
        try {expected.apply({EditProperties{{{id,point,name}},value,false}},expected.revision());}
        catch(const Error& e) {check(e.code=="UNRESOLVED_POINT_EDIT","Intentional topology refusal code");rejected=true;}
        check(rejected&&w.statusBar()->currentMessage().contains("UNRESOLVED_POINT_EDIT"),"GUI reports intentional topology refusal");
        check(same(w.host.session,expected),"Refused Points edit atomically preserves complete Session");
    } else {
        expected.apply({EditProperties{{{id,point,name}},value,false}},expected.revision());canonical(w,expected);
        check(evaluate(w.host.session.document()).at({id,point,name})==value,"Property evaluated exact value");
    }
}
void enabled(Window& w,Session& expected,const Id& id,bool value) {
    auto* c=named<QCheckBox>(w,"point-edit-enabled");reveal(w,c);check(c->isChecked()!=value,"Actual Point Edit transition");
    QTest::mouseClick(c,Qt::LeftButton);events();expected.apply({EnablePointEdit{id,value}},expected.revision());canonical(w,expected);
    check(named<QCheckBox>(w,"point-edit-enabled")->isChecked()==value,"Point Edit control reflects canonical state");
}
void angle(Window& w,Session& expected,const Id& id,const Id& point={},const char* property="generator.rotation",const char* knob_name="primitive-angle-knob") {
    auto* knob=named<QWidget>(w,knob_name);reveal(w,knob);
    const Ref ref{id,point,property};const double initial=evaluate(expected.document()).at(ref);
    const QPoint right(knob->width()/2+15,knob->height()/2),down(knob->width()/2,knob->height()/2+15);
    QTest::mousePress(knob,Qt::LeftButton,Qt::NoModifier,right);expected.begin_gesture(expected.revision());
    check(same(w.host.session,expected),"Angle press equals independent gesture begin");
    QTest::mouseMove(knob,down);events();expected.update_gesture({EditProperties{{ref},initial+90,false}});
    check(same(w.host.session,expected),"Quarter-turn preview equals independent full Session");
    QTest::mouseRelease(knob,Qt::LeftButton,Qt::NoModifier,down);events();expected.commit_gesture();canonical(w,expected);
    check(field(w,id,property,point)->text().toDouble()==initial+90,"Angle and exact numeric value agree");
}
void paint(Window& w,const Session& expected,const QString& suffix) {
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);
    check(image==Canvas::render_artboard(expected.document(),"comp","art",1,false),"Artwork equals complete independent canonical projection");
    check(image.pixelColor(180,180)==QColor(Qt::red)&&image.pixelColor(440,180)==QColor(Qt::green)&&
        image.pixelColor(520,440)==QColor(Qt::blue)&&image.pixelColor(20,20).alpha()==0,"Independent retained/other artwork colors");
    events();const auto canvas=w.canvas->grab().toImage();
    const auto sample=[&](double x,double y) {
        const QPoint p(qRound(w.canvas->width()/2.0+(x-320)*w.canvas->zoom()),qRound(w.canvas->height()/2.0+(y-240)*w.canvas->zoom()));
        return canvas.pixelColor(qRound(p.x()*canvas.devicePixelRatio()),qRound(p.y()*canvas.devicePixelRatio()));
    };
    check(sample(180,180)==QColor(Qt::red)&&sample(440,180)==QColor(Qt::green)&&sample(520,440)==QColor(Qt::blue),"Actual Canvas retains primitive and other artwork colors");
    const auto prefix=qEnvironmentVariable("NECT_PRIMITIVE_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(image.save(prefix+suffix+".artwork.png"),"Artwork evidence saved");evidence(w,suffix);
}
void preserved_corrections(const Session& s) {
    check(evaluate(s.document()).at({"star","star-source-outer-1-5","x"})==245&&
        evaluate(s.document()).at({"polygon","polygon-source-outer-1-3","x"})==390,"Absolute corrections survive source/topology edits");
    check(s.document().objects.at("star").source->id=="star-source"&&
        s.document().objects.at("polygon").source->id=="polygon-source"&&
        s.document().objects.at("other").source->parameters.at("width").binding->source==Ref{"star","star-source-outer-1-5","x"},
        "Generator and linked angular point identities retained");
}
void point_context(const QString& scratch) {
    Window w(scratch+"/point-recovery");w.host.session=Session(fixture());w.host.session_id="primitive-point-context-session";
    w.host.edited();w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;
    const Id point="star-source-outer-1-5";select(w,"star",point);
    check(same(w.host.session,expected),"Generated point selection fully Session neutral");
    const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    const auto source=expected.document().objects.at("star").source;
    const auto correction_id=expected.document().objects.at("star").point_edit->id;
    number(w,expected,"star","x","240",240,false,point);number(w,expected,"star","y","220",220,false,point);
    number(w,expected,"star","in.angle","725.5",725.5,false,point);
    angle(w,expected,"star",point,"in.angle","point-angle-knob-in.angle");
    number(w,expected,"star","in.length","18",18,false,point);
    number(w,expected,"star","out.angle","-450.25",-450.25,false,point);
    angle(w,expected,"star",point,"out.angle","point-angle-knob-out.angle");
    number(w,expected,"star","out.length","22",22,false,point);
    check(expected.document().objects.at("star").source==source&&
        expected.document().objects.at("star").point_edit->id==correction_id&&
        expected.document().objects.at("star").contours.empty(),"Absolute point edits retain full generator/correction identity and geometry ownership");
    check(evaluate(expected.document()).at({"other","","generator.width"})==24,"Existing angular Ref follows corrected coordinate without retargeting");
    enabled(w,expected,"star",false);
    check(evaluate(expected.document()).at({"star",point,"in.length"})==0&&
        expected.document().objects.at("star").point_edit->overrides.at(point).at("in.length").literal==18,"Bypass restores generated handles and retains authored correction");
    enabled(w,expected,"star",true);paint(w,expected,".point");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Point authoring keeps standard pane/Canvas widths");
    const auto native=scratch+"/point-retained.nect";w.host.save(native);check(same(w.host.session,expected),"Point native save fully Session neutral");
    w.host.changed={};w.hide();Window cold(scratch+"/point-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Fresh native full point source/overrides/links/stacks/Artboards readback");
    const Session reopened=cold.host.session;select(cold,"star",point);
    for(const auto* name:{"x","y","in.angle","in.length","out.angle","out.length"}) {
        auto* c=field(cold,"star",name,point);reveal(cold,c);
        check(c->text().toDouble()==evaluate(expected.document()).at({"star",point,name}),"Fresh exact selected-point numeric control readback");
    }
    reveal(cold,named<QWidget>(cold,"point-angle-knob-in.angle"));reveal(cold,named<QWidget>(cold,"point-angle-knob-out.angle"));
    check(same(cold.host.session,reopened),"Fresh point navigation and control readback Session neutral");paint(cold,reopened,".point-cold");cold.host.changed={};cold.hide();
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    QTemporaryDir scratch(qEnvironmentVariable("NECT_PRIMITIVE_CONTEXT_SCRATCH",QDir::tempPath())+"/primitive-context-XXXXXX");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("PrimitiveContext");
    try {
        check(scratch.isValid(),"Owned scratch");
        if(app.arguments().contains("--point-context")) {
            point_context(scratch.path());
            std::cout<<"PASS "<<checks<<" standard-pane selected-point contextual authoring; physical/subjective input NOT_RUN\n";return 0;
        }
        Window w(scratch.filePath("recovery"));
        w.host.session=Session(fixture());w.host.session_id="primitive-context-session";w.host.edited();
        w.show();events();w.canvas->fit_artboard();events();Session expected=w.host.session;
        select(w,"star");check(same(w.host.session,expected),"Selection fully Session neutral");
        const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
        number(w,expected,"star","generator.outer_radius","75",75);
        number(w,expected,"star","generator.inner_radius","32",32);
        number(w,expected,"star","generator.rotation","720.125",720.125);angle(w,expected,"star");
        number(w,expected,"star","generator.points","10",10);preserved_corrections(expected);
        number(w,expected,"star","generator.points","6",6,true);
        enabled(w,expected,"star",false);check(evaluate(expected.document()).at({"star","star-source-outer-1-5","x"})!=245,"Bypass shows current generator fallback");
        number(w,expected,"star","generator.outer_radius","82",82);
        check(!expected.document().objects.at("star").point_edit->enabled&&
            expected.document().objects.at("star").point_edit->overrides.at("star-source-outer-1-5").at("x").literal==245,"Source edit while bypassed retains absolute correction");
        enabled(w,expected,"star",true);preserved_corrections(expected);paint(w,expected,".star");
        select(w,"polygon");check(same(w.host.session,expected),"Other shape selection Session neutral");
        number(w,expected,"polygon","generator.radius","72",72);
        number(w,expected,"polygon","generator.rotation","-360.25",-360.25);angle(w,expected,"polygon");
        number(w,expected,"polygon","generator.points","12",12);number(w,expected,"polygon","generator.points","5",5,true);
        enabled(w,expected,"polygon",false);number(w,expected,"polygon","generator.radius","80",80);
        enabled(w,expected,"polygon",true);preserved_corrections(expected);paint(w,expected,".polygon");
        check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Standard pane/Canvas widths remain fixed");
        const auto native=scratch.filePath("retained.nect");w.host.save(native);check(same(w.host.session,expected),"Native save fully Session neutral");
        w.host.changed={};w.hide();Window cold(scratch.filePath("cold"));cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
        check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Fresh native Window full source/correction/link/stack/Artboard order readback");
        const Session reopened=cold.host.session;
        for(const auto& id:{"star","polygon"}) {
            select(cold,id);auto* c=field(cold,id,"generator.rotation");reveal(cold,c);
            check(c->text().toDouble()==evaluate(expected.document()).at({id,"","generator.rotation"}),"Fresh exact rotation control readback");
            reveal(cold,named<QWidget>(cold,"primitive-angle-knob"));reveal(cold,named<QCheckBox>(cold,"point-edit-enabled"));
            check(named<QCheckBox>(cold,"point-edit-enabled")->isChecked(),"Fresh Point Edit authored enable readback");
        }
        check(same(cold.host.session,reopened),"Fresh navigation and control readback fully Session neutral");paint(cold,reopened,".cold");cold.host.changed={};cold.hide();
        std::cout<<"PASS "<<checks<<" standard-pane retained primitive contextual authoring; physical/subjective input NOT_RUN\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
