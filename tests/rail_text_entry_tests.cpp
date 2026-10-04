#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QMenu>
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
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
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
