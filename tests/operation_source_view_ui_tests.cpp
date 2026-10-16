#include <QAction>
#include <QFontMetrics>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTimer>
#include "window.hpp"
#include "semantic_control.hpp"
#include "semantic_slider.hpp"
#include <QMouseEvent>
#include <QPointer>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolButton>
#include <QJsonDocument>
#include <QJsonObject>
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){QPointer<QWidget> alive=c;for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(alive,"Exact production control survives reveal events");check(alive->isVisible()&&alive->isEnabled(),"Exact production control visible and enabled");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}



constexpr double exact_amount=12.34567890123456,exact_rotation=-725.1234567890123;
Document fixture(){
    Session session(empty_document("slider-document","composition","artboard"));
    auto source=default_primitive("built-source","nect.shape.rectangle");
    auto offset=default_operation("offset","nect.shape.offset");offset.parameters.at("amount").literal=exact_amount;
    auto repeater=default_operation("repeater","nect.shape.repeater");repeater.parameters.at("copies").literal=4;
    repeater.parameters.at("rotation").literal=exact_rotation;repeater.parameters.at("position_x").literal=1;
    auto macro_source=source;macro_source.id="macro-source";
    auto macro_offset=offset;macro_offset.id="node-offset";
    auto macro_repeater=repeater;macro_repeater.id="node-repeater";
    MacroDefinitionRevision revision;revision.revision=2;revision.graph_version=2;revision.interface_version=2;
    revision.input={"input","local_paths_and_paint"};revision.output={"output","local_paths_and_paint"};
    revision.nodes={{macro_offset,"offset-in","offset-out"},{macro_repeater,"repeater-in","repeater-out"}};
    revision.edges={{{"","input"},{"node-offset","offset-in"}},
        {{"node-offset","offset-out"},{"node-repeater","repeater-in"}},
        {{"node-repeater","repeater-out"},{"","output"}}};
    revision.output_mapping={"node-repeater","repeater-out"};
    revision.public_parameters={{"macro.repeater.copies","Published copies","node-repeater","copies","number","scalar","local_paths_and_paint"},
        {"macro.offset.amount","Published amount","node-offset","amount","number","du","local_paths_and_paint"},
        {"macro.repeater.rotation","Published rotation","node-repeater","rotation","number","degree","local_paths_and_paint"}};
    auto initial=revision;initial.revision=1;initial.interface_version=1;
    initial.public_parameters={revision.public_parameters.at(1)};
    MacroDefinition definition;definition.id="definition";definition.label="Slider chain";definition.latest_revision=2;
    definition.revisions={{1,initial},{2,revision}};
    auto second_source=source;second_source.id="second-source";
    auto second_offset=offset;second_offset.id="second-offset";
    auto second_repeater=repeater;second_repeater.id="second-repeater";second_repeater.parameters.at("copies").literal=9;
    session.apply({CreatePrimitive{"composition","","built","Built",source},AddOperation{"built",offset,1},AddOperation{"built",repeater,2},
        CreatePrimitive{"composition","","macro","Macro",macro_source},MacroCommand{CreateMacroDefinition{definition}},
        MacroCommand{InstantiateMacro{"macro","definition","instance",2,1}},
        CreatePrimitive{"composition","","second","Second",second_source},AddOperation{"second",second_offset,1},
        AddOperation{"second",second_repeater,2}},0);
    return session.document();
}





ProcessingEntry& entry(Document& d){for(auto& e:d.objects.at("built").stack)if(e.id=="offset")return e;throw std::runtime_error("Independent target entry missing");}
void flow(const QString& root,int mode){
 auto initial=fixture();const auto target=operation_ref("built","offset","enabled"),source=operation_ref("second","second-offset","enabled");const Expression seed{" ! ref ( \"second\" , \"\" , \"op.second-offset.enabled\" ) ",1},draft{"  ! ref (\n\"second\",\n\"\",\n\"op.second-offset.enabled\"\n)  ",1};Session unlinked(initial);if(mode==1){entry(initial).enabled_driver=source;auto linked=unlinked;linked.apply({LinkOperationEnabled{target,source,false}},0);check(linked.document()==initial,"Independent exact authored linked source Doc BEFORE Window");}if(mode==2){entry(initial).enabled_expression=seed;auto expressed=unlinked;expressed.apply({SetOperationEnabledExpression{target,seed,false}},0);check(expressed.document()==initial,"Independent exact authored expression Doc BEFORE Window");}valid(initial);Session base(initial);auto intended=initial;entry(intended).enabled_driver.reset();entry(intended).enabled_expression=draft;valid(intended);auto canonical_draft=base;canonical_draft.apply({SetOperationEnabledExpression{target,draft,mode!=0}},0);check(canonical_draft.document()==intended,"Independent exact multiline draft is native/Core-valid BEFORE Window, without authoring it in actual view test");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",initial);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QPushButton> driver=control<QPushButton>(w,"operation-enabled-driver-offset");reveal(driver);bool visited=false;QString callback_error;
 QTimer::singleShot(0,&w,[&]{QPointer<QDialog> dialog=w.findChild<QDialog*>("operation-enabled-dialog-offset");try{
  check(dialog&&dialog->isVisible(),"Original real source dialog visible");visited=true;QPointer<QComboBox> action=dialog->findChild<QComboBox*>("operation-enabled-mode-offset"),sources=dialog->findChild<QComboBox*>("operation-enabled-source-offset");QPointer<QLineEdit> search=dialog->findChild<QLineEdit*>("operation-enabled-source-search-offset");QPointer<QPlainTextEdit> editor=dialog->findChild<QPlainTextEdit*>("operation-enabled-expression-source-offset");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(action&&sources&&search&&editor&&buttons,"Original exact source controls exist");if(mode==2)check(editor->toPlainText()==QString::fromStdString(seed.source),"Original committed exact expression populates draft BEFORE editing");dialog->resize(800,600);for(int n=0;n<3;++n)events();check(dialog&&dialog->width()>=800&&dialog->height()>=600,"Owned larger source viewport established via QWidget resize API");const QSize original=dialog->size();const auto link=action->findData("link"),expression=action->findData("expression"),none=action->findData("none");check(link>=0&&expression>=0&&none>=0,"All original view modes offered");action->setCurrentIndex(expression);editor->setPlainText(QString::fromStdString(draft.source));action->setCurrentIndex(link);search->setText("op.second-offset.enabled");check(sources->count()==1&&sources->itemText(0).contains("[second-offset]"),"Exact stable source candidate selected through original search");sources->setCurrentIndex(0);const auto selected_data=sources->currentData();const auto selected_text=sources->currentText();
  for(const auto next:{expression,none,link,expression,link}){action->setCurrentIndex(next);for(int n=0;n<3;++n)events();check(dialog&&action&&sources&&search&&editor,"Exact source controls survive each settled visibility transition");check(dialog->width()>=original.width()&&dialog->height()>=original.height(),"Repeated mode changes never shrink owned larger dialog");check(editor->toPlainText()==QString::fromStdString(draft.source)&&search->text()=="op.second-offset.enabled"&&sources->currentData()==selected_data&&sources->currentText()==selected_text,"Exact multiline draft/search and selected stable source candidate survive hidden/visible modes");same(w.host.session,base);}
  action->setCurrentIndex(expression);events();check(dialog&&editor&&editor->isVisible()&&dialog->width()>=original.width()&&dialog->height()>=original.height(),"Larger final expression view retained");check(dialog->grab().save(path+"-view.png"),"Actual larger source dialog/multiline draft rendered");QJsonObject record{{"mode",mode},{"original_width",original.width()},{"original_height",original.height()},{"final_width",dialog->width()},{"final_height",dialog->height()},{"draft",editor->toPlainText()},{"search",search->text()},{"source_label",sources->currentText()},{"source_data",sources->currentData().toInt()},{"device_pixel_ratio",dialog->devicePixelRatioF()}};QFile file(path+"-view.json");const auto bytes=QJsonDocument(record).toJson(QJsonDocument::Indented);check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(bytes)==bytes.size(),"Actual larger-view state readback retained once");QSignalSpy cancelled(buttons,&QDialogButtonBox::rejected);QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);check(cancelled.count()==1&&!dialog->isVisible(),"Original real Cancel closes without committing draft/source selection");
 }catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());}if(dialog&&dialog->isVisible())dialog->reject();});
 QSignalSpy opened(driver,&QAbstractButton::clicked);QTest::mouseClick(driver,Qt::LeftButton);events();check(visited&&opened.count()==1,"One actual original source opener and bounded modal completion");check(callback_error.isEmpty(),callback_error.toUtf8().constData());same(w.host.session,base);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Immutable full native preserves original None/Link/Expression source/literal/graph/pin and History despite uncommitted multiline draft");w.close();events();same(w.host.session,base);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_OPERATION_SOURCE_VIEW_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Operation source view cases failed: "+std::to_string(failures));std::cout<<"PASS Operation source view "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Operation source view "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
