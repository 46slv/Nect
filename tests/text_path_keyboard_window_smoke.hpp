#pragma once

// Existing Qt Tab/F4 route, qualified neutral before pending-scalar discovery.
// This is native Qt synthetic input; physical OS input remains NOT_RUN.
namespace text_path_keyboard_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
struct ReverseFocusProof {
    QString name,reference,object,field;
};
inline ReverseFocusProof reverse_focus;
class FocusOutProof final : public QObject {
public:
    int reason=-1;
    bool eventFilter(QObject*,QEvent* event) override {
        if(event->type()==QEvent::FocusOut)reason=static_cast<QFocusEvent*>(event)->reason();
        return false;
    }
};
inline void context(bool pending,const std::string& cause="current",bool attached=false,bool reverse=false) {
    std::cout<<"PATH_KEYBOARD_BEGIN pending="<<pending<<" cause="<<cause<<" attached="<<attached<<" reverse="<<reverse<<std::endl;
    auto d=text_path_rechoose_window_smoke::fixture(attached);d.id="keyboard-path-document";
    check(decode(encode(d))==d,"Complete keyboard fixture native-validates BEFORE Window");
    Session initial(d),scalar(d),expected(d);
    auto scalar_document=d;
    if(pending) {
        scalar_document.objects.at("second").text->parameters.at("font_size").literal=64;
        scalar.apply({EditProperties{{{"second","","text.font_size"}},64,false}},scalar.revision());
    }
    check(scalar.document()==scalar_document&&decode(encode(scalar_document))==scalar_document,
        "Independent complete scalar oracle native-validates BEFORE Window");
    auto complete=scalar_document;auto next=*complete.objects.at("second").text;
    next.path_attachment=TextPathAttachment{"path","contour","distance",1100,3.5,false};
    complete.objects.at("second").text=next;expected=scalar;
    expected.apply({UpdateText{"second",next}},expected.revision());
    check(expected.document()==complete&&decode(encode(complete))==complete,
        "Complete keyboard attachment oracle native-validates BEFORE Window");
    auto incoming=d;if(cause=="target")incoming.objects.at("second").name="Incoming full keyboard target";
    check(decode(encode(incoming))==incoming,"Complete incoming refusal fixture native-validates BEFORE Window");
    Session popup_incoming=scalar;auto popup_base=d;
    if(cause=="popup-target") {
        auto changed=d;changed.objects.at("second").name="Incoming target while keyboard popup open";
        popup_incoming=Session(changed);
        popup_incoming.apply({EditProperties{{{"second","","text.font_size"}},64,false}},popup_incoming.revision());
        changed.objects.at("second").text->parameters.at("font_size").literal=64;
        check(popup_incoming.document()==changed&&popup_incoming.revision()==scalar.revision()&&decode(encode(changed))==changed,
            "Independent popup-time full target and equal-revision scalar history native-validate BEFORE Window");
    }else if(cause=="popup-revision") {
        auto changed=scalar_document;changed.objects.at("source").name="Incoming revision while keyboard popup open";
        popup_incoming.apply({Rename{"source",changed.objects.at("source").name}},popup_incoming.revision());
        check(popup_incoming.document()==changed&&decode(encode(changed))==changed,
            "Independent popup-time complete revision/history native-validate BEFORE Window");
    }else if(cause=="popup-path") {
        popup_base.objects.at("path").name="Incoming full chosen Path while keyboard popup open";
        popup_base.objects.at("path").contours.back().points.back().y.literal=250;
        check(decode(encode(popup_base))==popup_base,"Independent popup-time full chosen Path native-validates BEFORE Window");
        popup_incoming=Session(popup_base);
        popup_incoming.apply({EditProperties{{{"second","","text.font_size"}},64,false}},popup_incoming.revision());
        auto changed=popup_base;changed.objects.at("second").text->parameters.at("font_size").literal=64;
        check(popup_incoming.document()==changed&&popup_incoming.revision()==scalar.revision()&&decode(encode(changed))==changed,
            "Independent full incoming Path/scalar/equal revision history native-validate BEFORE Window");
        complete=changed;next=*changed.objects.at("second").text;
        next.path_attachment=TextPathAttachment{"path","contour","distance",1100,3.5,false};
        complete.objects.at("second").text=next;expected=popup_incoming;
        expected.apply({UpdateText{"second",next}},expected.revision());
        check(expected.document()==complete&&decode(encode(complete))==complete,
            "Independent attachment to full incoming Path native-validates BEFORE Window");
    }
    QTemporaryDir scratch;check(scratch.isValid(),"Keyboard Window owns isolated settings/recovery/native files");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    w.resize(1400,900);w.host.session=initial;w.host.session_id="keyboard-path-session";
    w.host.edited();w.show();w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties dock exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    QPointer<QComboBox> choice=w.findChild<QComboBox*>("text-path-contour");
    QPointer<QLineEdit> scalar_control,start=w.findChild<QLineEdit*>("text-path-start"),spacing=w.findChild<QLineEdit*>("text-path-spacing");
    for(auto* input:w.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="second"&&ref.value("field")=="text.font_size")scalar_control=input;
    }
    check(area&&choice&&scalar_control&&start&&spacing,"Exact original same-target scalar/source controls exist");
    int activated=0,changed=0;
    text_path_pending_chooser_window_smoke::PendingCleanup cleanup{w,scalar_control};
    const auto selected=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
    start->setText("1100");spacing->setText("3.5");
    area->ensureWidgetVisible(scalar_control);events();text_path_rechoose_window_smoke::point(w,scalar_control);
    const auto draft=cause=="invalid"?"not-a-number":"64";if(pending){scalar_control->selectAll();QTest::keyClicks(scalar_control,draft);events();}
    check(scalar_control->hasFocus()&&same(w.host.session,initial)&&scalar_control->isModified()==pending,
        "Original Font size entry keeps full Session neutral before actual Tab route");
    const auto original_choice=choice;const auto original_scalar=scalar_control;
    FocusOutProof focus_out;scalar_control->installEventFilter(&focus_out);
    const auto traverse=[&]{QTest::keyClick(w.windowHandle(),Qt::Key_Tab,reverse?Qt::ShiftModifier:Qt::NoModifier);events();};
    if(cause=="target")w.host.session=Session(incoming);
    else if(cause=="revision")w.host.session.apply({Rename{"source","Incoming keyboard revision"}},w.host.session.revision());
    if(cause=="target"||cause=="revision"||cause=="invalid") {
        const auto before=w.host.session;traverse();
        check(same(w.host.session,before),"Keyboard refusal preserves complete incoming authored/preview/history/context");
        check(original_scalar&&original_choice&&start&&spacing&&scalar_control->text()==draft&&
            start->text()=="1100"&&spacing->text()=="3.5"&&!choice->view()->isVisible(),
            "Rejected forward Tab retains original exact draft/control lifetime and does not open popup");
        const auto code=cause=="target"?"PROPERTY_CONFLICT":cause=="revision"?"REVISION_CONFLICT":"INVALID_VALUE";
        check(w.statusBar()->currentMessage().startsWith(code),"Keyboard preparation refusal reports exact guarded status");
        std::cout<<"PATH_KEYBOARD_REFUSAL cause="<<cause<<" unchanged=1 native=PASS"<<std::endl;return;
    }
    int tabs=0;
    for(;tabs<100;++tabs) {
        choice=w.findChild<QComboBox*>("text-path-contour");
        if(choice&&choice->hasFocus())break;
        traverse();
        auto* focus=QApplication::focusWidget();
        std::cout<<"PATH_KEYBOARD_TAB pending="<<pending<<" step="<<tabs+1<<" class="<<(focus?focus->metaObject()->className():"none")
            <<" name="<<(focus?focus->objectName().toStdString():"none")<<" ref="<<(focus?focus->property("nect-reference").toByteArray().toStdString():"")
            <<" revision="<<w.host.session.revision()<<" original_scalar="<<bool(original_scalar)<<" original_choice="<<bool(original_choice)
            <<" reverse="<<reverse<<" reason="<<focus_out.reason
            <<" start="<<(w.findChild<QLineEdit*>("text-path-start")?w.findChild<QLineEdit*>("text-path-start")->text().toStdString():"missing")
            <<" spacing="<<(w.findChild<QLineEdit*>("text-path-spacing")?w.findChild<QLineEdit*>("text-path-spacing")->text().toStdString():"missing")<<std::endl;
        check(same(w.host.session,pending?scalar:initial),"Actual Tab commits only independent own scalar and preserves full native/history/context");
        if(reverse&&tabs==0) {
            check(focus_out.reason==Qt::BacktabFocusReason,"Actual Shift+Tab produces the installed Qt Backtab focus reason");
            if(!pending) {
                check(focus,"Neutral actual reverse traversal has a live focus destination");
                reverse_focus={focus->objectName(),focus->property("nect-reference").toString(),
                    focus->property("nect-text-scalar-fx-action-object").toString(),focus->property("nect-text-scalar-fx-action-field").toString()};
                const auto ref=QJsonDocument::fromJson(reverse_focus.reference.toUtf8()).object();
                check(reverse_focus.name=="property-source-pick"&&ref.value("object")=="second"&&
                    ref.value("point").toString().isEmpty()&&ref.value("field")=="text.origin_y",
                    "Neutral Shift+Tab qualifies exact existing same-target Origin Y source action");
            }else {
                check(focus&&focus->objectName()==reverse_focus.name&&focus->property("nect-reference").toString()==reverse_focus.reference,
                    "Prepared Shift+Tab restores the exact neutral-qualified existing reverse destination");
            }
        }
        if(pending&&tabs==0) {
            check(!original_scalar&&!original_choice,"Forward Tab QPointer proves originals expire only after prepared rearm");
            if(!reverse)check(focus&&focus->objectName()=="property-expression"&&
                focus->property("nect-text-scalar-fx-action-object").toString()=="second"&&
                focus->property("nect-text-scalar-fx-action-field").toString()=="text.font_size",
                "First actual Tab restores exact existing same-target Font size action without shortcut remap");
            auto* current_start=w.findChild<QLineEdit*>("text-path-start");auto* current_spacing=w.findChild<QLineEdit*>("text-path-spacing");
            check(current_start&&current_spacing&&current_start->text()=="1100"&&current_spacing->text()=="3.5",
                "Prepared first Tab immediately retains exact original numeric drafts");
        }
    }
    check(tabs<100&&choice&&choice->hasFocus(),"Existing actual Tab route reaches production Source contour");
    start=w.findChild<QLineEdit*>("text-path-start");spacing=w.findChild<QLineEdit*>("text-path-spacing");
    std::cout<<"PATH_KEYBOARD_FOCUS pending="<<pending<<" tabs="<<tabs<<" start="<<(start?start->text().toStdString():"missing")
        <<" spacing="<<(spacing?spacing->text().toStdString():"missing")<<" native=PASS"<<std::endl;
    check(start&&spacing&&start->text()=="1100"&&spacing->text()=="3.5","Keyboard focus retains original exact Path numeric drafts");
    const int index=choice->findData("path\ncontour");check(index>0,"Exact original stable Path/Contour exists at keyboard focus");
    QObject::connect(choice,qOverload<int>(&QComboBox::activated),&w,[&](int){++activated;});
    QObject::connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),&w,[&](int){++changed;});
    area->ensureWidgetVisible(choice);events();QTest::keyClick(w.windowHandle(),Qt::Key_F4);events();
    check(choice&&choice->view()->isVisible()&&same(w.host.session,scalar)&&activated==0&&changed==0,
        "Existing actual F4 opens production popup without authored/source changes");
    std::cout<<"PATH_KEYBOARD_QUALIFIED pending="<<pending<<" tabs="<<tabs<<" popup=1 source_neutral=1"<<std::endl;
    auto* popup=choice->view()->window();
    const bool popup_refusal=cause=="popup-target"||cause=="popup-revision";
    const bool popup_path=cause=="popup-path";
    if(popup_refusal||popup_path) {
        w.host.session=popup_incoming;
        std::cout<<"PATH_KEYBOARD_POPUP_INCOMING cause="<<cause<<" revision="<<w.host.session.revision()<<std::endl;
    }
    if(cause=="cancel") {
        QTest::keyClick(popup->windowHandle(),Qt::Key_Escape);events();
        check(choice&&!choice->view()->isVisible()&&activated==0&&changed==0&&same(w.host.session,scalar),
            "Real keyboard Escape preserves scalar-only authored state/history and source neutrality");
        text_path_batch_window_smoke::history(w,"Undo");auto undo=scalar;undo.undo(undo.revision());
        check(same(w.host.session,undo)&&undo.document()==d,"Keyboard cancel keeps independent Scalar Undo");
        text_path_batch_window_smoke::history(w,"Redo");undo.redo(undo.revision());
        check(same(w.host.session,undo),"Keyboard cancel keeps independent Scalar Redo");
        std::cout<<"PATH_KEYBOARD_CANCEL scalar_only=1 native=PASS"<<std::endl;return;
    }
    const int previous_index=choice->currentIndex();
    QTest::keyClick(popup->windowHandle(),Qt::Key_Home);events();
    for(int row=0;row<index;++row){QTest::keyClick(popup->windowHandle(),Qt::Key_Down);events();}
    check(choice->view()->currentIndex().row()==index,"Actual popup keyboard navigation targets exact stable Contour");
    QTest::keyClick(popup->windowHandle(),Qt::Key_Return);events();
    check(choice&&choice->currentIndex()==index&&!choice->view()->isVisible()&&activated==1&&changed==(previous_index==index?0:1)&&same(w.host.session,(popup_refusal||popup_path)?popup_incoming:scalar),
        "Actual keyboard contour activation is source-neutral with exact index/activation semantics");
    auto* button=w.findChild<QPushButton*>("text-path-apply");check(button,"Production attach action remains available");
    if(popup_refusal) {
        const auto code=cause=="popup-target"?"PROPERTY_CONFLICT":"REVISION_CONFLICT";
        check(w.statusBar()->currentMessage().startsWith(code),"Keyboard choice reports exact incoming context refusal");
        area->ensureWidgetVisible(button);events();text_path_rechoose_window_smoke::point(w,button);
        check(same(w.host.session,popup_incoming)&&w.statusBar()->currentMessage().startsWith(code),
            "Attach after refused keyboard choice preserves complete incoming authored/preview/history/context atomically");
        check(choice&&start&&spacing&&start->text()=="1100"&&spacing->text()=="3.5"&&
            w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
            "Popup-time refusal retains original controls/raw drafts/target scope");
        std::cout<<"PATH_KEYBOARD_POPUP_REFUSAL cause="<<cause<<" choice_and_attach_atomic=1 native=PASS"<<std::endl;return;
    }
    area->ensureWidgetVisible(button);events();text_path_rechoose_window_smoke::point(w,button);
    check(same(w.host.session,expected),"Keyboard choice then production Attach matches complete independent canonical oracle");
    text_path_batch_window_smoke::history(w,"Undo");expected.undo(expected.revision());
    check(same(w.host.session,expected)&&expected.document()==(popup_path?popup_incoming.document():scalar.document()),"Attachment Undo is separate and restores full scalar-only document");
    if(pending){text_path_batch_window_smoke::history(w,"Undo");expected.undo(expected.revision());check(same(w.host.session,expected)&&expected.document()==(popup_path?popup_base:d),"Scalar Undo restores full incoming native Text/styles/Refs/Path");}
    if(pending){text_path_batch_window_smoke::history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Separate Scalar Redo matches canonical history");}
    text_path_batch_window_smoke::history(w,"Redo");expected.redo(expected.revision());check(same(w.host.session,expected),"Separate attachment Redo matches full canonical history");
    const auto native=scratch.filePath("keyboard-result.nect");w.host.save(native);w.host.flush();
    check(same(w.host.session,expected),"Owned native save preserves full live Session/history");
    Window reopened(scratch.filePath("cold-recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1400,900);reopened.host.open(native);reopened.show();events();
    check(reopened.host.session.document()==expected.document()&&encode(reopened.host.session.document())==encode(expected.document()),
        "Isolated cold native Window preserves full source/Refs/styles/IDs/Path/Contour/Composition");
    reopened.host.changed={};reopened.hide();events();
    check(w.canvas->selections()==selected&&w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Keyboard/history/cold native preserve exact target and scope");
    std::cout<<"PATH_KEYBOARD_PASS pending="<<pending<<" tabs="<<tabs<<" native=PASS physical OS input NOT_RUN"<<std::endl;
}
inline void run() {
    const int before=text_path_batch_window_smoke::checks;
    context(false); // A pending failure is product evidence only after this route qualifies.
    context(true);context(true,"current",true);context(true,"cancel");
    for(const auto* cause:{"target","revision","invalid"})context(true,cause);
    std::cout<<"PathKeyboard Windows cases=7 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;
}
inline void run_reverse() {
    const int before=text_path_batch_window_smoke::checks;
    context(false,"current",false,true);
    context(true,"current",false,true);
    context(true,"current",true,true);context(true,"cancel",false,true);
    for(const auto* cause:{"target","revision","invalid"})context(true,cause,false,true);
    std::cout<<"PathReverseKeyboard Windows cases=7 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;
}
inline void run_popup_incoming() {
    const int before=text_path_batch_window_smoke::checks;
    // Reuse the neutral native route qualified by run_reverse on unchanged
    // production Window; these cases exercise only the later popup-time edge.
    reverse_focus={QStringLiteral("property-source-pick"),
        QStringLiteral("{\"field\":\"text.origin_y\",\"object\":\"second\",\"point\":\"\"}"),{}, {}};
    for(const auto* cause:{"popup-target","popup-revision"})context(true,cause,false,true);
    std::cout<<"PathKeyboardPopupIncoming Windows cases=2 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;
}
inline void run_popup_source() {
    const int before=text_path_batch_window_smoke::checks;
    reverse_focus={QStringLiteral("property-source-pick"),
        QStringLiteral("{\"field\":\"text.origin_y\",\"object\":\"second\",\"point\":\"\"}"),{}, {}};
    context(true,"popup-path",true,true);
    std::cout<<"PathKeyboardPopupSource Windows cases=1 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;
}
}
