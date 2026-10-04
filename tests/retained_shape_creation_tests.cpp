#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);QApplication::processEvents();}
bool reachable(QWidget* w){return w&&w->isVisible()&&w->visibleRegion().contains(w->rect());}
struct Snapshot {
    Document document,preview;
    std::string native;
    std::uint64_t revision,generation;
    HistoryInfo history;
    bool undo,redo,gesture;
    explicit Snapshot(const Session& s):document(s.document()),preview(s.preview_document()),native(encode(document)),
        revision(s.revision()),generation(s.gesture_generation()),history(s.history()),
        undo(s.can_undo()),redo(s.can_redo()),gesture(s.gesture_active()){}
    void unchanged(const Session& s)const{
        check(s.document()==document&&s.preview_document()==preview&&encode(s.document())==native,
            "Navigation/refused input preserves complete authored, preview and native source");
        check(s.revision()==revision&&s.gesture_generation()==generation&&s.history()==history&&
            s.can_undo()==undo&&s.can_redo()==redo&&s.gesture_active()==gesture,
            "Navigation/refused input preserves complete history/revision/gesture state");
    }
};
Document fixture(){
    auto d=empty_document("retained-document","composition","board");
    auto& board=d.compositions.front().artboards.front();board.width=640;board.height=480;
    Object path;path.id="unrelated-path";path.name="Unrelated editable Path";
    Point a;a.id="kept-first";a.x.literal=35;a.y.literal=50;a.out_length.literal=20;
    Point b;b.id="kept-last";b.x.literal=95;b.y.literal=95;b.in_angle.literal=180;b.in_length.literal=20;
    path.contours={{"kept-contour",false,{a,b}}};path.stack={default_operation("kept-stroke","nect.paint.stroke")};
    d.objects.emplace(path.id,path);d.compositions.front().roots={path.id};
    Session s(d);s.apply({CreateText{"composition",{},"unrelated-text","Unrelated editable Text",default_text("kept-text-source","Kept ABC")},
        Set{{"unrelated-text",{},"transform.tx"},25},Set{{"unrelated-text",{},"transform.ty"},410}},s.revision());
    return s.document();
}
QAction* action(Window& w,const QString& name,bool by_text=false){
    for(auto* a:w.findChildren<QAction*>())if((by_text?a->text():a->objectName())==name)return a;
    throw std::runtime_error("Canonical action missing");
}
void history(Window& w,const QString& label){action(w,label,true)->trigger();events();}
QLineEdit* source_input(Window& w,const Id& object,const std::string& field){
    auto* inspector=w.findChild<QScrollArea*>("inspector-scroll");check(inspector,"Actual Inspector scroll exists");
    for(auto* input:inspector->findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")==QString::fromStdString(object)&&
            ref.value("point").toString().isEmpty()&&ref.value("field")==QString::fromStdString(field)){
            inspector->ensureWidgetVisible(input);events();check(reachable(input),"Exact retained source scalar is fully reachable");return input;
        }
    }
    throw std::runtime_error("Exact retained source scalar missing");
}
void evidence(Window& w,const QString& name){
    const auto dir=qEnvironmentVariable("NECT_RETAINED_SHAPE_EVIDENCE");
    if(!dir.isEmpty())check(w.grab().save(dir+"/"+name+".png"),"Editable Window evidence saved");
}
void tool(Window& w){
    auto* direct=w.findChild<QToolButton*>("tool-direct-selection");
    check(direct&&direct->isChecked()&&w.canvas->direct_selection_mode()&&!w.canvas->draw_mode()&&
        !w.canvas->text_mode()&&!w.canvas->hand_mode()&&!w.canvas->zoom_mode()&&
        !w.canvas->anchor_edit()&&!w.canvas->guide_edit_mode(),"One-shot creation/source edits retain explicit Direct Selection Tool");
}
void geometry(Window& w,const Id& id,bool circle,double width,double height){
    const auto& o=w.host.session.document().objects.at(id);const auto& source=*o.source;
    const auto& values=w.canvas->evaluated_values();
    if(circle){
        const auto r=width/2;
        check(values.at({id,source.id+"-east","x"})==320+r&&values.at({id,source.id+"-west","x"})==320-r&&
            values.at({id,source.id+"-north","y"})==240-r&&values.at({id,source.id+"-south","y"})==240+r,
            "Independent analytic Circle anchors follow precise retained radius");
    }else{
        check(values.at({id,source.id+"-top-left","x"})==320-width/2&&
            values.at({id,source.id+"-top-left","y"})==240-height/2&&
            values.at({id,source.id+"-bottom-right","x"})==320+width/2&&
            values.at({id,source.id+"-bottom-right","y"})==240+height/2,
            "Independent analytic Rectangle corners follow precise retained dimensions");
    }
    const auto scale=w.devicePixelRatioF();
    const auto image=Canvas::render_artboard(w.host.session.document(),"composition","board",scale,false);
    const double stroke=values.at({id,{},"op."+o.stack.front().id+".width"});
    int left=image.width(),top=image.height(),right=-1,bottom=-1,count=0;
    // Existing Path/Text are outside this region, so every painted pixel belongs to the new shape.
    for(int y=static_cast<int>(100*scale);y<365*scale;++y)
        for(int x=static_cast<int>(170*scale);x<470*scale;++x)if(image.pixelColor(x,y).alpha()>128){
            left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);++count;
        }
    check(count>100,"Production artboard renderer paints the actual created shape");
    const auto close=[&](double pixel,double expected){return std::abs(pixel/scale-expected)<1.6;};
    check(close(left,320-width/2-stroke/2)&&close(right+1,320+width/2+stroke/2)&&
        close(top,240-height/2-stroke/2)&&close(bottom+1,240+height/2+stroke/2),
        "Rendered stroke bounds match independently computed source dimensions");
    std::cout<<"shape="<<source.type<<" source="<<width<<"x"<<height<<" pixels="<<left<<","<<top<<","<<right<<","<<bottom<<" DPR="<<scale<<'\n';
}
void run(QTemporaryDir& scratch,QSettings& preferences,bool circle){
    const QString prefix=circle?"circle":"rectangle";
    Window w(scratch.filePath(prefix+"-recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1000,650);w.show();w.activateWindow();events();
    auto& s=w.host.session;s=Session(fixture());w.host.edited();events();w.canvas->set_selection("unrelated-path","kept-first");
    auto* direct=w.findChild<QToolButton*>("tool-direct-selection");QTest::mouseClick(direct,Qt::LeftButton);events();w.canvas->fit_artboard();events();
    const Snapshot before(s);const auto selection=w.canvas->selections();
    auto* button=w.findChild<QToolButton*>(circle?"canvas-create-circle":"canvas-create-rectangle");
    auto* scroll=w.findChild<QScrollArea*>("canvas-utility-scroll");check(button&&scroll,"Real Utility creation button and scroll exist");
    const auto bounds=QRect(button->mapTo(scroll->widget(),QPoint{}),button->size());
    scroll->ensureVisible(bounds.center().x(),bounds.center().y(),bounds.width()/2+1,bounds.height()/2+1);events();
    check(reachable(button),"Actual one-shot Utility button is fully reachable at narrow Window");
    before.unchanged(s);check(w.canvas->selections()==selection,"Revealing Utility retains unrelated stable selection");tool(w);
    auto* create=action(w,circle?"add-circle":"add-rectangle");QSignalSpy triggered(create,&QAction::triggered);
    check(button->defaultAction()==create&&triggered.isValid(),"Utility forwards to exactly the canonical creation action");
    QTest::mouseClick(button,Qt::LeftButton);events();
    check(triggered.count()==1&&s.revision()==before.revision+1,"One real click creates exactly one Session edit");
    const auto id=w.canvas->selected_object;check(!before.document.objects.contains(id)&&w.canvas->selected_point.empty()&&
        w.canvas->selections()==std::vector<Canvas::Selection>{{id,{}}},"One-shot creation selects only the new stable Object");tool(w);
    const auto& created_object=s.document().objects.at(id);check(created_object.source&&created_object.contours.empty()&&!created_object.point_edit,
        "Actual shape retains its generator with no duplicated authored contour or implicit correction/conversion");
    const auto source_id=created_object.source->id;
    Primitive source{source_id,circle?"nect.shape.circle":"nect.shape.rectangle",1,
        {{"center_x",{320,{}}},{"center_y",{240,{}}}}};
    if(circle)source.parameters.emplace("radius",Scalar{100,{}});
    else{source.parameters.emplace("width",Scalar{220,{}});source.parameters.emplace("height",Scalar{140,{}});}
    Session oracle(before.document);oracle.apply({CreatePrimitive{"composition",{},id,created_object.name,source}},oracle.revision());
    check(s.document()==oracle.document(),"Actual creation equals independent canonical centered-source oracle for complete Document");
    std::vector<Document> states{before.document,s.document()};
    std::vector<std::uint64_t> history_ids{before.history.current_id,s.history().current_id};
    geometry(w,id,circle,circle?200:220,circle?200:140);
    for(const auto* parameter:circle?std::initializer_list<const char*>{"center_x","center_y","radius"}:
        std::initializer_list<const char*>{"center_x","center_y","width","height"}){
        const Snapshot navigation(s);source_input(w,id,std::string("generator.")+parameter);navigation.unchanged(s);tool(w);
    }
    evidence(w,prefix+"-created");const auto canvas_before=w.canvas->grab().toImage();
    const std::vector<std::pair<std::string,double>> edits=circle?
        std::vector<std::pair<std::string,double>>{{"radius",73.125}}:
        std::vector<std::pair<std::string,double>>{{"width",173.25},{"height",89.5}};
    for(const auto& [parameter,value]:edits){
        const Ref ref{id,{},"generator."+parameter};auto* input=source_input(w,id,ref.field);
        const auto revision=s.revision();oracle.apply({EditProperties{{ref},value,false}},oracle.revision());
        input->setFocus();input->selectAll();QTest::keyClicks(input,QString::number(value,'g',17));QTest::keyClick(input,Qt::Key_Return);events();
        check(s.revision()==revision+1&&s.document()==oracle.document(),"Real precise scalar input equals one canonical edit for every authored field");
        check(s.document().objects.at(id).source->id==source_id&&!s.document().objects.at(id).point_edit&&
            s.document().objects.at(id).contours.empty(),"Precise source edit preserves generator identity and generated geometry ownership");
        check(s.document().objects.at("unrelated-path")==before.document.objects.at("unrelated-path")&&
            s.document().objects.at("unrelated-text")==before.document.objects.at("unrelated-text"),"Source editing preserves all unrelated Path/Text fields and stable IDs");
        tool(w);states.push_back(s.document());history_ids.push_back(s.history().current_id);
    }
    geometry(w,id,circle,circle?146.25:173.25,circle?146.25:89.5);
    check(w.canvas->grab().toImage()!=canvas_before,"Actual editable Canvas paint changes after precise source editing");
    evidence(w,prefix+"-precise");
    const Snapshot valid(s);auto* invalid=source_input(w,id,circle?"generator.radius":"generator.width");
    invalid->setFocus();invalid->selectAll();QTest::keyClicks(invalid,"-1");QTest::keyClick(invalid,Qt::Key_Return);events();valid.unchanged(s);
    for(std::size_t i=states.size()-1;i>0;--i){history(w,"Undo");check(s.document()==states[i-1]&&
        encode(s.document())==encode(states[i-1])&&s.history().current_id==history_ids[i-1],"Complete Undo crosses each precise edit and creation boundary exactly");tool(w);}
    for(std::size_t i=1;i<states.size();++i){history(w,"Redo");check(s.document()==states[i]&&
        s.history().current_id==history_ids[i],"Complete Redo restores each exact source edit and creation boundary");tool(w);}
    const auto native=scratch.filePath(prefix+"-edited.nect");const Snapshot saved(s);w.host.save(native);events();saved.unchanged(s);
    w.host.open(native);events();check(s.document()==oracle.document()&&encode(s.document())==encode(states.back()),"Same Window native reopen preserves all precise retained source");
    geometry(w,id,circle,circle?146.25:173.25,circle?146.25:89.5);
    Window reopened(scratch.filePath(prefix+"-reopened"),std::make_unique<FolderLibrary>(preferences),&preferences);reopened.host.open(native);events();
    check(reopened.host.session.document()==oracle.document(),"New Window native reopen preserves exact shape IDs, source and unrelated Objects");
    check(preferences.value("unrelated")=="preserved"&&preferences.value("workspace/tools/textCreationDirection")=="vertical", "Creation/precision/native paths preserve unrelated and remembered Text preferences");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings preferences(scratch.filePath("preferences.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserved");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        run(scratch,preferences,true);run(scratch,preferences,false);
        std::cout<<"PASS "<<checks<<" actual Utility retained-shape creation/precision checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
