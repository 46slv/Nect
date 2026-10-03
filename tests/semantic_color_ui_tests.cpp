#include "semantic_color_control.hpp"

#include <QApplication>
#include <QColorDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTest>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace nect::desktop;

namespace {
using Value=SemanticColorInput::Value;
int checks=0;
void check(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
    ++checks;
}
void events() {
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
}
Value exact_color() {return {0.12345678912345678,0.45678912345678912,0.7891234567891234,0.3456789123456789};}
QColor displayed(const Value& value) {return QColor::fromRgbF(value[0],value[1],value[2],value[3]);}
Value components(const QColor& color) {return {color.redF(),color.greenF(),color.blueF(),color.alphaF()};}
void prepare(SemanticColorInput& input) {
    input.resize(320,80);input.show();input.window()->show();input.hex_input()->setFocus(Qt::OtherFocusReason);events();
}
void type(SemanticColorInput& input,const QString& text) {
    input.hex_input()->setFocus(Qt::OtherFocusReason);input.hex_input()->selectAll();
    QTest::keyClicks(input.hex_input(),text);
}
void finish(SemanticColorInput& input) {QTest::keyClick(input.hex_input(),Qt::Key_Return);events();}
void edit(SemanticColorInput& input,const QString& text) {type(input,text);finish(input);}
QColorDialog* picker(SemanticColorInput& input) {
    QTest::mouseClick(input.swatch_button(),Qt::LeftButton);events();
    auto* dialog=input.findChild<QColorDialog*>("semantic-color-picker");
    check(dialog!=nullptr,"The native swatch activation opens a QColorDialog");
    return dialog;
}

void exact_baseline_and_noops() {
    QWidget parent;
    const auto exact=exact_color();SemanticColorInput input(exact,&parent);
    std::vector<Value> edits;input.commit=[&](Value value) {edits.push_back(value);};prepare(input);
    check(input.parentWidget()==&parent&&input.swatch_button()->parentWidget()==&input&&input.hex_input()->parentWidget()==&input,
          "Color input and children follow Qt parent ownership");
    check(input.value()==exact&&edits.empty(),"Construction and showing preserve the exact baseline without editing");
    check(!input.swatch_button()->icon().isNull()&&input.swatch_button()->iconSize()==QSize(28,20),
          "The swatch shows checkerboard-backed color pixels");
    check(input.hex_input()->text().size()==9&&input.hex_input()->text().startsWith('#')&&!input.hex_input()->isModified(),
          "Initial HEX includes RGBA while remaining an unmodified display");
    check(!input.hex_input()->accessibleName().isEmpty()&&!input.swatch_button()->accessibleName().isEmpty()&&
          input.swatch_button()->toolTip().contains(QString::number(exact[3],'g',17)),
          "The exact alpha is exposed in accessible swatch help");
    check(components(displayed(exact))!=exact,"The precision fixture would lose information through QColor conversion");
    check(!input.accept_picker_color(displayed(exact))&&input.value()==exact&&edits.empty(),
          "Accepting an unchanged high-precision displayed QColor requests no quantizing edit");
    const auto original_hex=input.hex_input()->text();
    finish(input);
    check(edits.empty()&&input.value()==exact,"Untouched Return requests no edit");
    edit(input,original_hex);
    check(edits.empty()&&input.value()==exact&&!input.hex_input()->isModified(),
          "Retyping identical displayed HEX preserves all original doubles");
    edit(input,original_hex.toLower());
    check(edits.empty()&&input.value()==exact&&input.hex_input()->text()==original_hex,
          "Case-only HEX edits remain no-ops and return to normalized display");
    for(const double alpha:{0.0,1.0}) {
        auto boundary=exact;boundary[3]=alpha;input.set_value(boundary);
        check(!input.accept_picker_color(displayed(boundary)),"Unchanged boundary-alpha picker result is a no-op");
        edit(input,input.hex_input()->text());
        check(input.value()==boundary&&edits.empty(),"Transparent and opaque alpha stay exact through repeated no-op editing");
    }
}

void hex_edits_alpha_and_errors() {
    SemanticColorInput input(Value{0,0,0,0});prepare(input);
    std::vector<Value> edits;input.commit=[&](Value value) {edits.push_back(value);};
    edit(input,"#11223344");
    check(edits==std::vector<Value>{Value{17/255.0,34/255.0,51/255.0,68/255.0}}&&input.value()==edits.back(),
          "Eight-digit HEX emits one direct canonical double RGBA tuple");
    finish(input);edit(input,"11223344");
    check(edits.size()==1,"Return plus repeated identical bare HEX cannot duplicate a commit");
    edit(input,"#AABBCCDD");
    check(edits.size()==2&&edits.back()==Value{170/255.0,187/255.0,204/255.0,221/255.0},
          "A second distinct edit uses the latest display baseline exactly once");
    edit(input,"#010203");
    check(edits.size()==3&&edits.back()==Value{1/255.0,2/255.0,3/255.0,1},
          "Six-digit HEX explicitly selects opaque alpha");
    edit(input,"  00aa11ff  ");
    check(edits.size()==4&&edits.back()==Value{0,170/255.0,17/255.0,1},
          "Whitespace, lowercase and optional hash match the existing HEX parser");
    const auto baseline=input.value();const auto baseline_hex=input.hex_input()->text();
    for(const auto* invalid:{"#123","#GG223344","#0011223344","#1234567","#-12233"}) {
        edit(input,invalid);
        check(edits.size()==4&&input.value()==baseline&&!input.error_text().isEmpty()&&
              input.hex_input()->property("nect-color-input-error").toBool(),
              "Malformed HEX shows a local error without replacing the baseline or emitting a commit");
        finish(input);
        check(edits.size()==4,"Repeated invalid Return still cannot commit");
    }
    QTest::keyClick(input.hex_input(),Qt::Key_Escape);events();
    check(input.value()==baseline&&input.hex_input()->text()==baseline_hex&&!input.hex_input()->isModified()&&
          input.error_text().isEmpty()&&!input.hex_input()->property("nect-color-input-error").toBool(),
          "Escape restores exact baseline, clears draft/error and emits no commit");
    edit(input,"#8899AABB");
    check(edits.size()==5&&input.error_text().isEmpty(),"Valid editing resumes after malformed drafts and Escape");

    const auto exact=exact_color();input.set_value(exact);edits.clear();
    const auto original_hex=input.hex_input()->text();
    edit(input,"#FF"+original_hex.mid(3));
    auto expected=exact;expected[0]=1;
    check(edits==std::vector<Value>{expected},"Changing one HEX byte retains exact untouched RGB and alpha components");
    input.set_value(Value{exact[0],exact[1],exact[2],0.99999});edits.clear();
    edit(input,input.hex_input()->text().left(7));
    check(edits.size()==1&&edits.back()[3]==1&&edits.back()[0]==exact[0],
          "Removing the alpha byte explicitly makes a nearly opaque baseline fully opaque");
}

void picker_accept_cancel_and_component_precision() {
    const auto exact=exact_color();SemanticColorInput input(exact);prepare(input);
    std::vector<Value> edits;input.commit=[&](Value value) {edits.push_back(value);};
    check(!input.accept_picker_color(QColor())&&edits.empty()&&input.value()==exact,
          "An invalid/canceled picker result emits no edit");
    check(!input.accept_picker_color(QColor::fromRgbF(1.2,0.2,0.3,1))&&edits.empty()&&input.value()==exact&&!input.error_text().isEmpty(),
          "Extended RGB is refused instead of silently clamping into the canonical sRGB range");
    auto* dialog=picker(input);
    check(dialog->testOption(QColorDialog::ShowAlphaChannel),"The picker exposes alpha rather than silently discarding it");
    dialog->setCurrentColor(QColor(255,0,0,128));dialog->reject();events();
    check(edits.empty()&&input.value()==exact,"Cancel discards picker changes without touching the exact baseline");
    dialog=picker(input);dialog->accept();events();
    check(edits.empty()&&input.value()==exact,"Untouched native picker acceptance preserves all original doubles");
    auto selected=displayed(exact);selected.setRedF(0.9);
    auto expected=exact;expected[0]=selected.redF();
    check(input.accept_picker_color(selected)&&edits==std::vector<Value>{expected}&&input.value()==expected,
          "A picker RGB change emits once and retains the exact untouched channels and alpha");
    check(!input.accept_picker_color(displayed(expected))&&edits.size()==1,
          "Repeated identical picker acceptance is a no-op against its latest baseline");
    input.set_value(exact);edits.clear();selected=displayed(exact);selected.setAlphaF(0.8);
    expected=exact;expected[3]=selected.alphaF();
    check(input.accept_picker_color(selected)&&edits==std::vector<Value>{expected},
          "An alpha-only picker edit preserves the exact original RGB doubles");
    input.set_value(exact);edits.clear();dialog=picker(input);
    selected=dialog->currentColor();selected.setBlueF(0.1);dialog->setCurrentColor(selected);
    const auto actual=dialog->currentColor();expected=exact;expected[2]=actual.blueF();
    dialog->accept();events();
    check(edits==std::vector<Value>{expected}&&input.value()==expected,
          "Native picker acceptance delivers one canonical change after its dialog handler finishes");
}

void draft_to_picker_transition() {
    const auto exact=exact_color();SemanticColorInput input(exact);prepare(input);
    std::vector<Value> edits;input.commit=[&](Value value) {edits.push_back(value);};
    type(input,"#11223344");auto* dialog=picker(input);
    const Value expected{17/255.0,34/255.0,51/255.0,68/255.0};
    check(edits==std::vector<Value>{expected}&&input.value()==expected&&dialog->isVisible(),
          "Switching a valid HEX draft to the swatch settles one edit before keeping the picker open");
    check(dialog->currentColor().rgba64()==displayed(expected).rgba64(),
          "The picker initializes from the newly accepted HEX baseline");
    dialog->reject();events();
    check(edits.size()==1&&input.value()==expected,"Cancel after a draft-to-picker transition retains only the preceding HEX edit");
    type(input,input.hex_input()->text());dialog=picker(input);
    check(edits.size()==1&&dialog->isVisible(),"A no-op HEX draft still permits its queued swatch activation");
    dialog->reject();events();
    type(input,"#invalid");dialog=picker(input);
    check(edits.size()==1&&!input.error_text().isEmpty()&&dialog->isVisible(),
          "A malformed HEX draft cannot commit or prevent opening the baseline picker");
    dialog->accept();events();
    check(edits.size()==1&&input.value()==expected&&input.error_text().isEmpty()&&!input.hex_input()->isModified(),
          "An unchanged picker acceptance restores the baseline over an invalid draft without editing");
    auto deleting=std::make_unique<SemanticColorInput>(exact);prepare(*deleting);int commits=0;
    deleting->commit=[&](Value) {++commits;deleting.reset();};
    type(*deleting,"#11223344");QTest::mouseClick(deleting->swatch_button(),Qt::LeftButton);events();
    check(!deleting&&commits==1,"Draft-to-swatch activation safely cancels the picker request when its HEX callback destroys the row");
}

void external_refresh_disable_and_queued_cancellation() {
    const auto exact=exact_color();const Value refreshed{0.2,0.3,0.4,0.5};
    QWidget parent;SemanticColorInput input(exact,&parent);prepare(input);
    std::vector<Value> edits;input.commit=[&](Value value) {edits.push_back(value);};
    input.hex_input()->setText("#11223344");finish(input);
    check(edits.empty()&&input.value()==exact,"Programmatic HEX setText is display-only without a modified user draft");
    input.set_value(exact);
    type(input,"#11223344");QTest::keyClick(input.hex_input(),Qt::Key_Return);
    input.set_value(refreshed);events();
    check(edits.empty()&&input.value()==refreshed&&!input.hex_input()->isModified(),
          "External exact refresh cancels an older queued HEX submission");
    type(input,"#11223344");QTest::keyClick(input.hex_input(),Qt::Key_Return);
    QTest::keyClick(input.hex_input(),Qt::Key_Escape);events();
    check(edits.empty()&&input.value()==refreshed,"Escape invalidates a pending HEX commit before delivery");
    type(input,"#11223344");QTest::keyClick(input.hex_input(),Qt::Key_Return);input.setEnabled(false);events();
    check(edits.empty()&&input.value()==refreshed&&!input.hex_input()->isModified(),
          "Disabling the input restores its baseline and invalidates queued edits");
    check(!input.accept_picker_color(QColor(255,0,0,128)),"Disabled direct picker acceptance cannot request a commit");
    QTest::mouseClick(input.swatch_button(),Qt::LeftButton);input.swatch_button()->click();events();
    check(!input.findChild<QColorDialog*>("semantic-color-picker"),"Disabled swatch rejects mouse and programmatic activation");
    input.hex_input()->setText("#55667788");input.hex_input()->setModified(true);
    QMetaObject::invokeMethod(input.hex_input(),"editingFinished",Qt::DirectConnection);events();
    check(edits.empty()&&input.value()==refreshed,"Disabled HEX ignores even an explicitly signaled modified draft");
    input.set_value(exact);input.setEnabled(true);events();
    input.swatch_button()->click();input.setEnabled(false);input.setEnabled(true);events();
    check(!input.findChild<QColorDialog*>("semantic-color-picker")&&edits.empty(),
          "Disable/re-enable cancels a previously queued swatch request");
    input.swatch_button()->setFocus(Qt::OtherFocusReason);input.swatch_button()->click();
    QTest::keyClick(input.swatch_button(),Qt::Key_Escape);events();
    check(!input.findChild<QColorDialog*>("semantic-color-picker")&&edits.empty(),
          "Escape cancels a previously queued swatch request");
    input.swatch_button()->click();input.set_value(refreshed);events();
    check(!input.findChild<QColorDialog*>("semantic-color-picker")&&edits.empty()&&input.value()==refreshed,
          "External refresh cancels a previously queued swatch request");
    input.set_value(exact);edit(input,"#55667788");check(edits.size()==1,"Reenabled color input can commit a new valid edit");
    input.set_value(exact);edits.clear();auto* dialog=picker(input);
    const QPointer<QColorDialog> alive_dialog(dialog);input.set_value(refreshed);events();
    check(!alive_dialog&&edits.empty()&&input.value()==refreshed,
          "External refresh cancels an open picker and protects the latest exact baseline");
    dialog=picker(input);dialog->setCurrentColor(QColor(255,0,0,128));dialog->accept();
    input.set_value(exact);events();
    check(edits.empty()&&input.value()==exact,"External refresh invalidates a queued picker acceptance");
    dialog=picker(input);const QPointer<QColorDialog> disabled_dialog(dialog);parent.setEnabled(false);events();
    check(!disabled_dialog&&edits.empty()&&input.value()==exact,
          "Inherited disabling closes an open picker without editing");
}

void callback_lifetime_and_owner_refusal() {
    const auto exact=exact_color();int commits=0;
    auto input=std::make_unique<SemanticColorInput>(exact);prepare(*input);
    const QPointer<SemanticColorInput> alive(input.get());
    input->commit=[&](Value) {++commits;input.reset();};
    edit(*input,"#11223344");
    check(!alive&&commits==1,"HEX callback can synchronously destroy the Inspector row safely");
    input=std::make_unique<SemanticColorInput>(exact);prepare(*input);
    input->commit=[&](Value) {++commits;input.reset();};
    auto* dialog=picker(*input);dialog->setCurrentColor(QColor(255,0,0,128));dialog->accept();events();
    check(!input&&commits==2,"Queued native picker callback can synchronously destroy its owner safely");
    input=std::make_unique<SemanticColorInput>(exact);
    input->commit=[&](Value) {++commits;input.reset();};
    const auto direct_changed=input->accept_picker_color(QColor(255,0,0,128));
    check(direct_changed&&!input&&commits==3,"Synchronous picker acceptance can safely return after its callback destroys the row");
    input=std::make_unique<SemanticColorInput>(exact);prepare(*input);
    input->commit=[&](Value) {++commits;};
    type(*input,"#11223344");QTest::keyClick(input->hex_input(),Qt::Key_Return);input.reset();events();
    check(commits==3,"Destroying the input cancels its undelivered HEX callback");
    input=std::make_unique<SemanticColorInput>(exact);prepare(*input);
    input->commit=[&](Value) {++commits;};dialog=picker(*input);
    dialog->setCurrentColor(QColor(255,0,0,128));dialog->accept();input.reset();events();
    check(commits==3,"Destroying the input cancels its undelivered picker callback");
    input=std::make_unique<SemanticColorInput>(exact);prepare(*input);
    const QPointer<SemanticColorInput> safe(input.get());
    input->commit=[&,safe](Value) {++commits;if(safe)safe->set_value(exact);};
    edit(*input,"#11223344");
    check(commits==4&&input->value()==exact&&!input->hex_input()->isModified(),
          "The Session owner can refuse an edit and restore exact display without recursive commits");
    for(const auto invalid:{Value{-0.1,0,0,1},Value{0,1.1,0,1},
                           Value{0,0,std::numeric_limits<double>::infinity(),1},
                           Value{0,0,0,std::numeric_limits<double>::quiet_NaN()}}) {
        bool refused=false;try {input->set_value(invalid);}catch(const std::invalid_argument&) {refused=true;}
        check(refused&&input->value()==exact&&commits==4,"Invalid external baseline is refused without changing the display");
        QWidget parent;const auto before=parent.children().size();refused=false;
        try {SemanticColorInput invalid_input(invalid,&parent);}catch(const std::invalid_argument&) {refused=true;}
        check(refused&&parent.children().size()==before,"Invalid construction leaves no child widget behind");
    }
}
}

int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    try {
        exact_baseline_and_noops();
        hex_edits_alpha_and_errors();
        picker_accept_cancel_and_component_precision();
        draft_to_picker_transition();
        external_refresh_disable_and_queued_cancellation();
        callback_lifetime_and_owner_refusal();
        std::cout<<"PASS "<<checks<<" semantic color Qt checks (physical OS input NOT_RUN)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;
    }
}
