#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QImage>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QStatusBar>
#include <QPointF>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
QByteArray reference(const Ref&r){return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},
    {"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);}
QWidget* named(Window&w,const char*name){for(auto* x:w.findChildren<QWidget*>(QString::fromLatin1(name)))if(x->isVisible())return x;throw std::runtime_error("Visible angle dial is missing");}
QLineEdit* field(Window&w,const Ref&r){for(auto* x:w.findChildren<QLineEdit*>())if(x->isVisible()&&x->property("nect-reference").toByteArray()==reference(r))return x;throw std::runtime_error("Visible numeric Ref is missing");}
QPointF point(double degrees){const auto radians=degrees*std::acos(-1.0)/180.0;return {22+16*std::sin(radians),22-16*std::cos(radians)};}
void mouse(QWidget* knob,QEvent::Type type,double degrees,Qt::MouseButton button,Qt::MouseButtons buttons){
    const auto local=point(degrees);QMouseEvent event(type,local,QPointF(knob->mapToGlobal(QPoint(0,0)))+local,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(knob,&event);events();
}
void press(QWidget*k,double angle=0){mouse(k,QEvent::MouseButtonPress,angle,Qt::LeftButton,Qt::LeftButton);}
void move(QWidget*k,double angle){mouse(k,QEvent::MouseMove,angle,Qt::NoButton,Qt::LeftButton);}
void release(QWidget*k,double angle=0){mouse(k,QEvent::MouseButtonRelease,angle,Qt::LeftButton,Qt::NoButton);}
QPointF rendered_indicator(Window&w,QWidget*k){
    events();QTest::qWait(20);
    for(auto*p=k->parentWidget();p;p=p->parentWidget())if(auto*scroll=qobject_cast<QScrollArea*>(p)){scroll->ensureWidgetVisible(k);break;}
    events();QTest::qWait(150);const auto whole=w.grab().toImage();const auto dpr=whole.devicePixelRatio();
    const auto origin=k->mapTo(&w,QPoint(0,0));const auto image=whole.copy(QRect(qRound(origin.x()*dpr),qRound(origin.y()*dpr),qRound(k->width()*dpr),qRound(k->height()*dpr)));
    QPointF sum;int count=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto color=image.pixelColor(x,y);
        const QPointF delta((x+0.5)/dpr-22,(y+0.5)/dpr-22);const auto radius=std::hypot(delta.x(),delta.y());
        if(radius>4&&radius<14&&color.blue()-color.red()>50&&color.green()>100){sum+=delta;++count;}}
    check(count>5,"Rendered Repeater dial exposes a visible indicator stroke");return sum/count;
}
const Ref repeater_rotation{"angle-shape","","op.angle-repeater.rotation"};
const Ref primitive_rotation{"angle-shape","","generator.rotation"};
void load(Window&w){
    auto d=empty_document("repeater-angle-doc","angle-comp","angle-art");Session session(d);
    auto source=default_primitive("angle-source","nect.shape.polygon");
    session.apply({CreatePrimitive{"angle-comp","","angle-shape","Angle Shape",source},
        AddOperation{"angle-shape",default_operation("angle-repeater","nect.shape.repeater"),1}},0);
    w.canvas->set_selection({});w.host.session=Session(session.document());w.host.session_id+="-angle-fixture";w.host.edited();
    w.canvas->set_selection("angle-shape");events();
}
void edit(Window&w,const Ref&r,const char*text){auto*x=field(w,r);x->setFocus();x->selectAll();QTest::keyClicks(x,text);QTest::keyClick(x,Qt::Key_Return);events();}
void live_numeric_preview(Window&w){
    load(w);auto* zero_dial=named(w,"repeater-angle-knob-angle-repeater");const auto zero_direction=rendered_indicator(w,zero_dial);
    check(zero_direction.y()<-2&&std::abs(zero_direction.x())<2,"Existing Repeater zero-degree indicator remains at the top");
    const auto exact=QStringLiteral("725.1234567890123");edit(w,repeater_rotation,exact.toUtf8().constData());
    auto* numeric=field(w,repeater_rotation);auto* dial=named(w,"repeater-angle-knob-angle-repeater");
    check(dial->property("nect-reference").toByteArray()==reference(repeater_rotation),"Repeater dial and numeric editor share the exact stable rotation Ref");
    const auto initial=evaluate(w.host.session.document()).at(repeater_rotation);const auto rev=w.host.session.revision();
    press(dial);move(dial,10);
    const auto preview=evaluate(w.host.session.preview_document()).at(repeater_rotation);
    check(numeric->text().toDouble()==preview&&std::abs(preview-initial-10)<1e-9,"Live numeric field previews exact Repeater dial value");
    release(dial,10);check(w.host.session.revision()==rev+1,"One Repeater drag commits one revision");
}
void precision_history_turns_and_cancel(Window&w){
    load(w);const QString long_value="725.1234567890123";edit(w,repeater_rotation,long_value.toUtf8().constData());
    auto* numeric=field(w,repeater_rotation);auto* dial=named(w,"repeater-angle-knob-angle-repeater");
    const auto authored=evaluate(w.host.session.document()).at(repeater_rotation);
    check(numeric->text()==QString::number(authored,'g',17),"Long-fraction Repeater numeric display round-trips the authored double");
    check(dial->accessibleDescription().contains(QString::number(authored,'g',17)),"Dial accessibility reports the exact unwrapped authored angle");
    const auto before=w.host.session.document();const auto rev=w.host.session.revision();
    press(dial);move(dial,10);const auto live=evaluate(w.host.session.preview_document()).at(repeater_rotation);
    check(numeric->text()==QString::number(live,'g',17),"Long-fraction numeric field follows exact live dial preview");
    release(dial,10);check(w.host.session.revision()==rev+1,"Dial commits one revision after a continuous gesture");
    w.host.session.undo(w.host.session.revision());w.host.edited();events();
    check(w.host.session.document()==before&&field(w,repeater_rotation)->text()==QString::number(authored,'g',17),"One Undo restores the exact prior double and numeric display");
    w.host.session.redo(w.host.session.revision());w.host.edited();events();
    check(std::abs(evaluate(w.host.session.document()).at(repeater_rotation)-(authored+10))<1e-9,"One Redo restores the same signed preview");

    edit(w,repeater_rotation,"-725.25");dial=named(w,"repeater-angle-knob-angle-repeater");
    check(evaluate(w.host.session.document()).at(repeater_rotation)==-725.25,"Signed negative multi-turn angle remains authored");
    const auto full_rev=w.host.session.revision();press(dial);for(int angle=45;angle<=360;angle+=45)move(dial,angle%360);release(dial,0);
    check(w.host.session.revision()==full_rev+1&&std::abs(evaluate(w.host.session.document()).at(repeater_rotation)-(-365.25))<1e-9,"Full clockwise turn adds 360 without normalizing the authored value");
    dial=named(w,"repeater-angle-knob-angle-repeater");const auto reverse_rev=w.host.session.revision();press(dial);
    for(int angle=-45;angle>=-360;angle-=45)move(dial,angle);release(dial,0);
    check(w.host.session.revision()==reverse_rev+1&&std::abs(evaluate(w.host.session.document()).at(repeater_rotation)-(-725.25))<1e-9,"Full counterclockwise turn subtracts 360 without wrapping");

    edit(w,repeater_rotation,"0");dial=named(w,"repeater-angle-knob-angle-repeater");
    const auto neutral=w.host.session.document();const auto neutral_rev=w.host.session.revision();press(dial);release(dial,0);
    check(w.host.session.revision()==neutral_rev&&w.host.session.document()==neutral,"No-motion click leaves history and authored state unchanged");
    dial=named(w,"repeater-angle-knob-angle-repeater");press(dial);
    check(w.host.session.gesture_active(),"Fresh Repeater dial begins the floating-residue trace");
    for(const double angle:{-26.099815652003855,-93.32846161547081,52.900539558665514,50.17519401215165,4.952368972990399,-77.02241444254298,7.673784129310434,25.757542601731984,0.0})move(dial,angle);release(dial,0);
    check(w.host.session.revision()==neutral_rev&&w.host.session.document()==neutral&&!w.host.session.gesture_active(),"Floating-residue no-net trace cancels instead of creating a microscopic angle revision");
    dial=named(w,"repeater-angle-knob-angle-repeater");press(dial);move(dial,25);
    check(w.host.session.gesture_active(),"A Repeater drag owns a live Session preview");QTest::keyClick(dial,Qt::Key_Escape);events();
    check(w.host.session.revision()==neutral_rev&&!w.host.session.gesture_active()&&w.host.session.document()==neutral,"Escape cancels the Repeater preview atomically");
}
void invalid_expression_draft(Window&w){
    load(w);edit(w,repeater_rotation,"725.1234567890123");
    const auto before=encode(w.host.session.document());const auto rev=w.host.session.revision();
    auto* numeric=field(w,repeater_rotation);numeric->setFocus();numeric->selectAll();QTest::keyClicks(numeric,"=sin(");QTest::keyClick(numeric,Qt::Key_Return);events();
    bool retained=false;for(auto* editor:w.findChildren<QPlainTextEdit*>())if(editor->isVisible()&&editor->property("nect-reference").toByteArray()==reference(repeater_rotation)&&editor->toPlainText()=="sin(")retained=true;
    check(retained&&encode(w.host.session.document())==before&&w.host.session.revision()==rev,"Invalid Repeater expression remains a separately retained draft without authored mutation");
    check(field(w,repeater_rotation)->text()==QString::number(evaluate(w.host.session.document()).at(repeater_rotation),'g',17),"Invalid-expression fallback keeps the exact numeric display");
}
void numeric_draft_refusal(Window&w){
    load(w);auto* numeric=field(w,repeater_rotation);auto* dial=named(w,"repeater-angle-knob-angle-repeater");
    numeric->setText("45");numeric->setModified(true);const auto before=encode(w.host.session.document());const auto rev=w.host.session.revision();
    press(dial);release(dial);
    check(!w.host.session.gesture_active()&&numeric->text()=="45"&&numeric->isModified(),"Repeater dial refuses to overwrite an in-progress numeric draft");
    check(encode(w.host.session.document())==before&&w.host.session.revision()==rev&&
        w.statusBar()->currentMessage().startsWith("UNCOMMITTED_INPUT"),"Draft refusal keeps authored bytes/revision unchanged and explains the conflict");
    bool finished=false;QObject signal_scope;QObject::connect(numeric,&QLineEdit::editingFinished,&signal_scope,[&]{finished=true;});numeric->setFocus();events();
    w.canvas->set_selection({});events();
    check(finished&&evaluate(w.host.session.document()).at(repeater_rotation)==45&&w.host.session.revision()==rev+1&&
        !w.host.session.gesture_active(),"Leaving the dirty field commits its ordinary numeric edit once without reentrant Inspector teardown");
    w.canvas->set_selection("angle-shape");events();numeric=field(w,repeater_rotation);
    check(numeric->text()=="45","Inspector rebuild displays the accepted focus-out numeric value");
    numeric->setText("50");numeric->setModified(true);numeric->setFocus();const auto refresh_revision=w.host.session.revision();w.refresh();events();
    check(w.host.session.revision()==refresh_revision+1&&evaluate(w.host.session.document()).at(repeater_rotation)==50&&
        field(w,repeater_rotation)->text()=="50","Same-selection refresh accepts a dirty numeric edit and rebuilds from current Session values");
    dial=named(w,"repeater-angle-knob-angle-repeater");const auto before_dial_revision=w.host.session.revision();
    press(dial);move(dial,5);release(dial,5);
    check(w.host.session.revision()==before_dial_revision+1&&
        std::abs(evaluate(w.host.session.document()).at(repeater_rotation)-55)<1e-9,
        "Rebuilt dial remains live from the accepted numeric baseline after focus-out/rebuild sequencing");
}
void busy_and_aba(Window&w){
    load(w);auto* dial=named(w,"repeater-angle-knob-angle-repeater");auto& s=w.host.session;
    s.begin_gesture(s.revision());s.update_gesture({EditProperties{{repeater_rotation},222,false}});const auto busy=s.preview_document();
    press(dial);release(dial);check(s.gesture_active()&&s.preview_document()==busy,"Busy Session gesture is not stolen or canceled by the Repeater dial");s.cancel_gesture();
    for(int action=0;action<4;++action){
        w.host.edited();events();dial=named(w,"repeater-angle-knob-angle-repeater");press(dial);move(dial,5);
        check(s.gesture_active(),"Old Repeater dial owns its initial preview");s.cancel_gesture();s.begin_gesture(s.revision());
        s.update_gesture({EditProperties{{repeater_rotation},321,false}});const auto replacement=s.preview_document();
        if(action==0){move(dial,15);release(dial,15);}else if(action==1)release(dial,5);else if(action==2)QTest::keyClick(dial,Qt::Key_Escape);else w.refresh(false);
        events();check(s.gesture_active()&&s.preview_document()==replacement,"Same-revision replacement gesture survives stale preview/commit/cancel/rebuild callback");s.cancel_gesture();
    }
    w.host.edited();events();dial=named(w,"repeater-angle-knob-angle-repeater");
    press(dial);move(dial,5);s.cancel_gesture();s.begin_gesture(s.revision());
    s.update_gesture({EditProperties{{repeater_rotation},321,false}});move(dial,15);release(dial,15);
    check(s.gesture_active()&&evaluate(s.preview_document()).at(repeater_rotation)==321,"Stale dial callback may not commit or cancel replacement preview B");
    s.cancel_gesture();const auto initial=evaluate(s.document()).at(repeater_rotation);auto* numeric=field(w,repeater_rotation);
    press(dial);check(s.gesture_active()&&evaluate(s.preview_document()).at(repeater_rotation)==initial&&
        numeric->text()==QString::number(initial,'g',17),"Same live dial re-begins from committed state after foreign preview B is canceled");
    move(dial,10);const auto reused_preview=evaluate(s.preview_document()).at(repeater_rotation);
    check(std::abs(reused_preview-(initial+10))<1e-9,"Reused dial authors from the refreshed committed baseline, not canceled preview B");
    release(dial,10);check(std::abs(evaluate(s.document()).at(repeater_rotation)-(initial+10))<1e-9,"Reused dial commits its own exact +10 sample");
}
void stale_target_validation(Window&w){
    load(w);auto* dial=named(w,"repeater-angle-knob-angle-repeater");auto& s=w.host.session;
    const auto original=s.document();const auto original_rev=s.revision();
    s.apply({Set{repeater_rotation,30}},s.revision());press(dial);release(dial);
    check(!s.gesture_active()&&evaluate(s.document()).at(repeater_rotation)==30&&w.statusBar()->currentMessage().startsWith("REVISION_CONFLICT"),"Stale Repeater revision refuses before changing authored state");
    load(w);dial=named(w,"repeater-angle-knob-angle-repeater");auto changed=s.document();changed.objects.at("angle-shape").source->id="replaced-source";s=Session(changed);
    const auto source_bytes=encode(s.document());press(dial);release(dial);
    check(!s.gesture_active()&&encode(s.document())==source_bytes,"Replaced object source identity refuses stale Repeater dial even at same revision");
    load(w);dial=named(w,"repeater-angle-knob-angle-repeater");changed=s.document();{
        auto& stack=changed.objects.at("angle-shape").stack;auto op=std::find_if(stack.begin(),stack.end(),[](const auto& item){return item.id=="angle-repeater";});
        check(op!=stack.end(),"Repeater test operation exists before replacement");*op=default_operation("angle-repeater","nect.shape.offset");
    }s=Session(changed);
    const auto operation_bytes=encode(s.document());press(dial);release(dial);
    check(!s.gesture_active()&&encode(s.document())==operation_bytes,"Replaced Repeater operation identity refuses the stale exact Ref");
    load(w);dial=named(w,"repeater-angle-knob-angle-repeater");changed=s.document();changed.id="replacement-document";s=Session(changed);
    const auto document_bytes=encode(s.document());press(dial);release(dial);
    check(!s.gesture_active()&&encode(s.document())==document_bytes,"Changed document identity refuses stale Repeater dial at same Session id and revision");
    load(w);dial=named(w,"repeater-angle-knob-angle-repeater");const auto session_id=w.host.session_id;w.host.session_id+="-replacement";
    press(dial);release(dial);check(!s.gesture_active()&&w.host.session_id!=session_id,"Changed Host Session identity refuses the old Repeater dial");
    (void)original;(void)original_rev;
}
void driven_and_range_policy(Window&w){
    load(w);auto& s=w.host.session;const Ref source_rotation{"angle-driver","","generator.rotation"};
    s.apply({CreatePrimitive{"angle-comp","","angle-driver","Driver",default_primitive("angle-driver-source","nect.shape.polygon")},
        Set{source_rotation,120},Link{repeater_rotation,{source_rotation,1,0,"copy_local_value"}}},s.revision());w.host.edited();events();
    check(!named(w,"repeater-angle-knob-angle-repeater")->isEnabled()&&evaluate(s.document()).at(repeater_rotation)==120,"Binding-driven Repeater rotation disables its dial");
    load(w);s=w.host.session;s.apply({SetExpression{{repeater_rotation},Expression{"30",1}}},s.revision());w.host.edited();events();
    check(!named(w,"repeater-angle-knob-angle-repeater")->isEnabled()&&evaluate(s.document()).at(repeater_rotation)==30,"Expression-driven Repeater rotation disables its dial");
    load(w);edit(w,repeater_rotation,"999999999");auto*dial=named(w,"repeater-angle-knob-angle-repeater");auto*numeric=field(w,repeater_rotation);
    const auto before=s.document();const auto rev=s.revision();press(dial);move(dial,20);
    check(s.gesture_active()&&evaluate(s.preview_document()).at(repeater_rotation)==999999999&&numeric->text()=="999999999","Out-of-range dial sample retains the last valid preview and keeps the gesture recoverable");
    move(dial,0);move(dial,0.5);check(s.gesture_active()&&numeric->text().toDouble()>999999999,"Moving back into range recovers the same Repeater gesture");release(dial,0.5);
    check(s.revision()==rev+1&&evaluate(s.document()).at(repeater_rotation)>999999999&&s.document()!=before,"Recovered in-range value commits once; invalid samples never cancel the gesture");
    load(w);edit(w,repeater_rotation,"999999999");dial=named(w,"repeater-angle-knob-angle-repeater");numeric=field(w,repeater_rotation);
    press(dial);move(dial,-10);check(evaluate(s.preview_document()).at(repeater_rotation)==999999989,"An in-range sample becomes the current gesture’s last valid value");
    QTest::keyClick(dial,Qt::Key_Escape);events();check(!s.gesture_active()&&numeric->text()=="999999999","Escape restores the new gesture’s initial value after a valid preview");
    press(dial);move(dial,20);check(s.gesture_active()&&evaluate(s.preview_document()).at(repeater_rotation)==999999999&&numeric->text()=="999999999","Repeated gesture resets its last-valid marker before an out-of-range first sample");
    move(dial,0);move(dial,0.5);release(dial,0.5);check(evaluate(s.document()).at(repeater_rotation)>999999999,"Repeated gesture recovers from its own range failure and commits its valid sample");
}
void repeated_and_coexisting(Window&w){
    load(w);auto* repeater=named(w,"repeater-angle-knob-angle-repeater");auto* primitive=named(w,"primitive-angle-knob");
    check(primitive->property("nect-reference").toByteArray()==reference(primitive_rotation),"Primitive and Repeater radial controls coexist with independent exact Refs");
    press(repeater);move(repeater,8);const auto repeater_preview=w.host.session.preview_document();press(primitive);release(primitive);
    check(w.host.session.gesture_active()&&w.host.session.preview_document()==repeater_preview,"Primitive busy begin cannot steal the Repeater preview");
    release(repeater,8);check(!w.host.session.gesture_active(),"Repeater commits after the coexisting primitive dial is refused");
    w.host.edited();events();primitive=named(w,"primitive-angle-knob");press(primitive);move(primitive,8);
    check(w.host.session.gesture_active(),"Primitive coexisting dial owns its preview before Inspector rebuild");w.refresh(false);events();
    check(!w.host.session.gesture_active(),"Inspector rebuild cancels the primitive adapter even when a Repeater adapter also exists");
    repeater=named(w,"repeater-angle-knob-angle-repeater");press(repeater);move(repeater,8);
    check(w.host.session.gesture_active(),"Repeater dial owns its preview before Inspector rebuild");w.refresh(false);events();
    check(!w.host.session.gesture_active(),"Inspector rebuild cancels the Repeater adapter synchronously");
    QTemporaryDir close_dir;check(close_dir.isValid(),"isolated close fixture scratch exists");
    QSettings close_settings(close_dir.filePath("settings.ini"),QSettings::IniFormat);
    Window close_window(close_dir.path(),std::make_unique<FolderLibrary>(close_settings));close_window.show();events();
    auto& close_session=close_window.host.session;const auto close_composition=close_session.document().compositions.front().id;
    Point close_point;close_point.id="close-point";close_point.x.literal=50;close_point.y.literal=40;
    close_session.apply({CreatePath{close_composition,"","close-object","Close",{{"close-contour",false,{close_point}}}},
        AddOperation{"close-object",default_operation("angle-repeater","nect.shape.repeater"),1}},close_session.revision());
    close_window.host.edited();close_window.canvas->set_selection("close-object");events();
    auto* close_dial=named(close_window,"repeater-angle-knob-angle-repeater");press(close_dial);move(close_dial,8);
    check(close_session.gesture_active(),"Repeater dial owns its preview before Window close");close_window.close();events();
    check(!close_session.gesture_active(),"Window close cancels a Repeater preview before Host flush");
    close_window.show();events();close_dial=named(close_window,"repeater-angle-knob-angle-repeater");const auto close_revision=close_session.revision();
    press(close_dial);move(close_dial,5);release(close_dial,5);
    check(close_session.revision()==close_revision+1,"Surviving Repeater control remains usable after a close attempt");
}
}
int main(int argc,char**argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{
    QTemporaryDir dir;check(dir.isValid(),"test scratch exists");QSettings settings(dir.filePath("settings.ini"),QSettings::IniFormat);
    Window w(dir.path(),std::make_unique<FolderLibrary>(settings));w.show();events();
    if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")check(w.devicePixelRatioF()>=1.9,"High-DPI run uses an actual scaled widget DPR");
    live_numeric_preview(w);
    precision_history_turns_and_cancel(w);invalid_expression_draft(w);numeric_draft_refusal(w);busy_and_aba(w);stale_target_validation(w);
    driven_and_range_policy(w);repeated_and_coexisting(w);
    std::cout<<"PASS "<<checks<<" Repeater angle checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
