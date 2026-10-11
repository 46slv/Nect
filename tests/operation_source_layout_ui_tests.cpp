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




const QString instructions="The expression can use true, false, or an optional negation of a built-in operation enabled Ref in this Composition. Apply commits one Session command; Cancel leaves it unchanged.";
bool label_fits(QDialog* dialog,QLabel* label,const QString& expected,const QString& path,const QString& phase){
 check(dialog&&label&&dialog->isVisible()&&label->isVisible()&&label->text()==expected,"Exact original source paragraph/diagnostic visible after settled events");const auto content=label->contentsRect();const QFontMetrics fm(label->font());const auto needed=fm.boundingRect(QRect(0,0,content.width(),1000000),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,expected);const QRect bounds(QPoint(0,0),dialog->size()),actual(label->mapTo(dialog,QPoint(0,0)),label->size());const bool fit=content.height()+1>=needed.height()&&content.width()+1>=needed.width()&&bounds.contains(actual);QJsonObject record{{"phase",phase},{"text",expected},{"dialog_width",dialog->width()},{"dialog_height",dialog->height()},{"available_width",content.width()},{"available_height",content.height()},{"required_width",needed.width()},{"required_height",needed.height()},{"font_family",label->font().family()},{"font_point_size",label->font().pointSizeF()},{"device_pixel_ratio",dialog->devicePixelRatioF()},{"fits",fit}};QFile file(path+"-"+phase+"-metrics.json");const auto bytes=QJsonDocument(record).toJson(QJsonDocument::Indented);check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(bytes)==bytes.size(),"Actual settled source label measurement retained once");check(dialog->grab().save(path+"-"+phase+".png"),"Actual settled source dialog rendered before geometry assertion");std::cout<<"OPERATION_SOURCE_LAYOUT phase="<<phase.toStdString()<<" available="<<content.width()<<"x"<<content.height()<<" required="<<needed.width()<<"x"<<needed.height()<<" dialog="<<dialog->width()<<"x"<<dialog->height()<<" fit="<<fit<<"\n";return fit;
}
void flow(const QString& root,int mode){
 auto initial=fixture();valid(initial);Session base(initial);const auto target=operation_ref("built","offset","enabled");const Expression invalid{"not_boolean",1};auto refused=base;QString expected_error;try{refused.apply({SetOperationEnabledExpression{target,invalid,false}},0);throw std::runtime_error("Independent invalid expression unexpectedly accepted");}catch(const Error& e){expected_error=QString::fromUtf8(e.what());}check(!expected_error.isEmpty(),"Independent canonical invalid source diagnostic BEFORE Window");same(refused,base);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",initial);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selection("built");events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();QPointer<QPushButton> driver=control<QPushButton>(w,"operation-enabled-driver-offset");reveal(driver);bool visited=false;QString callback_error;
 QTimer::singleShot(0,&w,[&]{QPointer<QDialog> dialog=w.findChild<QDialog*>("operation-enabled-dialog-offset");try{
  check(dialog&&dialog->isVisible(),"Actual original source modal loop entered");visited=true;QPointer<QComboBox> action=dialog->findChild<QComboBox*>("operation-enabled-mode-offset");QPointer<QLabel> status=dialog->findChild<QLabel*>("operation-enabled-status-offset");QPointer<QPlainTextEdit> editor=dialog->findChild<QPlainTextEdit*>("operation-enabled-expression-source-offset");auto* buttons=dialog->findChild<QDialogButtonBox*>();check(action&&status&&editor&&buttons,"Exact existing modal source controls exist");if(mode){const auto index=action->findData(mode==1?"link":"expression");check(index>=0,"Original supported mode exists");action->setCurrentIndex(index);}for(int n=0;n<3;++n)events();check(dialog&&status&&action&&editor,"Original modal controls survive settled natural layout events");bool geometry_ok=label_fits(dialog,status,instructions,path,"instructions");const QRect bounds(QPoint(0,0),dialog->size());for(auto button:{buttons->button(QDialogButtonBox::Apply),buttons->button(QDialogButtonBox::Cancel)})check(button&&button->isVisible()&&bounds.contains(QRect(button->mapTo(dialog,QPoint(0,0)),button->size())),"Existing Apply/Cancel remain within visible dialog bounds");if(mode>=2)check(editor->isVisible()&&bounds.contains(QRect(editor->mapTo(dialog,QPoint(0,0)),editor->size())),"Original expression field stays fully inside dialog");
  if(mode==3){editor->setPlainText(QString::fromStdString(invalid.source));QSignalSpy applied(buttons->button(QDialogButtonBox::Apply),&QAbstractButton::clicked);QTest::mouseClick(buttons->button(QDialogButtonBox::Apply),Qt::LeftButton);for(int n=0;n<3;++n)events();check(applied.count()==1&&dialog&&dialog->isVisible()&&status&&editor&&editor->toPlainText()==QString::fromStdString(invalid.source),"Invalid source Apply remains editable with unchanged draft");same(w.host.session,base);geometry_ok &=label_fits(dialog,status,expected_error,path,"diagnostic");}
  check(geometry_ok,"Whole settled original source instruction/diagnostic text fits its visible label without cropping");
 }catch(const std::exception& e){callback_error=QString::fromUtf8(e.what());}if(dialog&&dialog->isVisible())dialog->reject();});
 QSignalSpy opened(driver,&QAbstractButton::clicked);QTest::mouseClick(driver,Qt::LeftButton);events();check(visited&&opened.count()==1,"One real original source opener and bounded modal completion");same(w.host.session,base);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Source view/draft/refusal/Cancel retains immutable full original native authoring and History");w.close();events();same(w.host.session,base);check(callback_error.isEmpty(),callback_error.toUtf8().constData());
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_OPERATION_SOURCE_LAYOUT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<4;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Operation source layout cases failed: "+std::to_string(failures));std::cout<<"PASS Operation source layout "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Operation source layout "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
