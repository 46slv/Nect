#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QScrollArea>
#include <QImage>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
QByteArray reference(const Ref&r){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},{"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);}
QWidget* knob(Window&w){for(auto*k:w.findChildren<QWidget*>("primitive-angle-knob"))if(k->isVisible())return k;throw std::runtime_error("primitive rotation has a visible radial control");}
QLineEdit* numeric(Window&w,const Ref&r){for(auto*n:w.findChildren<QLineEdit*>())if(n->isVisible()&&n->property("nect-reference").toByteArray()==reference(r))return n;throw std::runtime_error("numeric Ref missing");}
QPointF point(double angle){const auto radians=angle*std::acos(-1.0)/180;return {22+16*std::cos(radians),22+16*std::sin(radians)};}
void mouse(QWidget*k,QEvent::Type type,double angle,Qt::MouseButton button,Qt::MouseButtons buttons){const auto local=point(angle);QMouseEvent e(type,local,QPointF(k->mapToGlobal(QPoint(0,0)))+local,button,buttons,Qt::NoModifier);QApplication::sendEvent(k,&e);events();}
void press(QWidget*k){mouse(k,QEvent::MouseButtonPress,0,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget*k,double angle){mouse(k,QEvent::MouseMove,angle,Qt::NoButton,Qt::LeftButton);}
void release(QWidget*k,double angle){mouse(k,QEvent::MouseButtonRelease,angle,Qt::LeftButton,Qt::NoButton);}
void edit(Window&w,const Ref&r,const QString&value){auto*n=numeric(w,r);n->setText(value);n->setModified(true);n->editingFinished();events();}
QPointF indicator(Window&w,QWidget*k){
 events();QTest::qWait(20);
 for(auto*p=k->parentWidget();p;p=p->parentWidget())if(auto*scroll=qobject_cast<QScrollArea*>(p)){scroll->ensureWidgetVisible(k);break;}
 events();QTest::qWait(150);const auto whole=w.grab().toImage();const auto dpr=whole.devicePixelRatio();const auto origin=k->mapTo(&w,QPoint(0,0));const auto cell=whole.copy(QRect(qRound(origin.x()*dpr),qRound(origin.y()*dpr),qRound(k->width()*dpr),qRound(k->height()*dpr)));
 QPointF sum;int count=0;for(int y=0;y<cell.height();++y)for(int x=0;x<cell.width();++x){const auto color=cell.pixelColor(x,y);const QPointF delta((x+0.5)/dpr-22,(y+0.5)/dpr-22);const auto radius=std::hypot(delta.x(),delta.y());if(radius>4&&radius<14&&color.blue()-color.red()>50&&color.green()>100){sum+=delta;++count;}}
 check(count>5,"actual rendered dial has a visible directional indicator");return sum/count;
}
void load(Window&w,const char*type){auto d=empty_document("angle-doc","comp","art");Session s(d);s.apply({CreatePrimitive{"comp","","shape","Shape",default_primitive("source",type)}},0);w.host.session=Session(s.document());w.host.session_id+="-next";w.host.edited();w.canvas->set_selection("shape");events();}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 {
  Session session(empty_document("generation-doc","generation-comp","generation-art"));const auto document=session.document();const auto history=session.history();check(session.gesture_generation()==0,"fresh Session generation starts zero");session.begin_gesture(0);check(session.gesture_generation()==1,"successful begin gets a new transient generation");bool refused=false;try{session.begin_gesture(0);}catch(const Error&e){refused=e.code=="GESTURE_ACTIVE";}check(refused&&session.gesture_generation()==1&&session.gesture_active(),"rejected nested begin preserves current owner generation");session.update_gesture({});check(session.gesture_generation()==1,"preview reset retains gesture identity");session.cancel_gesture();session.begin_gesture(0);check(session.gesture_generation()==2,"replacement preview at same revision has distinct identity");session.commit_gesture();refused=false;try{session.begin_gesture(1);}catch(const Error&e){refused=e.code=="REVISION_CONFLICT";}check(refused&&session.gesture_generation()==2&&!session.gesture_active(),"rejected stale begin cannot consume or activate generation");check(session.document()==document&&session.history()==history&&session.revision()==0,"transient generation never changes document or History");
 }
 QTemporaryDir dir;check(dir.isValid(),"scratch available");QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.show();events();const Ref rotation{"shape","","generator.rotation"};if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")check(w.devicePixelRatioF()>=1.9,"high-DPI run has actual scaled widget DPR");
 for(const auto*type:{"nect.shape.polygon","nect.shape.star"}){
  load(w,type);auto*k=knob(w);check(k->property("nect-reference").toByteArray()==reference(rotation),"dial shares exact authored numeric Ref");
  edit(w,rotation,"725.1234567890123");check(numeric(w,rotation)->text().toDouble()==evaluate(w.host.session.document()).at(rotation),"visible numeric refresh preserves full authored double precision");
  const auto precision_bytes=encode(w.host.session.document());const auto precision_revision=w.host.session.revision();edit(w,rotation,"=sin(");check(numeric(w,rotation)->text().toDouble()==evaluate(w.host.session.document()).at(rotation),"invalid expression fallback retains exact numeric display");bool draft_found=false;for(auto*editor:w.findChildren<QPlainTextEdit*>())if(editor->isVisible()&&editor->property("nect-reference").toByteArray()==reference(rotation)&&editor->toPlainText()=="sin(")draft_found=true;check(draft_found&&encode(w.host.session.document())==precision_bytes&&w.host.session.revision()==precision_revision,"invalid expression draft is retained separately without authored mutation");load(w,type);
  edit(w,rotation,"0");auto direction=indicator(w,knob(w));check(direction.x()>2&&std::abs(direction.y())<2,"primitive zero indicator points along local +X");edit(w,rotation,"90");direction=indicator(w,knob(w));check(direction.y()>2&&std::abs(direction.x())<2,"primitive positive quarter-turn points clockwise down");
  edit(w,rotation,"0");k=knob(w);const auto zero_revision=w.host.session.revision();press(k);for(const double angle:{-26.099815652003855,-93.32846161547081,52.900539558665514,50.17519401215165,4.952368972990399,-77.02241444254298,7.673784129310434,25.757542601731984,0.0})move(k,angle);release(k,0);check(w.host.session.revision()==zero_revision&&evaluate(w.host.session.document()).at(rotation)==0,"zero-winding floating residue is an exact no-net gesture");
  edit(w,rotation,"725");k=knob(w);check(evaluate(w.host.session.document()).at(rotation)==725&&k->accessibleDescription().contains("indicator 5 degrees"),"exact unwrapped numeric value and modulo dial agree");
  const auto before=w.host.session.document();const auto revision=w.host.session.revision();const auto geometry=k->geometry();
  press(k);move(k,10);check(w.host.session.gesture_active()&&std::abs(evaluate(w.host.session.preview_document()).at(rotation)-735)<1e-9,"dial uses canonical gesture preview");check(w.host.session.document()==before&&w.host.session.revision()==revision,"preview preserves committed source point identities and revision");check(numeric(w,rotation)->text().toDouble()==735,"numeric field follows live preview before commit");release(k,10);
  check(w.host.session.revision()==revision+1&&std::abs(evaluate(w.host.session.document()).at(rotation)-735)<1e-9,"one drag creates one exact authored revision");check(numeric(w,rotation)->text().toDouble()==735,"dial refreshes numeric value");
  w.host.session.undo(w.host.session.revision());w.host.edited();events();check(w.host.session.document()==before,"one Undo restores exact document");w.host.session.redo(w.host.session.revision());w.host.edited();events();check(numeric(w,rotation)->text().toDouble()==735,"Redo restores both controls");
  k=knob(w);const auto bytes=encode(w.host.session.document());const auto rev=w.host.session.revision();press(k);move(k,25);QTest::keyClick(k,Qt::Key_Escape);events();check(!w.host.session.gesture_active()&&w.host.session.revision()==rev&&encode(w.host.session.document())==bytes,"Escape cancels without mutation");
  press(k);release(k,0);check(w.host.session.revision()==rev&&!w.host.session.gesture_active(),"no-change click has no history");k=knob(w);press(k);check(w.host.session.gesture_active(),"repeat gesture starts after no-change click");move(k,25);move(k,0);release(k,0);check(w.host.session.revision()==rev&&!w.host.session.gesture_active(),"no-net drag has no history");check(knob(w)->geometry()==geometry,"gesture geometry stable");
  k=knob(w);const auto turns_revision=w.host.session.revision();press(k);for(int angle=45;angle<=360;angle+=45)move(k,angle);release(k,360);check(w.host.session.revision()==turns_revision+1&&std::abs(evaluate(w.host.session.document()).at(rotation)-1095)<1e-8,"full clockwise turn adds authored360 without normalization");
  k=knob(w);press(k);for(int angle=-45;angle>=-360;angle-=45)move(k,angle);release(k,-360);check(std::abs(evaluate(w.host.session.document()).at(rotation)-735)<1e-8,"counterclockwise full turn subtracts authored360");
  edit(w,rotation,"-725");check(evaluate(w.host.session.document()).at(rotation)==-725,"negative authored turns retained");const auto prior=encode(w.host.session.document());edit(w,rotation,"1000000001");check(encode(w.host.session.document())==prior,"out-of-range numeric rejects atomically");edit(w,rotation,"nan");check(encode(w.host.session.document())==prior,"nonfinite numeric rejects atomically");
  check(decode(encode(w.host.session.document()))==w.host.session.document(),"native round-trip exact");
  // A failed begin may not steal or cancel another control's existing gesture.
  w.host.edited();events();k=knob(w);w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({EditProperties{{rotation},123,false}});
  const auto other_preview=w.host.session.preview_document();press(k);release(k,0);check(w.host.session.gesture_active()&&w.host.session.preview_document()==other_preview,"busy Session gesture is untouched");w.host.session.cancel_gesture();
  // Same-revision ABA: old dial callbacks must not operate on replacement preview B.
  for(const int action:{0,1,2,3}){
   w.host.edited();events();k=knob(w);press(k);move(k,5);check(w.host.session.gesture_active(),"dial owns initial preview A");w.host.session.cancel_gesture();w.host.session.begin_gesture(w.host.session.revision());w.host.session.update_gesture({EditProperties{{rotation},321,false}});const auto b=w.host.session.preview_document();
   if(action==0){move(k,15);release(k,15);}else if(action==1)release(k,5);else if(action==2)QTest::keyClick(k,Qt::Key_Escape);else w.refresh(false);
   check(w.host.session.gesture_active()&&w.host.session.preview_document()==b,"stale dial cannot update commit cancel or teardown unrelated preview B");w.host.session.cancel_gesture();
  }
  w.host.edited();events();k=knob(w);const auto stable=encode(w.host.session.document());const auto stable_revision=w.host.session.revision();press(k);move(k,20);w.refresh(false);events();check(!w.host.session.gesture_active()&&w.host.session.revision()==stable_revision&&encode(w.host.session.document())==stable,"Inspector rebuild cancels own preview without commit");
  k=knob(w);w.host.session.apply({Set{rotation,12}},w.host.session.revision());press(k);release(k,0);check(!w.host.session.gesture_active()&&evaluate(w.host.session.document()).at(rotation)==12,"stale revision refuses before mutation");
  w.host.edited();events();k=knob(w);w.host.session_id+="-replacement";const auto replacement=encode(w.host.session.document());press(k);release(k,0);check(!w.host.session.gesture_active()&&encode(w.host.session.document())==replacement,"stale session refuses before mutation");
  w.host.edited();events();k=knob(w);auto*n=numeric(w,rotation);n->setText("45");n->setModified(true);const auto draft_revision=w.host.session.revision();press(k);check(!w.host.session.gesture_active()&&n->isModified()&&n->text()=="45"&&w.statusBar()->currentMessage().startsWith("UNCOMMITTED_INPUT"),"pending numeric draft refuses dial begin and preserves draft");release(k,0);check(w.host.session.revision()==draft_revision,"draft refusal has no history");n->setModified(false);w.host.edited();events();
  w.host.session.apply({SetExpression{{rotation},Expression{"30",1}}},w.host.session.revision());w.host.edited();events();check(!knob(w)->isEnabled()&&evaluate(w.host.session.document()).at(rotation)==30,"expression driven angle disables dial");
  load(w,type);k=knob(w);auto changed=w.host.session.document();changed.objects.at("shape").source->id="replaced-source";w.host.session=Session(changed);const auto changed_bytes=encode(w.host.session.document());press(k);release(k,0);check(!w.host.session.gesture_active()&&encode(w.host.session.document())==changed_bytes,"source identity replacement refuses even at same revision");
  load(w,type);k=knob(w);changed=w.host.session.document();changed.id="other-document";w.host.session=Session(changed);press(k);release(k,0);check(!w.host.session.gesture_active()&&w.host.session.document().id=="other-document","document identity replacement refuses even at same revision");
  load(w,type);auto*draft=numeric(w,rotation);draft->setText("45");draft->setModified(true);w.refresh(false);check(draft->isModified()&&draft->text()=="45","no-dial teardown does not erase an independent numeric draft");events();
  load(w,type);edit(w,rotation,"999999999");k=knob(w);const auto limit_bytes=encode(w.host.session.document());const auto limit_rev=w.host.session.revision();press(k);move(k,10);move(k,20);release(k,20);check(k->accessibleDescription().contains("999999999")&&numeric(w,rotation)->text().toDouble()==999999999,"range cancellation disarms further movement and restores both controls");check(!w.host.session.gesture_active()&&w.host.session.revision()==limit_rev&&encode(w.host.session.document())==limit_bytes,"out-of-range dial cancels whole preview without partial edit");
  load(w,type);const Ref driver{"driver","","generator.rotation"};w.host.session.apply({CreatePrimitive{"comp","","driver","Driver",default_primitive("driver-source","nect.shape.polygon")},Set{driver,120},Link{rotation,Binding{driver,1,0,"copy_local_value"}}},w.host.session.revision());w.host.edited();events();check(!knob(w)->isEnabled()&&evaluate(w.host.session.document()).at(rotation)==120,"binding driven primitive disables dial");
  load(w,type);k=knob(w);const auto close_bytes=encode(w.host.session.document());press(k);move(k,10);w.close();events();check(!w.host.session.gesture_active()&&encode(w.host.session.document())==close_bytes,"Window close cancels own preview before flushing");w.show();events();k=knob(w);press(k);check(w.host.session.gesture_active(),"surviving Inspector remains usable after a close attempt without rebuilding");QTest::keyClick(k,Qt::Key_Escape);events();


 }
 load(w,"nect.shape.polygon");w.host.session.apply({AddOperation{"shape",default_operation("baseline-repeater","nect.shape.repeater"),1}},w.host.session.revision());w.host.edited();events();QWidget* repeater=nullptr;for(auto*k:w.findChildren<QWidget*>("repeater-angle-knob-baseline-repeater"))if(k->isVisible()){repeater=k;break;}check(repeater,"existing Repeater control remains available");const auto repeat_direction=indicator(w,repeater);check(repeat_direction.y()<-2&&std::abs(repeat_direction.x())<2,"existing Repeater zero-at-top rendering is unchanged");
 std::cout<<"PASS "<<checks<<" primitive angle checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
