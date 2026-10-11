#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>

// Source-action activation is a different owner from the Path combo popup.
// Qt 6.5.3 QAbstractButton uses platform ButtonPressKeys; Windows inherits
// Space/Select. Qualify actual Space press/release before pending claims.
namespace text_path_source_keyboard_window_smoke {
using text_path_batch_window_smoke::check;
using text_path_batch_window_smoke::events;
using text_path_batch_window_smoke::same;
inline void context(bool pending,const std::string& mode="cancel") {
    std::cout<<"PATH_SOURCE_KEYBOARD_BEGIN pending="<<pending<<" mode="<<mode<<" platform="<<QGuiApplication::platformName().toStdString()
        <<" Qt="<<qVersion()<<" pid="<<QCoreApplication::applicationPid()<<std::endl;
    auto d=text_path_rechoose_window_smoke::fixture(false);
    d.id="source-action-keyboard-document";
    const Ref target{"second","","text.origin_y"},source{"source","","text.origin_y"};
    check(decode(encode(d))==d,"Complete source-action fixture native-validates BEFORE Window");
    Session initial(d),scalar(d);auto scalar_document=d;
    if(pending) {
        scalar_document.objects.at("second").text->parameters.at("font_size").literal=64;
        scalar.apply({EditProperties{{{"second","","text.font_size"}},64,false}},scalar.revision());
    }
    check(scalar.document()==scalar_document&&decode(encode(scalar_document))==scalar_document,
        "Independent complete scalar/history oracle native-validates BEFORE Window");
    Session linked=scalar;auto linked_document=scalar_document;
    auto& value=linked_document.objects.at("second").text->parameters.at("origin_y");
    value.binding=Binding{source,1,0};value.expression.reset();
    linked.apply({LinkProperties{{target},source,false}},linked.revision());
    check(linked.document()==linked_document&&decode(encode(linked_document))==linked_document,
        "Independent exact source/target Link oracle native-validates BEFORE Window");
    QTemporaryDir scratch;check(scratch.isValid(),"Source-action Window owns settings/recovery/native scratch");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    w.resize(1400,900);w.host.session=initial;w.host.session_id="source-action-keyboard-session";
    w.host.edited();w.show();w.activateWindow();events();
    w.canvas->set_active_artboard("composition","artboard",false);w.canvas->set_selection("second");events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Production Properties dock exists");dock->show();dock->raise();events();
    auto* area=w.findChild<QScrollArea*>("inspector-scroll");
    QPointer<QLineEdit> size,start=w.findChild<QLineEdit*>("text-path-start"),spacing=w.findChild<QLineEdit*>("text-path-spacing");
    QPointer<QComboBox> contour=w.findChild<QComboBox*>("text-path-contour");
    for(auto* input:w.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object")=="second"&&ref.value("field")=="text.font_size")size=input;
    }
    check(area&&size&&start&&spacing&&contour,"Exact same-target Font size and Path controls exist");
    text_path_pending_chooser_window_smoke::PendingCleanup cleanup{w,size};
    start->setText("1100");spacing->setText("3.5");
    area->ensureWidgetVisible(size);events();text_path_rechoose_window_smoke::point(w,size);
    if(pending){QTest::keyClick(w.windowHandle(),Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"64");events();}
    check(size->hasFocus()&&size->isModified()==pending&&same(w.host.session,initial),
        "Font size draft remains complete-Session neutral before supported reverse input");
    const auto original_size=size,original_start=start;const auto original_contour=contour;
    const auto selected=w.canvas->selections();const auto composition=w.canvas->active_composition(),artboard=w.canvas->active_artboard();
    const auto session_id=w.host.session_id;
    text_path_keyboard_window_smoke::FocusOutProof focus_out;size->installEventFilter(&focus_out);
    QTest::keyClick(w.windowHandle(),Qt::Key_Tab,Qt::ShiftModifier);events();
    QPointer<QPushButton> action=qobject_cast<QPushButton*>(QApplication::focusWidget());
    const auto ref=action?QJsonDocument::fromJson(action->property("nect-reference").toByteArray()).object():QJsonObject{};
    check(action&&action->objectName()=="property-source-pick"&&ref.value("object")=="second"&&
        ref.value("point").toString().isEmpty()&&ref.value("field")=="text.origin_y"&&focus_out.reason==Qt::BacktabFocusReason,
        "Actual first Shift+Tab reaches exact supported same-target Origin Y action");
    check(same(w.host.session,scalar)&&w.host.session_id==session_id&&w.canvas->selections()==selected&&
        w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Reverse completion matches independent full scalar Session and exact scope");
    check(bool(original_size)==!pending&&bool(original_contour)==!pending&&bool(original_start)==!pending,
        "Original controls expire only for the already-qualified pending rearm");
    start=w.findChild<QLineEdit*>("text-path-start");spacing=w.findChild<QLineEdit*>("text-path-spacing");contour=w.findChild<QComboBox*>("text-path-contour");
    check(start&&spacing&&contour&&start->text()=="1100"&&spacing->text()=="3.5",
        "Source action entry retains complete raw Path drafts");
    int pressed=0,released=0,clicked=0;
    QObject::connect(action,&QPushButton::pressed,&w,[&]{++pressed;});
    QObject::connect(action,&QPushButton::released,&w,[&]{++released;});
    QObject::connect(action,&QPushButton::clicked,&w,[&]{++clicked;});
    QTest::keyPress(w.windowHandle(),Qt::Key_Space);events();
    check(action&&action->isDown()&&pressed==1&&released==0&&clicked==0&&
        !w.findChild<QDialog*>("property-source-picker")&&same(w.host.session,scalar),
        "Supported actual Space press arms existing action without mutation or picker");
    QTest::keyRelease(w.windowHandle(),Qt::Key_Space);events();
    QPointer<QDialog> picker=w.findChild<QDialog*>("property-source-picker");
    check(action&&pressed==1&&released==1&&clicked==1&&picker&&picker->isVisible()&&picker->parentWidget()==&w&&picker->testAttribute(Qt::WA_DeleteOnClose),
        "Supported actual Space release opens exactly the owned production Source picker");
    auto* search=picker->findChild<QLineEdit*>("property-source-picker-search");
    auto* list=picker->findChild<QListWidget*>("property-source-picker-list");
    check(search&&search->hasFocus()&&list,"Owned picker starts at its existing search control");
    bool source_present=false,target_present=false;
    for(int i=0;i<list->count();++i){
        const auto item=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();
        const Ref candidate{item.value("object").toString().toStdString(),item.value("point").toString().toStdString(),item.value("field").toString().toStdString()};
        if(candidate==source)source_present=bool(list->item(i)->flags()&Qt::ItemIsEnabled);
        if(candidate==target)target_present=true;
    }
    check(source_present&&!target_present,"Picker contains exact compatible independent source and excludes its frozen target");
    check(same(w.host.session,scalar)&&w.host.session_id==session_id&&w.canvas->selections()==selected&&
        w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Picker opening is source-neutral for full authored/preview/history/identity/context");
    std::cout<<"PATH_SOURCE_KEYBOARD_OPEN pending="<<pending<<" key=Space pressed="<<pressed<<" released="<<released<<" clicked="<<clicked
        <<" target=second/text.origin_y session="<<session_id.toStdString()<<" revision="<<w.host.session.revision()<<" neutral=1"<<std::endl;
    if(mode!="cancel") {
        check(search->hasFocus(),"Qualified picker search owns actual input focus");
        QTest::keyClicks(search,"source text.origin_y");events();
        int visible=0;for(int i=0;i<list->count();++i)if(!list->item(i)->isHidden())++visible;
        check(visible==1&&same(w.host.session,scalar),"Actual search uniquely resolves stable source without authored mutation");
        QTest::keyClick(picker->windowHandle(),Qt::Key_Tab);events();
        check(list->hasFocus(),"Actual picker Tab reaches its existing source list");
        QTest::keyClick(picker->windowHandle(),Qt::Key_Home);events();
        auto* item=list->currentItem();const auto chosen=item?QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray()).object():QJsonObject{};
        check(item&&!item->isHidden()&&chosen.value("object")=="source"&&chosen.value("point").toString().isEmpty()&&chosen.value("field")=="text.origin_y",
            "Actual source list navigation selects exact stable compatible source");
        check(same(w.host.session,scalar)&&w.host.session_id==session_id&&
            w.canvas->selections()==std::vector<Canvas::Selection>{{"source",{}}}&&
            w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
            "Source browsing changes only viewed selection with full Session/revision/scope neutral");
        check(!action&&!start&&!spacing&&!contour,"Source browsing expires only original Inspector controls");
        std::cout<<"PATH_SOURCE_KEYBOARD_BROWSE pending="<<pending<<" mode="<<mode<<" source=source/text.origin_y revision="<<w.host.session.revision()<<" neutral=1 controls=expired"<<std::endl;
        if(mode=="link") {
            auto* buttons=picker->findChild<QDialogButtonBox*>();check(buttons,"Existing Source picker confirm owner exists");
            QTest::keyClick(picker->windowHandle(),Qt::Key_Tab);events();
            check(buttons->button(QDialogButtonBox::Ok)->hasFocus(),"Actual source-list Tab reaches existing OK button");
            QTest::keyClick(picker->windowHandle(),Qt::Key_Space);events();
            check(!picker&&same(w.host.session,linked)&&w.host.session_id==session_id&&w.canvas->selections()==selected&&
                w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
                "Actual existing OK activation links exact frozen target/source and full independent canonical Session");
            text_path_batch_window_smoke::history(w,"Undo");linked.undo(linked.revision());
            check(same(w.host.session,linked)&&linked.document()==scalar_document,"Source Link Undo independently restores complete scalar-only native document");
            if(pending){text_path_batch_window_smoke::history(w,"Undo");linked.undo(linked.revision());
                check(same(w.host.session,linked)&&linked.document()==d,"Independent Scalar Undo remains separate after Source Link");
                text_path_batch_window_smoke::history(w,"Redo");linked.redo(linked.revision());check(same(w.host.session,linked),"Independent Scalar Redo remains separate");}
            text_path_batch_window_smoke::history(w,"Redo");linked.redo(linked.revision());
            check(same(w.host.session,linked)&&linked.document()==linked_document,"Independent Source Link Redo restores entire linked native state");
            scalar=linked;
        }
    }
    if(picker){QTest::keyClick(picker->windowHandle(),Qt::Key_Escape);events();}
    check(!picker&&same(w.host.session,scalar)&&w.host.session_id==session_id&&w.canvas->selections()==selected&&
        w.canvas->active_composition()==composition&&w.canvas->active_artboard()==artboard,
        "Actual Escape destroys only owned picker and preserves scalar Session/target/scope");
    if(mode=="cancel")check(action&&start&&spacing&&contour&&start->text()=="1100"&&spacing->text()=="3.5",
        "Picker Cancel keeps exact existing controls and raw Path drafts alive");
    else check(w.findChild<QLineEdit*>("text-path-start")&&w.findChild<QLineEdit*>("text-path-spacing"),
        "Source browser completion restores current target Path controls");
    if(pending&&mode!="link"){text_path_batch_window_smoke::history(w,"Undo");auto history=scalar;history.undo(history.revision());
        check(same(w.host.session,history)&&history.document()==d,"Cancel keeps independent Scalar Undo");
        text_path_batch_window_smoke::history(w,"Redo");history.redo(history.revision());check(same(w.host.session,history),"Cancel keeps independent Scalar Redo");scalar=history;}
    const auto native=scratch.filePath("source-entry.nect");w.host.save(native);w.host.flush();
    check(same(w.host.session,scalar),"Save preserves full live independent Session/history");
    Window cold(scratch.filePath("cold-recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    cold.setAttribute(Qt::WA_DontShowOnScreen);cold.resize(1400,900);cold.host.open(native);cold.show();events();
    check(cold.host.session.document()==scalar.document()&&encode(cold.host.session.document())==encode(scalar.document()),
        "Cold Window with owned settings preserves complete native Text/Path/Refs/styles/Composition");
    cold.host.changed={};cold.hide();events();
    std::cout<<"PATH_SOURCE_KEYBOARD_PASS pending="<<pending<<" mode="<<mode<<" native=PASS physical OS input NOT_RUN"<<std::endl;
}
inline void run(){const int before=text_path_batch_window_smoke::checks;context(false);context(true);
    std::cout<<"PathSourceKeyboard Windows cases=2 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;}
inline void run_choice(){const int before=text_path_batch_window_smoke::checks;
    context(false,"browse-cancel");context(false,"link");context(true,"link");
    std::cout<<"PathSourceKeyboardChoice Windows cases=3 checks="<<text_path_batch_window_smoke::checks-before<<" failures=0 physical OS input NOT_RUN"<<std::endl;}
}
