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
Document fixture(){
 auto d=empty_document("size-document","composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.width_driver=ArtboardSizeDriver{Ref{"parent","","artboard.width"}};b.height_driver=ArtboardSizeDriver{Ref{"parent","","artboard.height"}};
 b.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 comp.artboards.push_back({"parent","Parent",1600,0,1500,1000});comp.artboards.push_back({"other","Other",3200,0,1800,1200});
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};valid(d);return d;
}
void same(const Session& a,const Session& b){
 std::cout<<"SNAPSHOT doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}


QString axis(bool width){return width?"width":"height";}
Expression intended_source(bool width){return {std::string("ref(\"other\",\"\",\"artboard.")+(width?"width":"height")+"\") +\n1",1};}
Ref target(bool width){return {"board","",width?"artboard.width":"artboard.height"};}
QPlainTextEdit* editor(Window& w,bool width){return control<QPlainTextEdit>(w,"artboard-"+axis(width)+"-expression");}
QCheckBox* consent(Window& w,bool width){return control<QCheckBox>(w,"artboard-"+axis(width)+"-replace");}
void draft(Window& w,bool width,const QString& source,bool replace){editor(w,width)->setPlainText(source);consent(w,width)->setChecked(replace);events();}
void retained(Window& w,bool primary,const QString& first,const QString& second,bool first_replace,bool second_replace){check(editor(w,primary)->toPlainText()==first&&editor(w,!primary)->toPlainText()==second,"Both exact private source strings survive normal event-ready refresh");check(consent(w,primary)->isChecked()==first_replace&&consent(w,!primary)->isChecked()==second_replace,"Each private explicit replacement intent stays with its target");}
void key(Window& w,bool width,Qt::Key key,Qt::KeyboardModifiers modifiers=Qt::NoModifier){auto* e=editor(w,width);reveal(e);e->setFocus();QTest::keyClick(e,key,modifiers);events();}

void consumers(const QString& root,int mode){
 const bool width=mode==6;const auto initial=fixture();Session base(initial);auto linked_doc=initial;auto& linked_board=linked_doc.compositions.front().artboards.front();(width?linked_board.width_driver:linked_board.height_driver)=ArtboardSizeDriver{Ref{"other","",width?"artboard.width":"artboard.height"}};valid(linked_doc);
 auto unlinked_doc=linked_doc;auto& unlinked_board=unlinked_doc.compositions.front().artboards.front();if(width){unlinked_board.width=1800;unlinked_board.width_driver.reset();}else {unlinked_board.height=1200;unlinked_board.height_driver.reset();}valid(unlinked_doc);
 auto linked=base;linked.apply({LinkArtboardSize{target(width),{"other","",width?"artboard.width":"artboard.height"},true}},0);check(linked.document()==linked_doc,"Independent Link consumes own expression draft with exact full intended Document BEFORE Window");
 auto unlinked=linked;unlinked.apply({UnlinkArtboardSize{target(width)}},linked.revision());check(unlinked.document()==unlinked_doc,"Independent Unlink freezes current source with full intended Document BEFORE Window");auto undone=unlinked;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-linked-expected.nect",linked_doc);write(path+"-unlinked-expected.nect",unlinked_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,base);
 draft(w,width,QString::fromStdString(intended_source(width).source),true);draft(w,!width,"ref(",false);same(w.host.session,base);
 auto* source=control<QComboBox>(w,"artboard-"+axis(width)+"-link-source");const int index=source->findText("Other · "+axis(width)+" (other/artboard."+axis(width)+")");check(index>=0,"Exact current stable Link source identity exists");source->setCurrentIndex(index);
 auto click=[&](const QString& suffix){auto* button=control<QPushButton>(w,"artboard-"+axis(width)+suffix);reveal(button);QSignalSpy observed_signal(button,&QPushButton::clicked);QTest::mouseClick(button,Qt::LeftButton);check(observed_signal.count()>0,"Exact source consumer clicked");events();};
 click("-link");same(w.host.session,linked);retained(w,width,"","ref(",false,false);
 click("-unlink");same(w.host.session,unlinked);retained(w,width,"","ref(",false,false);check(!consent(w,width)->isEnabled(),"Unlinked literal has no source-replacement consent");
 w.host.save(path+"-unlinked-saved.nect");w.host.flush();events();same(w.host.session,unlinked);retained(w,width,"","ref(",false,false);check(read(path+"-unlinked-saved.nect")==read(path+"-unlinked-expected.nect"),"Only committed Link/Unlink target enters exact native bytes, peer draft remains private");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}

void primary_flow(const QString& root,int mode){
 const bool primary=mode%2==0;const auto initial=fixture();Session base(initial);const auto first_source=intended_source(primary),second_source=intended_source(!primary);const auto first_text=QString::fromStdString(first_source.source),second_text=QString::fromStdString(second_source.source),incomplete=QString("ref(");
 auto first_doc=initial;auto& first_board=first_doc.compositions.front().artboards.front();(primary?first_board.width_driver:first_board.height_driver)=ArtboardSizeDriver{first_source};valid(first_doc);
 auto both_doc=first_doc;auto& both_board=both_doc.compositions.front().artboards.front();(primary?both_board.height_driver:both_board.width_driver)=ArtboardSizeDriver{second_source};valid(both_doc);
 auto first=base;first.apply({SetArtboardSizeExpression{target(primary),first_source,true}},0);check(first.document()==first_doc,"Independent first explicit source equals full manual intended Document BEFORE Window");
 auto both=first;both.apply({SetArtboardSizeExpression{target(!primary),second_source,true}},first.revision());check(both.document()==both_doc,"Independent sibling explicit source equals full manual intended Document BEFORE Window");auto undone=both;undone.undo(undone.revision());auto redone=undone;redone.redo(redone.revision());
 auto replacement=initial;replacement.id+="-other-document";replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;valid(replacement);Session replaced(replacement);
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial-expected.nect",initial);write(path+"-first-expected.nect",first_doc);write(path+"-both-expected.nect",both_doc);write(path+"-replacement-expected.nect",replacement);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);w.host.session=base;w.host.edited();w.canvas->set_active_artboard("composition","board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();same(w.host.session,base);
 draft(w,primary,first_text,true);draft(w,!primary,incomplete,false);retained(w,primary,first_text,incomplete,true,false);same(w.host.session,base);
 if(mode>=4){if(mode==4)w.host.session=replaced;else w.host.session_id+="-replacement";w.host.edited();events();same(w.host.session,mode==4?replaced:base);retained(w,primary,"","",false,false);w.close();events();same(w.host.session,mode==4?replaced:base);return;}
 if(mode<2){w.host.save(path+"-initial-saved.nect");w.host.flush();events();same(w.host.session,base);check(read(path+"-initial-saved.nect")==read(path+"-initial-expected.nect"),"Save excludes both valid and incomplete private source drafts");}else {w.host.edited();events();same(w.host.session,base);}
 std::cout<<"SIZE_DRAFT mode="<<mode<<" first="<<editor(w,primary)->toPlainText().toStdString()<<" peer="<<editor(w,!primary)->toPlainText().toStdString()<<" consent="<<consent(w,primary)->isChecked()<<"/"<<consent(w,!primary)->isChecked()<<"\n";check(editor(w,primary)->grab().save(path+"-draft.png"),"Actual retained source editor render");retained(w,primary,first_text,incomplete,true,false);
 key(w,!primary,Qt::Key_Escape);same(w.host.session,base);retained(w,primary,first_text,"",true,false);
 draft(w,!primary,second_text,true);same(w.host.session,base);key(w,primary,Qt::Key_Return,Qt::ControlModifier);same(w.host.session,first);retained(w,primary,first_text,second_text,false,true);
 w.host.save(path+"-first-saved.nect");w.host.flush();events();same(w.host.session,first);check(read(path+"-first-saved.nect")==read(path+"-first-expected.nect"),"Save contains only committed first source, not sibling draft");retained(w,primary,first_text,second_text,false,true);
 key(w,!primary,Qt::Key_Return,Qt::ControlModifier);same(w.host.session,first);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Sibling draft retains original revision through another source commit and Save");retained(w,primary,first_text,second_text,false,true);
 key(w,!primary,Qt::Key_Escape);same(w.host.session,first);retained(w,primary,first_text,"",false,false);
 draft(w,!primary,second_text,true);same(w.host.session,first);key(w,!primary,Qt::Key_Return,Qt::ControlModifier);same(w.host.session,both);retained(w,primary,first_text,second_text,false,false);
 w.host.save(path+"-both-saved.nect");w.host.flush();events();same(w.host.session,both);check(read(path+"-both-saved.nect")==read(path+"-both-expected.nect"),"Both explicitly committed source native bytes match full intended Document");
 w.host.session.undo(w.host.session.revision());same(w.host.session,undone);w.host.session.redo(w.host.session.revision());same(w.host.session,redone);w.close();events();same(w.host.session,redone);
}
void flow(const QString& root,int mode){if(mode>=6)consumers(root,mode);else primary_flow(root,mode);}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_SIZE_DRAFT_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}if(failures)throw std::runtime_error("Size draft cases failed: "+std::to_string(failures));std::cout<<"PASS Size draft "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Size draft "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
