#include "window.hpp"
#include "inherited_grid_alignment_fixture.hpp"
#include <QApplication>
#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <QPointer>
#include <QScrollArea>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
Document fixture(){auto d=inherited_grid_fixture::document();d.compositions.front().artboards.front().layout->margin=Margin{10,20,30,40};return d;}
const Artboard& board(const Document&d,const Id&id="A"){for(const auto&b:d.compositions.front().artboards)if(b.id==id)return b;throw std::runtime_error("missing Artboard");}
Artboard effective(const Document&d,const Id&id="A"){return evaluate_artboard(d.compositions.front(),id);}
template<class T>T* control(Window&w,const char*name){for(auto*p:w.findChildren<T*>(name))if(p->isVisible())return p;throw std::runtime_error(std::string("missing control ")+name);}
void edit(Window&w,const QString&id="A"){
 auto* list=w.findChild<QListWidget*>("artboards");check(list,"Artboard list available");bool found=false;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()==id){list->setCurrentItem(list->item(i));found=true;break;}check(found,"stable target selected");events();control<QPushButton>(w,"artboard-edit")->click();events();
}
void click(Window&w,const char*name){auto*p=control<QPushButton>(w,name);check(p->isEnabled(),"public action enabled");p->click();events();}
void value(Window&w,const char*name,const QString&v){control<QLineEdit>(w,name)->setText(v);}
void history(Window&w,const QString&name){for(auto*a:w.findChildren<QAction*>())if(a->text()==name){a->trigger();events();return;}throw std::runtime_error("missing History action");}
void load(Window&w,Document d=fixture()){w.canvas->cancel_interaction();w.host.session=Session(std::move(d));w.refresh();events();edit(w);}
void reset(Window&w,const std::string&field){w.host.session.apply({ArtboardTemplateCommand{ResetArtboardTemplateOverride{"comp","A",field}}},w.host.session.revision());w.host.edited();events();edit(w);}
struct FrameSnapshot {
 Document document,preview;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
 explicit FrameSnapshot(const Session&s):document(s.document()),preview(s.preview_document()),native(encode(document)),
  revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
 void unchanged(const Session&s)const{
  check(s.document()==document&&s.preview_document()==preview&&encode(s.document())==native,
   "Frame input preserves complete authored Document/native/preview");
  check(s.revision()==revision&&s.history()==history&&s.gesture_generation()==generation&&s.gesture_active()==gesture,
   "Frame input preserves revision/history/gesture ownership");
 }
};
void frame_numeric_context(bool name_context=false){
 QTemporaryDir scratch;check(scratch.isValid(),"Frame regression owns settings/recovery/native files");
 QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);settings.setValue("unrelated","preserved");
 Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
 w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1100,750);w.show();
 const auto drain=[] {QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  QTest::qWait(20);QApplication::processEvents();};
 auto d=fixture();Session seed(d);seed.apply({CreatePrimitive{"comp",{},"retained","Retained Rectangle",
  default_primitive("retained-source","nect.shape.rectangle")}},seed.revision());
 auto&s=w.host.session;s=Session(seed.document());w.host.edited();drain();QApplication::setActiveWindow(&w);drain();
 const auto select_frame=[&]{
 auto*list=w.findChild<QListWidget*>("artboards");check(list&&list->isVisible(),"Actual Artboards list reachable");
 QListWidgetItem* target=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()=="A")target=list->item(i);
 check(target,"Exact Artboard row exists");list->scrollToItem(target);drain();
 QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(target).center());drain();
 check(w.canvas->active_composition()=="comp"&&w.canvas->active_artboard()=="A","Actual list click binds exact Composition/Artboard");
 auto*button=control<QPushButton>(w,"artboard-edit");check(button->isEnabled(),"Edit frame is enabled");
 QTest::mouseClick(button,Qt::LeftButton);drain();
 };select_frame();
 const auto draft=[&](const char* name,const char*text){
  auto*input=control<QLineEdit>(w,name);auto*parent=input->parentWidget();
  while(parent&&!qobject_cast<QScrollArea*>(parent))parent=parent->parentWidget();
  auto*scroll=qobject_cast<QScrollArea*>(parent);check(scroll&&input->isEnabled()&&!input->isReadOnly(),"Actual frame editor is enabled in Inspector");
  scroll->ensureWidgetVisible(input);drain();QTest::mouseClick(input,Qt::LeftButton);
  QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,text);drain();
  check(input->hasFocus()&&input->isModified()&&input->text()==text,"Keyboard leaves exact unfinished frame draft focused");
  return QPointer<QLineEdit>(input);
 };
 const FrameSnapshot original(s);auto external=board(s.document());external.x=1100;
 s.begin_gesture(s.revision());s.update_gesture({UpdateArtboard{"comp",external}});const FrameSnapshot preview(s);
 check(preview.preview!=preview.document&&s.gesture_active(),"External canonical crop preview is active");w.host.edited();drain();preview.unchanged(s);
 auto old=draft(name_context?"artboard-name":"artboard-x",name_context?"Preview draft":"1250");preview.unchanged(s);s.cancel_gesture();const FrameSnapshot cancelled(s);
 check(old&&old->hasFocus()&&old->isModified()&&s.revision()==preview.revision&&s.gesture_generation()==preview.generation,
  "Canonical cancel retains revision/generation while preview-born draft stays focused");
 check(s.document()==original.document&&s.preview_document()==original.document,"Cancel restores complete authored source before refresh");
 w.host.edited();drain();std::cout<<"Frame cancel refresh name="<<board(s.document()).name<<" x="<<board(s.document()).x<<" revision="<<s.revision()
  <<" expected_x="<<board(cancelled.document).x<<" expected_revision="<<cancelled.revision<<std::endl;cancelled.unchanged(s);
 check(control<QLineEdit>(w,"artboard-x")->text().toDouble()==1000,"Replacement field shows canonical crop after cancel");
 const auto undo_redo=[&](const FrameSnapshot&before,const FrameSnapshot&after){
  history(w,"Undo");drain();check(s.document()==before.document&&s.preview_document()==before.document&&encode(s.document())==before.native,
   "Frame Undo restores complete prior authored source/native");
  history(w,"Redo");drain();check(s.document()==after.document&&s.preview_document()==after.document&&encode(s.document())==after.native,
   "Frame Redo restores complete edited source/native");
 };
 if(name_context){
  const auto rename=[&](const char*text){
   const FrameSnapshot before(s);auto input=draft("artboard-name",text);before.unchanged(s);
   auto named=board(before.document);named.name=text;Session oracle(before.document);oracle.apply({UpdateArtboard{"comp",named}},oracle.revision());
   QTest::keyClick(input,Qt::Key_Return);drain();check(s.document()==oracle.document()&&s.preview_document()==oracle.document()&&
    encode(s.document())==encode(oracle.document())&&s.revision()==before.revision+1&&s.history().states.size()==before.history.states.size()+1&&!s.gesture_active(),
    "Name Return is one canonical transaction preserving complete crop/size/geometry/Anchor/Template/layout/other boards");
   const FrameSnapshot after(s);undo_redo(before,after);
  };
  rename("Named frame");const FrameSnapshot before_external(s);auto stale=draft("artboard-name","Old draft");before_external.unchanged(s);
  auto newer=board(s.document());newer.name="External name";s.apply({UpdateArtboard{"comp",newer}},s.revision());const FrameSnapshot incoming(s);
  check(stale&&stale->hasFocus()&&stale->isModified(),"External rename arrives while old Name draft stays focused");
  w.host.edited();drain();incoming.unchanged(s);check(control<QLineEdit>(w,"artboard-name")->text()=="External name","Replacement Name shows external source");undo_redo(before_external,incoming);
  const FrameSnapshot before_preview(s);auto prior=draft("artboard-name","Prior draft");before_preview.unchanged(s);
  auto proposed=board(s.document());proposed.name="External preview";s.begin_gesture(s.revision());s.update_gesture({UpdateArtboard{"comp",proposed}});
  const FrameSnapshot active(s);check(prior&&prior->hasFocus(),"External Name preview starts with old draft focused");w.host.edited();drain();active.unchanged(s);
  auto during=draft("artboard-name","During preview");active.unchanged(s);QTest::keyClick(during,Qt::Key_Return);drain();active.unchanged(s);
  auto cancel=draft("artboard-name","Cancelled draft");active.unchanged(s);s.cancel_gesture();const FrameSnapshot after_cancel(s);
  check(cancel&&cancel->hasFocus()&&s.revision()==active.revision&&s.gesture_generation()==active.generation,"Cancel keeps focused preview-born Name and same revision/generation");
  w.host.edited();drain();after_cancel.unchanged(s);rename("Fresh frame");
  const auto native=scratch.filePath("frame-name.nect");const FrameSnapshot saved(s);w.host.save(native);drain();saved.unchanged(s);
  {QFile file(native);check(file.open(QIODevice::ReadOnly)&&file.readAll().toStdString()==saved.native,"Name Host Save writes complete native source");}
  w.host.open(native);drain();check(s.document()==saved.document&&s.preview_document()==saved.document&&encode(s.document())==saved.native&&
   s.revision()==0&&s.history().states.size()==1&&!s.gesture_active(),"Name same Window native reopen preserves complete authored source");
  select_frame();const FrameSnapshot before_reload(s);auto outgoing=draft("artboard-name","Outgoing reload draft");before_reload.unchanged(s);
  const auto old_session=w.host.session_id;check(outgoing&&outgoing->hasFocus()&&outgoing->isModified(),"Same-ID reopen starts with unfinished focused Name");
  w.host.open(native);drain();check(w.host.session_id!=old_session&&s.document()==before_reload.document&&s.preview_document()==before_reload.document&&
   encode(s.document())==before_reload.native&&s.revision()==0&&s.history().states.size()==1&&!s.gesture_active(),"Name old Session draft cannot overwrite complete canonical source flushed by Host open");
  check(settings.value("unrelated")=="preserved","Name editing preserves unrelated settings");return;
 }
 const auto commit=[&](const char*name,const char*text,double Artboard::*member,const char*axis){
  const FrameSnapshot before(s);auto input=draft(name,text);before.unchanged(s);Session oracle(before.document);
  if(axis&&board(before.document).template_assignment)oracle.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{
   "comp","A",std::string("frame.")+axis,std::stod(text)}}},oracle.revision());
  else{auto next=board(before.document);next.*member=std::stod(text);oracle.apply({UpdateArtboard{"comp",next}},oracle.revision());}
  QTest::keyClick(input,Qt::Key_Return);drain();
  check(s.document()==oracle.document()&&s.preview_document()==oracle.document()&&encode(s.document())==encode(oracle.document())&&
   s.revision()==before.revision+1&&s.history().states.size()==before.history.states.size()+1&&!s.gesture_active(),
   "Normal frame Return is one complete canonical transaction retaining all unrelated source/IDs/Anchor/Template/layout");
  const FrameSnapshot after(s);undo_redo(before,after);
 };
 commit("artboard-x","1300",&Artboard::x,nullptr);commit("artboard-y","250",&Artboard::y,nullptr);
 commit("artboard-width","450",&Artboard::width,"width");commit("artboard-height","350",&Artboard::height,"height");
 const auto native=scratch.filePath("frame-context.nect");const FrameSnapshot saved(s);w.host.save(native);drain();saved.unchanged(s);
 {QFile file(native);check(file.open(QIODevice::ReadOnly)&&file.readAll().toStdString()==saved.native,"Host Save writes complete native source");}
 w.host.open(native);drain();check(s.document()==saved.document&&s.preview_document()==saved.document&&encode(s.document())==saved.native&&
  s.revision()==0&&s.history().states.size()==1&&!s.gesture_active(),"Same Window native reopen retains complete frame/source/geometry/Anchor/Template/other Artboards");
 select_frame();const FrameSnapshot before_external(s);auto stale=draft("artboard-x","1700");before_external.unchanged(s);
 auto newer=board(s.document());newer.x=1500;s.apply({UpdateArtboard{"comp",newer}},s.revision());const FrameSnapshot incoming(s);
 check(stale&&stale->hasFocus()&&stale->isModified(),"External canonical crop arrives while unfinished draft stays focused");
 w.host.edited();drain();incoming.unchanged(s);check(control<QLineEdit>(w,"artboard-x")->text().toDouble()==1500,"Refresh shows newer canonical crop");
 undo_redo(before_external,incoming);
 const FrameSnapshot before_preview(s);auto old_before_preview=draft("artboard-y","900");before_preview.unchanged(s);
 auto preview_board=board(s.document());preview_board.y=300;s.begin_gesture(s.revision());s.update_gesture({UpdateArtboard{"comp",preview_board}});
 const FrameSnapshot active_preview(s);check(old_before_preview&&old_before_preview->hasFocus(),"External preview starts with old numeric draft focused");
 w.host.edited();drain();active_preview.unchanged(s);
 auto preview_return=draft("artboard-y","950");active_preview.unchanged(s);QTest::keyClick(preview_return,Qt::Key_Return);drain();active_preview.unchanged(s);
 auto preview_cancel=draft("artboard-y","975");active_preview.unchanged(s);s.cancel_gesture();const FrameSnapshot after_cancel(s);
 check(preview_cancel&&preview_cancel->hasFocus()&&s.gesture_generation()==active_preview.generation&&s.revision()==active_preview.revision,
  "Preview-born Y draft remains focused through canonical cancel with same generation/revision");
 w.host.edited();drain();after_cancel.unchanged(s);commit("artboard-y","325",&Artboard::y,nullptr);
 const FrameSnapshot before_reload(s);auto reload_old=draft("artboard-x","1900");before_reload.unchanged(s);const auto old_session=w.host.session_id;
 check(reload_old&&reload_old->hasFocus()&&reload_old->isModified(),"Same-ID native reload starts with unfinished frame draft focused");
 w.host.open(native);drain();check(w.host.session_id!=old_session&&s.document()==before_reload.document&&s.preview_document()==before_reload.document&&
  encode(s.document())==before_reload.native&&s.revision()==0&&s.history().states.size()==1&&!s.gesture_active(),
  "New Session rejects old numeric draft and keeps complete canonical source flushed by same-file Host open");
 select_frame();auto plain=seed.document();plain.compositions.front().artboards[1].template_assignment.reset();
 s=Session(plain);w.host.edited();drain();select_frame();commit("artboard-width","475",&Artboard::width,"width");
 const FrameSnapshot valid(s);auto invalid=draft("artboard-x","nan");valid.unchanged(s);QTest::keyClick(invalid,Qt::Key_Return);drain();valid.unchanged(s);
 check(w.statusBar()->currentMessage().contains("INVALID_VALUE"),"Nonfinite frame coordinate refuses atomically");
 check(settings.value("unrelated")=="preserved","Frame editing preserves unrelated workspace settings");
}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 if(app.arguments().contains("--frame-context-only")){frame_numeric_context();std::cout<<"PASS "<<checks<<" Frame numeric context checks; physical OS input NOT_RUN\n";return 0;}
 if(app.arguments().contains("--frame-name-context-only")){frame_numeric_context(true);std::cout<<"PASS "<<checks<<" Frame Name context checks; physical OS input NOT_RUN\n";return 0;}
 check(argc==2,"core CLI path required");QTemporaryDir dir;check(dir.isValid(),"owned scratch available");QSettings settings(dir.filePath("library.ini"),QSettings::IniFormat);Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.show();events();load(w);
 check(control<QLineEdit>(w,"margin-left")->text().toDouble()==10&&control<QLineEdit>(w,"margin-bottom")->text().toDouble()==40,"inherited Margin shown as evaluated insets");
 check(control<QLineEdit>(w,"grid-x")->text().toDouble()==40&&control<QLineEdit>(w,"grid-width")->text().toDouble()==100,"inherited Grid shown as evaluated bounds");
 const auto original=encode(w.host.session.document());value(w,"margin-left","15");click(w,"margin-apply");auto applied=encode(w.host.session.document());
 check(board(w.host.session.document()).template_assignment->margin_overridden&&!board(w.host.session.document()).template_assignment->grid_overridden,"Margin Apply overrides only Margin");check(effective(w.host.session.document()).layout->margin->left==15&&effective(w.host.session.document()).layout->grid->bounds.x==40,"Margin edit preserves inherited Grid");history(w,"Undo");check(encode(w.host.session.document())==original,"Undo restores exact original inheritance");history(w,"Redo");check(encode(w.host.session.document())==applied,"Redo restores exact family override");
 auto source=board(w.host.session.document(),"source");source.layout->grid->bounds.x=70;w.host.session.apply({SetArtboardLayout{"comp","source",source.layout}},w.host.session.revision());w.host.edited();events();check(effective(w.host.session.document()).layout->grid->bounds.x==70&&effective(w.host.session.document()).layout->margin->left==15,"source Grid remains live after local Margin edit");
 load(w);const auto grid_id=board(w.host.session.document()).template_assignment->grid_id;value(w,"grid-x","55");click(w,"grid-apply");check(board(w.host.session.document()).template_assignment->grid_overridden&&!board(w.host.session.document()).template_assignment->margin_overridden,"Grid Apply overrides only Grid");check(board(w.host.session.document()).layout->grid->id==grid_id&&effective(w.host.session.document()).layout->grid->bounds.x==55,"Grid Apply retains target-local stable identity");
 load(w);click(w,"margin-clear");check(!effective(w.host.session.document()).layout->margin&&effective(w.host.session.document()).layout->grid.has_value()&&!board(w.host.session.document()).template_assignment->grid_overridden,"Clear inherited Margin suppresses only Margin");reset(w,"layout.margin");check(effective(w.host.session.document()).layout->margin->left==10,"Reset restores inherited Margin");
 load(w);click(w,"grid-clear");check(!effective(w.host.session.document()).layout->grid&&effective(w.host.session.document()).layout->margin.has_value()&&!board(w.host.session.document()).template_assignment->margin_overridden,"Clear inherited Grid suppresses only Grid");reset(w,"layout.grid");check(effective(w.host.session.document()).layout->grid->bounds.x==40,"Reset restores inherited Grid");
 load(w);click(w,"grid-copy-margin-box");const auto copied=effective(w.host.session.document());check(copied.layout->grid->bounds==LayoutRect{10,20,copied.width-40,copied.height-60},"copy inherited Margin box has exact local bounds");check(!board(w.host.session.document()).template_assignment->margin_overridden&&board(w.host.session.document()).layout->grid->id==grid_id,"copy changes only Grid with stable ID");source=board(w.host.session.document(),"source");source.layout->margin->left=25;w.host.session.apply({SetArtboardLayout{"comp","source",source.layout}},w.host.session.revision());w.host.edited();events();check(effective(w.host.session.document()).layout->margin->left==25&&effective(w.host.session.document()).layout->grid->bounds.x==10,"copy is one-shot while inherited Margin keeps following");

 load(w);{const auto before=encode(w.host.session.document());auto* input=control<QLineEdit>(w,"margin-left");input->selectAll();QTest::keyClicks(input,"17");events();check(encode(w.host.session.document())==before&&w.host.session.gesture_active(),"typing previews without committing");check(effective(w.host.session.preview_document()).layout->margin->left==17&&!board(w.host.session.preview_document()).template_assignment->grid_overridden,"preview is family-local");click(w,"margin-left-cancel");check(encode(w.host.session.document())==before&&!w.host.session.gesture_active()&&w.host.session.revision()==0,"Cancel preserves bytes and History");}
 load(w);{auto* stale=control<QPushButton>(w,"margin-apply");value(w,"margin-left","88");w.host.session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.margin",std::optional<Margin>{Margin{19,20,30,40}}}}},w.host.session.revision());const auto newer=encode(w.host.session.document());stale->click();events();check(encode(w.host.session.document())==newer&&w.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"stale captured revision refuses overwrite");}
 load(w);{auto* stale=control<QPushButton>(w,"grid-apply");value(w,"grid-x","88");const auto before=encode(w.host.session.document());w.host.session_id+="-replacement";stale->click();events();check(encode(w.host.session.document())==before&&w.host.session.revision()==0,"stale Session identity refuses commit");}
 {auto d=fixture();auto& src=d.compositions.front().artboards.front();src.layout->margin->left_expression=Expression{"12",1};src.layout->grid->columns=2;src.layout->grid->column_gutter=3;src.layout->grid->bounds_x_expression=Expression{"45",1};load(w,d);check(control<QLineEdit>(w,"margin-left")->text().toDouble()==12&&control<QLineEdit>(w,"grid-x")->text().toDouble()==45,"inherited driven source displays evaluated values");value(w,"margin-top","21");click(w,"margin-apply");check(!board(w.host.session.document()).layout->margin->left_expression&&board(w.host.session.document()).layout->margin->left==12,"new Margin override does not clone inherited driver");edit(w);click(w,"grid-copy-margin-box");const auto& grid=*board(w.host.session.document()).layout->grid;check(!grid.bounds_x_expression&&grid.columns==2&&grid.column_gutter==3,"copy inherited Grid retains counts/gutters without source metadata");check(board(w.host.session.document(),"source")==src,"inherited source authored record unchanged");}
 {auto d=fixture();auto& target=d.compositions.front().artboards[1];target.layout=ArtboardLayout{Margin{10,20,30,40},{}};target.template_assignment->margin_overridden=true;target.layout->margin->left_expression=Expression{R"(ref("source","","artboard.width") * 0 + 12)",1};load(w,d);check(control<QLineEdit>(w,"margin-left")->isReadOnly(),"local driven Margin remains read-only");value(w,"margin-top","22");click(w,"margin-apply");check(board(w.host.session.document()).layout->margin->left_expression==target.layout->margin->left_expression&&!board(w.host.session.document()).template_assignment->grid_overridden,"local Margin source preserved without overriding Grid");edit(w);const auto before=encode(w.host.session.document());click(w,"margin-clear");check(encode(w.host.session.document())==before&&!w.host.session.gesture_active()&&w.statusBar()->currentMessage().contains("DRIVEN_"),"driven Margin Clear rejected atomically");}
 {auto d=fixture();auto& target=d.compositions.front().artboards[1];auto grid=*d.compositions.front().artboards.front().layout->grid;grid.id=target.template_assignment->grid_id;grid.bounds_x_expression=Expression{R"(ref("source","","artboard.width") * 0 + 45)",1};target.layout=ArtboardLayout{{},grid};target.template_assignment->grid_overridden=true;load(w,d);check(control<QLineEdit>(w,"grid-x")->isReadOnly(),"local driven Grid remains read-only");value(w,"grid-y","35");click(w,"grid-apply");check(board(w.host.session.document()).layout->grid->bounds_x_expression==grid.bounds_x_expression&&!board(w.host.session.document()).template_assignment->margin_overridden,"local Grid source preserved without overriding Margin");edit(w);const auto before=encode(w.host.session.document());click(w,"grid-clear");check(encode(w.host.session.document())==before&&w.statusBar()->currentMessage().contains("DRIVEN_"),"driven Grid Clear rejected atomically");edit(w);click(w,"grid-copy-margin-box");check(encode(w.host.session.document())==before,"copy refuses replacement of driven local bounds");}
 {auto d=fixture();auto& target=d.compositions.front().artboards[1];target.template_assignment.reset();target.layout=ArtboardLayout{Margin{1,2,3,4},Grid{"plain-grid",{5,6,70,80},1,1,0,0}};load(w,d);value(w,"margin-left","9");click(w,"margin-apply");check(board(w.host.session.document()).layout->grid->id=="plain-grid"&&!board(w.host.session.document()).template_assignment&&board(w.host.session.document()).layout->margin->left==9,"plain-Artboard family edit behavior unchanged");}
 load(w);click(w,"grid-copy-margin-box");

 load(w);click(w,"grid-clear");edit(w);value(w,"grid-x","5");value(w,"grid-width","100");click(w,"grid-apply");check(board(w.host.session.document()).layout->grid->id==grid_id&&effective(w.host.session.document()).layout->grid->bounds.x==5&&!board(w.host.session.document()).template_assignment->margin_overridden,"Apply after suppressed Grid uses assignment identity and preserves Margin inheritance");
 load(w);{const auto before=encode(w.host.session.document());auto* input=control<QLineEdit>(w,"grid-x");input->selectAll();QTest::keyClicks(input,"66");events();check(w.host.session.gesture_active()&&encode(w.host.session.document())==before,"Grid preview leaves committed state unchanged");click(w,"grid-bounds-x-cancel");check(encode(w.host.session.document())==before&&!w.host.session.gesture_active(),"Grid Cancel discards actual Session preview");}
 load(w);click(w,"grid-copy-margin-box");

 for(const auto* name:{"margin-clear","grid-clear"}){load(w);auto* stale=control<QPushButton>(w,name);w.host.session=Session(empty_document("new-document","new-comp","new-frame"));w.host.session_id+="-new";const auto replacement=encode(w.host.session.document());stale->click();events();check(encode(w.host.session.document())==replacement&&w.host.session.revision()==0,"stale Clear after replaced document refuses without lookup exception");}
 load(w);click(w,"grid-copy-margin-box");

 load(w);{auto* input=control<QLineEdit>(w,"margin-left");input->selectAll();QTest::keyClicks(input,"18");events();auto* stale=control<QPushButton>(w,"margin-apply");w.host.session.cancel_gesture();w.host.session.apply({ArtboardTemplateCommand{SetArtboardTemplateOverride{"comp","A","layout.margin",std::optional<Margin>{Margin{23,20,30,40}}}}},w.host.session.revision());const auto newer=encode(w.host.session.document());stale->click();events();check(encode(w.host.session.document())==newer&&!w.host.session.gesture_active(),"stale active preview cannot overwrite newer committed revision");}
 load(w);{const auto before=encode(w.host.session.document());auto* input=control<QLineEdit>(w,"grid-width");input->selectAll();QTest::keyClicks(input,"-1");events();click(w,"grid-apply");check(encode(w.host.session.document())==before&&w.host.session.revision()==0,"invalid Grid draft cannot commit earlier valid preview");click(w,"grid-bounds-width-cancel");check(!w.host.session.gesture_active()&&encode(w.host.session.document())==before,"Cancel clears invalid draft safely");}
 load(w);click(w,"grid-copy-margin-box");
 const auto file=dir.filePath("layout.nect");w.host.save(file);QFile saved(file);check(saved.open(QIODevice::ReadOnly),"Host native saved");const auto bytes=saved.readAll();check(bytes.toStdString()==encode(w.host.session.document()),"saved exact authored bytes");QProcess cold;cold.start(QString::fromLocal8Bit(argv[1]),QStringList{"--validate"});check(cold.waitForStarted(5000),"cold process starts");cold.write(bytes);cold.closeWriteChannel();check(cold.waitForFinished(10000)&&cold.exitCode()==0&&cold.exitStatus()==QProcess::NormalExit,"cold native validation passes");
 std::cout<<"PASS "<<checks<<" Template layout Window/Host checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
