#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QLabel>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <algorithm>
#include <memory>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace{
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full caller native-valid BEFORE Window");}
void same(const Session& a,const Session& b){check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Entire caller Session/native/History/preview/revision/generation unchanged");}
void write(const QString& path,const Document& d){QFile f(path);const auto b=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(b.data(),b.size())==static_cast<qint64>(b.size()),"Immutable actual or independent native retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error("Original visible source producer missing");}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive&&alive->isVisible()&&alive->isEnabled(),"Original source producer survives reveal and is enabled");}
namespace batch{
ColorValue precise_color(){ColorValue color;color.rgba={.123456789012345,.43210987654321,.7654321098765,.87654321098765};return color;}
Gradient gradient(const Id& id){
    Gradient value;value.id=id;GradientStop first;first.id=id+"-start";
    GradientStop last;last.id=id+"-end";last.offset.literal=1;last.rgba[0].literal=.8;value.stops={first,last};return value;
}
Document fixture(bool mixed=false,const std::string& kind="nect.paint.fill"){
    auto document=empty_document("paint-document","composition","artboard");
    for(const auto* id:{"first","second"}){
        Object object;object.id=id;object.name="Same visible name";object.kind=Kind::path;
        object.source=default_primitive(object.id+"-source","nect.shape.rectangle");
        object.transform[4].literal=object.id=="first"?11.123456789:211.987654321;
        object.compositing.opacity.literal=.375;object.compositing.blend="multiply";
        auto offset=default_operation(object.id+"-offset","nect.shape.offset");offset.parameters.at("amount").literal=3.25;
        auto paint=default_operation(object.id=="first"?"first-paint-id":"other-paint-id",kind);
        paint.composite=object.id=="first"?"above":"below";
        if(kind=="nect.paint.fill")paint.fill_rule=object.id=="first"?"evenodd":"nonzero";
        else if(object.id=="second"){
            paint.version=2;paint.line_join="round";paint.line_cap="square";paint.parameters.emplace("miter_limit",Scalar{7.25,{},{}});
            paint.parameters.at("width").expression=Expression{"3.5",1};
        }
        auto color=precise_color();if(mixed&&object.id=="second")color.rgba[0]+=1e-10;
        for(std::size_t i=0;i<4;++i)paint.parameters.at(std::array<const char*,4>{"r","g","b","a"}[i]).literal=color.rgba[i];
        auto unrelated=default_operation(object.id+"-unrelated","nect.paint.fill");
        unrelated.gradient=gradient(object.id+"-unrelated-gradient");
        unrelated.parameters.at("r").expression=Expression{"0.25",1};
        object.stack={offset,paint,unrelated};document.objects.emplace(object.id,object);
    }
    document.objects.at("first").source->parameters.at("width").binding=Binding{{"second","","generator.width"},1.25,0};
    document.objects.at("first").stack[1].enabled_driver=operation_ref("second","other-paint-id","enabled");
    document.compositions.front().roots={"first","second"};return document;
}

}
namespace instance{
ColorValue precise_color(){ColorValue color;color.rgba={.123456789012345,.43210987654321,.7654321098765,.87654321098765};return color;}
Gradient gradient(const Id& id){
    Gradient value;value.id=id;GradientStop first;first.id=id+"-start";
    GradientStop last;last.id=id+"-end";last.offset.literal=1;last.rgba[0].literal=.8;value.stops={first,last};return value;
}
Object rectangle(const Id& id,const Id& fill){
    Object object;object.id=id;object.name="Same visible name";
    object.source=default_primitive(id+"-rectangle","nect.shape.rectangle");
    object.stack={default_operation(fill,"nect.paint.fill")};return object;
}
Document fixture(bool empty=false){
    auto document=empty_document("instance-color-doc","composition","source-board");
    document.compositions.front().artboards.push_back({"target-board","Target",100,0,64,64});
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    auto outside=rectangle("outside","outside-fill");
    if(empty)root=rectangle("source-root","root-fill"); // Root paint must never become a selectable descendant.
    else{
        auto first=rectangle("first-item","first-fill");
        auto driver=rectangle("color-driver","driver-fill");
        const auto precise=precise_color();constexpr std::array<const char*,4> channels{"r","g","b","a"};
        for(std::size_t i=0;i<4;++i){
            driver.stack[0].parameters.at(channels[i]).literal=precise.rgba[i];
            first.stack[0].parameters.at(channels[i]).literal=.01;
            first.stack[0].parameters.at(channels[i]).binding=Binding{operation_ref(driver.id,"driver-fill",channels[i]),1,0};
        }
        auto other=default_operation("other-fill","nect.paint.fill");other.parameters.at("r").expression=Expression{"0.25",1};
        auto gradient_fill=default_operation("inactive-gradient-fill","nect.paint.fill");gradient_fill.gradient=gradient("inactive-gradient");
        gradient_fill.gradient->enabled=false;
        first.stack.insert(first.stack.begin(),default_operation("offset","nect.shape.offset"));
        first.stack.push_back(other);first.stack.push_back(gradient_fill);
        Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;folder.children={"second-item"};
        auto second=rectangle("second-item","second-fill");second.stack[0].parameters.at("r").expression=Expression{"0.75",1};
        second.stack[0].parameters.at("g").binding=Binding{operation_ref(driver.id,"driver-fill","g"),1,0};
        auto stroke=rectangle("stroke-only","discarded");stroke.stack={default_operation("stroke","nect.paint.stroke")};
        auto gradients=rectangle("gradient-only","gradient-fill");gradients.stack[0].gradient=gradient("gradient");
        root.children={first.id,folder.id,driver.id,stroke.id,gradients.id};
        for(const auto& object:{first,folder,second,driver,stroke,gradients})document.objects.emplace(object.id,object);
    }
    document.objects.emplace(root.id,root);document.objects.emplace(outside.id,outside);
    document.compositions.front().roots={root.id,outside.id};
    Session session(std::move(document));
    session.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target-board","template",Id{"content-instance"}}},
        DefinitionCommand{CreateInstance{"composition","","plain-instance","definition","Plain"}}},0);
    return session.document();
}

}

void flow(const QString& root,int mode){
 const auto initial=mode==0?batch::fixture():instance::fixture();valid(initial);Session base(initial);auto caller_doc=initial;caller_doc.objects.at(mode==0?"first":"first-item").stack.at(1).parameters.at("r").literal=.21234567890123456;valid(caller_doc);Session caller(caller_doc);check(caller.document().id==base.document().id&&caller.revision()==base.revision()&&caller.gesture_generation()==base.gesture_generation(),"Independent full native caller/History/context BEFORE Window; reused source-refusal contract, no new authored semantics");const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();if(mode==0)w.canvas->set_selections({Canvas::Selection{"first",{}},Canvas::Selection{"second",{}}});else w.canvas->set_selection("plain-instance");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual original Property dock");dock->show();dock->raise();events();if(mode==1){auto* objects=control<QComboBox>(w,"instance-color-source-object");const int object_index=objects->findData("first-item");check(object_index>=0,"Exact stable original descendant ID");objects->setCurrentIndex(object_index);auto* fills=control<QComboBox>(w,"instance-color-source-fill");const int fill_index=fills->findData("first-fill");check(fill_index>=0,"Exact stable original Fill ID");fills->setCurrentIndex(fill_index);events();}QPointer<QPushButton> producer=control<QPushButton>(w,mode==0?"paint-color-batch-choose":"instance-color-choose");reveal(producer);QPointer<QLabel> status_label=producer->parentWidget()->findChild<QLabel*>(mode==0?"paint-color-batch-status":"instance-color-error");check(status_label,"Actual original local refusal QLabel");same(w.host.session,base);bool dialog_seen=false;std::exception_ptr failure;if(mode==0)w.host.session=caller;else QTimer::singleShot(0,&w,[&]{QPointer<QColorDialog> dialog=w.findChild<QColorDialog*>("instance-color-dialog");if(!dialog)return;dialog_seen=true;try{check(dialog->isVisible(),"Original local chooser visible");w.host.session=caller;same(w.host.session,caller);dialog->accept();}catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();events();if(failure)std::rethrow_exception(failure);same(w.host.session,caller);const QString expected=mode==0?"PROPERTY_CONFLICT: The retained paint operation changed; refresh before applying a color":"PROPERTY_CONFLICT: The Instance or source Fill changed while choosing a color";check(status_label&&status_label->text()==expected&&emitted.count()==1&&producer&&!producer->isEnabled()&&dialog_seen==(mode==1),"Original source-refusal contract retained; one original producer and full precise error string");const auto visible=status_label->visibleRegion().boundingRect(),label=status_label->rect();const bool fully_visible=!label.isEmpty()&&visible.contains(label);const auto point=status_label->mapTo(&w,QPoint{});write(path+"-observed.nect",w.host.session.document());check(w.grab().save(path+"-window.png")&&status_label->parentWidget()->grab().save(path+"-panel.png"),"Actual original Window/Color panel rendered without force layout or post-refusal scroll");QJsonObject state{{"signal_count",emitted.count()},{"status",status_label->text()},{"fully_visible",fully_visible},{"label_x",point.x()},{"label_y",point.y()},{"label_width",label.width()},{"label_height",label.height()},{"visible_width",visible.width()},{"visible_height",visible.height()},{"window_width",w.width()},{"window_height",w.height()},{"dialog_seen",dialog_seen},{"document_equal",w.host.session.document()==caller.document()},{"history_equal",w.host.session.history()==caller.history()}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original native visible-region/producer readback immutable");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"COLOR_REFUSAL_VISIBILITY mode="<<mode<<" full="<<fully_visible<<" label="<<label.width()<<"x"<<label.height()<<" visible="<<visible.width()<<"x"<<visible.height()<<" at="<<point.x()<<","<<point.y()<<"\n";check(fully_visible,"Original exact refusal label fully visible in actual scrolled Inspector after natural Qt settling; authored caller remains exact");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full caller/native/History remains exact through view-only status placement");w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" QWidget geometry; physical/native OS picker NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_COLOR_REFUSAL_VISIBILITY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<2;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Color refusal visibility cases failed: "+std::to_string(failures));std::cout<<"PASS Color refusal visibility "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Color refusal visibility "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
