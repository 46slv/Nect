#include "window.hpp"
#include "nect/io.hpp"
#include <QAccessible>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);++checks;}
void events() {QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QApplication::processEvents();}
template<class T>T* visible(QWidget& parent,const char* name,const QString& tag={}) {
    for(auto* widget:parent.findChildren<T*>(QString::fromLatin1(name)))
        if(widget->isVisible()&&(tag.isNull()||widget->property("font-tag").toString()==tag))return widget;
    throw std::runtime_error(std::string("Missing visible control: ")+name+" ["+tag.toStdString()+"]");
}
struct Snapshot {
    std::string native;std::uint64_t revision;HistoryInfo history;
    explicit Snapshot(const Window& window):native(encode(window.host.session.document())),revision(window.host.session.revision()),history(window.host.session.history()){}
    void unchanged(const Window& window) const {
        check(native==encode(window.host.session.document())&&revision==window.host.session.revision()&&history==window.host.session.history(),
            "Rejected/cancelled/no-op draft preserves authored bytes, revision, and History");
    }
};
Document fixture() {
    auto document=empty_document("typography-document","comp","art");
    Object text;text.id="text";text.name="Exact typography";text.kind=Kind::text;text.text=default_text("source","AV ffi");
    text.text->family="Arial";text.text->locale="en-US";
    text.text->font_features={{"KERN",1,"whole_text"},{"lig ",4,"whole_text"}};
    text.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object peer=text;peer.id="peer";peer.name="Text on Path";peer.text->id="peer-source";
    Object path;path.id="path";path.name="Path";path.kind=Kind::path;
    Contour contour;contour.id="contour";
    Point first;first.id="first";first.x.literal=10;first.y.literal=100;
    Point second;second.id="second";second.x.literal=400;second.y.literal=100;
    contour.points={first,second};path.contours={contour};
    peer.text->path_attachment=TextPathAttachment{"path","contour","distance",0,0,false};
    Object hidden=text;hidden.id="hidden";hidden.name="Hidden Text";hidden.text->id="hidden-source";hidden.visible=false;
    Object other=text;other.id="other";other.name="Other Composition Text";other.text->id="other-source";
    document.objects.emplace(text.id,text);document.objects.emplace(peer.id,peer);document.objects.emplace(path.id,path);
    document.objects.emplace(hidden.id,hidden);document.objects.emplace(other.id,other);
    document.compositions.front().roots={"text","peer","path","hidden"};
    auto other_plane=empty_document("unused","other-comp","other-art").compositions.front();other_plane.roots={"other"};
    document.compositions.push_back(other_plane);validate(document);return document;
}
void write_native(const QString& path,const Document& document) {
    QFile file(path);check(file.open(QIODevice::WriteOnly),"Native fixture file opens");const auto bytes=encode(document);
    check(file.write(bytes.data(),static_cast<qint64>(bytes.size()))==static_cast<qint64>(bytes.size()),"Native fixture bytes written");
}
void load(Window& window,const QString& path,const char* object="text") {
    window.host.open(path);window.canvas->set_selection(object);events();
}
QDialog* open(Window& window,const char* name,const QString& tag={}) {
    auto* button=visible<QPushButton>(window,name,tag);
    check(button->isEnabled(),"Typography row action enabled for one whole Text");
    button->click();events();auto* dialog=visible<QDialog>(window,"text-typography-dialog");
    check(dialog->parentWidget()==&window,"Draft is Window-owned, surviving Inspector replacement");return dialog;
}
void set(QDialog& dialog,const QString& tag,const QString& value) {
    dialog.findChild<QLineEdit*>("text-typography-tag")->setText(tag);
    dialog.findChild<QLineEdit*>("text-typography-value")->setText(value);
}
void apply(QDialog& dialog) {dialog.findChild<QPushButton*>("text-typography-apply")->click();events();}
void cancel(QDialog& dialog) {dialog.findChild<QPushButton*>("text-typography-cancel")->click();events();}
void rejection(Window& window,QDialog& dialog,const char* code,const Snapshot& before) {
    apply(dialog);check(dialog.isVisible(),"Rejected Apply leaves draft open");
    check(dialog.findChild<QLabel*>("text-typography-status")->text().contains(QString::fromLatin1(code)),"Dialog exposes precise visible failure code");
    before.unchanged(window);
}
void one_edit(Window& window,const Snapshot& before,const Command& expected,const char* history_label) {
    Session oracle(decode(before.native));oracle.apply({expected},0);
    check(window.host.session.document()==oracle.document(),"UI result equals the exact single typed Session command");
    const auto cursor=std::find_if(before.history.states.begin(),before.history.states.end(),[&](const HistoryState& state){return state.id==before.history.current_id;});
    const auto next_size=static_cast<std::size_t>(std::distance(before.history.states.begin(),cursor))+2;
    check(window.host.session.revision()==before.revision+1&&window.host.session.history().states.size()==next_size,
        "Changed Apply produces exactly one revision and one Undo state");
    check(window.host.session.history().states.back().label==oracle.history().states.back().label&&
          window.host.session.history().states.back().label.starts_with(history_label),"History names the typed typography command");
    const auto changed=encode(window.host.session.document());
    window.host.session.undo(window.host.session.revision());window.host.edited();events();
    check(encode(window.host.session.document())==before.native,"Undo restores exact pre-edit native state");
    window.host.session.redo(window.host.session.revision());window.host.edited();events();
    check(encode(window.host.session.document())==changed,"Redo restores exact typography authored state");
}
void accessible(QWidget* widget) {
    auto* iface=QAccessible::queryAccessibleInterface(widget);
    check(iface&&!iface->text(QAccessible::Name).isEmpty(),"Native control has an accessible name");
    check(widget->focusPolicy()!=Qt::NoFocus,"Native action/editor is keyboard-focusable");
}
void receipt_string_presentation() {
    // This is a pure presentation fixture, not evidence of actual font shaping.
    TextLayout receipt;receipt.font_request.family="Requested %2 <family>";
    receipt.font_request.locale="Locale %4 <tag>";receipt.font_request.weight=650;
    receipt.font_request.font_features={{"%3  ",17,"whole_text"},{"<br>",29,"whole_text"}};
    TextFontRun run;run.utf16_start=0;run.utf16_length=8;
    run.family="Actual %5 <family>";run.face="Face %6 <br>";run.locale="Run %1 <locale>";
    TextFontScriptFeatures script;script.utf16_start=0;script.utf16_length=8;script.script=2;
    script.features={{"%3  ",17,"unknown"},{"<br>",29,"available"}};
    run.script_features.push_back(script);receipt.font_runs.push_back(run);
    const auto text=format_text_font_receipt(receipt);
    check(text.contains("Requested (evaluated): Requested %2 <family> · weight 650"),"Requested family placeholders remain literal receipt data");
    check(text.contains("locale Locale %4 <tag>"),"Requested locale markup and placeholders remain literal data");
    check(text.contains("family Actual %5 <family> · face Face %6 <br> · locale Run %1 <locale>"),"Actual family/face/locale strings are never recursively substituted");
    check(text.contains("Feature [%3  ] = 17 · script 2"),"Run feature receipt preserves valid percent-digit padded tags");
    check(text.contains("Feature [<br>] = 29 · script 2"),"Run feature receipt retains valid markup-like four-byte tags");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        receipt_string_presentation();
        QTemporaryDir directory;check(directory.isValid(),"Temporary native fixture directory exists");
        QSettings settings(directory.filePath("settings.ini"),QSettings::IniFormat);
        const auto native_path=directory.filePath("fixture.nect");write_native(native_path,fixture());
        Window window(directory.filePath("recovery"),std::make_unique<FolderLibrary>(settings));window.show();events();load(window,native_path);
        if(qEnvironmentVariable("QT_SCALE_FACTOR")=="2")check(window.devicePixelRatioF()>=1.9,"High-DPI test uses 2x widget pixel density");
        auto* section=visible<QGroupBox>(window,"text-advanced-typography");
        check(section->title()=="Advanced Typography","Bounded Inspector section exists");
        auto rows=section->findChildren<QLabel*>("text-font-feature-row");
        check(rows.size()==2&&rows[0]->text().contains("[KERN]")&&rows[1]->text().contains("[lig ]"),"Feature rows preserve order, case, and padded tag boundaries");
        check(visible<QLabel>(window,"text-font-axis-row","wdth")->text().contains("87.123456789012295"),"Axis row displays round-trippable authored double");
#ifndef _WIN32
        check(visible<QLabel>(window,"text-font-receipt")->text().contains("TEXT_PLATFORM_UNSUPPORTED")&&
              visible<QLabel>(window,"text-font-receipt")->text().contains("No backend submission"),"Linux reports absent projection without inventing applied font evidence");
        check(visible<QLabel>(window,"text-font-discovery-status")->text().contains("TEXT_PLATFORM_UNSUPPORTED"),"Installed-font discovery failure is explicit and keeps authoring available");
        check(!window.canvas->projection_succeeded(),"Authored Text selection never clears the actual projection failure");
        for(const auto* invalid:{"missing","path"}) {
            window.canvas->set_selection(invalid);events();check(window.canvas->selections().empty(),"Projection-failure fallback refuses missing and non-Text objects");
        }
        window.canvas->set_selections({{"text","invented-point"}});events();
        check(window.canvas->selections().empty(),"Projection-failure fallback never manufactures Text point selection");
        window.canvas->set_selections({{"other",{}},{"text",{}}});events();
        check(window.canvas->selections()==std::vector<Canvas::Selection>{{"text",{}}}&&window.canvas->active_composition()=="comp",
            "Text fallback cannot select an object outside the active Composition ownership tree");
        window.canvas->set_selection("hidden");events();
        check(window.canvas->selections()==std::vector<Canvas::Selection>{{"hidden",{}}},"Authored Structure selection retains existing hidden-object semantics without a viewport hit");
        window.host.session.apply({DeleteObjects{{"hidden"}}},window.host.session.revision());window.host.edited();
        window.canvas->set_selection("hidden");events();check(window.canvas->selections().empty(),"Deleted native Text identity cannot survive as a fallback target");
        window.host.session.undo(window.host.session.revision());window.host.edited();window.canvas->set_selection("text");events();
#endif
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-feature-add");set(*dialog,"liga","4294967295");apply(*dialog);
            one_edit(window,before,AddTextFontFeature{"text",{"liga",0xffffffffU,"whole_text"}},"Add Text font feature");
            const auto& features=window.host.session.document().objects.at("text").text->font_features;
            check(features.size()==3&&features[0].feature_tag=="KERN"&&features[1].feature_tag=="lig "&&features[2].feature_tag=="liga", "Adding appends a case-sensitive exact feature without reordering");
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-feature-edit","lig ");
            check(dialog->findChild<QLineEdit*>("text-typography-tag")->isReadOnly(),"Existing feature edits keep the tag identity read-only");
            set(*dialog,"lig ","0");apply(*dialog);one_edit(window,before,UpdateTextFontFeature{"text","lig ",0},"Update Text font feature");
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-feature-remove","KERN");apply(*dialog);
            one_edit(window,before,RemoveTextFontFeature{"text","KERN"},"Remove Text font feature");
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-axis-add");set(*dialog," A  ","1.2345678901234567e-120");apply(*dialog);
            one_edit(window,before,SetTextAdditionalAxis{"text"," A  ",1.2345678901234567e-120},"Set Text axis");
            const auto axis_rows=visible<QGroupBox>(window,"text-advanced-typography")->findChildren<QLabel*>("text-font-axis-row");
            check(axis_rows.size()==2&&axis_rows[0]->property("font-tag").toString()==" A  "&&axis_rows[1]->property("font-tag").toString()=="wdth", "Axis rows follow lexical exact-tag order");
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-axis-edit","wdth");
            check(dialog->findChild<QLineEdit*>("text-typography-value")->text().toDouble()==87.1234567890123,"Axis editor reopens without fixed-precision loss");
            set(*dialog,"wdth","8.712345678901231e1");apply(*dialog);
            one_edit(window,before,SetTextAdditionalAxis{"text","wdth",8.712345678901231e1},"Set Text axis");
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-axis-remove"," A  ");apply(*dialog);
            one_edit(window,before,RemoveTextAdditionalAxis{"text"," A  "},"Remove Text axis");
        }
        {
            const auto expected=window.host.session.document();const auto saved=directory.filePath("roundtrip.nect");window.host.save(saved);
            window.host.open(saved);window.canvas->set_selection("text");events();
            check(window.host.session.document()==expected,"Native save and fresh Session reopen retain the exact edited typography");
        }
        for(const auto& tag:QStringList{"%3  ","<br>"}) {
            Snapshot before(window);auto* dialog=open(window,"text-font-feature-add");set(*dialog,tag,"29");apply(*dialog);
            one_edit(window,before,AddTextFontFeature{"text",{tag.toStdString(),29,"whole_text"}},"Add Text font feature");
            auto* row=visible<QLabel>(window,"text-font-feature-row",tag);
            check(row->text().contains("["+tag+"] = 29")&&row->textFormat()==Qt::PlainText,"Authored feature label displays percent-digit/markup-like tags literally");
            auto* button=visible<QPushButton>(window,"text-font-feature-edit",tag);
            check(button->accessibleName()=="Edit whole-text feature ["+tag+"]","Accessible action name preserves exact tag bytes");
            QTextDocument tooltip;tooltip.setHtml(button->toolTip());
            check(tooltip.toPlainText().replace(QChar(0xa0),QChar(' ')).startsWith(button->accessibleName()),"Rendered tooltip escapes markup-like tags and retains meaningful spaces");
            dialog=open(window,"text-font-feature-edit",tag);
            check(dialog->findChild<QLineEdit*>("text-typography-tag")->text()==tag&&
                dialog->findChild<QLabel*>("text-typography-tag-boundary")->text().contains("["+tag+"]"),"Edit dialog reopens the literal exact tag");
            cancel(*dialog);
        }
        for(const auto* name:{"text-font-feature-add","text-font-axis-add","text-font-feature-remove","text-font-axis-remove"}) {
            Snapshot before(window);const bool removing=QString::fromLatin1(name).endsWith("remove");
            auto* dialog=open(window,name,removing?(QString::fromLatin1(name).contains("feature")?QString("liga"):QString("wdth")):QString{});
            if(!removing)set(*dialog,"TEMP","99");cancel(*dialog);before.unchanged(window);
        }
        for(const auto& item:std::vector<std::pair<const char*,QString>>{{"text-font-feature-edit","liga"},{"text-font-axis-edit","wdth"}}) {
            Snapshot before(window);auto* dialog=open(window,item.first,item.second);apply(*dialog);before.unchanged(window);
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-feature-add");
            for(const auto& tag:QStringList{"abc","abcde",QString::fromUtf8("abé "),"ab\n "}) {
                set(*dialog,tag,"1");rejection(window,*dialog,"INVALID_TEXT_FONT_TAG",before);
                check(dialog->findChild<QLineEdit*>("text-typography-tag")->text()==tag,"Invalid tag is retained, never truncated or normalized");
            }
            for(const auto& number:QStringList{"-1","1.5","4294967296","1e2",""}) {
                set(*dialog,"TEMP",number);rejection(window,*dialog,"INVALID_TEXT_FONT_FEATURE",before);
            }
            set(*dialog,"liga","10");rejection(window,*dialog,"DUPLICATE_TEXT_FONT_FEATURE",before);cancel(*dialog);
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-axis-add");
            for(const auto& tag:QStringList{"wght","ital"}) {set(*dialog,tag,"5");rejection(window,*dialog,"TEXT_AXIS_CONFLICT",before);}
            set(*dialog,"wdth","5");rejection(window,*dialog,"DUPLICATE_TEXT_AXIS",before);
            for(const auto& value:QStringList{"nan","inf","1e999","","1,000"}) {set(*dialog,"TEST",value);rejection(window,*dialog,"INVALID_TEXT_AXIS_VALUE",before);}
            cancel(*dialog);
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-axis-add");set(*dialog,"WGHT","1e300");apply(*dialog);
            one_edit(window,before,SetTextAdditionalAxis{"text","WGHT",1e300},"Set Text axis");
            check(window.host.session.document().objects.at("text").text->additional_axis_values.at("WGHT")==1e300,"Unrepresentable backend float intent remains authored, with exact case preserved");
        }
        {
            auto* dialog=open(window,"text-font-feature-add");set(*dialog,"TEMP","2");
            window.host.session.apply({Rename{"path","Changed elsewhere"}},window.host.session.revision());window.host.edited();events();
            Snapshot after_external(window);rejection(window,*dialog,"REVISION_CONFLICT",after_external);
            check(dialog->findChild<QLineEdit*>("text-typography-value")->text()=="2","Refresh retains stale draft values");cancel(*dialog);
        }
        {
            auto* dialog=open(window,"text-font-axis-add");set(*dialog,"TEST","1.2345678901234567");
            window.canvas->set_selection("peer");events();Snapshot after_selection(window);
            rejection(window,*dialog,"SELECTION_CONFLICT",after_selection);
            window.canvas->set_selection("text");events();rejection(window,*dialog,"SELECTION_CONFLICT",after_selection);cancel(*dialog);
        }
        {
            auto* dialog=open(window,"text-font-feature-add");set(*dialog,"TEMP","3");
            load(window,native_path);Snapshot replacement(window);rejection(window,*dialog,"SESSION_CONFLICT",replacement);cancel(*dialog);
        }
        {
            auto* dialog=open(window,"text-font-feature-add");set(*dialog,"TEMP","4");
            auto replaced=window.host.session.document();replaced.objects.at("text").text->id="replacement-source";
            window.host.session=Session(replaced);window.host.edited();events();Snapshot replacement(window);
            rejection(window,*dialog,"TEXT_EDIT_CONFLICT",replacement);cancel(*dialog);load(window,native_path);
        }
        {
            auto* dialog=open(window,"text-font-feature-add");set(*dialog,"TEMP","5");
            window.host.session.apply({DeleteObjects{{"text"}}},window.host.session.revision());window.host.edited();events();
            Snapshot removed(window);rejection(window,*dialog,"REVISION_CONFLICT",removed);cancel(*dialog);
            window.host.session.undo(window.host.session.revision());window.host.edited();window.canvas->set_selection("text");events();
        }
        {
            auto* dialog=open(window,"text-font-axis-add");set(*dialog,"TEST","9");window.host.session.begin_gesture(window.host.session.revision());
            Snapshot before(window);rejection(window,*dialog,"GESTURE_ACTIVE",before);window.host.session.cancel_gesture();cancel(*dialog);
        }
        window.canvas->set_selections({{"text",{}},{"peer",{}}});events();
        bool advanced=false;for(auto* item:window.findChildren<QGroupBox*>("text-advanced-typography"))advanced=advanced||item->isVisible();
        check(!advanced,"Multi-selection does not expose single-Text typography actions");
        window.canvas->set_selection("peer");events();
        {
            Snapshot before(window);const auto attachment=window.host.session.document().objects.at("peer").text->path_attachment;
            auto* dialog=open(window,"text-font-feature-add");set(*dialog,"ss01","7");apply(*dialog);
            one_edit(window,before,AddTextFontFeature{"peer",{"ss01",7,"whole_text"}},"Add Text font feature");
            check(window.host.session.document().objects.at("peer").text->path_attachment==attachment,"Typography edits preserve Text-on-Path attachment identity and settings");
        }
        window.canvas->set_selection("text");events();
        {
            auto* add=visible<QPushButton>(window,"text-font-feature-add");accessible(add);add->setFocus();QTest::keyClick(add,Qt::Key_Space);events();
            auto* dialog=visible<QDialog>(window,"text-typography-dialog");dialog->resize(320,300);events();
            check(dialog->width()<=320,"Typography dialog genuinely fits a 320-logical-pixel width");
            auto* tag=dialog->findChild<QLineEdit*>("text-typography-tag");auto* value=dialog->findChild<QLineEdit*>("text-typography-value");
            accessible(tag);accessible(value);accessible(dialog->findChild<QPushButton*>("text-typography-apply"));
            tag->setFocus();QTest::keyClicks(tag,"TAB ");QTest::keyClick(tag,Qt::Key_Tab);events();
            check(value->hasFocus(),"Keyboard Tab advances from exact tag to its numeric value");
            value->selectAll();QTest::keyClicks(value,"4294967295");Snapshot before(window);
            check(dialog->findChild<QLabel*>("text-typography-tag-boundary")->text().contains("[TAB ]"),"Visible draft boundaries expose typed trailing spaces");
            for(auto* control:{static_cast<QWidget*>(tag),static_cast<QWidget*>(value),static_cast<QWidget*>(dialog->findChild<QPushButton*>("text-typography-apply"))}) {
                const QRect rect(control->mapTo(dialog,QPoint{}),control->size());
                check(dialog->rect().contains(rect)&&control->width()>50&&control->height()>10,"Narrow dialog keeps input and Apply hit regions visible");
            }
            QTest::keyClick(value,Qt::Key_Escape);events();before.unchanged(window);
        }
        {
            Snapshot before(window);auto* dialog=open(window,"text-font-feature-add");
            auto* tag=dialog->findChild<QLineEdit*>("text-typography-tag");tag->setFocus();QTest::keyClicks(tag,"KEY ");
            QTest::keyClick(tag,Qt::Key_Tab);auto* value=dialog->findChild<QLineEdit*>("text-typography-value");
            value->selectAll();QTest::keyClicks(value,"4294967295");
            auto* apply_button=dialog->findChild<QPushButton*>("text-typography-apply");apply_button->setFocus();QTest::keyClick(apply_button,Qt::Key_Space);events();
            one_edit(window,before,AddTextFontFeature{"text",{"KEY ",0xffffffffU,"whole_text"}},"Add Text font feature");
        }
        section=visible<QGroupBox>(window,"text-advanced-typography");
        for(auto* parent=section->parentWidget();parent;parent=parent->parentWidget())if(auto* dock=qobject_cast<QDockWidget*>(parent)) {
            window.resizeDocks({dock},{300},Qt::Horizontal);break;
        }
        events();section=visible<QGroupBox>(window,"text-advanced-typography");
        check(section->minimumSizeHint().width()<=300,"Advanced Typography itself requires no more than a 300-logical-pixel Inspector width");
        for(auto* button:section->findChildren<QPushButton*>()) {
            accessible(button);const QRect rect(button->mapTo(section,QPoint{}),button->size());
            check(section->rect().contains(rect)&&button->width()>=60,"Narrow Inspector keeps labeled typography actions inside the section");
        }
        std::cout<<"PASS "<<checks<<" Advanced Typography Qt contract checks (offscreen; not real GUI or shaping acceptance)\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}
