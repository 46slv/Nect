#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
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
const std::string shared_content="  Literal 日本語\n\tCafé e\xcc\x81 🐈\n\n";
const std::string changed_content="\n  Replacement 日本語\n\tCafé e\xcc\x81 🐈  \n";
const std::vector<Id> pair={"first","second"};

Document fixture(bool mixed=false,bool driven=false){
    auto document=empty_document("literal-content-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Unrelated source 日本語\n");
    source.text->parameters.at("font_size").literal=24;
    Object first=source;first.id="first";first.name="Same name";
    first.text=default_text("first-text",shared_content);
    first.text->family="Arial";first.text->locale="en-US";first.text->layout="frame";
    first.text->direction="vertical";first.text->alignment="center";
    first.text->family_driver=TextFamilyDriver{{"source","","text.family"}};
    first.text->locale_driver=TextLocaleDriver{{"source","","text.locale"}};
    first.text->direction_driver=TextDirectionDriver{{"source","","text.direction"}};
    first.text->layout_driver=TextLayoutDriver{{"source","","text.layout"}};
    first.text->alignment_driver=TextAlignmentDriver{{"source","","text.alignment"}};
    first.text->weight_expression=Expression{"500",1};
    first.text->italic_driver=TextItalicDriver{Expression{"true",1}};
    first.text->parameters.at("font_size").binding=Binding{{"source","","text.font_size"},1.25,0};
    first.text->parameters.at("tracking").expression=Expression{"2",1};
    first.text->parameters.at("frame_width").literal=271.25;
    first.text->parameters.at("frame_height").literal=136.5;
    first.text->font_features={{"kern",1,"whole_text"},{"liga",0,"whole_text"}};
    first.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object second=source;second.id="second";second.name="Same name";
    second.text=default_text("second-text",mixed?"":shared_content);
    second.text->family="Consolas";second.text->locale="ja-JP";second.text->alignment="end";
    second.text->weight=650;second.text->weight_driver=TextWeightDriver{{"source","","text.weight"},20};
    second.text->italic=true;second.text->italic_driver=TextItalicDriver{Ref{"source","","text.italic"}};
    second.text->parameters.at("font_size").literal=32;
    second.text->parameters.at("frame_width").literal=481.75;
    second.text->parameters.at("frame_height").literal=223.5;
    second.text->font_features={{"kern",0,"whole_text"}};
    second.text->additional_axis_values={{"opsz",21.75}};
    if(driven)second.text->content_driver=TextContentDriver{{"source","","text.content"}};
    Object path;path.id="shape";path.name="Unrelated path";path.kind=Kind::path;
    Contour contour;contour.id="contour";Point begin;begin.id="begin";begin.x.literal=10;begin.y.literal=100;
    Point end;end.id="end";end.x.literal=2010;end.y.literal=100;contour.points={begin,end};path.contours={contour};
    Object follower=source;follower.id="follower";follower.name="Unrelated linked Text";
    follower.text=default_text("follower-text","Retained unrelated literal\r\n");
    follower.text->parameters.at("font_size").literal=24;
    follower.text->content_driver=TextContentDriver{{"source","","text.content"}};
    document.objects.emplace(source.id,source);document.objects.emplace(first.id,first);
    document.objects.emplace(second.id,second);document.objects.emplace(path.id,path);
    document.objects.emplace(follower.id,follower);
    document.compositions.front().roots={"source","first","second","shape","follower"};
    return document;
}



QJsonArray view_json(const std::vector<Canvas::Selection>& selection){QJsonArray result;for(const auto& v:selection)result.append(QJsonObject{{"object",QString::fromStdString(v.object)},{"point",QString::fromStdString(v.point)}});return result;}
void view(Window& w,const std::vector<Canvas::Selection>& selection,const Id& board){check(w.canvas->selections()==selection&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()==board,"Exact independent manualCanvas selectedIDs/Composition/Artboard view");}

class ArrowEvents final:public QObject{public:QPointer<QWidget> target;Window* window=nullptr;int presses=0,releases=0;bool eventFilter(QObject* watched,QEvent* event)override{if(watched==target&&event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton)++presses;if(watched==window&&event->type()==QEvent::MouseButtonRelease&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton)++releases;return false;}};
void flow(const QString& root,int mode){
 const bool success=mode<2;const QString draft=mode==0?"+=2":mode==1?"26.125":"+";const double own=mode==0?26:26.125;
 auto initial=fixture(true);auto& first=initial.objects.at("first").text->parameters.at("font_size");first.literal=24;first.binding.reset();first.expression.reset();auto& second=initial.objects.at("second").text->parameters.at("font_size");second.literal=32.5;second.binding.reset();second.expression=Expression{"32.5",1};valid(initial);Session base(initial);const std::vector<Ref> refs{{"first","","text.font_size"}};
 auto prepared_doc=initial;prepared_doc.objects.at("first").text->parameters.at("font_size").literal=own;valid(prepared_doc);auto prepared=base;prepared.apply({EditProperties{refs,mode==0?2:own,mode==0}},0);check(prepared.document()==prepared_doc&&prepared.history().states.size()==base.history().states.size()+1&&prepared.revision()==1,"Independent manual own scalar/canonicalEditProperties oneHistory BEFOREWindow");
 auto intended=prepared_doc;intended.objects.at("first").text->parameters.at("font_size").binding=Binding{{"follower","","text.font_size"},1,0};valid(intended);auto goal=prepared;goal.apply({LinkProperties{refs,{"follower","","text.font_size"},false}},1);check(goal.document()==intended&&goal.history().states.size()==base.history().states.size()+2&&goal.revision()==2,"Independent manual own fallback/first-only Link/secondExpression/twoHistory BEFOREWindow");auto undone=goal;undone.undo(2);auto undo_history=goal.history();undo_history.current_id=prepared.history().current_id;check(undone.document()==prepared_doc&&undone.history()==undo_history&&undone.revision()==3&&undone.preview_document()==prepared_doc&&!undone.gesture_active(),"Independent LinkUndo preserves own scalar/fullHistory BEFOREWindow");auto undo_all=undone;undo_all.undo(3);auto all_history=goal.history();all_history.current_id=base.history().current_id;check(undo_all.document()==initial&&undo_all.history()==all_history&&undo_all.revision()==4&&undo_all.preview_document()==initial&&!undo_all.gesture_active(),"Independent ownEditUndo restoresfullcaller BEFOREWindow");auto redo_own=undo_all;redo_own.redo(4);check(redo_own.document()==prepared_doc&&redo_own.history()==undo_history&&redo_own.revision()==5&&redo_own.preview_document()==prepared_doc,"Independent ownEditRedo BEFOREWindow");auto redone=redo_own;redone.redo(5);check(redone.document()==intended&&redone.history()==goal.history()&&redone.revision()==6&&redone.preview_document()==intended,"Independent LinkRedo BEFOREWindow");
 const std::vector<Canvas::Selection> targets{{"first",{}}},source{{"follower",{}}};const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?intended:initial);write(path+"-prepared-expected.nect",success?prepared_doc:initial);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections(targets);events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Properties dock");dock->show();dock->raise();events();
 QPointer<QLineEdit> input;QPointer<QPushButton> pick;for(auto* c:w.findChildren<QLineEdit*>()){const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(c->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")input=c;}for(auto* c:w.findChildren<QPushButton*>("property-source-pick")){const auto ref=QJsonDocument::fromJson(c->property("nect-reference").toByteArray()).object();if(c->isVisible()&&ref["object"].toString()=="first"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")pick=c;}check(input&&pick,"Exactoriginal unlinked fontSize field/arrow");reveal(pick);reveal(input);input->setFocus(Qt::OtherFocusReason);events();input->selectAll();QTest::keyClicks(input,draft);check(input->text()==draft&&input->isModified(),"Owned Qtkeyboard leavesnumericdraft pending");same(w.host.session,base);view(w,targets,"artboard");
 const QPoint global=pick->mapToGlobal(pick->rect().center());QSignalSpy clicked(pick,&QPushButton::clicked);ArrowEvents actors;actors.target=pick;actors.window=&w;qApp->installEventFilter(&actors);QTest::mousePress(pick,Qt::LeftButton,Qt::NoModifier,pick->rect().center());events();const auto after_prepare=w.host.session;write(path+"-prepared-observed.nect",after_prepare.document());check(w.grab().save(path+"-prepared-window.png"),"Styled Window afteroriginalarrow preparation");const bool prepared_equal=after_prepare.document()==(success?prepared_doc:initial)&&after_prepare.history()==(success?prepared.history():base.history())&&after_prepare.preview_document()==(success?prepared_doc:initial)&&after_prepare.revision()==(success?1:0)&&!after_prepare.gesture_active();const bool grab_owned=QWidget::mouseGrabber()==&w;const bool invalid_draft_retained=input&&input->text()==draft&&input->isModified();QTest::mouseRelease(&w,Qt::LeftButton,Qt::NoModifier,w.mapFromGlobal(global));qApp->removeEventFilter(&actors);events();QPointer<QDialog> picker=w.findChild<QDialog*>("property-source-picker");const bool picker_open=picker&&picker->isVisible();int browse_count=0,link_count=0;
 if(success&&picker_open){auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();check(list&&buttons,"Originalchooser controls");QListWidgetItem* selected=nullptr;for(int i=0;i<list->count();++i){auto* row=list->item(i);const auto ref=QJsonDocument::fromJson(row->data(Qt::UserRole).toByteArray()).object();if(ref["object"].toString()=="follower"&&ref["point"].toString().isEmpty()&&ref["field"].toString()=="text.font_size")selected=row;}check(selected&&!selected->isHidden()&&(selected->flags()&Qt::ItemIsEnabled),"ExactcompatibleoriginalsourceRef");QSignalSpy browsed(list,&QListWidget::currentItemChanged);list->setCurrentItem(selected);events();browse_count=browsed.count();view(w,source,"artboard");check(picker->grab().save(path+"-picker.png"),"Styled originalchooser beforeLink");auto* ok=buttons->button(QDialogButtonBox::Ok);check(ok,"OriginalOk");QSignalSpy linked(ok,&QPushButton::clicked);ok->click();events();link_count=linked.count();}
 const auto observed=w.host.session;const auto status=w.statusBar()->currentMessage();write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Styled Window final");const auto& expected=success?goal:base;QJsonObject state{{"draft",draft},{"arrow_press_count",actors.presses},{"window_release_count",actors.releases},{"arrow_signal_count",clicked.count()},{"grab_owned_after_prepare",grab_owned},{"prepared_equal",prepared_equal},{"invalid_draft_retained",invalid_draft_retained},{"picker_open_after_release",picker_open},{"picker_visible_after",picker&&picker->isVisible()},{"browse_signal_count",browse_count},{"link_signal_count",link_count},{"status",status},{"document_equal",observed.document()==expected.document()},{"history_equal",observed.history()==expected.history()},{"preview_equal",observed.preview_document()==expected.preview_document()},{"view_equal",w.canvas->selections()==targets&&w.canvas->active_composition()=="composition"&&w.canvas->active_artboard()=="artboard"}};QFile meta(path+"-producer.json");check(meta.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutableoriginalactors/pending/readback");meta.write(QJsonDocument(state).toJson());meta.close();std::cout<<"TEXT_SINGLE_SCALAR_ARROW_PENDING mode="<<mode<<" prep="<<prepared_equal<<" picker="<<picker_open<<" doc="<<state["document_equal"].toBool()<<" status="<<status.toStdString()<<"\n";
 check(actors.presses==1&&actors.releases==1&&clicked.count()==0,"Exactoriginalarrow press/ownedWindowrelease consumed byWindow");check(prepared_equal,"Originalarrow own prep matchesindependent EditProperties orinvalidneutral");check(grab_owned==success&&picker_open==success,"Successfulprep ownsgrab andopenschooser; invalidrefusesboth");check(success?browse_count==1&&link_count==1:invalid_draft_retained,"Originalchooser actors orretained invaliddraft");same(w.host.session,expected);view(w,targets,"artboard");check(!(picker&&picker->isVisible()),"Chooser closed afterLink orabsentinvalid");check(success?status.isEmpty():status=="INVALID_VALUE: Enter a number, += / -= adjustment, or =expression","Exactsuccess orinvalidpending status");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,expected);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actualcomplete ownprepLink/invalidnativeSave");if(success){w.host.session.undo(2);same(w.host.session,undone);w.host.session.undo(3);same(w.host.session,undo_all);w.host.session.redo(4);same(w.host.session,redo_own);w.host.session.redo(5);same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" actualproductstyle/Qt originalarrow ownpending; clipboard/physical/geometry/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_SINGLE_SCALAR_ARROW_PENDING_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<3;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text single scalar arrow pending cases failed: "+std::to_string(failures));std::cout<<"PASS Text single scalar arrow pending "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text single scalar arrow pending "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
