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

ProcessingEntry& source_paint(Document& d){return d.objects.at("first-item").stack.at(1);}
void flow(const QString& root,int mode){
 const bool success=mode==0||mode==4;const Ref target=operation_ref("first-item","first-fill","color"),other=operation_ref("first-item","other-fill","color");const auto initial=fixture();valid(initial);Session base(initial);const QColor choice(153,102,51,204);ColorValue chosen;chosen.rgba={choice.redF(),choice.greenF(),choice.blueF(),choice.alphaF()};auto intended=initial;intended.objects.at("plain-instance").instance->color_overrides[target]=chosen;valid(intended);auto normal_goal=base;normal_goal.apply({DefinitionCommand{SetInstanceColorOverride{"plain-instance",target,chosen}}},0);check(normal_goal.document()==intended&&normal_goal.revision()==1&&normal_goal.history().states.size()==base.history().states.size()+1,"Independent full manually intended occurrence-only map agrees canonical one SetInstanceColorOverride History BEFORE Window");auto normal_undo=normal_goal;normal_undo.undo(1);check(normal_undo.document()==initial,"Independent entire native/source/inheritance Undo BEFORE Window");auto normal_redo=normal_undo;normal_redo.redo(2);check(normal_redo.document()==intended,"Independent entire intended occurrence native Redo BEFORE Window");
 auto caller_doc=initial;ColorValue external;external.rgba={.81234567890123456,.71234567890123456,.61234567890123456,.51234567890123456};if(mode==1)source_paint(caller_doc).parameters.at("r").literal=.21234567890123456;if(mode==2){caller_doc.objects.at("plain-instance").instance->color_overrides[target]=external;auto typed=base;typed.apply({DefinitionCommand{SetInstanceColorOverride{"plain-instance",target,external}}},0);check(typed.document()==caller_doc,"Independent full changed occurrence payload agrees canonical command BEFORE Window");}if(mode==3){auto& r=source_paint(caller_doc).parameters.at("r");r.binding.reset();r.expression=Expression{"0.123456789012345",1};auto typed=base;typed.apply({SetExpression{{operation_ref("first-item","first-fill","r")},Expression{"0.123456789012345",1},true}},0);check(typed.document()==caller_doc,"Independent full selected source Binding-to-Expression caller agrees explicit canonical replacement BEFORE Window");}if(mode==4){caller_doc.objects.at("content-instance").instance->color_overrides[other]=external;auto typed=base;typed.apply({DefinitionCommand{SetInstanceColorOverride{"content-instance",other,external}}},0);check(typed.document()==caller_doc,"Independent complete unrelated sibling occurrence change agrees canonical command BEFORE Window");}valid(caller_doc);Session caller(caller_doc);auto caller_intended=caller_doc;caller_intended.objects.at("plain-instance").instance->color_overrides[target]=chosen;valid(caller_intended);auto caller_goal=caller;caller_goal.apply({DefinitionCommand{SetInstanceColorOverride{"plain-instance",target,chosen}}},0);check(caller_goal.document()==caller_intended&&caller_goal.history().states.size()==caller.history().states.size()+1,"Independent complete caller atomic override oracle BEFORE Window; unrelated sibling and source preserved");auto caller_undo=caller_goal;caller_undo.undo(1);check(caller_undo.document()==caller_doc,"Independent complete caller Undo BEFORE Window");auto caller_redo=caller_undo;caller_redo.redo(2);check(caller_redo.document()==caller_intended,"Independent complete caller Redo BEFORE Window");const auto goal=mode==4?caller_goal:normal_goal,undone=mode==4?caller_undo:normal_undo,redone=mode==4?caller_redo:normal_redo;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("plain-instance");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Instance Property dock");dock->show();dock->raise();events();auto* objects=control<QComboBox>(w,"instance-color-source-object");const auto oi=objects->findData("first-item");check(oi>=0,"Exact stable descendant ID despite duplicate names");objects->setCurrentIndex(oi);auto* fills=control<QComboBox>(w,"instance-color-source-fill");const auto fi=fills->findData("first-fill");check(fi>=0,"Exact stable Fill ID despite otherpaint choices");fills->setCurrentIndex(fi);events();QPointer<QPushButton> producer=control<QPushButton>(w,"instance-color-choose");reveal(producer);QPointer<QLabel> status_label=producer->parentWidget()->findChild<QLabel*>("instance-color-error");check(status_label,"Actual original Instance local-color status");same(w.host.session,base);bool seen=false;std::exception_ptr failure;
 QTimer::singleShot(0,&w,[&]{QPointer<QColorDialog> dialog=w.findChild<QColorDialog*>("instance-color-dialog");if(!dialog)return;seen=true;try{check(dialog->isVisible(),"Actual original nonnative Qt local Instance picker visible");dialog->setCurrentColor(choice);check(dialog->currentColor()==choice,"Original picked color equals independent preWindow oracle");w.host.session=caller;same(w.host.session,caller);check(dialog->grab().save(path+"-dialog.png"),"Actual original local Instance chooser rendered beforeaccept");dialog->accept();}catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();if(failure)std::rethrow_exception(failure);const auto observed=w.host.session;const auto status=status_label?status_label->text():QString{};write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-status.png"),"Actual original Instance Window/status rendered");QJsonObject state{{"signal_count",emitted.count()},{"dialog_seen",seen},{"producer_enabled",producer&&producer->isEnabled()},{"status",status},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original Instance chooser immutable producer readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"INSTANCE_COLOR_CONTEXT mode="<<mode<<" status="<<status.toStdString()<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" history_equal="<<(observed.history()==(success?goal.history():caller.history()))<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1&&seen,"One original Instance local Fill chooser producer");check(success?status.isEmpty():(producer&&!producer->isEnabled()&&status=="PROPERTY_CONFLICT: The Instance or source Fill changed while choosing a color"),"Current occurrence-only edit or exact staged source/occurrence refusal");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable whole native preserves source bindings/expressions/otherinstance/template or complete caller");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" Qt nonnative chooser; native OS picker/physical NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_INSTANCE_COLOR_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<5;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Instance Color context cases failed: "+std::to_string(failures));std::cout<<"PASS Instance Color context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Instance Color context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
