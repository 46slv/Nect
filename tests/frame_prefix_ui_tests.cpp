#include "window.hpp"
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
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible()&&c->isEnabled(),"Exact production control visible and enabled");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}

double Artboard::* const members[]={&Artboard::x,&Artboard::y,&Artboard::width,&Artboard::height};
const char* fields[]={"x","y","width","height"};
Document fixture(bool inherited,int driven=-1){
 auto d=empty_document("frame-relative","composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.x=32.123456789012345;b.y=64.234567890123456;b.width=1400.345678901234567;b.height=1000.456789012345678;b.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 comp.artboards.push_back({"parent","Parent",3500,0,1500,1100});Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};
 if(inherited){comp.artboards.push_back({"target","Target",2000,0,640,480});Session seed(d);seed.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seed.document();}
 if(driven==2)d.compositions.front().artboards.front().width_driver=ArtboardSizeDriver{Ref{"parent","","artboard.width"}};if(driven==3)d.compositions.front().artboards.front().height_driver=ArtboardSizeDriver{Ref{"parent","","artboard.height"}};valid(d);return d;
}

void flow(const QString& root,int mode){
 const bool inherited=mode==2||mode==3,driven=mode==4||mode==5,unsupported=mode==6||mode==7;const int f=unsupported?mode-6:mode%2+2;const auto initial=fixture(inherited,driven?f:-1);Session base(initial);
 const auto field=std::string("artboard.")+fields[f],id=inherited?std::string("target"):std::string("board");const auto body=QString::fromStdString("  ref(\"parent\",\"\",\""+field+"\") + 1  ");const auto numeric="="+body;
 auto intended=initial;auto& b=inherited?intended.compositions.front().artboards.back():intended.compositions.front().artboards.front();if(!unsupported&&!driven)(f==2?b.width_driver:b.height_driver)=ArtboardSizeDriver{Expression{body.toStdString(),1}};valid(intended);
 auto applied=base;if(mode<4){applied.apply({SetArtboardSizeExpression{{id,"",field},{body.toStdString(),1},false}},0);check(applied.document()==intended,"Independent explicit source equals full manual intended Document BEFORE Window");}
 auto undone=applied;auto redone=applied;if(mode<4){undone.undo(undone.revision());redone=undone;redone.redo(redone.revision());}
 auto replacement=initial;replacement.id+="-replacement";replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);Session replaced(replacement);
 auto advanced_doc=initial;advanced_doc.compositions.front().artboards.front().name="Caller";valid(advanced_doc);auto advanced=base;advanced.apply({UpdateArtboard{"composition",advanced_doc.compositions.front().artboards.front()}},0);check(advanced.document()==advanced_doc,"Independent caller revision BEFORE Window");
 auto foreign=base;foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",advanced_doc.compositions.front().artboards.front()}});check(foreign.preview_document()==advanced_doc&&foreign.document()==initial,"Independent foreign caller pending snapshot BEFORE Window");auto cancelled=foreign;cancelled.cancel_gesture();
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-expected.nect",intended);write(path+"-replacement-expected.nect",replacement);write(path+"-caller-expected.nect",advanced_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.canvas->set_active_artboard("composition",QString::fromStdString(id).toStdString(),false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,base);
 const auto editor_name=QString("artboard-")+fields[f]+"-expression";auto source=[&]{return control<QPlainTextEdit>(w,editor_name);};const auto input_name=QString("artboard-")+fields[f];
 if(mode==8)w.host.session=replaced;else if(mode==9)w.host.session_id+="-replacement";else if(mode==10)w.host.session=advanced;else if(mode==11)w.host.session=foreign;else if(mode==12){w.host.session=foreign;w.host.edited();events();w.host.session.cancel_gesture();same(w.host.session,cancelled);}
 const auto& expected=mode==8?replaced:mode==10?advanced:mode==11?foreign:mode==12?cancelled:base;
 auto* input=control<QLineEdit>(w,input_name);reveal(input);const auto literal=input->text();input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,numeric);QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,expected);
 std::cout<<"FRAME_PREFIX mode="<<mode<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";check(w.statusBar()->grab().save(path+"-status.png"),"Actual native prefix outcome status");
 if(driven){check(input->isReadOnly()&&input->text()==literal,"Actual read-only Qt input cannot replace source with =body");check(source()->toPlainText().isEmpty(),"Read-only source stays untouched");}
 else if(unsupported){check(w.statusBar()->currentMessage().contains("UNSUPPORTED"),"Crop X/Y explicitly state unsupported typed expression");check(control<QPlainTextEdit>(w,"artboard-width-expression")->toPlainText().isEmpty()&&control<QPlainTextEdit>(w,"artboard-height-expression")->toPlainText().isEmpty(),"Unsupported coordinate input never targets a size source");}
 else if(mode>=8){check(w.statusBar()->currentMessage().contains("CONFLICT")||w.statusBar()->currentMessage().contains("GESTURE_ACTIVE"),"Old context/caller/preview-born prefix refused");check(source()->toPlainText().isEmpty(),"Rejected prefix never reaches old or another source editor");}
 else {check(source()->toPlainText()==body,"Exact raw =body handed to existing frozen size source editor");check(control<QLineEdit>(w,input_name)->text()==literal,"Handoff retires numeric text without changing exact authored double");check(source()->hasFocus()&&w.statusBar()->currentMessage().isEmpty(),"Source handoff focuses existing editor without stale numeric error");
  w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect"),"Private numeric handoff source excluded from native Save");check(source()->toPlainText()==body,"Normal event-ready Save keeps handed source draft");
  auto* e=source();reveal(e);e->setFocus();QTest::keyClick(e,Qt::Key_Return,Qt::ControlModifier);events();same(w.host.session,applied);w.host.save(path+"-saved.nect");w.host.flush();events();same(w.host.session,applied);check(read(path+"-saved.nect")==read(path+"-expected.nect"),"Explicit Apply exact full intended native source");w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);
 }
 if(mode==11){w.host.session.cancel_gesture();same(w.host.session,cancelled);}w.close();events();same(w.host.session,mode<4?redone:mode==11?cancelled:expected);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_FRAME_PREFIX_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<13;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Frame prefix cases failed: "+std::to_string(failures));std::cout<<"PASS Frame prefix "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Frame prefix "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
