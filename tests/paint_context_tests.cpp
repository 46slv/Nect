#include "window.hpp"
#include "visual_style.hpp"
#include "semantic_color_control.hpp"
#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
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
    Session s(empty_document("paint-context","comp","art"));
    auto source=default_primitive("tile-source","nect.shape.rectangle");
    source.parameters.at("center_x").literal=300;source.parameters.at("center_y").literal=180;
    source.parameters.at("width").literal=40;source.parameters.at("height").literal=24;
    auto other=default_primitive("other-source","nect.shape.rectangle");
    other.parameters.at("center_x").literal=520;other.parameters.at("center_y").literal=440;
    other.parameters.at("width").literal=30;other.parameters.at("height").literal=24;
    auto blue=default_operation("other-fill","nect.paint.fill");blue.parameters.at("b").literal=1;
    auto repeater=default_operation("tile-repeat","nect.shape.repeater");
    repeater.parameters.at("copies").literal=2;repeater.parameters.at("position_x").literal=80;
    ColorValue precise;precise.rgba={0.12345678912345678,0.45678912345678912,0.7891234567891234,0.3456789123456789};
    s.apply({CreatePrimitive{"comp","","tile","Retained Tile",source},
        AddOperation{"tile",default_operation("tile-fill","nect.paint.fill"),0},AddOperation{"tile",repeater,2},
        SetColor{operation_ref("tile","tile-fill","color"),precise},
        CreatePrimitive{"comp","","other","Other artwork",other},AddOperation{"other",blue,0},
        Set{{"tile","tile-source-top-left","x"},285},
        Link{{"other","","generator.width"},{{"tile","tile-source-top-left","x"},0.1,0,"copy_local_value"}}},0);
    auto d=s.document();auto& art=d.compositions.front().artboards.front();art.width=640;art.height=480;
    auto second=art;second.id="other-art";second.name="Other Artboard";second.x=700;
    d.compositions.front().artboards.push_back(second);return d;
}
void evidence(Window& w,const QString& suffix) {
    const auto prefix=qEnvironmentVariable("NECT_PAINT_CONTEXT_EVIDENCE");
    if(!prefix.isEmpty())check(w.grab().save(prefix+suffix+".png"),"Window evidence saved");
}
void select(Window& w) {
    auto* dock=named<QDockWidget>(w,"structure");dock->show();dock->raise();events();
    auto* tree=dock->findChild<QTreeWidget*>();check(tree,"Structure tree");tree->expandAll();
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator it(tree);*it;++it)
        if((*it)->data(0,Qt::UserRole).toString()=="tile"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty())row=*it;
    check(row,"Exact retained object row");tree->scrollToItem(row);events();
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
    check(w.canvas->selected_object=="tile"&&w.canvas->selected_point.empty(),"Exact retained object selected");
    dock=named<QDockWidget>(w,"properties");dock->show();dock->raise();events();
}
void reveal(Window& w,QWidget* c) {
    auto* area=named<QScrollArea>(w,"inspector-scroll");check(c->isVisible()&&c->isEnabled(),"Actual enabled paint control");
    area->verticalScrollBar()->setValue(c->mapTo(area->widget(),QPoint()).y()-area->viewport()->height()/2);events();
    std::cout<<"Reach "<<c->objectName().toStdString()<<" viewport="<<area->viewport()->width()
        <<" content="<<area->widget()->width()<<" min="<<area->widget()->minimumSizeHint().width()
        <<" hmax="<<area->horizontalScrollBar()->maximum()<<std::endl;
    const bool fits=area->horizontalScrollBar()->maximum()==0&&
        area->viewport()->rect().contains(QRect(c->mapTo(area->viewport(),QPoint()),c->size()))&&c->visibleRegion().contains(c->rect());
    if(!fits) {
        for(auto* x:area->widget()->findChildren<QWidget*>())if(x->minimumSizeHint().width()>area->viewport()->width()-60)
            std::cout<<"Minimum owner "<<x->metaObject()->className()<<" "<<x->objectName().toStdString()<<" min="<<x->minimumSizeHint().width()<<std::endl;
        evidence(w,".failure");
    }
    check(fits,"Paint control reachable with vertical scrolling only");
}
QLineEdit* field(Window& w,const char* property) {
    const auto key=QJsonDocument(QJsonObject{{"object","tile"},{"point",""},{"field",property}}).toJson(QJsonDocument::Compact);
    for(auto* p:w.findChildren<QLineEdit*>())if(p->isVisible()&&p->property("nect-reference").toByteArray()==key)return p;
    throw std::runtime_error(std::string("Missing paint property: ")+property);
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
void draft(QLineEdit* c,const QString& text) {
    QTest::mouseClick(c,Qt::LeftButton);QTest::keyClick(c,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(c,text);
}
ColorValue color(const Session& s,const char* op) {return color_value(s.document(),operation_ref("tile",op,"color"),evaluate(s.document()));}
void hex(Window& w,Session& expected,const char* op,const QString& text,const ColorValue& value) {
    const auto name=std::string("operation-hex-")+op;auto* c=named<QLineEdit>(w,name.c_str());reveal(w,c);draft(c,text);
    check(same(w.host.session,expected),"HEX draft fully Session neutral");QTest::keyClick(c,Qt::Key_Return);events();
    expected.apply({SetColor{operation_ref("tile",op,"color"),value}},expected.revision());canonical(w,expected);
    check(color(w.host.session,op)==value,"Exact RGBA/metadata readback");
}
void number(Window& w,Session& expected,const char* property,const char* text,double value) {
    auto* c=field(w,property);reveal(w,c);draft(c,text);check(same(w.host.session,expected),"Numeric draft fully Session neutral");
    QTest::keyClick(c,Qt::Key_Return);events();expected.apply({EditProperties{{{"tile","",property}},value,false}},expected.revision());canonical(w,expected);
}
void style(Window& w,Session& expected,const char* name,const char* selected,const char* cap,const char* join,double miter) {
    auto* c=named<QComboBox>(w,name);reveal(w,c);QTest::mouseClick(c,Qt::LeftButton);
    check(c->findData(selected)==c->currentIndex()+1,"Adjacent actual stroke option");QTest::keyClick(c,Qt::Key_Down);
    QTest::keyClick(c,Qt::Key_Return);events();expected.apply({StrokeStyle{"tile","tile-stroke",cap,join,miter}},expected.revision());canonical(w,expected);
}
void retained(const Document& initial,const Document& current) {
    const auto& a=initial.objects.at("tile");const auto& b=current.objects.at("tile");
    check(a.source==b.source&&a.point_edit==b.point_edit&&b.contours.empty(),"Complete retained source/correction without flattened copies");
    check(current.objects.size()==initial.objects.size()&&current.compositions==initial.compositions,"Virtual copies and ordered Artboards retained");
    check(current.objects.at("other")==initial.objects.at("other"),"Other artwork and exact point Ref retained");
    check(b.stack.size()==3&&b.stack[0].id=="tile-fill"&&b.stack[1].id=="tile-stroke"&&b.stack[2]==a.stack[2],"Exact ordered paint IDs and complete Repeater retained");
}
void paint(Window& w,const Session& expected,const QString& suffix) {
    const auto image=Canvas::render_artboard(w.host.session.document(),"comp","art",1,false);
    check(image==Canvas::render_artboard(expected.document(),"comp","art",1,false),"Artwork equals independent canonical projection");
    for(const auto& p:{QPoint(300,180),QPoint(380,180)})check(image.pixelColor(p)==QColor(Qt::red),"Independent repeated red Fill centers");
    // Earlier Fill is above the later Stroke (native composite: below).
    // Sample outside the source edge at y=168 to measure visible stroke extent.
    for(const auto& p:{QPoint(300,164),QPoint(380,164),QPoint(300,162)}) {
        if(image.pixelColor(p)!=QColor(Qt::green)) {
            std::cout<<"Stroke sample "<<p.x()<<","<<p.y()<<" actual="<<image.pixelColor(p).name(QColor::HexArgb).toStdString()<<std::endl;
            const auto prefix=qEnvironmentVariable("NECT_PAINT_CONTEXT_EVIDENCE");if(!prefix.isEmpty())image.save(prefix+".pixel-failure.artwork.png");evidence(w,".pixel-failure");
        }
        check(image.pixelColor(p)==QColor(Qt::green),"Independent repeated green Stroke and width extent");
    }
    check(image.pixelColor(520,440)==QColor(Qt::blue)&&image.pixelColor(20,20).alpha()==0,"Unrelated artwork and transparency");
    const auto canvas=w.canvas->grab().toImage();
    for(const auto& pair:{std::pair{QPoint(300,180),QColor(Qt::red)},std::pair{QPoint(380,180),QColor(Qt::red)},std::pair{QPoint(300,164),QColor(Qt::green)},std::pair{QPoint(520,440),QColor(Qt::blue)}}) {
        const auto p=pair.first;const QPoint screen(qRound(w.canvas->width()/2.0+(p.x()-320)*w.canvas->zoom()),qRound(w.canvas->height()/2.0+(p.y()-240)*w.canvas->zoom()));
        check(canvas.pixelColor(qRound(screen.x()*canvas.devicePixelRatio()),qRound(screen.y()*canvas.devicePixelRatio()))==pair.second,"Actual Canvas independent Fill/Stroke/other pixels");
    }
    const auto prefix=qEnvironmentVariable("NECT_PAINT_CONTEXT_EVIDENCE");if(!prefix.isEmpty())check(image.save(prefix+suffix+".artwork.png"),"Artwork evidence saved");evidence(w,suffix);
}
void gradient_context(const QString& scratch) {
    const auto initial=fixture();Window w(scratch+"/gradient-recovery");
    w.host.session=Session(initial);w.host.session_id="retained-gradient-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Gradient selection fully Session neutral");
    auto current_gradient=[](const Document& d) {
        for(const auto& operation:d.objects.at("tile").stack)if(operation.id=="tile-fill")return operation.gradient;
        throw std::runtime_error("Missing exact tile Fill");
    };
    const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    auto choose=[&](int index) {
        auto* c=named<QComboBox>(w,"gradient-mode-tile-fill");reveal(w,c);
        check(std::abs(index-c->currentIndex())==1,"Adjacent actual Paint mode option");
        const auto key=index>c->currentIndex()?Qt::Key_Down:Qt::Key_Up;QTest::mouseClick(c,Qt::LeftButton);
        QTest::keyClick(c,key);QTest::keyClick(c,Qt::Key_Return);events();
    };
    choose(1);
    // Bind only generated component identities; compute all authored values independently.
    const auto actual=current_gradient(w.host.session.document());
    check(actual&&actual->stops.size()==2,"Contextual retained-source Gradient created");
    Gradient gradient;gradient.id=actual->id;gradient.start_x.literal=280;gradient.start_y.literal=180;
    gradient.end_x.literal=320;gradient.end_y.literal=180;
    GradientStop first,last;first.id=actual->stops[0].id;last.id=actual->stops[1].id;last.offset.literal=1;
    const auto fallback=color(expected,"tile-fill");
    for(std::size_t n=0;n<3;++n){first.rgba[n].literal=fallback.rgba[n];last.rgba[n].literal=fallback.rgba[n]+(1-fallback.rgba[n])*0.6;}
    first.rgba[3].literal=1;last.rgba[3].literal=1;gradient.stops={first,last};
    expected.apply({SetGradient{"tile","tile-fill",gradient}},expected.revision());canonical(w,expected);retained(initial,expected.document());
    auto ref=[&](const std::string& suffix){return gradient_ref("tile","tile-fill",gradient.id,suffix);};
    auto suffix=[&](const Id& stop,const char* channel){return "stop."+stop+"."+channel;};
    // Owned pre-existing endpoint source must survive stop and mode edits.
    const Link endpoint_source{ref("end_y"),{ref("start_y"),1,0,"copy_local_value"}};
    w.host.session.apply({endpoint_source},w.host.session.revision());expected.apply({endpoint_source},expected.revision());w.host.edited();events();
    check(same(w.host.session,expected),"Owned Gradient endpoint driver fixture full equality");
    auto* paired_end=named<QPushButton>(w,"gradient-point-edit-tile-fill-end");
    check(!paired_end->isEnabled(),"Driven endpoint retains explicit paired-editor refusal");
    const auto alpha_property=ref(suffix(first.id,"a")).field;
    number(w,expected,alpha_property.c_str(),"0.3456789123456789",0.3456789123456789);
    auto hex_stop=[&](const Id& stop,const QString& text,const std::vector<Command>& commands) {
        auto* input=named<QLineEdit>(w,("gradient-stop-hex-"+stop).c_str());reveal(w,input);draft(input,text);
        check(same(w.host.session,expected),"Stop HEX draft fully neutral");QTest::keyClick(input,Qt::Key_Return);events();
        if(commands.empty())check(same(w.host.session,expected),"Displayed stop HEX noop preserves exact doubles and complete History");
        else {expected.apply(commands,expected.revision());canonical(w,expected);}
    };
    auto* input=named<QLineEdit>(w,("gradient-stop-hex-"+first.id).c_str());
    const auto displayed=input->text();hex_stop(first.id,displayed,{});
    hex_stop(first.id,"#FF"+displayed.mid(3),{Set{ref(suffix(first.id,"r")),1}});
    check(color_value(expected.document(),ref(suffix(first.id,"color")),evaluate(expected.document())).rgba[1]==fallback.rgba[1],"One stop channel preserves exact unedited green");
    hex_stop(first.id,"#FF"+displayed.mid(3,4),{Set{ref(suffix(first.id,"a")),1}});
    hex_stop(first.id,"#FF0000FF",{Set{ref(suffix(first.id,"g")),0},Set{ref(suffix(first.id,"b")),0}});
    hex_stop(last.id,"#0000FFFF",{Set{ref(suffix(last.id,"r")),0},Set{ref(suffix(last.id,"g")),0},Set{ref(suffix(last.id,"b")),1}});
    number(w,expected,"op.tile-fill.a","1",1);
    auto change_mode=[&](int index,const char* type,bool enabled) {
        choose(index);gradient=*current_gradient(expected.document());gradient.type=type;gradient.enabled=enabled;
        expected.apply({SetGradient{"tile","tile-fill",gradient}},expected.revision());canonical(w,expected);retained(initial,expected.document());
    };
    change_mode(2,"radial",true);change_mode(1,"linear",true);change_mode(0,"linear",false);change_mode(1,"linear",true);
    check(color(expected,"tile-fill").rgba[0]==fallback.rgba[0]&&color(expected,"tile-fill").rgba[1]==fallback.rgba[1]&&color(expected,"tile-fill").rgba[2]==fallback.rgba[2],"Mode changes preserve exact solid fallback RGB");
    auto* add=named<QPushButton>(w,"gradient-stop-add-tile-fill");reveal(w,add);QTest::mouseClick(add,Qt::LeftButton);events();
    const auto added=current_gradient(w.host.session.document());
    check(added&&added->stops.size()==3,"Actual stop insertion in retained context");
    gradient=*current_gradient(expected.document());
    GradientStop middle;middle.id=added->stops.back().id;middle.offset.literal=0.5;middle.rgba[0].literal=0.5;middle.rgba[2].literal=0.5;middle.rgba[3].literal=1;
    gradient.stops.push_back(middle);expected.apply({SetGradient{"tile","tile-fill",gradient}},expected.revision());canonical(w,expected);
    const auto middle_property=ref(suffix(middle.id,"offset")).field;number(w,expected,middle_property.c_str(),"0.6",0.6);
    auto* remove=named<QPushButton>(w,("gradient-stop-remove-"+middle.id).c_str());reveal(w,remove);QTest::mouseClick(remove,Qt::LeftButton);events();
    gradient=*current_gradient(expected.document());gradient.stops.pop_back();
    expected.apply({SetGradient{"tile","tile-fill",gradient}},expected.revision());canonical(w,expected);
    check(gradient.stops[0].id==first.id&&gradient.stops[1].id==last.id,"Original stop IDs and vector order retained");
    // The authored frame is local to the corrected source, copied by the later Repeater.
    const auto end_property=ref("end_x").field;number(w,expected,end_property.c_str(),"340",340);
    number(w,expected,end_property.c_str(),"320",320);
    auto pixels=[&](Window& window,const Session& independent,const QString& stage) {
        const auto image=Canvas::render_artboard(window.host.session.document(),"comp","art",1,false);
        check(image==Canvas::render_artboard(independent.document(),"comp","art",1,false),"Gradient complete artwork equals independent canonical projection");
        const auto canvas=window.canvas->grab().toImage();
        for(const int copy:{0,80})for(const int x:{288,312}) {
            const double t=(x+0.5-280)/40.0;const QColor analytical(qRound(255*(1-t)),0,qRound(255*t));
            auto near=[&](const QColor& c){return std::abs(c.red()-analytical.red())<=3&&c.green()==0&&std::abs(c.blue()-analytical.blue())<=3&&c.alpha()==255;};
            check(near(image.pixelColor(x+copy,180)),"Independent linear interpolation repeats the local Gradient field");
            const QPoint screen(qRound(window.canvas->width()/2.0+(x+copy-320)*window.canvas->zoom()),qRound(window.canvas->height()/2.0+(180-240)*window.canvas->zoom()));
            const auto visible=canvas.pixelColor(qRound(screen.x()*canvas.devicePixelRatio()),qRound(screen.y()*canvas.devicePixelRatio()));
            check(std::abs(visible.red()-analytical.red())<=8&&visible.green()==0&&std::abs(visible.blue()-analytical.blue())<=8,"Actual Canvas shows independently expected source and virtual-copy Gradient");
        }
        check(image.pixelColor(520,440)==QColor(Qt::blue)&&image.pixelColor(20,20).alpha()==0,"Unrelated artwork and transparency retained");
        const auto prefix=qEnvironmentVariable("NECT_PAINT_CONTEXT_EVIDENCE");if(!prefix.isEmpty())check(image.save(prefix+stage+".artwork.png"),"Gradient artwork evidence saved");evidence(window,stage);
    };
    retained(initial,w.host.session.document());pixels(w,expected,".gradient-edited");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Gradient preserves standard pane and Canvas widths");
    const auto native=scratch+"/retained-gradient.nect";w.host.save(native);check(same(w.host.session,expected),"Gradient native save fully neutral");w.host.changed={};w.hide();
    Window cold(scratch+"/gradient-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Cold full Gradient/source/correction/Refs/stack/Artboard/native equality");
    const Session reopened=cold.host.session;select(cold);retained(initial,reopened.document());
    auto* mode=named<QComboBox>(cold,"gradient-mode-tile-fill");reveal(cold,mode);check(mode->currentIndex()==1,"Cold contextual Linear Paint mode");
    auto* endpoint=field(cold,end_property.c_str());reveal(cold,endpoint);check(endpoint->text().toDouble()==320,"Cold local endpoint readback");
    for(const auto& stop:gradient.stops){auto* hex=named<QLineEdit>(cold,("gradient-stop-hex-"+stop.id).c_str());reveal(cold,hex);check(hex->text()==(stop.id==first.id?"#FF0000FF":"#0000FFFF"),"Cold exact stable stop control readback");}
    check(same(cold.host.session,reopened),"Cold Gradient navigation fully neutral");pixels(cold,reopened,".gradient-cold");cold.host.changed={};cold.hide();
}
void paint_order_context(const QString& scratch) {
    Session setup(fixture());auto red=color(setup,"tile-fill"),green=color(setup,"tile-stroke");
    red.rgba={1,0,0,1};green.rgba={0,1,0,1};
    setup.apply({SetColor{operation_ref("tile","tile-fill","color"),red},SetColor{operation_ref("tile","tile-stroke","color"),green},
        Set{{"tile","","op.tile-stroke.width"},16}},setup.revision());
    const auto initial=setup.document();Window w(scratch+"/order-recovery");w.host.session=Session(initial);
    w.host.session_id="paint-order-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
    Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Order selection fully Session neutral");
    const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
    auto layer_pixels=[&](Window& window,const Session& independent,bool above,const QString& suffix) {
        const auto image=Canvas::render_artboard(window.host.session.document(),"comp","art",1,false);
        check(image==Canvas::render_artboard(independent.document(),"comp","art",1,false),"Composite artwork equals independent canonical projection");
        const auto overlap=above?QColor(Qt::green):QColor(Qt::red);
        for(const int x:{300,380}) {
            check(image.pixelColor(x,170)==overlap&&image.pixelColor(x,180)==QColor(Qt::red)&&image.pixelColor(x,162)==QColor(Qt::green),"Independent overlap/fill/exterior samples distinguish paint composite");
        }
        check(image.pixelColor(520,440)==QColor(Qt::blue),"Other artwork remains blue");
        events();const auto canvas=window.canvas->grab().toImage();
        for(const int x:{300,380}) {
            const QPoint screen(qRound(window.canvas->width()/2.0+(x-320)*window.canvas->zoom()),qRound(window.canvas->height()/2.0+(170-240)*window.canvas->zoom()));
            check(canvas.pixelColor(qRound(screen.x()*canvas.devicePixelRatio()),qRound(screen.y()*canvas.devicePixelRatio()))==overlap,"Actual Canvas overlap follows selected paint composite");
        }
        const auto prefix=qEnvironmentVariable("NECT_PAINT_CONTEXT_EVIDENCE");if(!prefix.isEmpty())check(image.save(prefix+suffix+".artwork.png"),"Composite artwork saved");evidence(window,suffix);
    };
    layer_pixels(w,expected,false,".order-before");
    auto* composite=named<QComboBox>(w,"operation-composite-tile-stroke");reveal(w,composite);
    QTest::mouseClick(composite,Qt::LeftButton);QTest::keyClick(composite,Qt::Key_Down);QTest::keyClick(composite,Qt::Key_Return);events();
    expected.apply({OperationOptions{"tile","tile-stroke","above","nonzero"}},expected.revision());canonical(w,expected);
    retained(initial,w.host.session.document());layer_pixels(w,expected,true,".order-above");
    check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Composite preserves standard pane and Canvas widths");
    const auto native=scratch+"/paint-order.nect";w.host.save(native);check(same(w.host.session,expected),"Composite save fully Session neutral");w.host.changed={};w.hide();
    Window cold(scratch+"/order-cold");cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
    check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Cold complete source/correction/colors/style/Refs/Artboards/composite equality");
    const Session reopened=cold.host.session;select(cold);retained(initial,reopened.document());
    composite=named<QComboBox>(cold,"operation-composite-tile-stroke");reveal(cold,composite);check(composite->currentData().toString()=="above","Cold exact composite readback");
    check(same(cold.host.session,reopened),"Composite cold navigation fully Session neutral");layer_pixels(cold,reopened,true,".order-cold");cold.host.changed={};cold.hide();
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    QTemporaryDir scratch(qEnvironmentVariable("NECT_PAINT_CONTEXT_SCRATCH",QDir::tempPath())+"/paint-context-XXXXXX");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("PaintContext");
    try {
        check(scratch.isValid(),"Owned scratch");
        if(app.arguments().contains("--gradient-context")) {
            gradient_context(scratch.path());std::cout<<"PASS "<<checks<<" standard-pane retained Gradient authoring; physical/subjective input NOT_RUN\n";return 0;
        }
        if(app.arguments().contains("--paint-order-context")) {
            paint_order_context(scratch.path());std::cout<<"PASS "<<checks<<" standard-pane retained paint composite authoring; physical/subjective input NOT_RUN\n";return 0;
        }
        const auto initial=fixture();Window w(scratch.filePath("recovery"));
        w.host.session=Session(initial);w.host.session_id="paint-context-session";w.host.edited();w.show();events();w.canvas->fit_artboard();events();
        Session expected=w.host.session;select(w);check(same(w.host.session,expected),"Selection fully Session neutral");
        const auto canvas_width=w.canvas->width(),dock_width=named<QDockWidget>(w,"properties")->width();
        auto* c=named<QLineEdit>(w,"operation-hex-tile-fill");reveal(w,c);const auto displayed=c->text();draft(c,displayed);QTest::keyClick(c,Qt::Key_Return);events();
        check(same(w.host.session,expected),"Contextual displayed HEX noop keeps exact high-precision RGBA and History");
        auto partial=color(expected,"tile-fill");partial.rgba[0]=1;hex(w,expected,"tile-fill","#FF"+displayed.mid(3),partial);
        auto red=color(expected,"tile-fill");red.rgba={1,0,0,1};hex(w,expected,"tile-fill","#FF0000FF",red);
        auto green=color(expected,"tile-stroke");green.rgba={0,1,0,1};hex(w,expected,"tile-stroke","#00FF00FF",green);
        number(w,expected,"op.tile-fill.a","0.5",0.5);number(w,expected,"op.tile-fill.a","1",1);
        number(w,expected,"op.tile-stroke.width","16",16);
        style(w,expected,"stroke-line-cap-tile-stroke","round","round","miter",4);
        style(w,expected,"stroke-line-join-tile-stroke","round","round","round",4);
        number(w,expected,"op.tile-stroke.miter_limit","6",6);
        auto* swatch=named<QPushButton>(w,"operation-color-tile-fill");reveal(w,swatch);QTest::mouseClick(swatch,Qt::LeftButton);events();
        auto* dialog=named<QColorDialog>(w,"semantic-color-picker");check(dialog->testOption(QColorDialog::ShowAlphaChannel),"Actual contextual swatch opens alpha-aware picker");
        auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Picker buttons");
        QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);events();check(same(w.host.session,expected),"Picker cancel fully Session neutral");
        retained(initial,w.host.session.document());paint(w,expected,".edited");
        check(w.canvas->width()==canvas_width&&named<QDockWidget>(w,"properties")->width()==dock_width,"Paint preserves standard pane and Canvas widths");
        const auto native=scratch.filePath("paint.nect");w.host.save(native);check(same(w.host.session,expected),"Save fully Session neutral");w.host.changed={};w.hide();
        Window cold(scratch.filePath("cold"));cold.host.open(native);cold.show();events();cold.canvas->fit_artboard();events();
        check(cold.host.session.document()==expected.document()&&encode(cold.host.session.document())==encode(expected.document()),"Cold full native source/correction/paint/options/Refs/order equality");
        const Session reopened=cold.host.session;select(cold);retained(initial,reopened.document());
        for(const auto* op:{"tile-fill","tile-stroke"}) {
            const auto name=std::string("semantic-color-")+op;auto* input=named<SemanticColorInput>(cold,name.c_str());reveal(cold,input);
            check(input->value()==color(expected,op).rgba,"Cold exact paint color readback");
        }
        for(const auto* property:{"op.tile-fill.a","op.tile-stroke.width","op.tile-stroke.miter_limit"}) {
            auto* value=field(cold,property);reveal(cold,value);check(value->text().toDouble()==evaluate(expected.document()).at({"tile","",property}),"Cold exact scalar readback");
        }
        for(const auto* name:{"stroke-line-cap-tile-stroke","stroke-line-join-tile-stroke"}) {auto* value=named<QComboBox>(cold,name);reveal(cold,value);check(value->currentData().toString()=="round","Cold stroke options readback");}
        check(same(cold.host.session,reopened),"Cold navigation fully Session neutral");paint(cold,reopened,".cold");cold.host.changed={};cold.hide();
        std::cout<<"PASS "<<checks<<" standard-pane retained Fill/Stroke contextual authoring; physical/subjective input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
