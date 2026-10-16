#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
const char* names[]={"grid-x","grid-y","grid-width","grid-height"};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(15);}
template<class T>T* control(Window& w,const char* name){for(auto* c:w.findChildren<T*>(name))if(c->isVisible())return c;throw std::runtime_error(std::string("Missing control: ")+name);}
void reveal(QWidget* c){for(auto* p=c->parentWidget();p;p=p->parentWidget())if(auto* s=qobject_cast<QScrollArea*>(p))s->ensureWidgetVisible(c);events();check(c->isVisible(),"Exact production control visible");}
double& value(Grid& g,int field){switch(field){case 0:return g.bounds.x;case 1:return g.bounds.y;case 2:return g.bounds.width;default:return g.bounds.height;}}
double value(const Grid& g,int field){switch(field){case 0:return g.bounds.x;case 1:return g.bounds.y;case 2:return g.bounds.width;default:return g.bounds.height;}}
void valid(const Document& d){validate(d);check(decode(encode(d))==d,"Independent full Document native-valid BEFORE Window");}
Document fixture(int field,bool inherited,bool driven=false){
 auto d=empty_document("bounds-relative-"+std::to_string(field),"composition","board");auto& comp=d.compositions.front();auto& b=comp.artboards.front();b.width=1200;b.height=900;
 b.layout=ArtboardLayout{Margin{10.123456789012345,20.234567890123456,30.345678901234567,40.456789012345678},Grid{"grid",{15.123456789012345,20.234567890123456,1000.1234567890123,800.23456789012345},3,2,7.345678901234567,8.456789012345678}};
 auto& g=*b.layout->grid;g.columns_expression=Expression{"3",1};g.column_gutter_expression=Expression{"7",1};g.row_gutter_driver=Ref{"driver","","artboard.width"};
 if(driven){Expression e{QString::number(value(g,field),'g',17).toStdString(),1};switch(field){case 0:g.bounds_x_expression=e;break;case 1:g.bounds_y_expression=e;break;case 2:g.bounds_width_expression=e;break;default:g.bounds_height_expression=e;}}
 comp.artboards.push_back({"driver","Driver",1500,0,8,7});
 Object o;o.id="sentinel";o.kind=Kind::path;Contour c;c.id="contour";Point p;p.id="point";p.x.literal=51.25;p.y.literal=-21.75;c.points={p};o.contours={c};d.objects.emplace(o.id,o);comp.roots={o.id};
 if(inherited){comp.artboards.push_back({"target","Target",1600,0,1200,900});Session seed(d);seed.apply({ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Template","board",{}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target","template",{}}}},0);d=seed.document();}
 valid(d);return d;
}
Document candidate(const Document& initial,int field,bool inherited,double delta){
 auto d=initial;auto& comp=d.compositions.front();
 if(inherited){auto& target=comp.artboards.back();if(!target.template_assignment->grid_overridden){const auto& source=*initial.compositions.front().artboards.front().layout->grid;Grid grid{target.template_assignment->grid_id,source.bounds,3,2,7,8};target.layout=ArtboardLayout{{},grid};}target.template_assignment->grid_overridden=true;}
 auto& target=inherited?comp.artboards.back():comp.artboards.front();value(*target.layout->grid,field)+=delta;return d;
}
Document wanted(const Document& initial,int field,bool inherited,double delta){auto d=candidate(initial,field,inherited,delta);valid(d);return d;}
std::vector<Command> command(const Document& d,bool inherited){if(inherited)return {ArtboardTemplateCommand{SetArtboardTemplateOverride{"composition","target","layout.grid",d.compositions.front().artboards.back().layout->grid}}};return {SetArtboardLayout{"composition","board",d.compositions.front().artboards.front().layout}};}
void same(const Session& a,const Session& b){
 if(!(a.document()==b.document()&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation()))std::cerr<<"Snapshot doc="<<(a.document()==b.document())<<" preview="<<(a.preview_document()==b.preview_document())<<" history="<<(a.history()==b.history())<<" rev="<<a.revision()<<"/"<<b.revision()<<" active="<<a.gesture_active()<<"/"<<b.gesture_active()<<" gen="<<a.gesture_generation()<<"/"<<b.gesture_generation()<<"\n";
 check(a.document()==b.document()&&encode(a.document())==encode(b.document())&&a.preview_document()==b.preview_document()&&a.history()==b.history()&&a.revision()==b.revision()&&a.gesture_active()==b.gesture_active()&&a.gesture_generation()==b.gesture_generation(),"Full canonical Session equals independent pre-Window expectation");
}
void write(const QString& path,const Document& d){QFile f(path);const auto bytes=encode(d);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(bytes.data(),bytes.size())==static_cast<qint64>(bytes.size()),"Owned independent native oracle retained once");}
QByteArray read(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Owned native file readable");return f.readAll();}
QString prefix(const QString& root,int mode){return root+"/mode-"+QString::number(mode);}
void open(Window& w,const Document& d,bool inherited){w.host.session=Session(d);w.host.edited();w.canvas->set_active_artboard("composition",inherited?"target":"board",false);w.resize(1400,1000);w.show();w.activateWindow();events();auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties exists");dock->show();dock->raise();events();auto* edit=control<QPushButton>(w,"artboard-edit");reveal(edit);edit->click();events();}
void draft(Window& w,int field,const QString& text,bool keyboard=true){auto* input=control<QLineEdit>(w,names[field]);reveal(input);input->setFocus();if(keyboard){QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);}else{input->setText(text);input->setModified(true);check(QMetaObject::invokeMethod(input,"textEdited",Qt::DirectConnection,Q_ARG(QString,text)),"Complete native draft notification delivered");}events();check(input->text()==text,"Raw bounds draft retained");}
void enter(Window& w,int field){QTest::keyClick(control<QLineEdit>(w,names[field]),Qt::Key_Return);events();}
void reset(Window& w,const Document& d,const QString& key){w.host.session=Session(d);w.host.session_id+=key;w.host.edited();events();}
void cold(const QString& root,int mode){const auto expected_doc=wanted(fixture(mode/2,mode%2),mode/2,mode%2,0.5);Session expected(expected_doc);const auto path=prefix(root,mode);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-cold",std::make_unique<FolderLibrary>(settings),&settings);w.host.open(path+"-cold-input.nect");same(w.host.session,expected);w.host.save(path+"-resaved.nect");w.host.flush();same(w.host.session,expected);check(read(path+"-cold-input.nect")==read(path+"-expected.nect")&&read(path+"-resaved.nect")==read(path+"-expected.nect"),"Separate cold exact native save/resave bytes");QTest::qWait(120);}
void flow(const QString& root,int mode){
 const int field=mode/2;const bool inherited=mode%2;const auto initial=fixture(field,inherited),increased=wanted(initial,field,inherited,0.5),subtracted=wanted(increased,field,inherited,-0.5);
 Session before(initial),preview(initial);preview.begin_gesture(0);preview.update_gesture(command(increased,inherited));auto repeated=preview;repeated.cancel_gesture();repeated.begin_gesture(0);repeated.update_gesture(command(increased,inherited));auto committed=repeated;committed.commit_gesture();check(committed.document()==increased,"Canonical command agrees with independent intended Document BEFORE Window");
 auto canceled=preview;canceled.cancel_gesture();Session subtract(increased);subtract.begin_gesture(0);subtract.update_gesture(command(subtracted,inherited));subtract.commit_gesture();
 const auto absolute_doc=wanted(initial,field,inherited,0.25);Session absolute(initial);absolute.begin_gesture(0);absolute.update_gesture(command(absolute_doc,inherited));absolute.commit_gesture();
 Session rejected(initial);for(int attempt=0;attempt<2;++attempt){rejected.begin_gesture(0);bool refused=false;try{rejected.update_gesture(command(candidate(initial,field,inherited,-1200),inherited));}catch(const Error& e){refused=e.code=="INVALID_LAYOUT";}check(refused,"Independent invalid negative bounds command refuses BEFORE Window");rejected.cancel_gesture();}
 auto foreign=preview;foreign.cancel_gesture();foreign.begin_gesture(0);auto incoming=initial.compositions.front().artboards.front();incoming.name="Foreign pending";foreign.update_gesture({UpdateArtboard{"composition",incoming}});
 auto newer=preview;newer.cancel_gesture();newer.apply({UpdateArtboard{"composition",incoming}},0);
 const auto path=prefix(root,mode);write(path+"-initial.nect",initial);write(path+"-expected.nect",increased);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(path+"-recovery",std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,inherited);same(w.host.session,before);
 draft(w,field,"+=0.5");std::cerr<<"Observed "<<names[field]<<" status="<<w.statusBar()->currentMessage().toStdString()<<"\n";same(w.host.session,preview);draft(w,field,"+=0.5");same(w.host.session,repeated);enter(w,field);same(w.host.session,committed);
 check(control<QLineEdit>(w,names[field])->toolTip().contains("+="),"Bounds input documents one-shot relative grammar");check(w.grab().save(path+"-committed-window.png"),"Editable production source render retained");auto* group=control<QWidget>(w,"layout-grid");auto* bottom=control<QLineEdit>(w,"grid-row-gutter");const auto end=bottom->mapTo(group,QPoint(0,bottom->height())).y();check(group->grab(QRect(0,0,group->width(),end+6)).save(path+"-grid-rows.png"),"Actual bounds row render retained");
 w.host.save(path+"-saved.nect");w.host.flush();same(w.host.session,committed);check(QFile::copy(path+"-saved.nect",path+"-cold-input.nect"),"Actual saved native bytes retained before later edits");QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--cold",root,QString::number(mode)});check(child.waitForStarted(5000),"Owned separate cold starts");if(!child.waitForFinished(15000)){child.kill();child.waitForFinished();throw std::runtime_error("Owned cold deadline");}std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"Owned cold acceptance passes");
 w.host.session.undo(w.host.session.revision());committed.undo(committed.revision());w.host.edited();events();same(w.host.session,committed);check(w.host.session.document()==initial,"One Undo restores exact full source/inheritance/IDs");w.host.session.redo(w.host.session.revision());committed.redo(committed.revision());w.host.edited();events();same(w.host.session,committed);
 reset(w,increased,"-subtract");draft(w,field,"-=0.5");enter(w,field);same(w.host.session,subtract);check(w.host.session.document()==subtracted,"Subtraction uses captured exact value");
 reset(w,initial,"-zero");draft(w,field,"+=0");enter(w,field);same(w.host.session,before);check(w.statusBar()->currentMessage().isEmpty(),"Zero-net clears partial grammar errors without History/override");
 reset(w,initial,"-cancel");draft(w,field,"+=0.5");QTest::keyClick(control<QLineEdit>(w,names[field]),Qt::Key_Escape);events();same(w.host.session,canceled);
 for(const auto* text:{"+=","-=","+=nan","+=inf","+=1e309"}){reset(w,initial,"-invalid");draft(w,field,text,false);enter(w,field);same(w.host.session,before);check(control<QLineEdit>(w,names[field])->text()==text,"Invalid parser draft never replaced with zero");}
 reset(w,initial,"-negative");draft(w,field,"-=1200",false);enter(w,field);same(w.host.session,rejected);check(w.statusBar()->currentMessage().contains("INVALID_LAYOUT"),"Negative bounds range refused explicitly");
 reset(w,initial,"-foreign");draft(w,field,"+=0.5");w.host.session.cancel_gesture();w.host.session.begin_gesture(0);w.host.session.update_gesture({UpdateArtboard{"composition",incoming}});enter(w,field);same(w.host.session,foreign);w.host.session.cancel_gesture();
 reset(w,initial,"-revision");draft(w,field,"+=0.5");w.host.session.cancel_gesture();w.host.session.apply({UpdateArtboard{"composition",incoming}},0);enter(w,field);same(w.host.session,newer);check(w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Newer canonical revision preserved");
 if(field==0){reset(w,initial,"-absolute");const auto& target=inherited?absolute_doc.compositions.front().artboards.back():absolute_doc.compositions.front().artboards.front();draft(w,field,QString::number(value(*target.layout->grid,field),'g',17),false);enter(w,field);same(w.host.session,absolute);}
}
void driven(const QString& root,int field){const auto initial=fixture(field,false,true);Session before(initial);QSettings settings(root+"/settings.ini",QSettings::IniFormat);Window w(root+"/driven-"+QString::number(field),std::make_unique<FolderLibrary>(settings),&settings);open(w,initial,false);check(control<QLineEdit>(w,names[field])->isReadOnly(),"Driven bounds stays read-only");draft(w,field,"+=0.5",false);enter(w,field);control<QPushButton>(w,"grid-apply")->click();events();same(w.host.session,before);}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","windows");QApplication app(argc,argv);try{std::cout<<"Native platform="<<QGuiApplication::platformName().toStdString()<<" PID="<<QCoreApplication::applicationPid()<<"\n";if(argc==4&&std::string(argv[1])=="--cold")cold(QString::fromLocal8Bit(argv[2]),std::stoi(argv[3]));else{QTemporaryDir temporary;check(temporary.isValid(),"Owned scratch valid");const auto requested=qEnvironmentVariable("NECT_GRID_BOUNDS_RELATIVE_ROOT"),root=requested.isEmpty()?temporary.path():requested;int failures=0;for(int mode=0;mode<8;++mode){try{flow(root,mode);std::cout<<"CASE PASS mode="<<mode<<"\n";}catch(const std::exception& e){++failures;std::cerr<<"CASE FAIL mode="<<mode<<" reason="<<e.what()<<" checks="<<checks<<"\n";}}for(int field=0;field<4;++field)driven(root,field);if(failures)throw std::runtime_error("Independent bounds cases failed: "+std::to_string(failures));}std::cout<<"PASS Grid bounds relative "<<checks<<" checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL Grid bounds relative "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
