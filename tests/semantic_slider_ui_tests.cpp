#include "semantic_slider.hpp"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QWheelEvent>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using nect::desktop::SemanticSlider;

namespace {
int checks=0;
void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
void events() {QApplication::processEvents();}
void prepare(SemanticSlider& slider) {
    slider.resize(260,36);
    slider.show();
    slider.setFocus();
    events();
}
QPoint handle(const SemanticSlider& slider) {
    QStyleOptionSlider option;
    option.initFrom(&slider);
    option.orientation=slider.orientation();
    option.minimum=slider.minimum();
    option.maximum=slider.maximum();
    option.sliderPosition=slider.sliderPosition();
    option.sliderValue=slider.value();
    option.singleStep=slider.singleStep();
    option.pageStep=slider.pageStep();
    option.upsideDown=slider.invertedAppearance()!=(slider.layoutDirection()==Qt::RightToLeft);
    return slider.style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderHandle,&slider).center();
}
void mouse(SemanticSlider* slider,QEvent::Type type,const QPoint& position,
           Qt::MouseButton button,Qt::MouseButtons buttons) {
    QMouseEvent event(type,QPointF(position),QPointF(slider->mapToGlobal(position)),button,buttons,Qt::NoModifier);
    QApplication::sendEvent(slider,&event);
}
void press(SemanticSlider* slider) {mouse(slider,QEvent::MouseButtonPress,handle(*slider),Qt::LeftButton,Qt::LeftButton);}
void release(SemanticSlider* slider) {mouse(slider,QEvent::MouseButtonRelease,handle(*slider),Qt::LeftButton,Qt::NoButton);}
void key(SemanticSlider* slider,int code) {
    QKeyEvent event(QEvent::KeyPress,code,Qt::NoModifier);
    QApplication::sendEvent(slider,&event);
}
struct Recorder {
    bool allow=true;
    int begins=0,commits=0,cancels=0;
    std::vector<double> previews;
    std::vector<std::string> order;
    explicit Recorder(SemanticSlider& slider) {
        slider.begin=[this] {++begins;order.push_back("begin");return allow;};
        slider.preview_delta=[this](double delta) {previews.push_back(delta);order.push_back("preview");};
        slider.commit=[this] {++commits;order.push_back("commit");};
        slider.cancel=[this] {++cancels;order.push_back("cancel");};
    }
};
void invalid_range(int minimum,int maximum) {
    try {SemanticSlider slider(minimum,maximum);}
    catch(const std::invalid_argument&) {++checks;return;}
    throw std::runtime_error("Invalid slider range accepted");
}

void construction_and_idle() {
    SemanticSlider slider(-500000,500000);
    Recorder recorder(slider);
    check(slider.orientation()==Qt::Horizontal&&slider.minimum()==-500000&&slider.maximum()==500000,
          "Slider has the requested bounded horizontal delta range");
    check(slider.value()==0&&slider.sliderPosition()==0&&slider.singleStep()==1&&slider.hasTracking(),
          "Initial indicator is zero with single-step tracking");
    check(slider.focusPolicy()==Qt::StrongFocus&&!slider.accessibleName().isEmpty()&&slider.minimumWidth()>=100,
          "Slider has keyboard focus, an accessible name and usable width");
    slider.setValue(23);
    slider.setSliderPosition(-17);
    events();
    check(recorder.begins==0&&recorder.previews.empty()&&recorder.commits==0&&recorder.cancels==0,
          "Programmatic idle initialization never begins, previews or commits authored state");
    slider.disarm();
    check(slider.value()==0&&slider.sliderPosition()==0,"Disarm returns idle presentation to zero");
    SemanticSlider singleton(0,0);
    check(singleton.minimum()==0&&singleton.maximum()==0&&singleton.value()==0,
          "A zero-only delta range is valid for an immovable selection");
    invalid_range(1,2);
    invalid_range(-2,-1);
    invalid_range(2,-2);
    invalid_range(-500001,500000);
    invalid_range(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
}

void mouse_lifecycle() {
    SemanticSlider slider(-100,100);
    Recorder recorder(slider);
    prepare(slider);
    const auto start=handle(slider);
    press(&slider);
    check(recorder.begins==1&&slider.isSliderDown()&&recorder.commits==0,
          "Real handle press starts one owned interaction without committing");
    mouse(&slider,QEvent::MouseMove,start+QPoint(40,0),Qt::NoButton,Qt::LeftButton);
    check(!recorder.previews.empty()&&recorder.previews.back()>0&&recorder.commits==0,
          "Real Qt handle movement previews a positive delta after its input handler");
    const auto previews=recorder.previews.size();
    slider.setSliderPosition(7);
    slider.setSliderPosition(12);
    check(recorder.previews.size()==previews,"Setter previews are deferred beyond the Qt setter stack");
    events();
    check(recorder.previews.size()==previews+1&&recorder.previews.back()==12,
          "Owned programmatic position changes coalesce to the latest preview");
    release(&slider);
    check(recorder.commits==1&&recorder.cancels==0&&slider.value()==0&&!slider.isSliderDown(),
          "Release commits once and disarms transient presentation");
    release(&slider);
    check(recorder.commits==1,"Repeated release cannot commit again");

    press(&slider);
    slider.setSliderPosition(-19);
    release(&slider);
    check(recorder.previews.back()==-19&&recorder.commits==2&&recorder.order.back()=="commit",
          "Release flushes the latest unprocessed preview before committing");
    const auto completed=recorder.previews.size();
    events();
    check(recorder.previews.size()==completed,"Queued preview cannot run after gesture completion");
}

void cancellation_and_disarm() {
    SemanticSlider slider(-100,100);
    Recorder recorder(slider);
    prepare(slider);
    press(&slider);
    slider.setValue(21);
    events();
    key(&slider,Qt::Key_Escape);
    check(recorder.previews==std::vector<double>{21}&&recorder.cancels==1&&recorder.commits==0,
          "Escape calls cancel once after a live preview without committing");
    check(slider.value()==0&&slider.sliderPosition()==0&&!slider.isSliderDown(),
          "Escape clears the transient position and pressed indicator");
    release(&slider);
    key(&slider,Qt::Key_Escape);
    check(recorder.cancels==1&&recorder.commits==0,"Release and Escape after cancellation have no terminal callback");
    press(&slider);
    slider.setValue(31);
    key(&slider,Qt::Key_Escape);
    events();
    check(recorder.previews==std::vector<double>{21}&&recorder.cancels==2,
          "Cancellation drops a not-yet-delivered preview");

    int value_signals=0;
    QObject::connect(&slider,&QSlider::valueChanged,&slider,[&](int) {++value_signals;});
    press(&slider);
    slider.setValue(44);
    slider.disarm();
    events();
    release(&slider);
    check(slider.value()==0&&!slider.isSliderDown()&&value_signals==1&&recorder.commits==0&&recorder.cancels==2,
          "Explicit disarm blocks reset signals and prevents queued previews or terminal callbacks");
    slider.blockSignals(true);
    slider.disarm();
    check(slider.signalsBlocked(),"Disarm preserves a caller's existing signal blocking");
    slider.blockSignals(false);

    press(&slider);
    slider.setValue(5);
    slider.disarm();
    press(&slider);
    slider.setValue(9);
    events();
    check(recorder.previews.back()==9&&recorder.previews.size()==2,
          "An old queued preview cannot retarget a replacement interaction");
    release(&slider);
    check(recorder.commits==1,"A fresh interaction commits after repeated cancellation and disarming");
}

void refused_and_other_input() {
    SemanticSlider slider(-20,20);
    Recorder recorder(slider);
    prepare(slider);
    recorder.allow=false;
    press(&slider);
    slider.setValue(6);
    release(&slider);
    key(&slider,Qt::Key_Right);
    events();
    check(recorder.begins==2&&recorder.previews.empty()&&recorder.commits==0&&recorder.cancels==0&&!slider.isSliderDown(),
          "Begin refusal cannot arm mouse or keyboard mutation");
    slider.disarm();
    recorder.allow=true;
    mouse(&slider,QEvent::MouseButtonPress,handle(slider),Qt::RightButton,Qt::RightButton);
    key(&slider,Qt::Key_A);
    check(recorder.begins==2,"Right-click and unrelated keys do not begin a transaction");
    slider.setEnabled(false);
    press(&slider);
    key(&slider,Qt::Key_Right);
    check(recorder.begins==2,"Disabled input cannot start a transaction");
    slider.setEnabled(true);
    slider.clearFocus();
    for(bool focus:{false,true}) {
        if(focus)slider.setFocus();
        QWheelEvent wheel(QPointF(10,10),QPointF(slider.mapToGlobal(QPoint(10,10))),QPoint(),QPoint(0,120),
                          Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(&slider,&wheel);
    }
    check(recorder.begins==2&&slider.value()==0,"Wheel scrolling never authors values, even when focused");
    press(&slider);
    key(&slider,Qt::Key_Right);
    check(recorder.begins==3&&recorder.commits==0,"Keyboard actions cannot nest inside an owned mouse gesture");
    QFocusEvent lost(QEvent::FocusOut,Qt::OtherFocusReason);
    QApplication::sendEvent(&slider,&lost);
    check(recorder.cancels==1&&recorder.commits==0&&slider.value()==0&&!slider.isSliderDown(),
          "Losing focus cancels an interrupted mouse interaction");

    SemanticSlider unbound(-20,20);
    prepare(unbound);
    press(&unbound);
    key(&unbound,Qt::Key_Right);
    check(!unbound.isSliderDown()&&unbound.value()==0,"A missing begin callback never arms an unbound control");
}

void keyboard_lifecycle_and_limits() {
    SemanticSlider slider(-20,30);
    Recorder recorder(slider);
    prepare(slider);
    const std::vector<int> keys={Qt::Key_Right,Qt::Key_Left,Qt::Key_Up,Qt::Key_Down,
                                 Qt::Key_PageUp,Qt::Key_PageDown,Qt::Key_Home,Qt::Key_End};
    const std::vector<double> expected={1,-1,1,-1,10,-10,-20,30};
    for(const auto code:keys) {
        key(&slider,code);
        check(slider.value()==0&&!slider.isSliderDown(),"Each keyboard action disarms its transient delta");
    }
    check(recorder.previews==expected&&recorder.begins==8&&recorder.commits==8&&recorder.cancels==0,
          "Arrow, Page, Home and End keys each begin, preview and commit one bounded delta");
    for(std::size_t n=0;n<recorder.order.size();n+=3)
        check(recorder.order[n]=="begin"&&recorder.order[n+1]=="preview"&&recorder.order[n+2]=="commit",
              "Keyboard callback ordering is begin-preview-commit");
    SemanticSlider limits(-3,2);
    Recorder bounded(limits);
    key(&limits,Qt::Key_PageUp);
    key(&limits,Qt::Key_PageDown);
    key(&limits,Qt::Key_End);
    key(&limits,Qt::Key_Home);
    check(bounded.previews==std::vector<double>{2,-3,2,-3},"Page and endpoint actions respect asymmetric limits");
    limits.setInvertedControls(true);
    key(&limits,Qt::Key_Right);
    check(bounded.previews.back()==-1,"Native Qt inverted keyboard controls remain supported");
    SemanticSlider singleton(0,0);
    Recorder no_delta(singleton);
    key(&singleton,Qt::Key_Right);
    check(no_delta.previews==std::vector<double>{0}&&no_delta.begins==1&&no_delta.commits==1,
          "An accepted limit action has one zero preview and terminal callback");
}

void callback_destruction() {
    for(bool keyboard:{false,true}) {
        QPointer<SemanticSlider> slider=new SemanticSlider(-100,100);
        prepare(*slider);
        int begins=0,commits=0;
        slider->begin=[&] {++begins;delete slider.data();return true;};
        slider->commit=[&] {++commits;};
        if(keyboard)key(slider,Qt::Key_Right);else press(slider);
        check(!slider&&begins==1&&commits==0,"Synchronous destruction in begin is guarded for mouse and keyboard");
    }
    for(int source:{0,1,2}) {
        QPointer<SemanticSlider> slider=new SemanticSlider(-100,100);
        prepare(*slider);
        int previews=0,commits=0;
        slider->begin=[] {return true;};
        slider->preview_delta=[&](double) {++previews;delete slider.data();};
        slider->commit=[&] {++commits;};
        if(source==0)key(slider,Qt::Key_Right);
        else {
            const auto start=handle(*slider);
            press(slider);
            if(source==1)mouse(slider,QEvent::MouseMove,start+QPoint(40,0),Qt::NoButton,Qt::LeftButton);
            else {slider->setSliderPosition(11);events();}
        }
        check(!slider&&previews==1&&commits==0,"Preview destruction is safe after keyboard, mouse or queued setter input");
    }
    for(bool keyboard:{false,true}) {
        QPointer<SemanticSlider> slider=new SemanticSlider(-100,100);
        prepare(*slider);
        int commits=0;
        slider->begin=[] {return true;};
        slider->commit=[&] {++commits;delete slider.data();};
        if(keyboard)key(slider,Qt::Key_Right);else {press(slider);release(slider);}
        check(!slider&&commits==1,"Commit can synchronously delete the mouse or keyboard control");
    }
    QPointer<SemanticSlider> slider=new SemanticSlider(-100,100);
    prepare(*slider);
    int cancels=0;
    slider->begin=[] {return true;};
    slider->cancel=[&] {++cancels;delete slider.data();};
    press(slider);
    key(slider,Qt::Key_Escape);
    check(!slider&&cancels==1,"Cancel can synchronously delete its control");

    SemanticSlider stopped(-20,20);
    Recorder recorder(stopped);
    stopped.preview_delta=[&](double) {stopped.disarm();};
    key(&stopped,Qt::Key_Right);
    check(recorder.begins==1&&recorder.commits==0&&stopped.value()==0,
          "Explicit disarm inside preview prevents a later keyboard commit");
    stopped.begin=[&] {stopped.disarm();return true;};
    press(&stopped);
    check(!stopped.isSliderDown()&&recorder.commits==0,
          "Explicit disarm during begin cannot be overwritten by arming afterward");
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        construction_and_idle();
        mouse_lifecycle();
        cancellation_and_disarm();
        refused_and_other_input();
        keyboard_lifecycle_and_limits();
        callback_destruction();
        std::cout<<"PASS "<<checks<<" semantic slider Qt lifecycle checks (physical OS input NOT_RUN)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';
        return 1;
    }
}
