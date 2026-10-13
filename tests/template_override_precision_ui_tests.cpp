#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
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
 auto d=empty_document("frame-edit-doc","comp","source");auto& c=d.compositions.front();c.artboards.front().width=1200.1234567890123;c.artboards.front().height=800.9876543210987;c.artboards.front().layout=ArtboardLayout{Margin{10.1234567890123,20.2345678901234,30.3456789012345,40.4567890123456},Grid{"source-grid",{30.1234567890123,20.2345678901234,200.3456789012345,160.4567890123456},2,2,5.6789012345678,6.7890123456789}};c.artboards.push_back({"A","Target",100,100,640,480});c.artboards.push_back({"P","Independent",2000,0,900,700});
 Object root;root.id="root";root.name="Source";root.kind=Kind::group;root.children={"logo"};root.visible=false;Object logo;logo.id="logo";logo.name="Logo";logo.source=default_primitive("rect","nect.shape.rectangle");d.objects.emplace("root",root);d.objects.emplace("logo",logo);c.roots={"root"};Session s(d);
 s.apply({DefinitionCommand{CreateDefinition{{"def","Shared","root"}}},ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"tmpl","Template","source",Id{"def"}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","tmpl",Id{"content"}}},ArtboardGuideCommand{AddArtboardGuide{"comp","source",{"source-guide","Source Guide","x",20,true}}},ArtboardGuideCommand{AddArtboardGuide{"comp","A",{"local-guide","Local Guide","y",15,true}}},ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","source-guide","position",45.0}}},0);return s.document();
}

void view(Window& w){check(w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="A"&&w.canvas->selections().empty(),"Exact captured composition/target Artboard/no Object selection");}
void flow(const QString& root,int mode){
 const bool margin=mode<2;const bool changed=mode%2==0;const auto initial=fixture();valid(initial);const auto source=evaluate_artboard(initial.compositions.front(),"source");const auto current=evaluate_artboard(initial.compositions.front(),"A");check(current.layout&&current.layout->margin&&current.layout->grid&&source.layout&&current.layout->margin==source.layout->margin,"Independent full effective inherited layout BEFORE Window");
 auto goal_doc=initial;auto& target=goal_doc.compositions.front().artboards[1];const auto assignment=*target.template_assignment;if(!target.layout)target.layout=ArtboardLayout{};std::vector<Command> commands;
 if(margin){auto value=*current.layout->margin;if(changed)value.left=25.5;target.layout->margin=value;target.template_assignment->margin_overridden=true;commands={ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.margin",std::optional<Margin>{value}}}};}
 else{auto value=*current.layout->grid;value.id=assignment.grid_id;if(changed)value.columns=3;target.layout->grid=value;target.template_assignment->grid_overridden=true;commands={ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.grid",std::optional<Grid>{value}}}};}
 valid(goal_doc);Session base(initial),expected(initial);expected.apply(commands,0);check(expected.document()==goal_doc&&expected.revision()==1&&expected.history().states.size()==2&&expected.preview_document()==goal_doc&&!expected.gesture_active(),"Independent manual exact untouched values/one family/canonical History BEFORE Window");
 auto undone=expected;undone.undo(1);auto history=expected.history();history.current_id=base.history().current_id;check(undone.document()==initial&&undone.history()==history&&undone.revision()==2&&undone.preview_document()==initial,"Independent full Undo native/History BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==goal_doc&&redone.history()==expected.history()&&redone.revision()==3&&redone.preview_document()==goal_doc,"Independent full Redo native/History BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",goal_doc);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_active_artboard("comp","A",false);w.canvas->set_selections({});events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();view(w);QPointer<QPushButton> actor=control<QPushButton>(w,"artboard-template-set-override");reveal(actor);same(w.host.session,base);
 int selected_count=0,changed_count=0,apply_count=0;QJsonObject presented;std::exception_ptr failure;QSignalSpy opened(actor,&QPushButton::clicked);
 QTimer::singleShot(0,[&]{QPointer<QDialog> dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());try{check(dialog&&dialog->objectName()=="set-artboard-template-override-dialog","Original public Template override modal");auto* field=dialog->findChild<QComboBox*>("template-override-field");check(field,"Original family selector");QSignalSpy chosen(field,&QComboBox::currentIndexChanged);const auto index=field->findData(margin?QStringLiteral("layout.margin"):QStringLiteral("layout.grid"));check(index>=0,"Exact existing family choice");field->setCurrentIndex(index);selected_count=chosen.count();
 if(changed){if(margin){auto* input=dialog->findChild<QDoubleSpinBox*>("template-margin-left");check(input,"Original Margin left input");QSignalSpy change(input,qOverload<double>(&QDoubleSpinBox::valueChanged));input->setValue(25.5);changed_count=change.count();}else{auto* input=dialog->findChild<QSpinBox*>("template-grid-columns");check(input,"Original Grid columns input");QSignalSpy change(input,qOverload<int>(&QSpinBox::valueChanged));input->setValue(3);changed_count=change.count();}}
 for(auto* number:dialog->findChildren<QDoubleSpinBox*>())presented[number->objectName()]=number->value();check(dialog->grab().save(path+"-override.png"),"Actual product style original override modal BEFORE Apply");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Original Apply control");QPointer<QPushButton> apply=buttons->button(QDialogButtonBox::Apply);check(apply,"Original Apply button");QSignalSpy applied(apply,&QPushButton::clicked);apply->click();apply_count=applied.count();}catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});
 actor->click();events();if(failure)std::rethrow_exception(failure);const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual post-override Window BEFORE Save");
 QJsonObject v{{"mode",mode},{"family",margin?"layout.margin":"layout.grid"},{"one_member_changed",changed},{"open_signal_count",opened.count()},{"family_selection_signal_count",selected_count},{"number_change_signal_count",changed_count},{"apply_signal_count",apply_count},{"presented_numbers",presented},{"status",status},{"document_equal",observed.document()==expected.document()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"history_equal",observed.history()==expected.history()},{"revision",static_cast<qint64>(observed.revision())},{"gesture_active",observed.gesture_active()},{"gesture_generation",static_cast<qint64>(observed.gesture_generation())},{"view_equal",w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="A"&&w.canvas->selections().empty()}};QFile meta(path+"-producer.json");check(meta.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original Template modal readback BEFORE Save");meta.write(QJsonDocument(v).toJson());meta.close();check(opened.count()==selected_count&&selected_count==apply_count&&apply_count==1&&changed_count==(changed?1:0),"Original Set/family/one numeric member/Apply once");same(w.host.session,expected);view(w);check(status=="Template override applied to the captured Artboard","Original success message");
 w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual exact full-native Template override Save");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" actual product style/Qt Template modal; physical/clipboard/fonts/geometry NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEMPLATE_OVERRIDE_PRECISION_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
 for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Template override precision cases failed: "+std::to_string(failures));std::cout<<"PASS Template override precision "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL Template override precision "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
