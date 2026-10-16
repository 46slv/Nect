#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
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
Document fixture(){
    auto d=empty_document("instance-navigation","composition","source-board");
    d.compositions.front().artboards.push_back({"other-board","Other",700,0,640,480});
    Object folder;folder.id="folder";folder.name="Folder";folder.kind=Kind::group;folder.children={"root","decoy"};
    Object root;root.id="root";root.name="Shared source";root.kind=Kind::group;root.children={"rectangle","text"};
    Object rectangle;rectangle.id="rectangle";rectangle.name="Rectangle";
    rectangle.source=default_primitive("rectangle-source","nect.shape.rectangle");
    rectangle.stack={default_operation("fill","nect.paint.fill")};
    Object text;text.id="text";text.name="Text";text.kind=Kind::text;text.text=default_text("text-source","Retained text");
    text.text->parameters.at("font_size").expression=Expression{"24 + 2",1};
    Object decoy;decoy.id="decoy";decoy.name=root.name;decoy.kind=Kind::group;
    for(const auto& o:{folder,root,rectangle,text,decoy})d.objects.emplace(o.id,o);
    d.compositions.front().roots={folder.id};
    Session s(d);s.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        DefinitionCommand{CreateDefinition{{"decoy-definition","Shared",decoy.id}}},
        DefinitionCommand{CreateInstance{"composition",{},"first","definition","First"}},
        DefinitionCommand{CreateInstance{"composition",{},"second","definition","Second"}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","other-board","template",Id{"content"}}}},0);
    return s.document();
}

void flow(const QString& root,int mode){
 const bool navigates=mode==0||mode==4;const auto initial=fixture();valid(initial);Session base(initial);auto caller_doc=initial;if(mode==1)caller_doc.objects.at("first").instance->definition="decoy-definition";if(mode==2){auto& object=caller_doc.objects.at("first");object.instance.reset();object.kind=Kind::group;}if(mode==3){caller_doc.objects.erase("first");auto& roots=caller_doc.compositions.front().roots;roots.erase(std::remove(roots.begin(),roots.end(),"first"),roots.end());auto deleted=base;deleted.apply({DeleteObjects{{"first"}}},0);check(deleted.document()==caller_doc,"Independent complete missing original Instance agrees canonical deletion BEFORE Window");}if(mode==4){caller_doc.objects.at("first").transform[4].literal=.71234567890123456;auto transformed=base;transformed.apply({Set{{"first","","transform.tx"},.71234567890123456}},0);check(transformed.document()==caller_doc,"Independent complete placement-only caller keeps exact source association and agrees canonical Set BEFORE Window");}valid(caller_doc);Session caller(caller_doc);check(caller.document().id==base.document().id&&caller.revision()==base.revision()&&caller.gesture_generation()==base.gesture_generation()&&caller.document().definitions.at("definition").root=="root"&&caller.document().objects.contains("root"),"Independent sameDoc/SID/context but changed association/type/owner retains old source BEFORE Window");if(mode==1)check(caller.document().definitions.at(caller.document().objects.at("first").instance->definition).root=="decoy","Independent changed Instance actual source is different stable root BEFORE Window");const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("first");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual original Instance Property dock");dock->show();dock->raise();events();QPointer<QPushButton> producer=control<QPushButton>(w,"instance-source-go");reveal(producer);same(w.host.session,base);const auto selections=w.canvas->selections();const auto board=w.canvas->active_artboard();const auto zoom=w.canvas->zoom();check(w.canvas->selected_object=="first"&&selections==std::vector<Canvas::Selection>{{"first",{}}},"Original exact Instance view selection BEFORE caller replacement");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();same(w.host.session,caller);const auto status=w.statusBar()->currentMessage();const bool view_equal=w.canvas->selections()==(navigates?std::vector<Canvas::Selection>{{"root",{}}}:selections)&&w.canvas->selected_object==(navigates?"root":"first")&&w.canvas->active_artboard()==board&&w.canvas->zoom()==zoom;write(path+"-observed.nect",w.host.session.document());check(w.grab().save(path+"-window.png"),"Actual source navigation or refusal Window rendered");QJsonObject state{{"signal_count",emitted.count()},{"status",status},{"selected_object",QString::fromStdString(w.canvas->selected_object)},{"view_equal",view_equal},{"document_equal",w.host.session.document()==caller.document()},{"history_equal",w.host.session.history()==caller.history()}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original source navigation producer/view identity readback immutable");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"INSTANCE_SOURCE_CONTEXT mode="<<mode<<" selected="<<w.canvas->selected_object<<" view_equal="<<view_equal<<" status="<<status.toStdString()<<"\n";check(emitted.count()==1&&view_equal,"One original source navigation producer targets current original association or retains stale view exactly");check(navigates?status.isEmpty():status=="Instance context changed; select it again before going to its source.","Current source navigation or unchanged explicit refusal message");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable complete caller native unchanged by view-only source navigation/refusal");w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" Qt view action; physical NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_INSTANCE_SOURCE_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<5;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Instance Source context cases failed: "+std::to_string(failures));std::cout<<"PASS Instance Source context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Instance Source context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
