#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
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
const std::string exact_source="  Source\r\n日本語 🐈\xe2\x80\xa8line\xe2\x80\xa9tail\xc2\xa0\n";
Object text(const Id& id,const std::string& content){
    Object object;object.id=id;object.name="Same visible name";object.kind=Kind::text;
    object.text=default_text(id+"-text",content);return object;
}
Document fixture(bool empty=false,bool attached=false){
    auto document=empty_document("instance-text-doc","composition","source-board");
    document.compositions.front().artboards.push_back({"target-board","Target",100,0,64,64});
    Object root;root.id="source-root";root.name="Source";root.kind=Kind::group;
    auto outside=text("outside","Outside");
    if(empty)root=text("source-root","Root-only Text must not be selectable");
    else{
        auto first=text("first-item","Retained first literal\r\n");
        first.text->content_driver=TextContentDriver{{"content-driver","","text.content"}};
        first.text->family="Arial";first.text->locale="en-US";
        first.text->weight_expression=Expression{"650",1};first.text->italic=true;
        first.text->font_features={{"kern",1,"whole_text"}};
        first.text->additional_axis_values={{"wdth",87.1234567890123}};
        auto second=text("second-item","Second  literal\n");
        auto driver=text("content-driver",attached?"Single line":exact_source);
        Object folder;folder.id="folder";folder.kind=Kind::group;folder.children={second.id};
        Object shape;shape.id="shape";shape.source=default_primitive("shape-source","nect.shape.rectangle");
        root.children={first.id,folder.id,driver.id,shape.id};
        if(attached){
            Object path;path.id="path";path.kind=Kind::path;
            Contour contour;contour.id="contour";Point begin;begin.id="begin";
            Point end;end.id="end";end.x.literal=2000;contour.points={begin,end};path.contours={contour};
            root.children.push_back(path.id);document.objects.emplace(path.id,path);
            first.text->path_attachment=TextPathAttachment{"path","contour","distance",0,0,false};
            second.text->content_driver=TextContentDriver{{first.id,"","text.content"}};
            second.text->path_attachment=TextPathAttachment{"path","contour","distance",0,0,false};
        }
        for(const auto& object:{first,folder,second,driver,shape})document.objects.emplace(object.id,object);
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

void flow(const QString& root,int mode){
 const bool success=mode==0||mode==5;const auto initial=fixture();valid(initial);Session base(initial);const std::string pending="Pending local draft\nsecond line";auto intended=initial;intended.objects.at("plain-instance").instance->text_content_overrides["first-item"]=pending;valid(intended);auto normal_goal=base;normal_goal.apply({DefinitionCommand{SetInstanceTextContentOverride{"plain-instance","first-item",pending}}},0);check(normal_goal.document()==intended&&normal_goal.revision()==1&&normal_goal.history().states.size()==base.history().states.size()+1,"Independent full manually intended occurrence-only raw Text agrees canonical one History SetInstanceTextContentOverride BEFORE Window");auto normal_undo=normal_goal;normal_undo.undo(1);check(normal_undo.document()==initial,"Independent complete original source/links/raw/native Undo BEFORE Window");auto normal_redo=normal_undo;normal_redo.redo(2);check(normal_redo.document()==intended,"Independent complete intended raw/native Redo BEFORE Window");
 auto caller_doc=initial;if(mode==1)caller_doc.objects.at("first-item").text->content="Changed retained first literal\r\n";if(mode==2){caller_doc.objects.at("first-item").text->content_driver=TextContentDriver{{"second-item","","text.content"}};auto typed=base;typed.apply({LinkTextContent{{"first-item","","text.content"},{"second-item","","text.content"},true}},0);check(typed.document()==caller_doc,"Independent complete source driver replacement agrees explicit canonical LinkTextContent BEFORE Window");}if(mode==3){caller_doc.objects.at("plain-instance").instance->text_content_overrides["first-item"]="External local\r\n";auto typed=base;typed.apply({DefinitionCommand{SetInstanceTextContentOverride{"plain-instance","first-item","External local\r\n"}}},0);check(typed.document()==caller_doc,"Independent complete changed own occurrence content agrees canonical command BEFORE Window");}if(mode==4){caller_doc.objects.at("content-driver").text->content="Changed upstream source\r\n";auto typed=base;typed.apply({UpdateText{"content-driver",*caller_doc.objects.at("content-driver").text}},0);check(typed.document()==caller_doc,"Independent complete upstream source change agrees canonical UpdateText BEFORE Window");}if(mode==5){caller_doc.objects.at("content-instance").instance->text_content_overrides["second-item"]="Unrelated sibling local\r\n";auto typed=base;typed.apply({DefinitionCommand{SetInstanceTextContentOverride{"content-instance","second-item","Unrelated sibling local\r\n"}}},0);check(typed.document()==caller_doc,"Independent complete unrelated sibling override agrees canonical command BEFORE Window");}valid(caller_doc);Session caller(caller_doc);auto caller_intended=caller_doc;caller_intended.objects.at("plain-instance").instance->text_content_overrides["first-item"]=pending;valid(caller_intended);auto caller_goal=caller;caller_goal.apply({DefinitionCommand{SetInstanceTextContentOverride{"plain-instance","first-item",pending}}},0);check(caller_goal.document()==caller_intended&&caller_goal.history().states.size()==caller.history().states.size()+1,"Independent entire contextual occurrence/raw/source canonical oracle BEFORE Window");auto caller_undo=caller_goal;caller_undo.undo(1);check(caller_undo.document()==caller_doc,"Independent entire contextual native/raw Undo BEFORE Window");auto caller_redo=caller_undo;caller_redo.redo(2);check(caller_redo.document()==caller_intended,"Independent entire contextual raw/source Redo BEFORE Window");const auto goal=mode==5?caller_goal:normal_goal,undone=mode==5?caller_undo:normal_undo,redone=mode==5?caller_redo:normal_redo;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("plain-instance");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Actual Instance Properties dock");dock->show();dock->raise();events();auto* source=control<QComboBox>(w,"instance-text-content-source");const int si=source->findData("first-item");check(si>=0,"Original exact stable source Text ID");source->setCurrentIndex(si);events();QPointer<QPlainTextEdit> editor=control<QPlainTextEdit>(w,"instance-text-content-editor");reveal(editor);editor->setFocus(Qt::OtherFocusReason);events();check(editor,"Original local Text editor survives focus");QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);for(auto c:std::string("Pending local draft")){check(editor,"Original Text draft survives Qt keyboard character");QTest::keyClicks(editor.data(),QString(QChar(c)));}QTest::keyClick(editor,Qt::Key_Return);for(auto c:std::string("second line")){check(editor,"Original Text draft survives Qt keyboard character");QTest::keyClicks(editor.data(),QString(QChar(c)));}check(editor&&editor->toPlainText()==QString::fromStdString(pending),"Original exact modified raw Text draft; author state still separate");same(w.host.session,base);QPointer<QPushButton> producer=control<QPushButton>(w,"instance-text-content-apply");reveal(producer);QPointer<QLabel> status_label=producer->parentWidget()->findChild<QLabel*>("instance-text-content-error");check(status_label,"Original local Text draft status");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();const auto observed=w.host.session;const auto status=status_label?status_label->text():QString{};const bool pending_preserved=editor&&editor->toPlainText()==QString::fromStdString(pending);write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Original local Text Apply Window rendered");QJsonObject state{{"signal_count",emitted.count()},{"status",status},{"pending_preserved",pending_preserved},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile metadata(path+"-producer.json");check(metadata.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Original local Text Apply immutable producer readback");metadata.write(QJsonDocument(state).toJson());metadata.close();std::cout<<"INSTANCE_TEXT_CONTEXT mode="<<mode<<" status="<<status.toStdString()<<" pending="<<pending_preserved<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<" history_equal="<<(observed.history()==(success?goal.history():caller.history()))<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1,"One original local Text Apply producer");check(success?status.isEmpty():(pending_preserved&&status=="PROPERTY_CONFLICT: The Instance or source Text changed; reload the draft before applying"),"Exact current raw occurrence commit or retained pending draft/context refusal");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable complete native/raw/source/otherInstance or protected caller saved; pending draft qualified before Save callback");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" Qt editor input/Apply; physical/font repertoire NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_INSTANCE_TEXT_CONTEXT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<6;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Instance Text context cases failed: "+std::to_string(failures));std::cout<<"PASS Instance Text context "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Instance Text context "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
