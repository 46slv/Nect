#include "canvas.hpp"
#include "artboard_guide_alignment_fixture.hpp"
#include <QApplication>
#include <QTest>
#include <QMouseEvent>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace nect;
using nect::desktop::Canvas;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
struct Fixture {
 Session session; Canvas canvas; QString error,identity="session-one"; int commits=0;
 Fixture(Document d=guide_alignment_fixture::document(),HistoryLimits limits={}):session(std::move(d),limits),canvas(session){
  canvas.error=[this](const QString& e){error=e;};canvas.document_changed=[this]{++commits;};
  canvas.set_session_identity_provider([this]{return identity;});
  canvas.resize(1060,740);canvas.show();canvas.set_active_artboard("comp","A",true);events();
 }
 QPoint point(double x,double y){const auto b=evaluate_artboard(session.document().compositions.front(),canvas.active_artboard());return {qRound(canvas.width()/2.0+(x-b.x-b.width/2)*canvas.zoom()),qRound(canvas.height()/2.0+(y-b.y-b.height/2)*canvas.zoom())};}
 void arm(const Id& board,const Id& guide){canvas.set_active_artboard("comp",board,true);canvas.arm_artboard_guide_drag("comp",board,guide);events();}
 void press(QPoint p){QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,p);events();}
 void move(QPoint p){QTest::mouseMove(&canvas,p);events();}
 void release(QPoint p){QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,p);events();}
 double pos(const Id& board,const Id& guide,bool preview=false){for(const auto& g:effective_artboard_guides(preview?session.preview_document():session.document(),"comp",board))if(g.guide_id==guide)return g.position;throw std::runtime_error("missing guide");}
};
void near(double a,double b){check(std::abs(a-b)<1e-7,"exact local coordinate");}
void basic(const Id& board,const Id& guide,double x,double y,double start,bool vertical=false){
 Fixture f;f.arm(board,guide);auto p=f.point(x,y),q=p+(vertical?QPoint(0,20):QPoint(20,0));const auto original=encode(f.session.document());
 f.press(p);check(f.session.gesture_active(),"armed occurrence starts preview");f.move(q);near(f.pos(board,guide,true),start+20);check(encode(f.session.document())==original&&f.session.revision()==0,"preview is not committed");
 f.release(q);near(f.pos(board,guide),start+20);check(f.session.revision()==1&&f.commits==1&&!f.canvas.guide_edit_mode(),"release is one commit and disarms");
 f.session.undo(f.session.revision());check(encode(f.session.document())==original,"Undo restores exact override presence and authored bytes");
}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
 basic("A","GX",1090,110,90);basic("B","GX",2040,110,40);basic("A","local",1015,110,15);basic("source","GX",40,110,40);basic("B","GY",2100,40,30,true);
 {Fixture f;f.arm("B","GX");const auto before=encode(f.session.document());auto p=f.point(2040,110);f.press(p);f.move(p+QPoint(30,0));f.move(p);f.release(p);check(encode(f.session.document())==before&&f.session.revision()==0&&f.commits==0,"no-net inherited drag creates neither override nor History");}
 for(int cancel=0;cancel<5;++cancel){Fixture f;f.arm("A","GX");const auto before=encode(f.session.document());auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(20,0));if(cancel==0)QTest::keyClick(&f.canvas,Qt::Key_Escape);if(cancel==1)f.canvas.set_show_guides(false);if(cancel==2)f.canvas.set_guide_edit_mode(false);if(cancel==3)f.canvas.set_active_artboard("comp","B",true);if(cancel==4){f.identity="replacement";f.canvas.refresh();}f.release(p+QPoint(20,0));check(!f.session.gesture_active()&&!f.canvas.guide_edit_mode()&&encode(f.session.document())==before&&f.session.revision()==0,"canceled scoped drag has no commit");}
 {auto d=guide_alignment_fixture::document();d.compositions.front().guides.push_back({"overlap","Overlap","x",1090});Fixture f(d);f.arm("A","GX");auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(20,0));f.release(p+QPoint(20,0));near(f.pos("A","GX"),110);near(f.pos("B","GX"),40);near(f.pos("source","GX"),40);check(f.session.document().compositions.front().guides.back().position==1090,"global overlapping Guide not retargeted");}
 {Fixture f;f.arm("A","GX");auto p=f.point(1090,-20);f.press(p);check(!f.session.gesture_active()&&!f.canvas.guide_edit_mode(),"outside clipped segment misses and disarms");f.release(p);}
 {Fixture f;f.canvas.set_show_guides(false);bool refused=false;try{f.canvas.arm_artboard_guide_drag("comp","A","GX");}catch(const Error&){refused=true;}check(refused&&!f.canvas.show_guides(),"hidden overlays cannot arm or silently reveal");}
 {Fixture f;f.arm("A","GX");f.canvas.set_zoom(2);auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(40,0));near(f.pos("A","GX",true),110);f.canvas.fit_all_artboards();f.move(p+QPoint(60,0));near(f.pos("A","GX",true),120);f.release(p+QPoint(60,0));near(f.pos("A","GX"),120);}
 {Fixture f(guide_alignment_fixture::document(),HistoryLimits{1024,1});f.arm("A","GX");const auto before=encode(f.session.document());auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(20,0));f.release(p+QPoint(20,0));check(encode(f.session.document())==before&&f.session.revision()==0&&!f.session.gesture_active()&&!f.error.isEmpty(),"History admission rejection cancels scoped preview");}

 for(int cancel=0;cancel<5;++cancel){Fixture f;f.arm("A","GX");if(cancel==0)QTest::keyClick(&f.canvas,Qt::Key_Escape);if(cancel==1)f.canvas.set_show_guides(false);if(cancel==2)f.canvas.set_guide_edit_mode(false);if(cancel==3)f.canvas.set_active_artboard("comp","B",true);if(cancel==4){f.identity="replacement";f.canvas.refresh();}check(!f.canvas.guide_edit_mode(),"armed-only cancellation disarms");}
 {Fixture f;f.arm("A","GX");auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(20,0));f.release(p+QPoint(35,0));near(f.pos("A","GX"),125);}
 {Fixture f;f.arm("A","GX");const auto before=encode(f.session.document());auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(20,0));QMouseEvent invalid(QEvent::MouseButtonRelease,QPointF(1e200,100),QPointF(1e200,100),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&f.canvas,&invalid);events();check(encode(f.session.document())==before&&f.session.revision()==0&&!f.session.gesture_active(),"invalid release cancels prior valid preview");}
 for(int kind=0;kind<3;++kind){Fixture f;f.arm("A","GX");auto p=f.point(1090,110);const auto before=encode(f.session.document());f.press(p);if(kind==0){f.move(p+QPoint(20,0));f.move(p);}if(kind==1)f.move(p+QPoint(0,30));f.release(p);check(encode(f.session.document())==before&&f.session.revision()==0,"existing override no-net/no-motion/orthogonal drag preserves History");}
 {auto d=guide_alignment_fixture::document();d.compositions.front().guides.push_back({"different","Different","x",1200});Fixture f(d);f.arm("A","GX");auto p=f.point(1200,110);f.press(p);check(!f.session.gesture_active()&&!f.canvas.guide_edit_mode(),"armed miss cannot fall back to another global Guide");f.release(p);check(f.session.revision()==0,"miss leaves document revision unchanged");}
 for(int kind=0;kind<3;++kind){auto d=guide_alignment_fixture::document();auto& board=d.compositions.front().artboards[1];if(kind==0)board.template_assignment->guide_enabled_overrides["GX"]=false;if(kind==1)board.template_assignment->guide_position_overrides["GX"]=-1;if(kind==2)board.template_assignment.reset();Fixture f(d);bool refused=false;try{f.canvas.arm_artboard_guide_drag("comp","A","GX");}catch(const Error&){refused=true;}check(refused&&!f.canvas.guide_edit_mode(),"disabled/outside/missing occurrence cannot arm");}
 {auto d=guide_alignment_fixture::document();d.compositions.front().artboards[1].x=-1000;d.compositions.front().artboards[1].y=-500;Fixture f(d);f.arm("A","GX");auto p=f.point(-910,-400);f.press(p);f.release(p+QPoint(20,0));near(f.pos("A","GX"),110);}
 {Fixture f;f.arm("B","GX");auto p=f.point(2040,110);f.press(p);f.release(p-QPoint(60,0));near(f.pos("B","GX"),-20);check(f.session.revision()==1,"valid drag may persist beyond clip boundary");}
 {Fixture f;f.arm("A","GX");auto p=f.point(1090,110);f.press(p);f.move(p+QPoint(20,0));f.session.cancel_gesture();f.session.apply({ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",130.0}}},f.session.revision());const auto newer=encode(f.session.document());f.canvas.refresh();f.release(p+QPoint(30,0));check(encode(f.session.document())==newer&&!f.canvas.guide_edit_mode(),"external revision cannot be overwritten by captured drag");}
 {Fixture f;f.arm("A","GX");f.session.apply({ArtboardGuideCommand{SetArtboardGuideOverride{"comp","A","GX","position",130.0}}},f.session.revision());auto p=f.point(1090,110);f.press(p);f.release(p+QPoint(20,0));near(f.pos("A","GX"),130);check(!f.canvas.guide_edit_mode(),"revision change between arm and press cancels");}

 for(int phase=0;phase<3;++phase){Fixture f;f.arm("A","GX");auto p=f.point(1090,110);const auto before=encode(f.session.document());if(phase>0){f.press(p);f.move(p+QPoint(20,0));}if(phase==2){QMouseEvent invalid(QEvent::MouseMove,QPointF(1e200,100),QPointF(1e200,100),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&f.canvas,&invalid);}QTest::mousePress(&f.canvas,Qt::MiddleButton,Qt::NoModifier,p);QTest::mouseRelease(&f.canvas,Qt::MiddleButton,Qt::NoModifier,p);f.release(p);check(encode(f.session.document())==before&&f.session.revision()==0&&!f.session.gesture_active()&&!f.canvas.guide_edit_mode(),"pan interruption cancels armed/valid/invalid scoped gesture");}

 {auto d=guide_alignment_fixture::document();d.compositions.front().artboards[2].x=1000;d.compositions.front().artboards[2].template_assignment->guide_position_overrides["GX"]=90;Fixture f(d);f.arm("A","GX");auto p=f.point(1090,110);f.move(p);check(f.canvas.cursor().shape()==Qt::CrossCursor,"armed scoped hit advertises drag cursor");f.press(p);f.release(p+QPoint(20,0));near(f.pos("A","GX"),110);near(f.pos("B","GX"),90);near(f.pos("source","GX"),40);}
 {Fixture f;f.arm("B","GX");auto p=f.point(2040,110);f.press(p);f.move(p+QPoint(20,0));f.session.cancel_gesture();f.session.apply({ArtboardGuideCommand{UpdateArtboardGuide{"comp","source",{"GX","Renamed source","x",55,true}}}},f.session.revision());const auto newer=encode(f.session.document());f.canvas.refresh();f.release(p+QPoint(30,0));check(encode(f.session.document())==newer&&!f.canvas.guide_edit_mode(),"source Guide edit during drag cancels captured occurrence");near(f.pos("B","GX"),55);}
 {Fixture f;f.arm("B","GX");auto p=f.point(2040,110);f.press(p);f.release(p+QPoint(20,0));f.arm("B","GX");p=f.point(2060,110);f.press(p);f.release(p+QPoint(20,0));near(f.pos("B","GX"),80);check(f.session.revision()==2&&f.commits==2,"repeated one-shot interactions each commit once");}
 std::cout<<"PASS "<<checks<<" scoped Guide Canvas checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
