#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>
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
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
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
