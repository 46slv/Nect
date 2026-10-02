#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QTimer>
#include <QAction>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
Document fixture(){
 auto d=empty_document("frame-edit-doc","comp","source");auto& c=d.compositions.front();c.artboards.front().width=1200;c.artboards.front().height=800;c.artboards.front().layout=ArtboardLayout{Margin{10,20,30,40},Grid{"source-grid",{30,20,100,80},1,1,0,0}};c.artboards.push_back({"A","Target",100,100,640,480});c.artboards.push_back({"P","Independent",2000,0,900,700});
 Object root;root.id="root";root.name="Source";root.kind=Kind::group;root.children={"logo"};root.visible=false;Object logo;logo.id="logo";logo.name="Logo";logo.source=default_primitive("rect","nect.shape.rectangle");d.objects.emplace("root",root);d.objects.emplace("logo",logo);c.roots={"root"};Session s(d);
 s.apply({DefinitionCommand{CreateDefinition{{"def","Shared","root"}}},ArtboardTemplateCommand{CreateArtboardTemplate{"comp",{"tmpl","Template","source",Id{"def"}}}},ArtboardTemplateCommand{AssignArtboardTemplate{"comp","A","tmpl",Id{"content"}}},ArtboardGuideCommand{AddArtboardGuide{"comp","source",{"source-guide","Source Guide","x",20,true}}},ArtboardGuideCommand{AddArtboardGuide{"comp","A",{"local-guide","Local Guide","y",15,true}}},ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","source-guide","position",45.0}}},0);return s.document();
}
const Artboard& board(const Document&d,const Id&id="A"){for(const auto& b:d.compositions.front().artboards)if(b.id==id)return b;throw std::runtime_error("missing frame");}
Artboard effective(const Document&d){return evaluate_artboard(d.compositions.front(),"A");}
template<class T>T* control(Window&w,const char*name){for(auto*p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(std::string("missing control ")+name);}
void edit(Window&w){auto*list=w.findChild<QListWidget*>("artboards");check(list,"Artboard list available");bool found=false;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()=="A"){list->setCurrentItem(list->item(i));found=true;break;}check(found,"stable frame selected");events();control<QPushButton>(w,"artboard-edit")->click();events();}
void load(Window&w,Document d=fixture()){w.canvas->cancel_interaction();w.host.session=Session(std::move(d));w.refresh();events();edit(w);}
void enter(Window&w,const char*field,const QString&v){auto*input=control<QLineEdit>(w,field);input->setText(v);input->setModified(true);input->editingFinished();events();}
void reset_public(Window&w,const QString& field){
 std::exception_ptr failure;
 QTimer::singleShot(0,[&]{QPointer<QDialog> dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());try{check(dialog&&dialog->objectName()=="reset-artboard-template-override-dialog","public Reset dialog opened");auto* selector=dialog->findChild<QComboBox*>("template-reset-field");check(selector,"Reset field selector exists");const auto index=selector->findData(field);check(index>=0,"selected axis reset available");selector->setCurrentIndex(index);auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Reset buttons available");buttons->button(QDialogButtonBox::Apply)->click();}catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});
 control<QPushButton>(w,"artboard-template-reset-override")->click();if(failure)std::rethrow_exception(failure);events();
}
void history(Window&w,const QString&name){for(auto*a:w.findChildren<QAction*>())if(a->text()==name){a->trigger();events();return;}throw std::runtime_error("missing History action");}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 check(argc==2,"core CLI required");QTemporaryDir dir;check(dir.isValid(),"owned scratch available");QSettings settings(dir.filePath("library.ini"),QSettings::IniFormat);Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.show();events();load(w);
 const auto before=encode(w.host.session.document());const auto source=board(w.host.session.document(),"source");const auto objects=w.host.session.document().objects;const auto definitions=w.host.session.document().definitions;const auto assignment=*board(w.host.session.document()).template_assignment;
 check(control<QLineEdit>(w,"artboard-width")->text().toDouble()==1200,"inherited width displayed");enter(w,"artboard-width","640");check(effective(w.host.session.document()).width==640&&board(w.host.session.document()).template_assignment->width_override==640,"explicit fallback width creates effective override");
 check(effective(w.host.session.document()).height==800&&!board(w.host.session.document()).template_assignment->height_override,"width edit preserves height inheritance");check(w.host.session.revision()==1,"explicit edit commits once");const auto changed=encode(w.host.session.document());history(w,"Undo");check(encode(w.host.session.document())==before,"Undo restores exact inheritance");history(w,"Redo");check(encode(w.host.session.document())==changed,"Redo restores exact override");
 check(board(w.host.session.document(),"source")==source&&w.host.session.document().objects==objects&&w.host.session.document().definitions==definitions,"source and owned Definition content unchanged");const auto& after=*board(w.host.session.document()).template_assignment;check(after.grid_id==assignment.grid_id&&after.content_instance==assignment.content_instance&&after.guide_position_overrides==assignment.guide_position_overrides&&!after.margin_overridden&&!after.grid_overridden,"unrelated identity and inherited families intact");
 auto newer=source;newer.width=1300;newer.height=850;w.host.session.apply({UpdateArtboard{"comp",newer}},w.host.session.revision());w.host.edited();events();check(effective(w.host.session.document()).width==640&&effective(w.host.session.document()).height==850,"other axis follows later parent edits");
 edit(w);reset_public(w,"frame.width");check(effective(w.host.session.document()).width==1300,"public Reset restores current source width");
 load(w);enter(w,"artboard-height","480");check(effective(w.host.session.document()).height==480&&effective(w.host.session.document()).width==1200&&!board(w.host.session.document()).template_assignment->width_override,"height counterpart creates only its own override");
 load(w);w.host.session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","frame.width",700.0}}},0);w.host.edited();events();edit(w);enter(w,"artboard-width","640");check(effective(w.host.session.document()).width==640&&board(w.host.session.document()).width==640,"existing override can return to retained fallback");
 load(w);check(control<QLabel>(w,"artboard-width-source-state")->text().contains("Source: template ·"),"label reports canonical Template source");enter(w,"artboard-width","650");edit(w);check(control<QLabel>(w,"artboard-width-source-state")->text().contains("Source: template_override ·"),"label reports canonical axis override");
 for(const QString bad:{"0","-1","nan","10000001"}){load(w);const auto bytes=encode(w.host.session.document());enter(w,"artboard-width",bad);check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"invalid width rejects atomically");}
 load(w);{auto* stale=control<QLineEdit>(w,"artboard-width");w.host.session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","frame.width",700.0}}},0);const auto bytes=encode(w.host.session.document());stale->setText("640");stale->setModified(true);stale->editingFinished();events();check(encode(w.host.session.document())==bytes&&w.host.session.revision()==1,"stale frame control cannot overwrite newer revision");}
 {auto d=fixture();auto& target=d.compositions.front().artboards[1];target.width_driver=ArtboardSizeDriver{Ref{"P","","artboard.width"}};load(w,d);check(control<QLineEdit>(w,"artboard-width")->isReadOnly(),"independent typed width remains read-only");const auto bytes=encode(w.host.session.document());enter(w,"artboard-width","640");check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"programmatic input cannot bypass typed source guard");}
 {auto d=fixture();d.compositions.front().artboards[1].parent_size=ArtboardParent{"P",true,false};load(w,d);check(control<QLineEdit>(w,"artboard-width")->isReadOnly(),"assigned parent-size width is visibly protected");const auto bytes=encode(w.host.session.document());enter(w,"artboard-width","640");check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"assigned independent parent source remains protected");}
 load(w);{const auto bytes=encode(w.host.session.document());enter(w,"artboard-x","200");check(board(w.host.session.document()).x==200&&!board(w.host.session.document()).template_assignment->width_override&&!board(w.host.session.document()).template_assignment->height_override&&w.host.session.document().objects==objects,"crop-only change does not override frame size or move art");history(w,"Undo");check(encode(w.host.session.document())==bytes,"crop Undo exact");}
 {auto d=fixture();auto&target=d.compositions.front().artboards[1];target.template_assignment.reset();target.parent_size=ArtboardParent{"P",true,false};load(w,d);enter(w,"artboard-width","650");check(effective(w.host.session.document()).width==650&&(!board(w.host.session.document()).parent_size||!board(w.host.session.document()).parent_size->width),"plain Artboard parent-size literal behavior unchanged");}

 load(w);{const auto bytes=encode(w.host.session.document());auto* untouched=control<QLineEdit>(w,"artboard-width");untouched->editingFinished();events();check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"unmodified field focus-out does not create an override");auto* stale=control<QLineEdit>(w,"artboard-width");w.host.session_id+="-replacement";stale->setText("640");stale->setModified(true);stale->editingFinished();events();check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"stale Session field refuses atomically");}
 {auto d=fixture();d.compositions.front().artboards[1].height_driver=ArtboardSizeDriver{Expression{"700",1}};load(w,d);check(control<QLabel>(w,"artboard-height-source-state")->text().contains("Source: expression"),"independent expression source label preserved");enter(w,"artboard-width","640");check(effective(w.host.session.document()).width==640&&effective(w.host.session.document()).height==700&&board(w.host.session.document()).height_driver.has_value(),"other axis independent expression survives explicit override");edit(w);const auto bytes=encode(w.host.session.document());enter(w,"artboard-height","480");check(encode(w.host.session.document())==bytes&&w.host.session.revision()==1,"height expression cannot be bypassed by explicit literal edit");}
 load(w);w.host.session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","frame.height",600.0}}},0);w.host.edited();events();edit(w);enter(w,"artboard-height","480");check(effective(w.host.session.document()).height==480&&!board(w.host.session.document()).template_assignment->width_override,"existing height override returns to fallback without touching width");

 {auto d=fixture();d.compositions.front().artboards[1].parent_size=ArtboardParent{"P",false,true};load(w,d);const auto bytes=encode(w.host.session.document());check(control<QLineEdit>(w,"artboard-height")->isReadOnly(),"assigned parent-height visibly protected");enter(w,"artboard-height","480");check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"assigned height parent guard is atomic");}
 load(w);{const auto bytes=encode(w.host.session.document());enter(w,"artboard-y","250");enter(w,"artboard-name","Renamed");check(board(w.host.session.document()).y==250&&board(w.host.session.document()).name=="Renamed"&&!board(w.host.session.document()).template_assignment->width_override&&!board(w.host.session.document()).template_assignment->height_override&&w.host.session.document().objects==objects,"Y and rename preserve Template axes and artwork");history(w,"Undo");history(w,"Undo");check(encode(w.host.session.document())==bytes,"rename and Y Undo restore exact original");}
 load(w);{auto* stale=control<QLineEdit>(w,"artboard-width");w.host.session=Session(empty_document("replacement","new-comp","new-art"));w.host.session_id+="-new";const auto bytes=encode(w.host.session.document());stale->setText("640");stale->setModified(true);stale->editingFinished();events();check(encode(w.host.session.document())==bytes&&w.host.session.revision()==0,"missing captured IDs after document replacement refuse safely");}
 load(w);enter(w,"artboard-width","640");const auto path=dir.filePath("frame.nect");w.host.save(path);QFile file(path);check(file.open(QIODevice::ReadOnly),"Host Save available");const auto bytes=file.readAll();check(bytes.toStdString()==encode(w.host.session.document()),"exact native saved bytes");QProcess cold;cold.start(QString::fromLocal8Bit(argv[1]),QStringList{"--validate"});check(cold.waitForStarted(5000),"cold process starts");cold.write(bytes);cold.closeWriteChannel();check(cold.waitForFinished(10000)&&cold.exitStatus()==QProcess::NormalExit&&cold.exitCode()==0,"cold native validation passes");
 std::cout<<"PASS "<<checks<<" Template Frame Window/Host checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
