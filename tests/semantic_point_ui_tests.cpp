#include "semantic_point_control.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QTest>
#include <QVariant>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using nect::desktop::SemanticPointInput;
using Value=SemanticPointInput::Value;

namespace {
int checks=0;
void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
void events() {QApplication::processEvents();}
bool exact(Value actual,Value expected) {
    for(std::size_t axis=0;axis<2;++axis)
        if(std::bit_cast<std::uint64_t>(actual[axis])!=std::bit_cast<std::uint64_t>(expected[axis]))return false;
    return true;
}
double literal(const QString& text) {
    const auto bytes=text.toLatin1();
    double result=0;
    const auto parsed=std::from_chars(bytes.constData(),bytes.constData()+bytes.size(),result);
    check(parsed.ec==std::errc{}&&parsed.ptr==bytes.constData()+bytes.size(),
          "Displayed coordinate is a complete decimal double literal");
    return result;
}
void prepare(SemanticPointInput& input) {
    input.resize(620,90);
    input.show();
    input.x_input()->setFocus(Qt::OtherFocusReason);
    events();
}
void edit(QLineEdit* input,const QString& text) {
    input->setFocus(Qt::OtherFocusReason);
    input->selectAll();
    if(text.isEmpty())QTest::keyClick(input,Qt::Key_Backspace);
    else QTest::keyClicks(input,text);
}
void draft(SemanticPointInput& input,const QString& x,const QString& y) {
    edit(input.x_input(),x);
    edit(input.y_input(),y);
}
void click(QPushButton* button) {
    QTest::mouseClick(button,Qt::LeftButton);
    events();
}
void key(QWidget* input,Qt::Key code) {
    QTest::keyClick(input,code);
    events();
}
// The queued cases intentionally do not spin Qt's event loop between actions.
void escape_before_delivery(QWidget* input) {
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QApplication::sendEvent(input,&escape);
}

void construction_and_paired_apply() {
    QWidget parent;
    SemanticPointInput input({1.25,-2.5},&parent);
    std::vector<Value> commits;
    input.commit=[&](Value value) {commits.push_back(value);};
    check(input.parentWidget()==&parent&&input.x_input()->parentWidget()==&input&&
          input.y_input()->parentWidget()==&input&&input.apply_button()->parentWidget()==&input&&
          input.cancel_button()->parentWidget()==&input,
          "Point editor and native fields/buttons use normal Qt parent ownership");
    check(exact(input.value(),{1.25,-2.5})&&input.x_input()->text()=="1.25"&&
          input.y_input()->text()=="-2.5"&&input.error_text().isEmpty(),
          "Construction presents both exact coordinates with no error");
    check(input.apply_button()->text()=="Apply"&&input.cancel_button()->text()=="Cancel"&&
          !input.x_input()->accessibleName().isEmpty()&&!input.y_input()->accessibleName().isEmpty()&&
          !input.accessibleDescription().isEmpty()&&input.property("nect-exact-value").toBool(),
          "Paired control has explicit actions and accessible exact-value guidance");
    parent.resize(660,140);
    parent.show();
    prepare(input);
    edit(input.x_input(),"11.75");
    check(commits.empty()&&exact(input.value(),{1.25,-2.5}),
          "Typing the first axis never commits or changes the Cancel baseline");
    edit(input.y_input(),"-9.125");
    input.apply_button()->setFocus(Qt::OtherFocusReason);
    events();
    check(commits.empty()&&exact(input.value(),{1.25,-2.5}),
          "Typing the second axis and leaving a field still leave one paired draft");
    click(input.apply_button());
    check(commits.size()==1&&exact(commits.front(),{11.75,-9.125})&&
          exact(input.value(),{11.75,-9.125}),
          "A real Apply click requests exactly one complete coordinate pair");
    check(!input.x_input()->isModified()&&!input.y_input()->isModified()&&input.error_text().isEmpty(),
          "Successful delivery canonicalizes both transient fields and clears their modified flags");
    click(input.apply_button());
    key(input.x_input(),Qt::Key_Return);
    check(commits.size()==1,"Repeated Apply and Enter on the accepted pair are no-ops");
    draft(input,"+11.7500"," -9.1250 ");
    key(input.y_input(),Qt::Key_Enter);
    check(commits.size()==1&&input.x_input()->text()=="11.75"&&input.y_input()->text()=="-9.125",
          "Equivalent decimal spellings are normalized without a callback");
}

void keyboard_and_cancellation() {
    SemanticPointInput input({2,3});
    std::vector<Value> commits;
    input.commit=[&](Value value) {commits.push_back(value);};
    prepare(input);
    draft(input,"4.5","6.75");
    key(input.x_input(),Qt::Key_Return);
    check(commits.size()==1&&exact(commits.back(),{4.5,6.75}),
          "Return in X applies both axes once");
    draft(input,"8.125","-10.25");
    key(input.y_input(),Qt::Key_Enter);
    check(commits.size()==2&&exact(commits.back(),{8.125,-10.25}),
          "Keypad Enter in Y also applies one exact pair");
    draft(input,"30","40");
    key(input.x_input(),Qt::Key_Escape);
    check(commits.size()==2&&exact(input.value(),{8.125,-10.25})&&
          input.x_input()->text()=="8.125"&&input.y_input()->text()=="-10.25",
          "Escape in X restores both fields to the last accepted baseline");
    draft(input,"50","60");
    key(input.y_input(),Qt::Key_Escape);
    check(commits.size()==2&&input.x_input()->text()=="8.125"&&input.y_input()->text()=="-10.25",
          "Escape in Y cancels the whole pair without requesting an edit");
    draft(input,"70","80");
    click(input.cancel_button());
    check(commits.size()==2&&exact(input.value(),{8.125,-10.25})&&
          !input.x_input()->isModified()&&!input.y_input()->isModified(),
          "Cancel discards both drafts and modified flags with no callback");
    draft(input,"90","100");
    key(input.cancel_button(),Qt::Key_Return);
    check(commits.size()==2&&input.x_input()->text()=="8.125",
          "Return on Cancel cancels rather than accidentally applying");
    draft(input,"101","102");
    input.apply_button()->setFocus();
    key(input.apply_button(),Qt::Key_Space);
    check(commits.size()==3&&exact(commits.back(),{101,102}),
          "Native Space activation of Apply sends one pair");
    draft(input,"103","104");
    key(input.apply_button(),Qt::Key_Return);
    check(commits.size()==4&&exact(commits.back(),{103,104}),
          "Return on Apply follows the same paired path");
    draft(input,"105","106");
    key(&input,Qt::Key_Escape);
    check(commits.size()==4&&input.y_input()->text()=="104",
          "Escape on the container restores its paired draft");
}

void invalid_literals_and_atomicity() {
    SemanticPointInput input({7,8});
    int commits=0;
    input.commit=[&](Value) {++commits;};
    prepare(input);
    const std::vector<QString> invalid={""," ",".","-","1e","1junk","1,5","0x1p2",
        "+-1","++1","nan","NaN","inf","+inf","-inf","Infinity","1e9999","-1e9999","1e-9999"};
    for(const auto& text:invalid) {
        draft(input,text,"19.25");
        click(input.apply_button());
        check(commits==0&&exact(input.value(),{7,8})&&input.error_text().contains("INVALID_VALUE")&&
              input.error_text().contains("X"),
              "An invalid X refuses the whole pair and leaves the exact baseline untouched");
        draft(input,"23.75",text);
        key(input.y_input(),Qt::Key_Return);
        check(commits==0&&exact(input.value(),{7,8})&&input.error_text().contains("Y"),
              "An invalid Y cannot partially commit a valid X");
    }
    input.x_input()->setText("9\n10");
    input.y_input()->setText("11");
    click(input.apply_button());
    check(commits==0&&!input.error_text().isEmpty()&&exact(input.value(),{7,8}),
          "A multiline literal is refused without authored-value mutation");
    key(input.y_input(),Qt::Key_Escape);
    check(input.error_text().isEmpty()&&input.x_input()->text()=="7"&&input.y_input()->text()=="8",
          "Escape clears validation feedback and restores both finite baseline fields");
    draft(input," +9.25 "," -11.5 ");
    click(input.apply_button());
    check(commits==1&&exact(input.value(),{9.25,-11.5})&&input.error_text().isEmpty(),
          "Whitespace and a single leading plus accept complete finite decimal literals");
}

void precision_and_finite_refresh() {
    const auto old_locale=QLocale();
    QLocale::setDefault(QLocale(QLocale::German,QLocale::Germany));
    const std::vector<Value> values={
        {1.2345678901234567,-8.7654321098765432},
        {std::nextafter(1.0,2.0),std::nextafter(-1.0,-2.0)},
        {std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min()},
        {std::numeric_limits<double>::min(),-std::numeric_limits<double>::min()},
        {std::numeric_limits<double>::max(),-std::numeric_limits<double>::max()},
        {-0.0,123456789012345.67}};
    for(const auto value:values) {
        SemanticPointInput supplied(value);
        check(exact({literal(supplied.x_input()->text()),literal(supplied.y_input()->text())},value),
              "17-digit coordinate presentation preserves exact double bits across the full finite range");
        SemanticPointInput input({0,0});
        prepare(input);
        std::vector<Value> commits;
        input.commit=[&](Value requested) {commits.push_back(requested);};
        draft(input,supplied.x_input()->text(),supplied.y_input()->text());
        key(input.y_input(),Qt::Key_Return);
        check(commits.size()==1&&exact(commits.front(),value)&&exact(input.value(),value),
              "Keyboard Apply round-trips both exact doubles independently of the default locale");
        click(input.apply_button());
        check(commits.size()==1,"Round-tripped extremes stay no-ops on a repeated Apply");
        draft(input,"31","32");
        click(input.cancel_button());
        check(exact({literal(input.x_input()->text()),literal(input.y_input()->text())},value),
              "Cancel retains precision, subnormal values and signed zero");
    }
    QLocale::setDefault(old_locale);
    SemanticPointInput input({5,6});
    int commits=0;
    input.commit=[&](Value) {++commits;};
    draft(input,"77","88");
    for(const auto invalid:{std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}) {
        for(std::size_t axis=0;axis<2;++axis) {
            Value value={1,2};value[axis]=invalid;
            bool refused=false;
            try {input.set_value(value);}catch(const std::invalid_argument&) {refused=true;}
            check(refused&&exact(input.value(),{5,6})&&input.x_input()->text()=="77"&&
                  input.y_input()->text()=="88"&&commits==0,
                  "A nonfinite external refresh refuses before changing baseline, draft or callbacks");
            refused=false;
            try {SemanticPointInput bad(value);}catch(const std::invalid_argument&) {refused=true;}
            check(refused,"A nonfinite initial pair is rejected");
        }
    }
}

void queued_refresh_cancel_and_supersession() {
    SemanticPointInput input({1,2});
    std::vector<Value> commits;
    input.commit=[&](Value value) {commits.push_back(value);};
    prepare(input);
    draft(input,"3","4");
    input.apply_button()->click();
    check(commits.empty()&&exact(input.value(),{1,2}),
          "Apply delivery waits until the native activation stack returns");
    input.set_value({5.125,6.25});
    events();
    check(commits.empty()&&exact(input.value(),{5.125,6.25})&&input.x_input()->text()=="5.125"&&
          input.y_input()->text()=="6.25",
          "External refresh invalidates a queued draft and never requests a callback");
    draft(input,"7","8");
    input.apply_button()->click();
    input.set_value(input.value());
    events();
    check(commits.empty()&&input.x_input()->text()=="5.125",
          "Even an identical external refresh invalidates stale queued drafts");
    draft(input,"9","10");
    input.apply_button()->click();
    input.cancel_button()->click();
    events();
    check(commits.empty()&&exact(input.value(),{5.125,6.25}),
          "Cancel invalidates a queued Apply without committing its pair");
    draft(input,"11","12");
    input.apply_button()->click();
    escape_before_delivery(input.x_input());
    events();
    check(commits.empty()&&input.y_input()->text()=="6.25",
          "Escape drops a not-yet-delivered pair");
    draft(input,"13","14");
    input.apply_button()->click();
    input.x_input()->setText("15");
    events();
    check(commits.empty()&&exact(input.value(),{5.125,6.25}),
          "Changing a draft after activation prevents delivery of the superseded pair");
    input.apply_button()->click();
    input.apply_button()->click();
    events();
    check(commits.size()==1&&exact(commits.back(),{15,14}),
          "Repeated queued Apply coalesces to one latest complete pair");
    draft(input,"17","18");
    input.apply_button()->click();
    input.y_input()->setText("bad");
    input.apply_button()->click();
    events();
    check(commits.size()==1&&exact(input.value(),{15,14})&&input.error_text().contains("Y"),
          "A later invalid activation cannot release an older valid queued draft");
    input.set_value({19,20});
    check(commits.size()==1&&input.error_text().isEmpty()&&!input.x_input()->isModified()&&
          !input.y_input()->isModified(),
          "Programmatic refresh clears validation and draft state without requesting an edit");
}

void disabled_controls() {
    SemanticPointInput input({1,2});
    int commits=0;
    input.commit=[&](Value) {++commits;};
    prepare(input);
    draft(input,"3","4");
    input.apply_button()->click();
    input.setEnabled(false);
    input.setEnabled(true);
    events();
    check(commits==0&&exact(input.value(),{1,2}),
          "Disabling and reenabling before delivery permanently invalidates that pending draft");
    input.setEnabled(false);
    input.apply_button()->click();
    key(input.x_input(),Qt::Key_Return);
    click(input.apply_button());
    check(commits==0&&exact(input.value(),{1,2}),
          "Disabled paired editor prevents native mouse, Enter and programmatic button commits");
    input.set_value({5,6});
    events();
    check(commits==0&&input.x_input()->text()=="5"&&input.y_input()->text()=="6",
          "Disabled presentation can still refresh without an authored edit");
    input.setEnabled(true);
    for(auto* control:{static_cast<QWidget*>(input.x_input()),static_cast<QWidget*>(input.y_input()),
                       static_cast<QWidget*>(input.apply_button())}) {
        draft(input,"7","8");
        input.apply_button()->click();
        control->setEnabled(false);
        control->setEnabled(true);
        events();
        check(commits==0&&exact(input.value(),{5,6}),
              "Disabling either axis or Apply also invalidates queued delivery");
        control->setEnabled(false);
        input.apply_button()->click();
        key(input.x_input(),Qt::Key_Return);
        check(commits==0&&exact(input.value(),{5,6}),
              "An individually disabled input or Apply prevents the whole paired commit");
        control->setEnabled(true);
    }
    key(input.y_input(),Qt::Key_Return);
    check(commits==1&&exact(input.value(),{7,8}),
          "Reenabled editor can explicitly apply its latest draft once");
}

void callback_refresh_and_destruction() {
    SemanticPointInput input({1,2});
    prepare(input);
    int commits=0;
    input.commit=[&](Value requested) {
        ++commits;
        check(exact(requested,{3,4})&&exact(input.value(),requested),
              "Callback sees its full requested pair and completed transient baseline");
        input.set_value({9,10});
        input.commit={};
    };
    draft(input,"3","4");
    click(input.apply_button());
    check(commits==1&&exact(input.value(),{9,10})&&input.x_input()->text()=="9"&&input.y_input()->text()=="10",
          "Synchronous caller refresh and callback replacement prevail after delivery");
    int abandoned=0;
    {
        auto parent=std::make_unique<QWidget>();
        auto* pending=new SemanticPointInput({1,2},parent.get());
        pending->commit=[&](Value) {++abandoned;};
        pending->x_input()->setText("3");pending->y_input()->setText("4");
        pending->apply_button()->click();
        parent.reset();
    }
    events();
    check(abandoned==0,"Destroying a parent before queued delivery cancels its draft callback");
    for(bool enter:{false,true}) {
        auto parent=std::make_unique<QWidget>();
        auto* child=new SemanticPointInput({1,2},parent.get());
        QPointer<SemanticPointInput> alive(child);
        int delivered=0;
        bool finished=false;
        child->commit=[&](Value requested) {
            ++delivered;
            check(exact(requested,{7,8}),"Destructive callback receives the exact complete pair");
            parent.reset();
            finished=true;
        };
        parent->resize(660,140);parent->show();
        prepare(*child);
        draft(*child,"7","8");
        if(enter)key(child->y_input(),Qt::Key_Return);
        else click(child->apply_button());
        check(!alive&&delivered==1&&finished,
              "Real Enter or Apply safely permits synchronous parent rebuild and callback-storage destruction");
        events();
        check(delivered==1,"Destruction cannot leave another queued callback");
    }
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    try {
        construction_and_paired_apply();
        keyboard_and_cancellation();
        invalid_literals_and_atomicity();
        precision_and_finite_refresh();
        queued_refresh_cancel_and_supersession();
        disabled_controls();
        callback_refresh_and_destruction();
        std::cout<<"PASS "<<checks<<" semantic point Qt checks (physical OS input NOT_RUN)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';
        return 1;
    }
}
