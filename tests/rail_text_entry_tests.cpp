#include "window.hpp"
#include "visual_style.hpp"
#include "tool_rail.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>
#include <QToolTip>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QScrollArea>
#include <QScrollBar>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QApplication::processEvents();}
// Authored state and complete history are the oracle, never just visible labels.
auto snapshot(Session& session){return std::tuple{session.document(),encode(session.document()),session.revision(),session.history()};}
void unchanged(Session& session,const decltype(snapshot(std::declval<Session&>()))& before,const char* why){check(snapshot(session)==before,why);}
void click(Window& window,const char* name){
    auto* button=window.findChild<QToolButton*>(QString::fromLatin1(name));
    check(button&&button->isVisible()&&button->isEnabled(),"Real Rail control is visible and enabled");
    QTest::mouseClick(button,Qt::LeftButton);events();
}
void variant(Window& window,bool vertical){
    auto* button=window.findChild<QToolButton*>("tool-text");auto* menu=button->menu();
    const auto before=snapshot(window.host.session);bool opened=false,neutral=false;
    QTimer::singleShot(900,menu,[&]{
        opened=menu->isVisible();neutral=snapshot(window.host.session)==before;
        if(opened)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(vertical?1:0)).center());
        else menu->close();
    });
    QTest::mousePress(button,Qt::LeftButton);QTest::qWait(1000);QTest::mouseRelease(button,Qt::LeftButton);events();
    check(opened&&neutral,"Actual press-and-hold opens a document/history-neutral flyout");
    unchanged(window.host.session,before,"Picking a creation variant never changes selected Text or history");
    check(button->accessibleName().startsWith(vertical?"Vertical Text":"Horizontal Text"),"Group slot reflects remembered variant");
}
void keyboard_focus_help(){
    QTemporaryDir scratch;check(scratch.isValid(),"Rail focus check owns preferences and recovery");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.resize(1100,750);window.show();
    window.activateWindow();events();
    auto* rail=static_cast<ToolRail*>(window.findChild<QToolBar*>("tool-rail"));
    auto* pen=window.findChild<QToolButton*>("tool-pen");
    auto* text=window.findChild<QToolButton*>("tool-text");
    auto* anchor=window.findChild<QToolButton*>("tool-anchor");
    check(rail&&pen&&text&&anchor,"Adjacent real Rail controls exist");
    const auto before=snapshot(window.host.session);
    const auto preference=settings.value("workspace/tools/textCreationDirection");
    QCursor::setPos(window.canvas->mapToGlobal(window.canvas->rect().center()));
    QToolTip::hideText();pen->setFocus(Qt::OtherFocusReason);events();
    QTest::keyClick(pen,Qt::Key_Tab);events();
    check(text->hasFocus(),"Tab reaches Text without activating it");
    if(!QToolTip::isVisible()||QToolTip::text()!=text->toolTip())
        std::cerr<<"Focused tooltip visible="<<QToolTip::isVisible()<<" text="<<QToolTip::text().toStdString()<<'\n';
    check(QToolTip::isVisible()&&QToolTip::text()==text->toolTip()&&
        QToolTip::text().startsWith("Horizontal Text"),"Keyboard focus visibly names Text, current variant and hold hint away from pointer");
    QTest::keyClick(text,Qt::Key_Tab);events();
    check(anchor->hasFocus()&&QToolTip::isVisible()&&QToolTip::text()==anchor->toolTip(),
        "Next keyboard focus replaces Text help with adjacent Tool name and shortcut");
    QTest::keyClick(anchor,Qt::Key_Tab,Qt::ShiftModifier);events();
    check(text->hasFocus()&&QToolTip::text()==text->toolTip(),"Backtab restores current Text help");
    rail->set_vertical_text(true);events();
    check(QToolTip::isVisible()&&QToolTip::text()==text->toolTip()&&QToolTip::text().startsWith("Vertical Text"),
        "Focused Text help follows current workspace variant");
    window.canvas->setFocus();events();QTest::qWait(350);
    check(!QToolTip::isVisible(),"Leaving Rail clears keyboard help");
    text->setFocus(Qt::MouseFocusReason);events();
    check(!QToolTip::isVisible(),"Pointer focus retains the normal hover route");
    check(!text->menu()->isVisible()&&!window.canvas->text_mode()&&pen->isChecked()==false,
        "Keyboard traversal opens no variant menu and activates no Tool");
    unchanged(window.host.session,before,"Focus help preserves complete authored state, revision and history");
    check(settings.value("workspace/tools/textCreationDirection")==preference,
        "Focus help does not write the creation preference");
    std::cout<<"rail_keyboard_focus_help: "<<checks<<" checks passed\n";
}
void key_isolation(){
    QTemporaryDir scratch;check(scratch.isValid(),"Text key regression owns preferences, recovery and native file");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
    auto document=empty_document("text-key-document","composition","board");
    auto& board=document.compositions.front().artboards.front();board.width=640;board.height=480;
    Object text;text.id="text";text.name="Japanese Text";text.kind=Kind::text;
    text.text=default_text("text-source","日本語（ABC123）");text.text->family="Yu Gothic";
    text.text->parameters.at("origin_x").literal=100;text.text->parameters.at("origin_y").literal=100;
    text.stack={default_operation("text-fill","nect.paint.fill")};
    document.objects.emplace(text.id,text);document.compositions.front().roots={text.id};
    Session fixture(document);fixture.apply({
        CreatePrimitive{"composition",{},"rectangle","Retained Rectangle",default_primitive("rectangle-source","nect.shape.rectangle")},
        AddOperation{"rectangle",default_operation("rectangle-fill","nect.paint.fill"),0},
        Set{{"rectangle",{},"transform.tx"},320},Set{{"rectangle",{},"transform.ty"},300}},fixture.revision());
    auto& session=window.host.session;session=Session(fixture.document());window.host.edited();events();
    window.canvas->fit_artboard();window.canvas->set_snap_enabled(false);events();
    const auto select_text=[&]{
        auto* tree=window.findChild<QTreeWidget*>();check(tree&&tree->isVisible(),"Actual Structure is reachable");
        QTreeWidgetItem* row=nullptr;
        for(QTreeWidgetItemIterator i(tree);*i;++i)
            if((*i)->data(0,Qt::UserRole).toString()=="text"&&(*i)->data(0,Qt::UserRole+1).toString().isEmpty()){row=*i;break;}
        check(row!=nullptr,"Exact whole Text Structure row exists");tree->scrollToItem(row);events();
        QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());events();
        check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Structure pointer selects exact whole Text");
    };
    const auto neutral=[&](Qt::Key key,Qt::KeyboardModifiers modifiers,bool repeat=false){
        const auto before=snapshot(session);const auto selection=window.canvas->selections();
        auto* button=window.findChild<QToolButton*>("tool-text");const auto name=button->accessibleName();
        QApplication::setActiveWindow(&window);window.canvas->setFocus();events();
        check(window.isActiveWindow()&&window.canvas->hasFocus(),"Text arrow reaches actual active Window and focused Canvas");
        const auto image=window.canvas->grab().toImage();const auto zoom=window.canvas->zoom();
        if(repeat){QKeyEvent event(QEvent::KeyPress,key,modifiers,QString{},true,1);QApplication::sendEvent(window.canvas,&event);}
        else QTest::keyClick(window.canvas,key,modifiers);
        events();
        std::cout<<name.toStdString()<<" idle arrow: authored_equal="<<(snapshot(session)==before)
            <<" text_active="<<window.canvas->text_mode()<<'\n';
        check(snapshot(session)==before&&!session.gesture_active(),"Idle Text arrow preserves complete Document/native/revision/history");
        check(window.canvas->text_mode()&&button->isChecked()&&button->accessibleName()==name&&window.canvas->selections()==selection,
            "Idle Text arrow preserves active variant and exact whole Object or stable point selection");
        check(window.canvas->zoom()==zoom&&window.canvas->grab().toImage()==image,"Idle Text arrow preserves rendered artwork and viewport");
    };
    for(bool vertical:{false,true}){
        select_text();variant(window,vertical);
        neutral(Qt::Key_Right,Qt::NoModifier);neutral(Qt::Key_Down,Qt::ShiftModifier);neutral(Qt::Key_Left,Qt::NoModifier,true);
        click(window,"tool-direct-selection");
        const auto& source=*session.document().objects.at("rectangle").source;
        const QPoint corner(qRound(window.canvas->width()/2.0-source.parameters.at("width").literal/2*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0+(60-source.parameters.at("height").literal/2)*window.canvas->zoom()));
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,corner);events();
        check(window.canvas->selected_object=="rectangle"&&window.canvas->selected_point=="rectangle-source-top-left",
            "Direct pointer discovers exact retained stable point before Text reactivation");
        click(window,"tool-text");neutral(Qt::Key_Up,Qt::ShiftModifier);
    }
    select_text();click(window,"tool-selection");
    const auto before=session.document();const auto revision=session.revision();const auto history=session.history().states.size();
    Session oracle(before);oracle.apply({TranslateObjects{{"text"},1,0}},oracle.revision());
    QTest::keyClick(window.canvas,Qt::Key_Right);events();
    check(session.document()==oracle.document()&&session.revision()==revision+1&&session.history().states.size()==history+1,
        "Explicit Selection arrow still translates Text canonically in one transaction");
    const auto moved=session.document();session.undo(session.revision());window.host.edited();events();
    check(session.document()==before,"One Undo restores complete Text and unrelated retained source");
    session.redo(session.revision());window.host.edited();events();check(session.document()==moved,"One Redo restores complete Text translation");
    click(window,"tool-text");
    const auto file=scratch.filePath("text-key.nect.json");window.host.save(file);window.host.open(file);events();
    check(session.document()==moved&&window.canvas->text_mode(),"Same Window native reopen preserves full editable source and active Text Tool");
    select_text();neutral(Qt::Key_Right,Qt::NoModifier);
    std::cout<<"text_key_isolation: "<<checks<<" checks passed; DPR="<<window.devicePixelRatioF()<<'\n';
}
void cursor_continuity(){
    QTemporaryDir scratch;check(scratch.isValid(),"Text cursor probe owns preferences, recovery and native file");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
    auto document=empty_document("text-cursor-document","composition","board");
    document.compositions.front().artboards.front().width=640;
    document.compositions.front().artboards.front().height=480;
    Session fixture(document);
    fixture.apply({CreatePrimitive{"composition",{},"rectangle","Retained Rectangle",default_primitive("rectangle-source","nect.shape.rectangle")},
        AddOperation{"rectangle",default_operation("rectangle-fill","nect.paint.fill"),0},
        Set{{"rectangle",{},"transform.tx"},320},Set{{"rectangle",{},"transform.ty"},300}},fixture.revision());
    auto& session=window.host.session;session=Session(fixture.document());window.host.edited();events();
    window.canvas->fit_artboard();window.canvas->set_snap_enabled(false);events();
    const auto& source=*session.document().objects.at("rectangle").source;
    const QPoint corner(qRound(window.canvas->width()/2.0-source.parameters.at("width").literal/2*window.canvas->zoom()),
        qRound(window.canvas->height()/2.0+(60-source.parameters.at("height").literal/2)*window.canvas->zoom()));
    click(window,"tool-direct-selection");QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,corner);events();
    check(window.canvas->selected_object=="rectangle"&&window.canvas->selected_point=="rectangle-source-top-left",
        "Actual Direct pointer selects the retained stable point before Text hover");
    const auto move=[&](QPoint position){
        QMouseEvent event(QEvent::MouseMove,QPointF(position),QPointF(window.canvas->mapToGlobal(position)),
            Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(window.canvas,&event);events();
        check(event.isAccepted(),"Canvas accepts the dispatched mouse-move event");
    };
    const auto hover=[&](QPoint position){
        const auto before=snapshot(session);const auto selection=window.canvas->selections();
        const auto image=window.canvas->grab().toImage();const auto zoom=window.canvas->zoom();
        auto* button=window.findChild<QToolButton*>("tool-text");const auto variant_name=button->accessibleName();
        move(position);
        std::cout<<"Text hover: cursor="<<window.canvas->cursor().shape()<<" text_active="<<window.canvas->text_mode()<<'\n';
        check(window.canvas->cursor().shape()==Qt::IBeamCursor,"Active Text keeps its placement cursor over empty space and retained artwork");
        unchanged(session,before,"Text hover preserves complete Document/native/revision/history");
        check(window.canvas->text_mode()&&button->isChecked()&&button->accessibleName()==variant_name&&
            window.canvas->selections()==selection&&!session.gesture_active(),
            "Text hover retains active placement Tool and exact stable selection without an authored gesture");
        check(window.canvas->zoom()==zoom&&window.canvas->grab().toImage()==image,"Text hover preserves rendered artwork and viewport");
    };
    for(bool vertical:{false,true}){
        variant(window,vertical);
        check(window.canvas->cursor().shape()==Qt::IBeamCursor,"Rail Text activation sets the placement cursor");
        hover(QPoint(25,25));hover(corner);hover(window.canvas->rect().center());
        QApplication::setActiveWindow(&window);window.canvas->setFocus();events();
        const auto before=snapshot(session);
        QTest::keyPress(window.canvas,Qt::Key_Space);events();
        check(window.canvas->cursor().shape()==Qt::OpenHandCursor,"Temporary Space pan overrides the Text cursor");
        QTest::keyRelease(window.canvas,Qt::Key_Space);events();hover(QPoint(30,30));
        unchanged(session,before,"Temporary Hand and release preserve source and history");
        const auto point=window.canvas->rect().center()+QPoint(50,40);
        const auto source_before=session.document();const auto revision=session.revision();const auto history=session.history().states.size();
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,point);events();
        const auto id=window.canvas->selected_object;
        check(session.document().objects.at(id).text&&session.document().objects.at(id).text->direction==(vertical?"vertical":"horizontal"),
            "After hover the real Text placement still creates the selected writing variant");
        check(session.revision()==revision+1&&session.history().states.size()==history+1,"Placement after hover remains one authored transaction");
        const auto placed=session.document();session.undo(session.revision());window.host.edited();events();
        check(session.document()==source_before,"One Undo restores all retained source before placement");
        session.redo(session.revision());window.host.edited();events();check(session.document()==placed,"One Redo restores complete placed Text source");
        const auto file=scratch.filePath("text-cursor.nect.json");window.host.save(file);window.host.open(file);events();
        check(session.document()==placed&&window.canvas->text_mode(),"Same Window native reopen preserves editable Text and placement mode");
        hover(QPoint(35,35));
    }
    click(window,"tool-selection");move(QPoint(40,40));
    check(!window.canvas->text_mode()&&window.canvas->cursor().shape()==Qt::ArrowCursor,"Explicit Selection restores the ordinary hover cursor");
    std::cout<<"text_cursor_continuity: "<<checks<<" checks passed; DPR="<<window.devicePixelRatioF()<<'\n';
}
void editing_flow(){
    QTemporaryDir scratch;check(scratch.isValid(),"Text flow owns preferences, recovery and native file");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
    auto& session=window.host.session;
    const auto empty=snapshot(session);click(window,"tool-text");
    unchanged(session,empty,"Text activation is full source/revision/history neutral");
    QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,window.canvas->rect().center());events();
    const auto id=window.canvas->selected_object;const auto created=session.document();
    check(created.objects.contains(id)&&created.objects.at(id).text&&created.objects.at(id).text->direction=="horizontal",
        "Real Canvas pointer creates editable Horizontal Text");
    check(session.revision()==std::get<2>(empty)+1&&session.history().states.size()==std::get<3>(empty).states.size()+1,
        "Text placement is one authored transaction");
    const auto before_variant=snapshot(session);variant(window,true);
    unchanged(session,before_variant,"Vertical creation choice never converts existing Horizontal Text");
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* direction=window.findChild<QComboBox*>("text-direction");
    check(scroll&&direction&&direction->isEnabled(),"Existing Text Writing control is directly enabled");
    scroll->ensureWidgetVisible(direction);events();
    QTest::mouseClick(direction,Qt::LeftButton);events();
    auto* view=direction->view();const auto vertical=view->model()->index(1,0);
    check(view->isVisible()&&!view->visualRect(vertical).isEmpty(),"Real Writing pointer opens the direction choices");
    // Qt guards the popup's opening mouse release. A user moves to the row;
    // QTest::mouseClick alone teleports there and leaves that guard active.
    QTest::mouseMove(view->viewport(),view->visualRect(vertical).center());
    QTest::qWait(QApplication::doubleClickInterval()+10);
    QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(vertical).center());events();
    auto expected_vertical=created;expected_vertical.objects.at(id).text->direction="vertical";
    std::cout<<"Writing pointer result: direction="<<session.document().objects.at(id).text->direction
        <<" revision="<<session.revision()<<" expected_revision="<<std::get<2>(before_variant)+1
        <<" history="<<session.history().states.size()<<" expected_history="<<std::get<3>(before_variant).states.size()+1
        <<" selected="<<window.canvas->selected_object<<"\n";
    check(session.document()==expected_vertical&&session.revision()==std::get<2>(before_variant)+1&&
        session.history().states.size()==std::get<3>(before_variant).states.size()+1,
        "Pointer Writing change preserves complete authored source except direction in one transaction");
    const auto before_edit=snapshot(session);const QString content=QString::fromUtf8("日本語（ABC123）\n縦書き、句読点。");
    auto* edit=window.findChild<QPushButton*>("edit-text-content");
    check(edit&&edit->isEnabled(),"Inspector exposes the real Edit text entry");
    scroll->ensureWidgetVisible(edit);events();bool opened=false,draft_neutral=false,applied=false;
    QTimer::singleShot(0,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");
        if(!dialog)return;opened=dialog->isVisible();
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");
        auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!editor||!buttons){dialog->reject();return;}
        editor->setFocus();QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);
        QInputMethodEvent input;input.setCommitString(content);QApplication::sendEvent(editor,&input);
        draft_neutral=snapshot(session)==before_edit&&editor->toPlainText()==content;
        QTest::mouseClick(buttons->button(QDialogButtonBox::Apply),Qt::LeftButton);applied=!dialog->isVisible();
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(edit,Qt::LeftButton);events();
    check(opened&&draft_neutral&&applied,"Actual Edit text opens, Japanese IME draft stays neutral, pointer Apply commits");
    auto expected_edited=expected_vertical;expected_edited.objects.at(id).text->content=content.toStdString();
    check(session.document()==expected_edited&&session.revision()==std::get<2>(before_edit)+1&&
        session.history().states.size()==std::get<3>(before_edit).states.size()+1,
        "Content edit changes only exact Unicode source in one Undo transaction");
    QApplication::setActiveWindow(&window);window.canvas->setFocus();events();
    check(window.isActiveWindow()&&window.canvas->hasFocus(),"Undo flow has actual active Window and Canvas focus");
    QTest::keySequence(window.canvas,QKeySequence::Undo);events();
    check(session.document()==expected_vertical,"Actual Undo shortcut restores complete pre-content-edit source");
    QTest::keySequence(window.canvas,QKeySequence::Redo);events();
    check(session.document()==expected_edited,"Actual Redo shortcut restores exact editable Japanese vertical Text");
    const auto file=scratch.filePath("text-editing-flow.nect.json");window.host.save(file);
    window.host.open(file);events();check(session.document()==expected_edited,"Same Window native reopen preserves complete edited source");
    window.canvas->set_selection(id);events();
    direction=window.findChild<QComboBox*>("text-direction");edit=window.findChild<QPushButton*>("edit-text-content");
    check(direction&&direction->isEnabled()&&direction->currentIndex()==1&&edit&&edit->isEnabled(),
        "Reopened Japanese Text retains directly usable Writing and content controls");
    Window reopened(scratch.filePath("new-window-recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1100,750);reopened.show();reopened.host.open(file);events();
    check(reopened.host.session.document()==expected_edited,"New Window native reopen preserves exact editable source");
    std::cout<<"Text flow: actual Canvas create -> neutral Tool variant -> pointer Writing -> Japanese IME/Edit Apply -> shortcut Undo/Redo -> owned native save/same/new Window reopen; DPR="<<window.devicePixelRatio()<<"\n";
}
void pan_button_isolation(){
    QTemporaryDir scratch;check(scratch.isValid(),"Pan chord regression owns preferences, recovery and native file");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
    auto document=empty_document("text-pan-document","composition","board");
    document.compositions.front().artboards.front().width=640;document.compositions.front().artboards.front().height=480;
    Session fixture(document);auto fill=default_operation("rectangle-fill","nect.paint.fill");
    fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;fill.parameters.at("b").literal=0;
    fixture.apply({CreatePrimitive{"composition",{},"rectangle","Pan marker",default_primitive("rectangle-source","nect.shape.rectangle")},
        AddOperation{"rectangle",fill,0},Set{{"rectangle",{},"transform.tx"},320},Set{{"rectangle",{},"transform.ty"},240}},fixture.revision());
    auto& session=window.host.session;session=Session(fixture.document());window.host.edited();events();
    const auto marker=[&]{
        const auto image=window.canvas->grab().toImage();double sx=0,sy=0;int count=0;
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto color=image.pixelColor(x,y);
            if(color.red()>180&&color.green()<80&&color.blue()<80){sx+=x;sy+=y;++count;}}
        check(count>100,"Rendered source has an observable red marker");return QPointF(sx/count,sy/count)/image.devicePixelRatio();
    };
    const auto pointer=[&](QEvent::Type type,QPoint position,Qt::MouseButton button,Qt::MouseButtons held){
        QMouseEvent event(type,QPointF(position),QPointF(window.canvas->mapToGlobal(position)),button,held,Qt::NoModifier);
        QApplication::sendEvent(window.canvas,&event);events();check(event.isAccepted(),"Canvas accepts the exact multi-button input event");
    };
    for(bool vertical:{false,true}){
        window.canvas->fit_artboard();window.canvas->set_selection("rectangle");events();variant(window,vertical);
        const auto before=snapshot(session);const auto selection=window.canvas->selections();const auto zoom=window.canvas->zoom();
        auto* text=window.findChild<QToolButton*>("tool-text");const auto name=text->accessibleName();
        const auto origin=marker();const auto start=window.canvas->rect().center();
        pointer(QEvent::MouseButtonPress,start,Qt::MiddleButton,Qt::MiddleButton);
        pointer(QEvent::MouseMove,start+QPoint(20,12),Qt::NoButton,Qt::MiddleButton);
        check(window.canvas->cursor().shape()==Qt::ClosedHandCursor,"Middle press and movement own an active view pan");
        pointer(QEvent::MouseButtonPress,start+QPoint(20,12),Qt::LeftButton,Qt::MiddleButton|Qt::LeftButton);
        std::cout<<"Text pan chord: authored_equal="<<(snapshot(session)==before)<<" text_active="<<window.canvas->text_mode()<<'\n';
        unchanged(session,before,"Left press during middle pan cannot create Text or change authored history");
        pointer(QEvent::MouseButtonDblClick,start+QPoint(20,12),Qt::LeftButton,Qt::MiddleButton|Qt::LeftButton);
        pointer(QEvent::MouseButtonRelease,start+QPoint(20,12),Qt::LeftButton,Qt::MiddleButton);
        check(window.canvas->cursor().shape()==Qt::ClosedHandCursor,"Releasing the unrelated left button does not finish the middle pan");
        pointer(QEvent::MouseMove,start+QPoint(36,24),Qt::NoButton,Qt::MiddleButton);
        const auto delta=marker()-origin;
        check(std::abs(delta.x()-36)<0.6&&std::abs(delta.y()-24)<0.6,"The original middle gesture continues with the complete viewport displacement");
        pointer(QEvent::MouseButtonRelease,start+QPoint(36,24),Qt::MiddleButton,Qt::NoButton);
        unchanged(session,before,"Pan chord and release preserve complete Document/native/revision/history");
        check(!session.gesture_active()&&window.canvas->zoom()==zoom&&window.canvas->selections()==selection&&
            window.canvas->text_mode()&&text->isChecked()&&text->accessibleName()==name&&window.canvas->cursor().shape()==Qt::IBeamCursor,
            "Ending the owning middle button restores Text variant and exact selection without an authored gesture");
        // The same button ownership must hold for left-button temporary Hand.
        QApplication::setActiveWindow(&window);window.canvas->setFocus();events();QTest::keyPress(window.canvas,Qt::Key_Space);events();
        const auto temporary_origin=marker();
        pointer(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        pointer(QEvent::MouseMove,start+QPoint(10,8),Qt::NoButton,Qt::LeftButton);
        pointer(QEvent::MouseButtonPress,start+QPoint(10,8),Qt::MiddleButton,Qt::LeftButton|Qt::MiddleButton);
        pointer(QEvent::MouseButtonRelease,start+QPoint(10,8),Qt::MiddleButton,Qt::LeftButton);
        check(window.canvas->cursor().shape()==Qt::ClosedHandCursor,"Middle-button chord and release preserve the original temporary left pan");
        pointer(QEvent::MouseMove,start+QPoint(24,16),Qt::NoButton,Qt::LeftButton);
        const auto temporary_delta=marker()-temporary_origin;
        check(std::abs(temporary_delta.x()-24)<0.6&&std::abs(temporary_delta.y()-16)<0.6,"Temporary Hand keeps its original press coordinates across a button chord");
        pointer(QEvent::MouseButtonRelease,start+QPoint(24,16),Qt::LeftButton,Qt::NoButton);
        QTest::keyRelease(window.canvas,Qt::Key_Space);events();unchanged(session,before,"Temporary Hand chord remains authored-state neutral");
        check(window.canvas->cursor().shape()==Qt::IBeamCursor&&window.canvas->text_mode(),"Temporary Hand release restores persistent Text");
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,start+QPoint(50,40));events();
        const auto placed=session.document();const auto id=window.canvas->selected_object;
        check(placed.objects.at(id).text->direction==(vertical?"vertical":"horizontal")&&session.revision()==std::get<2>(before)+1,
            "After pan finishes an ordinary click still places the chosen Text variant in one revision");
        session.undo(session.revision());window.host.edited();events();check(session.document()==std::get<0>(before),"One Undo restores the complete pre-placement source");
        session.redo(session.revision());window.host.edited();events();check(session.document()==placed,"One Redo restores the complete placed Text");
        const auto file=scratch.filePath("pan-chord.nect.json");window.host.save(file);window.host.open(file);events();
        check(session.document()==placed&&window.canvas->text_mode(),"Native reopen retains editable source and persistent Text Tool after pan chord");
    }
    std::cout<<"text_pan_button_isolation: "<<checks<<" checks passed; DPR="<<window.devicePixelRatioF()<<'\n';
}
void double_click_isolation(){
    QTemporaryDir scratch;check(scratch.isValid(),"Text double-click owns preferences, recovery and native files");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
    auto document=empty_document("text-double-click-document","composition","board");
    document.compositions.front().artboards.front().width=640;document.compositions.front().artboards.front().height=480;
    Session fixture(document);fixture.apply({
        CreatePrimitive{"composition",{},"rectangle","Grouped artwork",default_primitive("rectangle-source","nect.shape.rectangle")},
        AddOperation{"rectangle",default_operation("rectangle-fill","nect.paint.fill"),0},
        Set{{"rectangle",{},"transform.tx"},320},Set{{"rectangle",{},"transform.ty"},240}},fixture.revision());
    document=fixture.document();Object group;group.id="group";group.name="Group";group.kind=Kind::group;group.children={"rectangle"};
    document.objects.emplace(group.id,group);document.compositions.front().roots={group.id};
    auto& session=window.host.session;
    for(bool vertical:{false,true}){
        session=Session(document);window.host.edited();events();window.canvas->set_selection("group");
        window.canvas->fit_artboard();window.canvas->set_snap_enabled(false);events();variant(window,vertical);
        check(window.canvas->drill_scope().empty(),"Text starts in the root scope over grouped artwork");
        const auto first=window.canvas->rect().center();
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,first);events();
        const auto created_id=window.canvas->selected_object;const auto created=session.document();
        check(created.objects.at(created_id).text&&created.objects.at(created_id).text->direction==(vertical?"vertical":"horizontal")&&
            created.compositions.front().roots.back()==created_id&&created.objects.at("group").children==std::vector<Id>{"rectangle"},
            "The first click places the chosen Text as a root without changing underlying Group children");
        const auto layout=evaluate_text_projection(created,created_id,window.canvas->evaluated_values());
        const QTransform view(window.canvas->zoom(),0,0,window.canvas->zoom(),window.canvas->width()/2.0-320*window.canvas->zoom(),
            window.canvas->height()/2.0-240*window.canvas->zoom());
        const auto text_bounds=view.mapRect(QRectF(layout.x,layout.y,std::max(1.0,layout.width),std::max(1.0,layout.height)));
        // A few pixels of movement can expose the underlying artwork on the
        // second press. Verify the actual hit as well as the projected bounds.
        std::optional<QPoint> second;
        const auto created_state=snapshot(session);
        click(window,"tool-selection");
        for(const auto offset:{QPoint(-3,-3),QPoint(3,-3),QPoint(-3,3),QPoint(3,3)}){
            const auto candidate=first+offset;if(text_bounds.contains(candidate))continue;
            window.canvas->set_selection(created_id);events();
            QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,candidate);events();
            if(window.canvas->selected_object=="group"&&window.canvas->drill_scope().empty()){second=candidate;break;}
        }
        check(second.has_value(),"A second click three pixels away hits grouped artwork outside the newly placed Text bounds");
        unchanged(session,created_state,"The hit probe preserves complete source/revision/history");
        window.canvas->set_selection(created_id);events();click(window,"tool-text");
        const auto before=snapshot(session);const auto selection=window.canvas->selections();const auto image=window.canvas->grab().toImage();
        auto* text=window.findChild<QToolButton*>("tool-text");const auto name=text->accessibleName();const auto zoom=window.canvas->zoom();
        QMouseEvent event(QEvent::MouseButtonDblClick,QPointF(*second),QPointF(window.canvas->mapToGlobal(*second)),
            Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(window.canvas,&event);events();QTest::mouseRelease(window.canvas,Qt::LeftButton,Qt::NoModifier,*second);events();
        std::cout<<name.toStdString()<<" second Text click: scope="<<window.canvas->drill_scope()<<" selected="<<window.canvas->selected_object
            <<" authored_equal="<<(snapshot(session)==before)<<'\n';
        check(window.canvas->drill_scope().empty()&&window.canvas->selections()==selection,
            "Text double-click cannot enter Selection Group drill or replace the placed Text selection");
        unchanged(session,before,"Text double-click preserves complete Document/native/revision/history");
        check(!session.gesture_active()&&window.canvas->text_mode()&&text->isChecked()&&text->accessibleName()==name&&
            window.canvas->cursor().shape()==Qt::IBeamCursor,"Text double-click retains the variant and placement cursor without an authored gesture");
        check(window.canvas->zoom()==zoom&&window.canvas->grab().toImage()==image,"Text double-click preserves rendered artwork and viewport");
        const auto next=first+QPoint(45,35);QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,next);events();
        const auto next_id=window.canvas->selected_object;const auto placed=session.document();
        check(placed.compositions.front().roots.back()==next_id&&placed.objects.at("group")==created.objects.at("group")&&
            placed.objects.at(next_id).text->direction==(vertical?"vertical":"horizontal")&&
            session.revision()==std::get<2>(before)+1&&session.history().states.size()==std::get<3>(before).states.size()+1,
            "Subsequent placement retains the intended root parent and variant in one transaction");
        session.undo(session.revision());window.host.edited();events();check(session.document()==created,"One Undo restores all pre-placement Text and grouped artwork");
        session.redo(session.revision());window.host.edited();events();check(session.document()==placed,"One Redo restores complete Text placement");
        const auto file=scratch.filePath("text-double-click.nect.json");window.host.save(file);window.host.open(file);events();
        check(session.document()==placed&&window.canvas->text_mode(),"Same Window native reopen retains editable Text and the persistent Tool");
        window.canvas->set_selection("group");click(window,"tool-selection");const auto selection_before=snapshot(session);
        QTest::mouseDClick(window.canvas,Qt::LeftButton,Qt::NoModifier,*second);events();
        check(window.canvas->drill_scope()=="group"&&window.canvas->selected_object=="rectangle",
            "Explicit Selection still enters the Group and selects its authored child");
        unchanged(session,selection_before,"Intentional Selection Group drill remains authored-state neutral");
        click(window,"tool-text");const auto scoped_before=snapshot(session);
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,next);events();
        check(session.document().objects.at("group").children.back()==window.canvas->selected_object&&
            session.document().objects.at(window.canvas->selected_object).text->direction==(vertical?"vertical":"horizontal"),
            "Text placement after intentional drill still uses the explicitly chosen Group parent");
        session.undo(session.revision());window.host.edited();events();check(session.document()==std::get<0>(scoped_before),"Scoped Text placement retains one complete Undo");
    }
    std::cout<<"text_double_click_isolation: "<<checks<<" checks passed; DPR="<<window.devicePixelRatioF()<<'\n';
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        if(app.arguments().contains("--keyboard-focus-help")){keyboard_focus_help();return 0;}
        if(app.arguments().contains("--double-click-isolation")){double_click_isolation();return 0;}
        if(app.arguments().contains("--pan-button-isolation")){pan_button_isolation();return 0;}
        if(app.arguments().contains("--cursor-continuity")){cursor_continuity();return 0;}
        if(app.arguments().contains("--key-isolation")){key_isolation();return 0;}
        if(app.arguments().contains("--editing-flow")){editing_flow();std::cout<<"text_editing_flow: "<<checks<<" checks passed\n";return 0;}
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
        auto& session=window.host.session;
        auto document=session.document();document.objects.clear();document.compositions.front().roots={"existing","driven"};
        Object first;first.id="existing";first.name="日本語 Text";first.kind=Kind::text;
        first.text=default_text("existing-source","日本語（ABC123）\n縦書き、句読点。");
        first.text->family="Yu Gothic";first.text->weight=600;first.text->italic=true;
        first.text->parameters.at("origin_x").literal=100;first.text->parameters.at("origin_y").literal=100;
        first.text->parameters.at("font_size").literal=28;first.stack={default_operation("existing-fill","nect.paint.fill")};
        auto second=first;second.id="driven";second.name="Driven Text";second.text->id="driven-source";
        second.transform[4].literal=300;
        second.stack.front().id="driven-fill";document.objects.emplace(first.id,first);document.objects.emplace(second.id,second);
        session=Session(document);window.host.edited();window.canvas->set_selection("existing");events();
        auto* structure=window.findChild<QDockWidget*>("structure");
        if(!structure)structure=window.findChild<QDockWidget*>("objects");
        auto* rail=window.findChild<QToolBar*>("tool-rail");check(rail&&rail->isVisible(),"Persistent dedicated Tool Rail exists");
        const auto rail_x=rail->mapTo(&window,QPoint{}).x();
        if(structure){structure->hide();events();check(rail->isVisible()&&rail->mapTo(&window,QPoint{}).x()==rail_x,"Structure collapse preserves Rail placement");structure->show();events();}
        const auto existing=snapshot(session);
        click(window,"tool-pen");
        bool cancel_opened=false;
        auto* cancel_button=window.findChild<QToolButton*>("tool-text");auto* cancel_menu=cancel_button->menu();
        QTimer::singleShot(900,cancel_menu,[&]{cancel_opened=cancel_menu->isVisible();QTest::keyClick(cancel_menu,Qt::Key_Escape);});
        QTest::mousePress(cancel_button,Qt::LeftButton);QTest::qWait(1000);QTest::mouseRelease(cancel_button,Qt::LeftButton);events();
        check(cancel_opened&&window.canvas->draw_mode()&&window.findChild<QToolButton*>("tool-pen")->isChecked(),"Escape closes variant flyout and preserves the previous active tool");
        unchanged(session,existing,"Canceling creation variant flyout preserves authored state/history");
        for(const auto* name:{"tool-pen","tool-anchor","tool-selection","tool-text"}){click(window,name);unchanged(session,existing,"Tool activation preserves full authored state/history");}
        variant(window,true);check(window.canvas->text_mode(),"Vertical variant activates actual Text placement mode");
        click(window,"tool-selection");click(window,"tool-text");unchanged(session,existing,"Ordinary Text click restores last variant without mutation");
        auto* text_button=window.findChild<QToolButton*>("tool-text");check(text_button->accessibleName().startsWith("Vertical"),"Ordinary click remembers Vertical creation variant");
        const auto original=session.document();const auto original_history=session.history();
        const auto position=window.canvas->rect().center();QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,position);events();
        const auto created=session.document();const auto created_id=window.canvas->selected_object;
        check(created.objects.size()==original.objects.size()+1&&session.revision()==1&&session.history().states.size()==original_history.states.size()+1,"One Canvas click creates one Text in one Undo transaction");
        check(created.objects.at(created_id).text->direction=="vertical"&&created.objects.at("existing")==original.objects.at("existing"),"Canvas creates Vertical Text and leaves existing Text byte-exact");
        check(window.canvas->text_mode()&&text_button->isChecked(),"Creation and selection refresh preserve active Text tool");
        session.undo(session.revision());window.host.edited();check(session.document()==original,"One Undo removes exactly the new Text");
        session.redo(session.revision());window.host.edited();check(session.document()==created,"One Redo restores identical editable Text");
        click(window,"tool-selection");window.canvas->set_selection("existing");events();
        auto* direction=window.findChild<QComboBox*>("text-direction");
        check(direction&&direction->isEnabled(),"Existing Text has directly enabled Writing control");
        auto* properties=window.findChild<QDockWidget*>("properties");properties->setMinimumWidth(300);properties->setMaximumWidth(300);properties->raise();events();
        auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");scroll->ensureWidgetVisible(direction);QTest::qWait(40);
        const auto before_direction=session.document();const auto direction_history=session.history().states.size();
        direction->setFocus();QTest::keyClick(direction,Qt::Key_Down);events();
        auto expected_source=*before_direction.objects.at("existing").text;expected_source.direction="vertical";
        check(session.document().objects.at("existing").text==expected_source,"Direct control changes only direction, preserving content/font/style/source identity");
        check(session.history().states.size()==direction_history+1,"Direct writing change has one Undo entry");
        const auto vertical_document=session.document();session.undo(session.revision());window.host.edited();check(session.document()==before_direction,"One Undo restores original horizontal Text");
        session.redo(session.revision());window.host.edited();check(session.document()==vertical_document,"One Redo restores vertical Text exactly");
        const auto file=scratch.filePath("rail-text.nect.json");window.host.save(file);
        Host reopened(scratch.filePath("reopen"));reopened.open(file);check(reopened.session.document()==vertical_document,"Native Host reopen preserves existing and created editable Text");
        if(const auto path=qEnvironmentVariable("NECT_RAIL_DOCUMENT");!path.isEmpty())window.host.save(path);
        if(const auto path=qEnvironmentVariable("NECT_RAIL_CAPTURE");!path.isEmpty()) {events();window.grab().save(path);}
        session.apply({LinkTextDirection{{"driven","","text.direction"},{"existing","","text.direction"},false}},session.revision());window.host.edited();window.canvas->set_selection("driven");events();
        direction=window.findChild<QComboBox*>("text-direction");check(direction&&!direction->isEnabled()&&direction->toolTip().contains("Unlink"),"Linked direction refuses direct editing with explicit reason");
        const auto linked=snapshot(session);variant(window,false);click(window,"tool-text");unchanged(session,linked,"Tool/variant activation never unlinks or converts selected driven Text");
        QTest::keyClick(window.canvas,Qt::Key_Escape);events();unchanged(session,linked,"Escape leaves Text tool without authoring");
        check(!window.canvas->text_mode()&&window.findChild<QToolButton*>("tool-selection")->isChecked(),"Escape synchronizes Selection state");
        click(window,"tool-text");window.canvas->set_selection({});events();const auto before_horizontal=session.document();
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,position);events();
        check(session.document().objects.size()==before_horizontal.objects.size()+1&&session.document().objects.at(window.canvas->selected_object).text->direction=="horizontal","Unselected Canvas creates remembered Horizontal variant");
        // A chosen world point checks placement through a rotated parent.
        auto scoped_doc=original;scoped_doc.objects.erase("driven");
        Object group;group.id="parent";group.name="Parent";group.kind=Kind::group;group.children={"existing"};
        group.transform={Scalar{0,{}},Scalar{1,{}},Scalar{-1,{}},Scalar{0,{}},Scalar{500,{}},Scalar{100,{}}};
        scoped_doc.objects.emplace(group.id,group);scoped_doc.compositions.front().roots={"parent"};
        auto& board=scoped_doc.compositions.front().artboards.front();board.x=0;board.y=0;board.width=640;board.height=480;
        Session scoped_session(scoped_doc);Canvas scoped(scoped_session);scoped.resize(740,580);scoped.show();events();scoped.fit_artboard();
        scoped.set_selection("existing");check(scoped.drill_scope()=="parent","Nested Text establishes its authored drill scope");
        scoped.set_text_mode(true,true);const auto scope_before=scoped_session.document();
        const QPoint target(qRound(scoped.width()/2.0+100*scoped.zoom()),qRound(scoped.height()/2.0+20*scoped.zoom()));
        QTest::mouseClick(&scoped,Qt::LeftButton,Qt::NoModifier,target);events();
        const auto& placed=scoped_session.document().objects.at(scoped.selected_object);
        const auto& coordinates=placed.text->parameters;const auto tolerance=1.0/scoped.zoom();
        check(std::abs(coordinates.at("origin_x").literal-160)<tolerance&&std::abs(coordinates.at("origin_y").literal-80)<tolerance,
            "Canvas placement resolves the clicked world point through rotated parent coordinates");
        const auto& children=scoped_session.document().objects.at("parent").children;
        check(children.size()==2&&children.back()==placed.id&&placed.text->direction=="vertical","Scoped creation retains structural parent and selected direction");
        scoped_session.undo(scoped_session.revision());check(scoped_session.document()==scope_before,"Scoped creation has one complete Undo");
        scoped_doc.objects.at("parent").transform[1].literal=0;scoped_doc.objects.at("parent").transform[3].literal=1;
        Session singular_session(scoped_doc);Canvas singular(singular_session);QString refusal;singular.error=[&](const QString& message){refusal=message;};
        singular.resize(740,580);singular.show();events();singular.fit_artboard();singular.set_selection("existing");singular.set_text_mode(true);
        check(singular.drill_scope()=="parent","Singular fixture retains the intended drill scope");const auto singular_before=snapshot(singular_session);
        QTest::mouseClick(&singular,Qt::LeftButton,Qt::NoModifier,singular.rect().center());events();
        unchanged(singular_session,singular_before,"Singular parent refuses creation without authored or history mutation");
        check(refusal.startsWith("SINGULAR_TRANSFORM"),"Singular placement gives an explicit refusal");
        std::cout<<"rail_text_entry_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
