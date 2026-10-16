#include "window.hpp"
#include <QPointer>
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
const char* names[]={"margin-left","margin-top","margin-right","margin-bottom"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
template<class T>T* control(Window& w,const QString& name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(("Production control missing: "+name).toStdString());}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int mode){
 auto d=empty_document("margin-scrub-"+std::to_string(mode),"composition","board");auto& b=d.compositions.front().artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15,20,1000,800},3,2,7,8}};
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);d.compositions.front().roots={o.id};
 if(mode==1){d.compositions.front().artboards.push_back({"target","Target",1500,0,1200,900});Session seeded(d);seeded.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seeded.document();}
 d.compositions.front().guides={{"layout-guide","Guide","x",10.123456789012345}};
 valid(d);return d;
}
void same(const Session& a,const Session& b){
 if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window snapshot");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native bytes readable");return f.readAll();}
void open(Window& w,const Document& initial,int mode){w.host.session=Session(initial);w.host.edited();w.canvas->set_active_artboard("composition",mode==1?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}

Document goal(const Document& initial,int f,bool inherited,double amount){
 auto d=initial;auto& b=inherited?d.compositions.front().artboards.back():d.compositions.front().artboards.front();
 if(inherited){b.layout=ArtboardLayout{initial.compositions.front().artboards.front().layout->margin,{}};b.template_assignment->margin_overridden=true;}
 auto& m=*b.layout->margin;if(f==0)m.left+=amount;else if(f==1)m.top+=amount;else if(f==2)m.right+=amount;else m.bottom+=amount;
 valid(d);return d;
}
std::vector<Command> commands(const Document& d,bool inherited){
 if(inherited)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.margin",d.compositions.front().artboards.back().layout->margin}}};
 return {SetArtboardLayout{"composition","board",d.compositions.front().artboards.front().layout}};
}

void flow(const QString& root,int mode){
 const int f=mode%4;const bool inherited=f>=2,active=mode>=4;const auto initial=fixture(inherited?1:0);
 const auto first_doc=goal(initial,f,inherited,0.5);Session before(initial),first(initial);first.begin_gesture(0);first.update_gesture(commands(first_doc,inherited));
 check(first.preview_document()==first_doc&&first.document()==initial,"Independent old valid preview BEFORE Window");
 auto replacement=initial;replacement.id+="-different-document";
 auto& source=replacement.compositions.front().artboards.front();source.layout->margin->left+=50;source.layout->margin->top+=50;source.layout->margin->right+=50;source.layout->margin->bottom+=50;
 replacement.objects.at("sentinel").contours.front().points.front().x.literal+=7;
 valid(replacement);Session idle(replacement),foreign(replacement);auto pending=replacement;pending.compositions.front().artboards.front().name="Replacement caller";
 valid(pending);foreign.begin_gesture(0);foreign.update_gesture({UpdateArtboard{"composition",pending.compositions.front().artboards.front()}});
 check(foreign.preview_document()==pending&&foreign.gesture_generation()==first.gesture_generation()&&foreign.revision()==first.revision(),"Independent different Document SAME revision/generation pending snapshot BEFORE Window");
 auto fresh_base=active?foreign:idle;if(active)fresh_base.cancel_gesture();
 const auto fresh_margin_doc=goal(replacement,f,inherited,0.5);
 auto fresh_margin=fresh_base;fresh_margin.begin_gesture(0);fresh_margin.update_gesture(commands(fresh_margin_doc,inherited));fresh_margin.commit_gesture();
 check(fresh_margin.document()==fresh_margin_doc,"Independent refreshed replacement Margin helper target BEFORE Window");
 auto fresh_undone=fresh_margin;fresh_undone.undo(fresh_undone.revision());auto fresh_redone=fresh_undone;fresh_redone.redo(fresh_redone.revision());
 auto fresh_grid_doc=fresh_margin_doc;auto& fresh_board=inherited?fresh_grid_doc.compositions.front().artboards.back():fresh_grid_doc.compositions.front().artboards.front();
 if(inherited){fresh_board.layout->grid=replacement.compositions.front().artboards.front().layout->grid;fresh_board.layout->grid->id=fresh_board.template_assignment->grid_id;fresh_board.template_assignment->grid_overridden=true;}
 fresh_board.layout->grid->bounds.x+=0.5;valid(fresh_grid_doc);
 std::vector<Command> grid_commands;
 if(inherited)grid_commands={ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.grid",fresh_board.layout->grid}}};
 else grid_commands={SetArtboardLayout{"composition","board",fresh_board.layout}};
 auto fresh_grid=fresh_redone;fresh_grid.begin_gesture(fresh_grid.revision());fresh_grid.update_gesture(grid_commands);fresh_grid.commit_gesture();
 check(fresh_grid.document()==fresh_grid_doc,"Independent refreshed Grid helper target BEFORE Window");
 auto fresh_guide_doc=fresh_grid_doc;fresh_guide_doc.compositions.front().guides.front().position+=1;valid(fresh_guide_doc);
 auto fresh_guide=fresh_grid;fresh_guide.begin_gesture(fresh_guide.revision());fresh_guide.update_gesture({UpdateGuide{"composition",fresh_guide_doc.compositions.front().guides.front()}});fresh_guide.commit_gesture();
 check(fresh_guide.document()==fresh_guide_doc,"Independent refreshed Guide helper target BEFORE Window");
 const auto path=root+"/mode-"+QString::number(mode);write(path+"-initial.nect",initial);write(path+"-replacement.nect",replacement);write(path+"-pending.nect",pending);write(path+"-fresh-margin-expected.nect",fresh_margin_doc);write(path+"-fresh-grid-expected.nect",fresh_grid_doc);write(path+"-fresh-guide-expected.nect",fresh_guide_doc);
 QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,inherited?1:0);same(w.host.session,before);
 QPointer<QLineEdit> input=control<QLineEdit>(w,names[f]);reveal(input);input->setFocus();
 if(active){QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=0.5");events();same(w.host.session,first);}
 const auto session_id=w.host.session_id;
 w.host.session=active?foreign:idle;check(w.host.session_id==session_id&&w.host.session.revision()==0,"Exact same Session ID/revision with replacement Document");
 same(w.host.session,active?foreign:idle);
 QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);
 for(const auto letter:QString("+=0.5")){if(!input||!input->isVisible())break;QTest::keyClicks(input,QString(letter));events();}
 const auto& expected=active?foreign:idle;
 std::cout<<"DOCUMENT_OWNERSHIP mode="<<mode<<" input_alive="<<bool(input)<<" document_same="<<(w.host.session.document()==expected.document())<<" preview_same="<<(w.host.session.preview_document()==expected.preview_document())<<" history_same="<<(w.host.session.history()==expected.history())<<" active="<<w.host.session.gesture_active()<<"/"<<expected.gesture_active()<<" generation="<<w.host.session.gesture_generation()<<"/"<<expected.gesture_generation()<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";
 check(w.statusBar()->grab().save(path+"-status.png"),"Actual replacement Document statusbar render");same(w.host.session,expected);
 if(input&&input->isVisible()){QTest::keyClick(input,Qt::Key_Return);events();}same(w.host.session,expected);
 check(w.statusBar()->currentMessage().contains("CONFLICT"),"Old literal controls explicitly refuse another Document");
 // Caller itself ends its pending gesture before legitimate refresh; old view never does.
 if(active)w.host.session.cancel_gesture();same(w.host.session,fresh_base);
 w.host.edited();events();input=control<QLineEdit>(w,names[f]);reveal(input);input->setFocus();
 QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=0.5");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,fresh_margin);
 w.host.save(path+"-fresh-margin-saved.nect");w.host.flush();same(w.host.session,fresh_margin);check(read(path+"-fresh-margin-saved.nect")==read(path+"-fresh-margin-expected.nect"),"Exact legitimate replacement Margin native bytes");
 w.host.session.undo(w.host.session.revision());same(w.host.session,fresh_undone);w.host.session.redo(w.host.session.revision());same(w.host.session,fresh_redone);w.host.edited();events();
 input=control<QLineEdit>(w,"grid-x");reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=0.5");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,fresh_grid);
 w.host.save(path+"-fresh-grid-saved.nect");w.host.flush();same(w.host.session,fresh_grid);check(read(path+"-fresh-grid-saved.nect")==read(path+"-fresh-grid-expected.nect"),"Exact legitimate replacement Grid native bytes");
 const auto visible_fields=[&]{int count=0;for(auto* c:w.findChildren<QLineEdit*>())if(c->isVisible()&&(c->objectName()=="margin-left"||c->objectName()=="grid-x"||c->objectName()=="guide-position-layout-guide"))++count;return count;};
 std::cout<<"SAVE_TIMING before_events="<<visible_fields()<<"\n";same(w.host.session,fresh_grid);
 events();same(w.host.session,fresh_grid);
 std::cout<<"SAVE_TIMING after_events="<<visible_fields()<<"\n";
 check(visible_fields()==3,"Existing Frame context visible after normal Qt event drain, no Frame entry/reset");
 check(w.grab().save(path+"-after-events-window.png"),"Actual Frame after event drain retained");

 input=control<QLineEdit>(w,"guide-position-layout-guide");reveal(input);input->setFocus();QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,"+=1");QTest::keyClick(input,Qt::Key_Return);events();same(w.host.session,fresh_guide);
 w.host.save(path+"-fresh-guide-saved.nect");w.host.flush();same(w.host.session,fresh_guide);check(read(path+"-fresh-guide-saved.nect")==read(path+"-fresh-guide-expected.nect"),"Exact legitimate replacement Guide native bytes");
 w.close();events();same(w.host.session,fresh_guide);
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{
 std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";QTemporaryDir temp;check(temp.isValid(),"Owned scratch valid");
 const auto requested=qEnvironmentVariable("NECT_FRAME_SAVE_TIMING_ROOT"),root=requested.isEmpty()?temp.path():requested;int failures=0;
 for(int mode=0;mode<1;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}
 if(failures)throw std::runtime_error("Independent Document ownership cases failed: "+std::to_string(failures));std::cout<<"PASS Frame Save timing "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Frame Save timing "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
