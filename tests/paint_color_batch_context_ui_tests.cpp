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

ProcessingEntry& paint(Document& d,const std::string& id){return d.objects.at(id).stack.at(1);}
Document recolor(Document d,const ColorValue& v){for(const auto* id:{"first","second"})for(std::size_t i=0;i<4;++i)paint(d,id).parameters.at(std::array<const char*,4>{"r","g","b","a"}[i]).literal=v.rgba[i];return d;}
void flow(const QString& root,int mode){
 const bool success=mode==0||mode==7,early=mode>=1&&mode<=3;const auto initial=fixture();valid(initial);Session base(initial);const std::vector<Ref> targets{operation_ref("first","first-paint-id","color"),operation_ref("second","other-paint-id","color")};const QColor choice(153,102,51,204);ColorValue chosen;chosen.rgba={choice.redF(),choice.greenF(),choice.blueF(),choice.alphaF()};const auto intended=recolor(initial,chosen);valid(intended);auto normal_goal=base;normal_goal.apply({SetColor{targets.at(0),chosen},SetColor{targets.at(1),chosen}},0);check(normal_goal.document()==intended&&normal_goal.revision()==1&&normal_goal.history().states.size()==base.history().states.size()+1,"Independent complete precise manual batch agrees canonical two SetColor commands in one atomic History BEFORE Window");auto normal_undo=normal_goal;normal_undo.undo(1);check(normal_undo.document()==initial,"Independent complete precise/source/native Undo BEFORE Window");auto normal_redo=normal_undo;normal_redo.redo(2);check(normal_redo.document()==intended,"Independent complete intended/source/native Redo BEFORE Window");
 auto caller_doc=initial;if(mode==1||mode==4)paint(caller_doc,"first").parameters.at("r").literal=.21234567890123456;if(mode==2||mode==5)paint(caller_doc,"second").parameters.at("a").literal=.71234567890123456;if(mode==3||mode==6){paint(caller_doc,"first").enabled_driver.reset();paint(caller_doc,"first").enabled_expression=Expression{"true",1};}if(mode==7)caller_doc.objects.at("first").stack.at(2).gradient->start_x.literal+=2;valid(caller_doc);Session caller(caller_doc);const auto caller_intended=recolor(caller_doc,chosen);valid(caller_intended);auto caller_goal=caller;caller_goal.apply({SetColor{targets.at(0),chosen},SetColor{targets.at(1),chosen}},0);check(caller_goal.document()==caller_intended&&caller_goal.history().states.size()==caller.history().states.size()+1,"Independent complete contextual batch oracle BEFORE Window; unrelated authored operation retained");auto caller_undo=caller_goal;caller_undo.undo(1);check(caller_undo.document()==caller_doc,"Independent entire contextual Undo BEFORE Window");auto caller_redo=caller_undo;caller_redo.redo(2);check(caller_redo.document()==caller_intended,"Independent contextual Redo BEFORE Window");const auto goal=mode==7?caller_goal:normal_goal,undone=mode==7?caller_undo:normal_undo,redone=mode==7?caller_redo:normal_redo;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections({Canvas::Selection{"first",{}},Canvas::Selection{"second",{}}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual multi-selection Property dock");dock->show();dock->raise();events();QPointer<QPushButton> producer=control<QPushButton>(w,"paint-color-batch-choose");reveal(producer);auto* panel=producer->parentWidget();QPointer<QLabel> status_label=panel->findChild<QLabel*>("paint-color-batch-status");check(status_label,"Actual selected paint panel status");same(w.host.session,base);if(early)w.host.session=caller;bool seen=false;QPointer<QColorDialog> dialog_seen;
 QTimer::singleShot(0,&w,[&]{auto* dialog=w.findChild<QColorDialog*>("paint-color-batch-dialog");if(!dialog)return;seen=true;dialog_seen=dialog;check(dialog->isVisible(),"Original Qt Color dialog visible");dialog->setCurrentColor(choice);check(dialog->currentColor()==choice,"Actual original chooser color equals independent pre-Window QColor oracle");if(!early)w.host.session=caller;same(w.host.session,caller);check(dialog->grab().save(path+"-dialog.png"),"Original Qt nonnative dialog rendered before acceptance");dialog->accept();});QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();const auto observed=w.host.session;const auto status=status_label?status_label->text():QString{};write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-status.png"),"Original multi-paint Window/panel refusal rendered");QJsonObject state{{"signal_count",emitted.count()},{"dialog_seen",seen},{"producer_enabled",producer&&producer->isEnabled()},{"status",status},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original chooser immutable readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"PAINT_COLOR_BATCH_CONTEXT mode="<<mode<<" status="<<status.toStdString()<<" dialog="<<seen<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" history_equal="<<(observed.history()==(success?goal.history():caller.history()))<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1,"One original chooser click");check(success?seen:(seen==!early&&producer&&!producer->isEnabled()&&status=="PROPERTY_CONFLICT: The retained paint operation changed; refresh before applying a color"),"Exact original current chooser or early/staged whole-paint context refusal");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full precise/native/source batch or untouched caller saved");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" Qt nonnative chooser; physical/native OS picker NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_PAINT_COLOR_BATCH_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Paint Color batch context cases failed: "+std::to_string(failures));std::cout<<"PASS Paint Color batch context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Paint Color batch context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
