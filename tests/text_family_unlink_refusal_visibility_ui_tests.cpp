#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
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
const std::string shared_content="  Literal 日本語\n\tCafé e\xcc\x81 🐈\n\n";
const std::string changed_content="\n  Replacement 日本語\n\tCafé e\xcc\x81 🐈  \n";
const std::vector<Id> pair={"first","second"};

Document fixture(bool mixed=false,bool driven=false){
    auto document=empty_document("literal-content-document","composition","artboard");
    Object source;source.id="source";source.name="Source";source.kind=Kind::text;
    source.text=default_text("source-text","Unrelated source 日本語\n");
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
    follower.text->content_driver=TextContentDriver{{"source","","text.content"}};
    document.objects.emplace(source.id,source);document.objects.emplace(first.id,first);
    document.objects.emplace(second.id,second);document.objects.emplace(path.id,path);
    document.objects.emplace(follower.id,follower);
    document.compositions.front().roots={"source","first","second","shape","follower"};
    return document;
}



void flow(const QString& root,int mode){
 auto initial=fixture(true);initial.objects.at("source").text->family="Nect Source Family";initial.objects.at("follower").text->family="Nect Source Family";initial.objects.at("second").text->family_driver=TextFamilyDriver{{"source","","text.family"}};valid(initial);Session base(initial);auto caller_doc=initial;
 if(mode==1){caller_doc.objects.at("first").text->family_driver=TextFamilyDriver{{"follower","","text.family"}};auto typed=base;typed.apply({LinkTextFamily{{"first","","text.family"},{"follower","","text.family"},true}},0);check(typed.document()==caller_doc&&text_family_property(caller_doc,{"first","","text.family"}).evaluated==text_family_property(initial,{"first","","text.family"}).evaluated,"Independent full newtargetRef withsameeval equals canonical explicit LinkTextFamily BEFORE Window");}
 if(mode==2){caller_doc.objects.at("source").text->family="Changed Source Family";auto typed=base;typed.apply({UpdateText{"source",*caller_doc.objects.at("source").text}},0);check(typed.document()==caller_doc,"Independent full upstreamfamily replacement equals canonical UpdateText BEFORE Window");}
 if(mode==3||mode==4){auto& t=*caller_doc.objects.at("second").text;if(mode==3)t.parameters.at("frame_width").literal=498.1234567890123;else t.locale="fr-FR";auto typed=base;typed.apply({UpdateText{"second",t}},0);check(typed.document()==caller_doc,"Independent full unrelated framewidth orotherfieldlocale replacement equals canonical UpdateText BEFORE Window");}
 valid(caller_doc);Session caller(caller_doc);auto intended=caller_doc;for(const auto& id:pair){auto& t=*intended.objects.at(id).text;t.family=mode==2?"Changed Source Family":"Nect Source Family";t.family_driver.reset();}valid(intended);auto goal=caller;goal.apply({UnlinkTextFamily{{"first","","text.family"}},UnlinkTextFamily{{"second","","text.family"}}},0);check(goal.document()==intended&&goal.revision()==1&&goal.history().states.size()==caller.history().states.size()+1,"Independent full manually intended family Unlink freezes each evaluated string andclears onlyfamily Ref; canonical oneHistory BEFORE Window");auto undone=goal;undone.undo(1);check(undone.document()==caller_doc,"Independent fullrawfamily Unlink Undo BEFORE Window");auto redone=undone;redone.redo(2);check(redone.document()==intended,"Independent fullrawfamily Unlink Redo BEFORE Window");const bool success=mode==0||mode==3||mode==4;
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-expected.nect",success?goal.document():caller.document());QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.resize(1400,1100);w.show();events();w.canvas->set_selections({{"first",{}},{"second",{}}});events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Original Font family source Property dock");dock->show();dock->raise();events();QPointer<QComboBox> field=control<QComboBox>(w,"text-string-source-batch-field");check(field&&field->currentData().toString()=="text.family","Original Font family source field");QPointer<QComboBox> editor=control<QComboBox>(w,"text-string-source-batch-mode");reveal(editor);editor->setFocus(Qt::OtherFocusReason);events();check(editor&&editor->currentData().toString().isEmpty(),"Original noaction source ComboBox");QSignalSpy drafted(editor,qOverload<int>(&QComboBox::currentIndexChanged));QTest::keyClick(editor,Qt::Key_Down);QTest::keyClick(editor,Qt::Key_Down);events();check(editor&&editor->currentData().toString()=="unlink"&&drafted.count()==2,"Original modeComboBox keyboard exact Unlink sources pending draft");same(w.host.session,base);QPointer<QPushButton> producer=control<QPushButton>(w,"text-string-source-batch-apply");reveal(producer);QPointer<QLabel> label=producer->parentWidget()->findChild<QLabel*>("text-string-source-batch-status");check(label,"Original family Unlink local status");w.host.session=caller;same(w.host.session,caller);QSignalSpy emitted(producer,&QPushButton::clicked);producer->click();events();const auto observed=w.host.session;const auto status=label?label->text():QString{};const bool pending=editor&&field&&editor->currentData().toString()=="unlink"&&field->currentData().toString()=="text.family";write(path+"-observed.nect",observed.document());check(w.grab().save(path+"-window.png"),"Actual productstyled original Font family Unlink Window rendered");const auto shown=label->visibleRegion().boundingRect(),rect=label->rect();const bool fully_visible=!rect.isEmpty()&&shown.contains(rect),editor_visible=editor->visibleRegion().boundingRect().contains(editor->rect()),field_visible=field->visibleRegion().boundingRect().contains(field->rect()),apply_visible=producer->visibleRegion().boundingRect().contains(producer->rect());auto* cancel=producer->parentWidget()->findChild<QPushButton*>("text-string-source-batch-cancel");check(cancel,"Original Fontfamily Unlink Cancel available");const bool cancel_visible=cancel->visibleRegion().boundingRect().contains(cancel->rect());const auto point=label->mapTo(&w,QPoint{});check(label->parentWidget()->grab().save(path+"-panel.png"),"Actual source Unlink panel withoutforcedlayout/postrefusalscroll");QJsonObject metadata{{"fully_visible",fully_visible},{"editor_visible",editor_visible},{"field_visible",field_visible},{"apply_visible",apply_visible},{"cancel_visible",cancel_visible},{"label_x",point.x()},{"label_y",point.y()},{"label_width",rect.width()},{"label_height",rect.height()},{"visible_width",shown.width()},{"visible_height",shown.height()},{"window_width",w.width()},{"window_height",w.height()},{"signal_count",emitted.count()},{"draft_signal_count",2},{"status",status},{"pending_preserved",pending},{"document_equal",observed.document()==(success?goal.document():caller.document())},{"history_equal",observed.history()==(success?goal.history():caller.history())},{"preview_equal",observed.preview_document()==(success?goal.preview_document():caller.preview_document())}};QFile state(path+"-producer.json");check(state.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Immutable original family Unlink producer readback");state.write(QJsonDocument(metadata).toJson());state.close();std::cout<<"TEXT_FAMILY_UNLINK_REFUSAL_VISIBILITY mode="<<mode<<" status="<<status.toStdString()<<" pending="<<pending<<" doc_equal="<<(observed.document()==(success?goal.document():caller.document()))<<"\n";same(w.host.session,success?goal:caller);check(emitted.count()==1,"One original Font family Unlink Apply");check(success?label==nullptr:(pending&&status=="PROPERTY_CONFLICT: The retained Text source changed; refresh before unlinking"),"Current full Unlink or exact retained targetsource refusal withUnlink/Fontfamily draft preserved");std::cout<<"TEXT_FAMILY_UNLINK_REFUSAL_VISIBILITY full="<<fully_visible<<" field="<<field_visible<<" mode="<<editor_visible<<" apply="<<apply_visible<<" label="<<rect.width()<<"x"<<rect.height()<<" visible="<<shown.width()<<"x"<<shown.height()<<" at="<<point.x()<<","<<point.y()<<"\n";check(fully_visible&&field_visible&&editor_visible&&apply_visible&&cancel_visible,"Original Fontfamily Unlink refusal/field/mode/Apply/Cancel fullyvisible after naturalQtsettling; fullcaller/pendingunchanged");w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,success?goal:caller);check(read(path+"-saved.nect")==read(path+"-expected.nect")&&QFile::copy(path+"-saved.nect",path+"-snapshot.nect"),"Actual immutable full Text/caller native saved; pending beforeSave only");if(success){w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);}w.close();events();
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);app.setStyleSheet(application_style_sheet());std::cout.setf(std::ios::unitbuf);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<" productstyle/Qt family Unlink; physical/fonts NOT_RUN\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_TEXT_FAMILY_UNLINK_REFUSAL_VISIBILITY_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=1;mode<2;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Text family unlink refusal visibility cases failed: "+std::to_string(failures));std::cout<<"PASS Text family unlink refusal visibility "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Text family unlink refusal visibility "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
