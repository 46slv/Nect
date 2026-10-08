#include "window.hpp"
#include "visual_style.hpp"
#include "tool_rail.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCompleter>
#include <QCursor>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QInputMethodEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>
#include <QToolTip>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyleOptionSpinBox>
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
void writing_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Writing pointer owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("writing-pointer-document","composition","board");
    Object text;text.id="text";text.name="Writing target";text.kind=Kind::text;
    text.text=default_text("text-source","Keep Japanese 日本語 and style");
    document.objects.emplace(text.id,text);document.compositions.front().roots.push_back(text.id);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* direction=window.findChild<QComboBox*>("text-direction");QPointer<QComboBox> original_direction=direction;
    struct PointerProbe final:QObject {
        int presses=0,focus=0;
        bool eventFilter(QObject*,QEvent* event)override {
            if(event->type()==QEvent::MouseButtonPress)++presses;
            if(event->type()==QEvent::FocusIn)++focus;
            return false;
        }
    } pointer_probe;
    if(direction)direction->installEventFilter(&pointer_probe);
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&direction&&size,"Actual direct Writing and Font size controls exist");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain"){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),
        "Focused pending or unchanged Font size field is fully Session neutral");
    scroll->ensureWidgetVisible(direction);events();
    check(size->hasFocus()&&size->isModified()==(mode!="plain"),"Revealing Writing retains the focused field and its draft state");
    if(mode=="document") {
        auto incoming=document;incoming.id="incoming-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    const auto first_position=direction->mapTo(&window,direction->rect().center());
    const auto* hit=window.childAt(first_position);
    check(hit==direction,"The first Window pointer position resolves to the actual Writing control");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,first_position);events();
    if(mode=="invalid"||mode=="document") {
        check(snapshot(window.host.session)==snapshot(expected),"Rejected draft/context keeps the complete incoming Session");
        check(original_direction&&!original_direction->view()->isVisible(),"Rejected draft/context never opens a writable popup");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":"SESSION_CONFLICT"),
            "Rejected draft/context reports its explicit cause");
        std::cout<<"text_writing_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode!="plain")expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    std::cout<<"Writing "<<mode<<" first pointer: revision="<<window.host.session.revision()
        <<" expected="<<expected.revision()<<" size="<<window.host.session.document().objects.at("text").text->parameters.at("font_size").literal
        <<" original-control="<<bool(original_direction)<<" popup="<<(original_direction&&original_direction->view()->isVisible())
        <<" press="<<pointer_probe.presses<<" focus="<<pointer_probe.focus
        <<" status="<<window.statusBar()->currentMessage().toStdString()<<std::endl;
    check(snapshot(window.host.session)==snapshot(expected),"First Writing pointer completes exactly the ordinary Font size transaction");
    check(original_direction&&original_direction->view()->isVisible(),
        "First Writing pointer opens the direct popup without losing the gesture to scalar blur");
    check(pointer_probe.presses==1&&pointer_probe.focus==1,"The one Window press delivered actual control focus and one pointer event");
    if(mode=="cancel") {
        QTest::keyClick(original_direction->view(),Qt::Key_Escape);events();
        check(snapshot(window.host.session)==snapshot(expected),"Popup Escape retains only the independent scalar transaction");
        size=nullptr;
        for(auto* input:window.findChildren<QLineEdit*>()) {
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
        }
        check(size,"Popup cancel retains a usable scalar field");
        scroll->ensureWidgetVisible(size);events();size->setFocus();
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
        expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
        check(snapshot(window.host.session)==snapshot(expected),"After popup cancel the next scalar edit uses fresh canonical context");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected),"Next scalar Undo preserves the earlier independently committed draft");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,
            "Separate scalar Undo after popup cancel restores the original source");
        std::cout<<"text_writing_pending_pointer cancel: "<<checks<<" checks passed\n";return;
    }
    if(mode=="revision") {
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
    }
    const auto index=original_direction->model()->index(1,0);
    auto* viewport=original_direction->view()->viewport();
    // Qt protects the opening release for the double-click interval. A later
    // independent choice must not be mistaken for that initial release.
    QTest::qWait(QApplication::doubleClickInterval()+20);
    QTest::mouseClick(viewport,Qt::LeftButton,Qt::NoModifier,original_direction->view()->visualRect(index).center());events();
    std::cout<<"Writing "<<mode<<" choice: revision="<<window.host.session.revision()
        <<" direction="<<window.host.session.document().objects.at("text").text->direction
        <<" status="<<window.statusBar()->currentMessage().toStdString()<<std::endl;
    if(mode=="revision") {
        check(snapshot(window.host.session)==snapshot(expected),"Old Writing popup refuses the changed revision without partial authoring");
        check(window.statusBar()->currentMessage().contains("STALE_CONTEXT"),"Old popup explicitly reports the changed context");
        std::cout<<"text_writing_pending_pointer revision: "<<checks<<" checks passed\n";return;
    }
    auto changed=*expected.document().objects.at("text").text;changed.direction="vertical";
    expected.apply({UpdateText{"text",changed}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Writing commits only direction with a separate canonical Undo entry");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document().objects.at("text").text->direction=="horizontal",
        "One Writing Undo retains independently committed Font size");
    if(mode=="plain"){
        check(expected.document()==document,"Ordinary Writing remains one Undo without a scalar draft");
        std::cout<<"text_writing_pending_pointer plain: "<<checks<<" checks passed\n";return;
    }
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,
        "Separate scalar Undo restores the complete original source");
    std::cout<<"text_writing_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void content_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Content pointer owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("content-pointer-document","composition","board");
    Object text;text.id="text";text.name="Content target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    document.objects.emplace(text.id,text);document.compositions.front().roots.push_back(text.id);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* edit=window.findChild<QPushButton*>("edit-text-content");
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&edit&&size,"Actual Edit text action and Font size field exist");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain"){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state is focused and neutral before Edit text");
    scroll->ensureWidgetVisible(edit);events();
    const auto point=edit->mapTo(&window,edit->rect().center());
    check(window.childAt(point)==edit,"First Window pointer targets the actual Edit text action");
    if(mode=="revision"){
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
    }else if(mode=="document"){
        auto incoming=document;incoming.id="incoming-document";window.host.session=Session(incoming);expected=window.host.session;
    }else if(mode!="invalid"&&mode!="plain")expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    const bool refusal=mode=="invalid"||mode=="revision"||mode=="document";
    bool opened=false,neutral=false,applied=false;
    QTimer::singleShot(1000,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-editor-dialog");
        if(!dialog)return;opened=dialog->isVisible();
        auto* editor=dialog->findChild<QPlainTextEdit*>("text-content-editor");
        auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!editor||!buttons){dialog->reject();return;}
        QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"Changed content");
        neutral=snapshot(window.host.session)==snapshot(expected)&&editor->toPlainText()=="Changed content";
        if(mode=="apply"){
            QTest::mouseClick(buttons->button(QDialogButtonBox::Apply),Qt::LeftButton);applied=!dialog->isVisible();
            if(dialog->isVisible())dialog->reject();
        }else dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(1100);events();
    if(refusal){
        check(!opened&&snapshot(window.host.session)==snapshot(expected),"Invalid draft/incoming context refuses dialog entry without partial source mutation");
        const auto* cause=mode=="invalid"?"INVALID_VALUE":mode=="revision"?"REVISION_CONFLICT":"SESSION_CONFLICT";
        check(window.statusBar()->currentMessage().contains(cause),"Refused content entry reports the exact missing validity/context");
        std::cout<<"text_content_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode=="apply"){
        auto changed=*expected.document().objects.at("text").text;changed.content="Changed content";
        expected.apply({UpdateText{"text",changed}},expected.revision());
        check(opened&&neutral&&applied&&snapshot(window.host.session)==snapshot(expected),"First pointer opens current source and Apply is a separate canonical content transaction");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected),"Content Undo preserves independently committed Font size");
    }
    check(snapshot(window.host.session)==snapshot(expected),"First Edit text pointer commits exactly the ordinary scalar transaction");
    check(opened,"First Edit text pointer opens its source dialog without a second click");
    check(neutral&&snapshot(window.host.session)==snapshot(expected),"Owned source dialog draft/Cancel preserves complete post-scalar Session");
    if(mode!="plain"){
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Independent scalar Undo restores the entire original source");
    }
    std::cout<<"text_content_pending_pointer "<<mode<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void family_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Family popup owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("family-pointer-document","composition","board");
    Object text;text.id="text";text.name="Family target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    document.objects.emplace(text.id,text);document.compositions.front().roots.push_back(text.id);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* family=window.findChild<QComboBox*>("text-family");QPointer<QComboBox> original_family=family;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&family&&size,"Actual family popup and Font size controls exist");
    check(QTest::qWaitFor([&]{return family->completer()&&family->completer()->model()->rowCount()>0;},3000),
        "Existing family completion catalogue is available; no font repertoire claim");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain") {QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state stays neutral before family arrow");
    scroll->ensureWidgetVisible(family);events();
    if(mode=="revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="document") {
        auto incoming=document;incoming.id="incoming-family-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    const auto arrow=family->mapTo(&window,QPoint(family->width()-8,family->height()/2));
    check(window.childAt(arrow)==family,"Actual Window arrow position hits the family control");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,arrow);events();
    if(mode=="invalid"||mode=="revision"||mode=="document") {
        check(snapshot(window.host.session)==snapshot(expected),"Rejected family entry keeps complete incoming source and history");
        check(original_family&&!original_family->view()->isVisible(),"Rejected draft/context does not open the family popup");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),
            "Rejected family entry reports its exact cause");
        std::cout<<"text_family_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode!="plain")expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"First family arrow commits only the independent scalar transaction");
    check(original_family&&original_family->view()->isVisible(),"First family arrow opens the existing catalogue without losing the pointer gesture");
    QTest::keyClick(original_family->view(),Qt::Key_Escape);events();
    check(snapshot(window.host.session)==snapshot(expected),"Family popup Escape does not choose a font or author any extra state");
    if(mode=="plain") {
        check(expected.document()==document,"No-draft popup and Cancel leave the exact original source/history");
        std::cout<<"text_family_pending_pointer plain: "<<checks<<" checks passed\n";return;
    }
    size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(size,"Popup Cancel leaves a usable current scalar field");
    scroll->ensureWidgetVisible(size);events();size->setFocus();
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after family Cancel uses fresh canonical context");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo preserves the independently committed pending size");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo restores the complete original source");
    std::cout<<"text_family_pending_pointer cancel: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void weight_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Weight step owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("weight-pointer-document","composition","board");
    Object text;text.id="text";text.name="Weight target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    document.objects.emplace(text.id,text);document.compositions.front().roots.push_back(text.id);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* weight=window.findChild<QSpinBox*>("text-weight");QPointer<QSpinBox> original_weight=weight;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&weight&&size,"Actual weight step and Font size controls exist");
    const auto stepped=weight->value()+weight->singleStep();
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain") {QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state remains neutral before weight step");
    scroll->ensureWidgetVisible(weight);events();
    if(mode=="revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="document") {
        auto incoming=document;incoming.id="incoming-weight-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    QStyleOptionSpinBox option;option.initFrom(weight);option.subControls=QStyle::SC_All;
    const auto up=weight->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,weight);
    const auto position=weight->mapTo(&window,up.center());
    check(!up.isEmpty()&&window.childAt(position)==weight,"Actual Window pointer resolves to the existing weight up step");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode=="revision"||mode=="document") {
        check(snapshot(window.host.session)==snapshot(expected),"Rejected weight entry preserves complete incoming source/history");
        check(original_weight&&original_weight->value()==stepped-original_weight->singleStep(),"Rejected draft/context does not step the weight draft");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),
            "Rejected weight entry reports its exact cause");
        std::cout<<"text_weight_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode!="plain")expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Weight step first commits only the independent size command");
    check(original_weight&&original_weight->value()==stepped,"First weight step survives scalar blur and changes the existing draft once");
    if(mode=="late-revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    QTest::keyClick(original_weight,Qt::Key_Return);events();
    if(mode=="late-revision") {
        check(snapshot(window.host.session)==snapshot(expected),"Prepared weight draft refuses an incoming revision without partial authoring");
        check(window.statusBar()->currentMessage().contains("REVISION_CONFLICT"),"Prepared weight finish reports its changed context");
        std::cout<<"text_weight_pending_pointer late-revision: "<<checks<<" checks passed\n";return;
    }
    auto changed=*expected.document().objects.at("text").text;changed.weight=static_cast<unsigned>(stepped);
    expected.apply({UpdateText{"text",changed}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Finishing the weight step uses the existing separate canonical command");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One weight Undo preserves the independently committed size");
    if(mode=="plain") {
        check(expected.document()==document,"No-draft weight step remains one independent Undo");
        std::cout<<"text_weight_pending_pointer plain: "<<checks<<" checks passed\n";return;
    }
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original source");
    std::cout<<"text_weight_pending_pointer valid: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void italic_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Italic pointer owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("italic-pointer-document","composition","board");
    Object text;text.id="text";text.name="Italic target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    document.objects.emplace(text.id,text);document.compositions.front().roots.push_back(text.id);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* italic=window.findChild<QCheckBox*>("text-italic");
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&italic&&size&&!italic->isChecked(),"Actual Font size and existing non-italic checkbox are available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain") {QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state stays neutral before Italic pointer");
    scroll->ensureWidgetVisible(italic);events();
    if(mode=="revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="document") {
        auto incoming=document;incoming.id="incoming-italic-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    const auto position=italic->mapTo(&window,QPoint(8,italic->height()/2));
    check(window.childAt(position)==italic,"Actual Window pointer resolves to the existing checkbox indicator");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode=="revision"||mode=="document") {
        check(snapshot(window.host.session)==snapshot(expected),"Rejected Italic click preserves complete incoming source/history");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),
            "Rejected Italic click reports its exact cause");
        std::cout<<"text_italic_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode!="plain")expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    auto changed=*expected.document().objects.at("text").text;changed.italic=true;
    expected.apply({UpdateText{"text",changed}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"First Italic click authors exactly scalar plus separate italic command");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Italic Undo preserves the independent size commit");
    if(mode=="plain") {
        check(expected.document()==document,"No-draft Italic click retains one independent Undo");
        std::cout<<"text_italic_pending_pointer plain: "<<checks<<" checks passed\n";return;
    }
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo restores exact original source");
    std::cout<<"text_italic_pending_pointer valid: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void structure_pending_pointer(){
    QTemporaryDir scratch;check(scratch.isValid(),"Structure pending-pointer owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("structure-pointer-document","composition","board");
    Object text;text.id="text";text.name="First Text";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Second Text";other.kind=Kind::text;
    other.text=default_text("other-source","Select exact other object");
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* tree=window.findChild<QDockWidget*>("structure")->findChild<QTreeWidget*>();
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&tree&&size,"Existing Structure and Text scalar field are reachable");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"64");events();
    check(size->hasFocus()&&size->isModified()&&snapshot(window.host.session)==snapshot(expected),"Pending size is source/history neutral before Structure pointer");
    QTreeWidgetItem* row=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)
        if((*i)->data(0,Qt::UserRole).toString()=="other"&&(*i)->data(0,Qt::UserRole+1).toString().isEmpty()){row=*i;break;}
    check(row,"Structure target resolves to the exact whole stable object ID");
    tree->scrollToItem(row);events();
    const auto position=tree->viewport()->mapTo(&window,tree->visualItemRect(row).center());
    check(window.childAt(position)==tree->viewport(),"Actual Window pointer hits the existing Structure viewport target row");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"First Structure click authors exactly the independent scalar and no selection state");
    check(window.canvas->selected_object=="other"&&window.canvas->selected_point.empty(),"First Structure pointer selects the exact other whole object");
    bool target_properties=false;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="other"&&ref.value("field").toString()=="text.font_size")target_properties=true;
    }
    check(target_properties,"First Structure pointer exposes the selected object's actual Properties");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Single scalar Undo restores full original source/history across selection");
    std::cout<<"text_structure_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void family_drive_pending_pointer(const std::string& mode="popup",bool weight=false,bool italic=false){
    const bool link=mode!="popup";
    const std::string source_field=italic?"text.italic":weight?"text.weight":"text.family";
    const char* route=italic?"text_italic_link_pending_pointer ":weight?"text_weight_link_pending_pointer ":"text_family_link_pending_pointer ";
    QTemporaryDir scratch;check(scratch.isValid(),"Family Drive menu owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("family-drive-pointer-document","composition","board");
    Object text;text.id="text";text.name="Family Drive target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing source";other.kind=Kind::text;
    other.text=default_text("other-source","Source choice stays untouched");
    if(weight)other.text->weight=700;
    if(italic)other.text->italic=true;
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* drive=window.findChild<QToolButton*>(italic?"text-italic-driver":weight?"text-weight-driver":"text-family-driver");
    QPointer<QMenu> popup=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&popup&&size,"Existing family Drive menu and scalar are available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain") {QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state remains neutral before family Drive pointer");
    scroll->ensureWidgetVisible(drive);events();
    if(mode=="entry-revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="entry-document") {
        auto incoming=document;incoming.id="incoming-entry-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits the existing family Drive button");
    bool opened=false,neutral=false;
    const bool rejected_entry=mode=="invalid"||mode=="entry-revision"||mode=="entry-document";
    if(mode!="plain"&&!rejected_entry)expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    const auto popup_delay=link?QApplication::doubleClickInterval()+20:250;
    QTimer::singleShot(popup_delay,&window,[&]{
        opened=popup&&popup->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(opened) {
            if(link)QTest::mouseClick(popup,Qt::LeftButton,Qt::NoModifier,popup->actionGeometry(popup->actions().front()).center());
            else QTest::keyClick(popup,Qt::Key_Escape);
        }
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(popup_delay+50);events();
    check(snapshot(window.host.session)==snapshot(expected),"Family Drive pointer commits only the independent size command");
    if(rejected_entry) {
        check(!opened,"Invalid or changed entry context does not open a writable source menu");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),
            "Rejected source entry reports its exact cause");
        std::cout<<route<<mode<<": "<<checks<<" checks passed\n";return;
    }
    std::cout<<"Family menu "<<mode<<" opened="<<opened<<" neutral="<<neutral<<" popup-exists="<<bool(popup)
        <<" revision="<<window.host.session.revision()<<" expected="<<expected.revision()
        <<" status="<<window.statusBar()->currentMessage().toStdString()<<std::endl;
    check(opened&&neutral,"First family Drive pointer opens its existing menu with source-neutral popup state");
    if(link) {
        QPointer<QDialog> picker=window.findChild<QDialog*>("text-source-picker");
        auto* list=picker?picker->findChild<QListWidget*>("text-source-picker-list"):nullptr;
        auto* buttons=picker?picker->findChild<QDialogButtonBox*>():nullptr;
        check(picker&&picker->isVisible()&&list&&buttons,"Existing first Drive action opens the actual source chooser");
        QListWidgetItem* source=nullptr;
        for(int i=0;i<list->count();++i) {
            const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();
            if(ref.value("object").toString()=="other"&&ref.value("field").toString()==QString::fromStdString(source_field))source=list->item(i);
        }
        check(source&&!source->isHidden(),"Existing chooser exposes the exact stable same-property source Ref");
        QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,list->viewport()->mapTo(picker,list->visualItemRect(source).center()));events();
        check(snapshot(window.host.session)==snapshot(expected),"Source selection preview is fully authored-state neutral");
        if(mode=="cancel") {
            auto* cancel=buttons->button(QDialogButtonBox::Cancel);
            QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,cancel->mapTo(picker,cancel->rect().center()));events();
            check(snapshot(window.host.session)==snapshot(expected),"Chooser Cancel retains only the independent size command");
            check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Chooser Cancel restores the exact original target selection");
            size=nullptr;
            for(auto* input:window.findChildren<QLineEdit*>()) {
                const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
                if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
            }
            check(size,"Chooser Cancel exposes a current scalar field");
            scroll->ensureWidgetVisible(size);events();size->setFocus();
            QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
            expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after chooser Cancel uses current canonical context");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
            check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo retains the independent initial size");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
            check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo after chooser Cancel restores exact original source");
            std::cout<<route<<"cancel: "<<checks<<" checks passed\n";return;
        }
        if(mode=="apply-revision") {
            window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
            expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        }
        if(mode=="apply-document") {
            auto incoming=document;incoming.id="incoming-apply-document";
            Session replacement(incoming);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Incoming chooser Document has the same canonical revision");
            window.host.session=replacement;expected=replacement;
        }
        if(mode=="apply-session")window.host.session_id="incoming-source-session";
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(picker,apply->rect().center()));events();
        if(mode=="apply-revision"||mode=="apply-document"||mode=="apply-session") {
            check(snapshot(window.host.session)==snapshot(expected),"Changed chooser context refuses Link without partial authored changes");
            check(picker&&picker->findChild<QLabel*>("text-source-picker-status")->text().contains(mode=="apply-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),
                "Changed chooser reports its exact bound-context cause");
            QTest::keyClick(picker,Qt::Key_Escape);events();
            check(snapshot(window.host.session)==snapshot(expected),"Closing the stale chooser preserves the complete incoming Session");
            std::cout<<route<<mode<<": "<<checks<<" checks passed\n";return;
        }
        if(italic)expected.apply({LinkTextItalic{{"text","","text.italic"},{"other","","text.italic"},false}},expected.revision());
        else if(weight)expected.apply({LinkTextWeight{{"text","","text.weight"},{"other","","text.weight"},false}},expected.revision());
        else expected.apply({LinkTextFamily{{"text","","text.family"},{"other","","text.family"},false}},expected.revision());
        std::cout<<(italic?"Italic":weight?"Weight":"Family")<<" Link actual-revision="<<window.host.session.revision()<<" expected="<<expected.revision()
            <<" status="<<(picker?picker->findChild<QLabel*>("text-source-picker-status")->text().toStdString():"closed")<<std::endl;
        check(snapshot(window.host.session)==snapshot(expected),"First existing family Link uses the independently committed scalar revision");
        check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Successful Link restores the exact target selection");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected),"One source Link Undo preserves the independently committed size");
        if(mode=="plain") {
            check(expected.document()==document,"No-draft source Link remains one independent Undo");
            std::cout<<route<<"plain: "<<checks<<" checks passed\n";return;
        }
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo after Link restores exact original source");
        std::cout<<route<<"valid: "<<checks<<" checks passed; Qt Window pointer route\n";return;
    }
    check(!popup||!popup->isVisible(),"Existing family Drive Escape closes without choosing a source");
    size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(size,"Drive Escape leaves a current usable Font size field");
    scroll->ensureWidgetVisible(size);events();size->setFocus();
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after Drive Escape uses fresh canonical context");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo preserves the initial independent size commit");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo after Drive Escape restores exact original source");
    std::cout<<"text_family_drive_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void alignment_link_pending_pointer(const std::string& mode){
    const bool link=true;
    const std::string source_field="text.alignment";
    const char* route="text_alignment_link_pending_pointer ";
    QTemporaryDir scratch;check(scratch.isValid(),"Alignment Drive menu owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("alignment-drive-pointer-document","composition","board");
    Object text;text.id="text";text.name="Alignment Drive target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing source";other.kind=Kind::text;
    other.text=default_text("other-source","Source choice stays untouched");
    other.text->alignment="center";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* drive=window.findChild<QToolButton*>("text-alignment-driver");
    QPointer<QMenu> popup=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&popup&&size,"Existing alignment Drive menu and scalar are available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain") {QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state remains neutral before alignment Drive pointer");
    scroll->ensureWidgetVisible(drive);events();
    if(mode=="entry-revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="entry-document") {
        auto incoming=document;incoming.id="incoming-entry-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits the existing alignment Drive button");
    bool opened=false,neutral=false;
    const bool rejected_entry=mode=="invalid"||mode=="entry-revision"||mode=="entry-document";
    if(mode!="plain"&&!rejected_entry)expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    const auto popup_delay=link?QApplication::doubleClickInterval()+20:250;
    QTimer::singleShot(popup_delay,&window,[&]{
        opened=popup&&popup->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(opened) {
            if(link)QTest::mouseClick(popup,Qt::LeftButton,Qt::NoModifier,popup->actionGeometry(popup->actions().at(1)).center());
            else QTest::keyClick(popup,Qt::Key_Escape);
        }
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(popup_delay+50);events();
    check(snapshot(window.host.session)==snapshot(expected),"Alignment Drive pointer commits only the independent size command");
    if(rejected_entry) {
        check(!opened,"Invalid or changed entry context does not open a writable source menu");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),
            "Rejected source entry reports its exact cause");
        std::cout<<route<<mode<<": "<<checks<<" checks passed\n";return;
    }
    std::cout<<"Alignment menu "<<mode<<" opened="<<opened<<" neutral="<<neutral<<" popup-exists="<<bool(popup)
        <<" revision="<<window.host.session.revision()<<" expected="<<expected.revision()
        <<" status="<<window.statusBar()->currentMessage().toStdString()<<std::endl;
    check(opened&&neutral,"First alignment Drive pointer opens its existing menu with source-neutral popup state");
    if(link) {
        QPointer<QDialog> picker=window.findChild<QDialog*>("text-source-picker");
        auto* list=picker?picker->findChild<QListWidget*>("text-source-picker-list"):nullptr;
        auto* buttons=picker?picker->findChild<QDialogButtonBox*>():nullptr;
        check(picker&&picker->isVisible()&&list&&buttons,"Existing first Drive action opens the actual source chooser");
        QListWidgetItem* source=nullptr;
        for(int i=0;i<list->count();++i) {
            const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();
            if(ref.value("object").toString()=="other"&&ref.value("field").toString()==QString::fromStdString(source_field))source=list->item(i);
        }
        check(source&&!source->isHidden(),"Existing chooser exposes the exact stable same-property source Ref");
        QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,list->viewport()->mapTo(picker,list->visualItemRect(source).center()));events();
        check(snapshot(window.host.session)==snapshot(expected),"Source selection preview is fully authored-state neutral");
        if(mode=="cancel") {
            auto* cancel=buttons->button(QDialogButtonBox::Cancel);
            QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,cancel->mapTo(picker,cancel->rect().center()));events();
            check(snapshot(window.host.session)==snapshot(expected),"Chooser Cancel retains only the independent size command");
            check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Chooser Cancel restores the exact original target selection");
            size=nullptr;
            for(auto* input:window.findChildren<QLineEdit*>()) {
                const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
                if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
            }
            check(size,"Chooser Cancel exposes a current scalar field");
            scroll->ensureWidgetVisible(size);events();size->setFocus();
            QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
            expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after chooser Cancel uses current canonical context");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
            check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo retains the independent initial size");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
            check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo after chooser Cancel restores exact original source");
            std::cout<<route<<"cancel: "<<checks<<" checks passed\n";return;
        }
        if(mode=="apply-revision") {
            window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
            expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        }
        if(mode=="apply-document") {
            auto incoming=document;incoming.id="incoming-apply-document";
            Session replacement(incoming);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Incoming chooser Document has the same canonical revision");
            window.host.session=replacement;expected=replacement;
        }
        if(mode=="apply-session")window.host.session_id="incoming-source-session";
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(picker,apply->rect().center()));events();
        if(mode=="apply-revision"||mode=="apply-document"||mode=="apply-session") {
            check(snapshot(window.host.session)==snapshot(expected),"Changed chooser context refuses Link without partial authored changes");
            check(picker&&picker->findChild<QLabel*>("text-source-picker-status")->text().contains(mode=="apply-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),
                "Changed chooser reports its exact bound-context cause");
            QTest::keyClick(picker,Qt::Key_Escape);events();
            check(snapshot(window.host.session)==snapshot(expected),"Closing the stale chooser preserves the complete incoming Session");
            std::cout<<route<<mode<<": "<<checks<<" checks passed\n";return;
        }
        expected.apply({LinkTextAlignment{{"text","","text.alignment"},{"other","","text.alignment"},false}},expected.revision());
        std::cout<<"Alignment"<<" Link actual-revision="<<window.host.session.revision()<<" expected="<<expected.revision()
            <<" status="<<(picker?picker->findChild<QLabel*>("text-source-picker-status")->text().toStdString():"closed")<<std::endl;
        check(snapshot(window.host.session)==snapshot(expected),"First existing alignment Link uses the independently committed scalar revision");
        check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Successful Link restores the exact target selection");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected),"One source Link Undo preserves the independently committed size");
        if(mode=="plain") {
            check(expected.document()==document,"No-draft source Link remains one independent Undo");
            std::cout<<route<<"plain: "<<checks<<" checks passed\n";return;
        }
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo after Link restores exact original source");
        std::cout<<route<<"valid: "<<checks<<" checks passed; Qt Window pointer route\n";return;
    }
    check(!popup||!popup->isVisible(),"Existing alignment Drive Escape closes without choosing a source");
    size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(size,"Drive Escape leaves a current usable Font size field");
    scroll->ensureWidgetVisible(size);events();size->setFocus();
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after Drive Escape uses fresh canonical context");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo preserves the initial independent size commit");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo after Drive Escape restores exact original source");
    std::cout<<"text_alignment_drive_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void locale_link_pending_pointer(const std::string& mode){
    const bool link=true;
    const std::string source_field="text.locale";
    const char* route="text_locale_link_pending_pointer ";
    QTemporaryDir scratch;check(scratch.isValid(),"Locale Drive menu owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("locale-drive-pointer-document","composition","board");
    Object text;text.id="text";text.name="Locale Drive target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing source";other.kind=Kind::text;
    other.text=default_text("other-source","Source choice stays untouched");
    other.text->locale="en-US";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* drive=window.findChild<QToolButton*>("text-locale-driver");
    QPointer<QMenu> popup=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&popup&&size,"Existing locale Drive menu and scalar are available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain") {QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Font size draft state remains neutral before locale Drive pointer");
    scroll->ensureWidgetVisible(drive);events();
    if(mode=="entry-revision") {
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="entry-document") {
        auto incoming=document;incoming.id="incoming-entry-document";
        window.host.session=Session(incoming);expected=window.host.session;
    }
    const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits the existing locale Drive button");
    bool opened=false,neutral=false;
    const bool rejected_entry=mode=="invalid"||mode=="entry-revision"||mode=="entry-document";
    if(mode!="plain"&&!rejected_entry)expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    const auto popup_delay=link?QApplication::doubleClickInterval()+20:250;
    QTimer::singleShot(popup_delay,&window,[&]{
        opened=popup&&popup->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(opened) {
            if(link)QTest::mouseClick(popup,Qt::LeftButton,Qt::NoModifier,popup->actionGeometry(popup->actions().at(1)).center());
            else QTest::keyClick(popup,Qt::Key_Escape);
        }
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(popup_delay+50);events();
    check(snapshot(window.host.session)==snapshot(expected),"Locale Drive pointer commits only the independent size command");
    if(rejected_entry) {
        check(!opened,"Invalid or changed entry context does not open a writable source menu");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),
            "Rejected source entry reports its exact cause");
        std::cout<<route<<mode<<": "<<checks<<" checks passed\n";return;
    }
    std::cout<<"Locale menu "<<mode<<" opened="<<opened<<" neutral="<<neutral<<" popup-exists="<<bool(popup)
        <<" revision="<<window.host.session.revision()<<" expected="<<expected.revision()
        <<" status="<<window.statusBar()->currentMessage().toStdString()<<std::endl;
    check(opened&&neutral,"First locale Drive pointer opens its existing menu with source-neutral popup state");
    if(link) {
        QPointer<QDialog> picker=window.findChild<QDialog*>("text-source-picker");
        auto* list=picker?picker->findChild<QListWidget*>("text-source-picker-list"):nullptr;
        auto* buttons=picker?picker->findChild<QDialogButtonBox*>():nullptr;
        check(picker&&picker->isVisible()&&list&&buttons,"Existing first Drive action opens the actual source chooser");
        QListWidgetItem* source=nullptr;
        for(int i=0;i<list->count();++i) {
            const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();
            if(ref.value("object").toString()=="other"&&ref.value("field").toString()==QString::fromStdString(source_field))source=list->item(i);
        }
        check(source&&!source->isHidden(),"Existing chooser exposes the exact stable same-property source Ref");
        QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,list->viewport()->mapTo(picker,list->visualItemRect(source).center()));events();
        check(snapshot(window.host.session)==snapshot(expected),"Source selection preview is fully authored-state neutral");
        if(mode=="cancel") {
            auto* cancel=buttons->button(QDialogButtonBox::Cancel);
            QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,cancel->mapTo(picker,cancel->rect().center()));events();
            check(snapshot(window.host.session)==snapshot(expected),"Chooser Cancel retains only the independent size command");
            check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Chooser Cancel restores the exact original target selection");
            size=nullptr;
            for(auto* input:window.findChildren<QLineEdit*>()) {
                const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
                if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
            }
            check(size,"Chooser Cancel exposes a current scalar field");
            scroll->ensureWidgetVisible(size);events();size->setFocus();
            QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
            expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after chooser Cancel uses current canonical context");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
            check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo retains the independent initial size");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
            check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo after chooser Cancel restores exact original source");
            std::cout<<route<<"cancel: "<<checks<<" checks passed\n";return;
        }
        if(mode=="apply-revision") {
            window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
            expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        }
        if(mode=="apply-document") {
            auto incoming=document;incoming.id="incoming-apply-document";
            Session replacement(incoming);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Incoming chooser Document has the same canonical revision");
            window.host.session=replacement;expected=replacement;
        }
        if(mode=="apply-session")window.host.session_id="incoming-source-session";
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(picker->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(picker,apply->rect().center()));events();
        if(mode=="apply-revision"||mode=="apply-document"||mode=="apply-session") {
            check(snapshot(window.host.session)==snapshot(expected),"Changed chooser context refuses Link without partial authored changes");
            check(picker&&picker->findChild<QLabel*>("text-source-picker-status")->text().contains(mode=="apply-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),
                "Changed chooser reports its exact bound-context cause");
            QTest::keyClick(picker,Qt::Key_Escape);events();
            check(snapshot(window.host.session)==snapshot(expected),"Closing the stale chooser preserves the complete incoming Session");
            std::cout<<route<<mode<<": "<<checks<<" checks passed\n";return;
        }
        expected.apply({LinkTextLocale{{"text","","text.locale"},{"other","","text.locale"},false}},expected.revision());
        std::cout<<"Locale"<<" Link actual-revision="<<window.host.session.revision()<<" expected="<<expected.revision()
            <<" status="<<(picker?picker->findChild<QLabel*>("text-source-picker-status")->text().toStdString():"closed")<<std::endl;
        check(snapshot(window.host.session)==snapshot(expected),"First existing locale Link uses the independently committed scalar revision");
        check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Successful Link restores the exact target selection");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected),"One source Link Undo preserves the independently committed size");
        if(mode=="plain") {
            check(expected.document()==document,"No-draft source Link remains one independent Undo");
            std::cout<<route<<"plain: "<<checks<<" checks passed\n";return;
        }
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo after Link restores exact original source");
        std::cout<<route<<"valid: "<<checks<<" checks passed; Qt Window pointer route\n";return;
    }
    check(!popup||!popup->isVisible(),"Existing locale Drive Escape closes without choosing a source");
    size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(size,"Drive Escape leaves a current usable Font size field");
    scroll->ensureWidgetVisible(size);events();size->setFocus();
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Scalar edit after Drive Escape uses fresh canonical context");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"Fresh scalar Undo preserves the initial independent size commit");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo after Drive Escape restores exact original source");
    std::cout<<"text_locale_drive_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";
}
void family_unlink_pending_pointer(bool weight=false,bool expression=false,bool italic=false){
    QTemporaryDir scratch;check(scratch.isValid(),"Family Unlink owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("family-unlink-pointer-document","composition","board");
    Object text;text.id="text";text.name="Linked family target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing source";other.kind=Kind::text;
    other.text=default_text("other-source","Retain source");
    if(italic)other.text->italic=true;
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    // Existing linked source is fixture intake; do not replay closed Link UI.
    Session fixture(document);
    if(!expression) {
        if(italic)fixture.apply({LinkTextItalic{{"text","","text.italic"},{"other","","text.italic"},false}},fixture.revision());
        else if(weight)fixture.apply({LinkTextWeight{{"text","","text.weight"},{"other","","text.weight"},false}},fixture.revision());
        else fixture.apply({LinkTextFamily{{"text","","text.family"},{"other","","text.family"},false}},fixture.revision());
    }
    document=fixture.document();window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* drive=window.findChild<QToolButton*>(italic?"text-italic-driver":weight?"text-weight-driver":"text-family-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()) {
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&menu&&size,"Existing linked-family driver and scalar are available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");events();
    check(size->hasFocus()&&size->isModified()&&snapshot(window.host.session)==snapshot(expected),"Pending size remains neutral before linked-family driver");
    scroll->ensureWidgetVisible(drive);events();
    const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits the linked family driver");
    bool menu_open=false,dialog_open=false,unlink_clicked=false;
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        menu_open=menu&&menu->isVisible();
        if(menu_open)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at((weight||italic)&&!expression?2:1)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        if((weight||italic)&&!expression)return;
        QDialog* dialog=italic?qobject_cast<QInputDialog*>(QApplication::activeModalWidget()):window.findChild<QDialog*>(expression?"text-weight-expression-dialog":"text-family-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!buttons){dialog->reject();return;}
        if(italic) {
            auto* editor=dialog->findChild<QLineEdit*>();
            if(!editor){dialog->reject();return;}
            editor->setFocus();QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"true");
            unlink_clicked=editor->text()=="true";
        } else if(expression) {
            auto* editor=dialog->findChild<QPlainTextEdit*>("text-weight-expression-draft");
            if(!editor){dialog->reject();return;}
            editor->setFocus();QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"500");
            unlink_clicked=editor->toPlainText()=="500";
        } else {
            auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-family-driver");
            if(!unlink){dialog->reject();return;}
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,unlink->mapTo(dialog,unlink->rect().center()));
            unlink_clicked=unlink->isChecked();
        }
        auto* apply=buttons->button(italic?QDialogButtonBox::Ok:QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(dialog,apply->rect().center()));
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1050);events();
    check(menu_open&&(((weight||italic)&&!expression)||(dialog_open&&unlink_clicked)),"First driver pointer reaches the existing explicit source action");
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    if(italic&&expression)expected.apply({SetTextItalicExpression{{"text","","text.italic"},Expression{"true",1},false}},expected.revision());
    else if(italic)expected.apply({UnlinkTextItalic{{"text","","text.italic"}}},expected.revision());
    else if(expression)expected.apply({SetTextWeightExpression{{"text","","text.weight"},Expression{"500",1},false}},expected.revision());
    else if(weight)expected.apply({UnlinkTextWeight{{"text","","text.weight"}}},expected.revision());
    else expected.apply({UnlinkTextFamily{{"text","","text.family"}}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Existing Unlink editor uses exact own scalar revision without changing a font value");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Unlink Undo restores the source driver and retains the independent size");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original linked source");
    std::cout<<(italic?(expression?"text_italic_expression_pending_pointer: ":"text_italic_unlink_pending_pointer: "):weight?(expression?"text_weight_expression_pending_pointer: ":"text_weight_unlink_pending_pointer: "):"text_family_unlink_pending_pointer: ")
        <<checks<<" checks passed; Qt Window pointer route\n";
}
void direction_link_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Writing source owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("direction-link-pointer-document","composition","board");
    Object text;text.id="text";text.name="Writing target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing vertical source";other.kind=Kind::text;
    other.text=default_text("other-source","Retain source");other.text->direction="vertical";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* drive=window.findChild<QToolButton*>("text-direction-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    const auto find_size=[&]()->QLineEdit*{
        for(auto* input:window.findChildren<QLineEdit*>()){
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")return input;
        }
        return nullptr;
    };
    auto* size=find_size();check(scroll&&drive&&menu&&size,"Existing Writing driver and scalar available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain"){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Writing source scalar draft is neutral before the actual first pointer");
    scroll->ensureWidgetVisible(drive);events();
    if(mode=="entry-revision"){
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="entry-document"){auto incoming=document;incoming.id="incoming-entry-document";window.host.session=Session(incoming);expected=window.host.session;}
    const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits existing Writing driver");
    const bool reject_entry=mode=="invalid"||mode=="entry-revision"||mode=="entry-document";
    if(mode!="plain"&&!reject_entry)expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    bool opened=false,neutral=false,dialog_open=false,choice=false,same_revision=true;
    QString status;
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        opened=menu&&menu->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(opened)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(1)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-direction-source-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* source=dialog->findChild<QComboBox*>("text-direction-source");auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!source||!buttons||source->count()!=1){dialog->reject();return;}
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,source->mapTo(dialog,QPoint(source->width()-8,source->height()/2)));
        QTest::qWait(QApplication::doubleClickInterval()+20);
        auto* view=source->view();const auto index=source->model()->index(0,0);
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());
        choice=source->currentIndex()==0&&source->currentText().contains("other")&&snapshot(window.host.session)==snapshot(expected);
        if(mode=="apply-revision"){
            window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
            expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        }
        if(mode=="apply-document"){
            auto incoming=document;incoming.id="incoming-apply-document";
            Session replacement(incoming);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());
            same_revision=replacement.revision()==window.host.session.revision();window.host.session=replacement;expected=replacement;
        }
        if(mode=="apply-session")window.host.session_id="incoming-source-session";
        auto* action=buttons->button(mode=="cancel"?QDialogButtonBox::Cancel:QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,action->mapTo(dialog,action->rect().center()));
        status=dialog->findChild<QLabel*>("text-direction-source-status")->text();
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1100);events();
    if(reject_entry){
        check(snapshot(window.host.session)==snapshot(expected)&&!opened&&!dialog_open,"Invalid or changed Writing entry is refused atomically before the source menu");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),"Writing entry reports exact cause");
        std::cout<<"text_direction_link_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    check(opened&&neutral&&dialog_open&&choice,"First Writing pointer opens the existing chooser with neutral exact source selection");
    if(mode=="apply-revision"||mode=="apply-document"||mode=="apply-session"){
        check(same_revision,"Incoming chooser Document has same canonical revision");
        check(snapshot(window.host.session)==snapshot(expected),"Changed Writing chooser context refuses partial authored mutation");
        check(status.contains(mode=="apply-revision"?"Text changed":"SESSION_CONFLICT")||status.contains(mode=="apply-revision"?"Text changed":"another document"),"Writing chooser reports bound-context cause");
        std::cout<<"text_direction_link_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode=="cancel"){
        check(snapshot(window.host.session)==snapshot(expected),"Writing chooser Cancel retains only independent scalar");
        size=find_size();check(size,"Writing Cancel exposes current scalar");scroll->ensureWidgetVisible(size);events();size->setFocus();
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
        expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
        check(snapshot(window.host.session)==snapshot(expected),"Writing Cancel leaves scalar usable at exact current revision");
    }else{
        expected.apply({LinkTextDirection{{"text","","text.direction"},{"other","","text.direction"},false}},expected.revision());
        std::cout<<"Direction Link actual-revision="<<window.host.session.revision()<<" expected="<<expected.revision()<<" status="<<status.toStdString()<<std::endl;
        check(snapshot(window.host.session)==snapshot(expected),"First Writing Link equals exact independent scalar and stable enum source command");
    }
    check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Writing chooser retains exact target selection");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Writing source or fresh scalar Undo retains independently committed size");
    if(mode!="plain"){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();}
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo restores full original source/history");
    std::cout<<"text_direction_link_pending_pointer "<<mode<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void layout_link_pending_pointer(const std::string& mode,const std::string& scalar_field="text.font_size"){
    const int scalar_value=scalar_field=="text.font_size"?64:777;
    QTemporaryDir scratch;check(scratch.isValid(),"Sizing source owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("layout-link-pointer-document","composition","board");
    Object text;text.id="text";text.name="Sizing target";text.kind=Kind::text;
    text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing frame source";other.kind=Kind::text;
    other.text=default_text("other-source","Retain source");other.text->layout="frame";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);
    document.compositions.front().roots={text.id,other.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    auto* drive=window.findChild<QToolButton*>("text-layout-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    const auto find_size=[&]()->QLineEdit*{
        for(auto* input:window.findChildren<QLineEdit*>()){
            const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
            if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()==QString::fromStdString(scalar_field))return input;
        }
        return nullptr;
    };
    auto* size=find_size();check(scroll&&drive&&menu&&size,"Existing Sizing driver and scalar available");
    scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain"){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":QString::number(scalar_value).toLatin1().constData());events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Sizing source scalar draft is neutral before the actual first pointer");
    scroll->ensureWidgetVisible(drive);events();
    if(mode=="entry-revision"){
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
        expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
    }
    if(mode=="entry-document"){auto incoming=document;incoming.id="incoming-entry-document";window.host.session=Session(incoming);expected=window.host.session;}
    const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits existing Sizing driver");
    const bool reject_entry=mode=="invalid"||mode=="entry-revision"||mode=="entry-document";
    if(mode!="plain"&&!reject_entry)expected.apply({EditProperties{{{"text","",scalar_field}},static_cast<double>(scalar_value),false}},expected.revision());
    bool opened=false,neutral=false,dialog_open=false,choice=false,same_revision=true;
    QString status;
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        opened=menu&&menu->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(opened)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(1)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        auto* dialog=window.findChild<QDialog*>("text-layout-source-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* source=dialog->findChild<QComboBox*>("text-layout-source");auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!source||!buttons||source->count()!=1){dialog->reject();return;}
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,source->mapTo(dialog,QPoint(source->width()-8,source->height()/2)));
        QTest::qWait(QApplication::doubleClickInterval()+20);
        auto* view=source->view();const auto index=source->model()->index(0,0);
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());
        choice=source->currentIndex()==0&&source->currentText().contains("other")&&snapshot(window.host.session)==snapshot(expected);
        if(mode=="apply-revision"){
            window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());
            expected.apply({EditProperties{{{"text","","text.tracking"}},2,false}},expected.revision());
        }
        if(mode=="apply-document"){
            auto incoming=document;incoming.id="incoming-apply-document";
            Session replacement(incoming);replacement.apply({EditProperties{{{"text","",scalar_field}},static_cast<double>(scalar_value),false}},replacement.revision());
            same_revision=replacement.revision()==window.host.session.revision();window.host.session=replacement;expected=replacement;
        }
        if(mode=="apply-session")window.host.session_id="incoming-source-session";
        auto* action=buttons->button(mode=="cancel"?QDialogButtonBox::Cancel:QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,action->mapTo(dialog,action->rect().center()));
        status=dialog->findChild<QLabel*>("text-layout-source-status")->text();
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1100);events();
    if(reject_entry){
        check(snapshot(window.host.session)==snapshot(expected)&&!opened&&!dialog_open,"Invalid or changed Sizing entry is refused atomically before the source menu");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"STALE_CONTEXT":"SESSION_CONFLICT"),"Sizing entry reports exact cause");
        std::cout<<"text_layout_link_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    std::cerr<<"Sizing scalar "<<scalar_field<<" menu="<<opened<<" source-dialog="<<dialog_open<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<" status="<<window.statusBar()->currentMessage().toStdString()<<"\n";
    check(opened&&neutral&&dialog_open&&choice,"First Sizing pointer opens the existing chooser with neutral exact source selection");
    if(mode=="apply-revision"||mode=="apply-document"||mode=="apply-session"){
        check(same_revision,"Incoming chooser Document has same canonical revision");
        check(snapshot(window.host.session)==snapshot(expected),"Changed Sizing chooser context refuses partial authored mutation");
        check(status.contains(mode=="apply-revision"?"Text changed":"SESSION_CONFLICT")||status.contains(mode=="apply-revision"?"Text changed":"another document"),"Sizing chooser reports bound-context cause");
        std::cout<<"text_layout_link_pending_pointer "<<mode<<": "<<checks<<" checks passed\n";return;
    }
    if(mode=="cancel"){
        check(snapshot(window.host.session)==snapshot(expected),"Sizing chooser Cancel retains only independent scalar");
        size=find_size();check(size,"Sizing Cancel exposes current scalar");scroll->ensureWidgetVisible(size);events();size->setFocus();
        QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");QTest::keyClick(size,Qt::Key_Return);events();
        expected.apply({EditProperties{{{"text","",scalar_field}},65,false}},expected.revision());
        check(snapshot(window.host.session)==snapshot(expected),"Sizing Cancel leaves scalar usable at exact current revision");
    }else{
        expected.apply({LinkTextLayout{{"text","","text.layout"},{"other","","text.layout"},false}},expected.revision());
        std::cout<<"Layout Link actual-revision="<<window.host.session.revision()<<" expected="<<expected.revision()<<" status="<<status.toStdString()<<std::endl;
        check(snapshot(window.host.session)==snapshot(expected),"First Sizing Link equals exact independent scalar and stable enum source command");
    }
    check(window.canvas->selected_object=="text"&&window.canvas->selected_point.empty(),"Sizing chooser retains exact target selection");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Sizing source or fresh scalar Undo retains independently committed size");
    if(mode!="plain"){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();}
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo restores full original source/history");
    std::cout<<"text_layout_link_pending_pointer "<<mode<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void direction_source_action_pending_pointer(const std::string& action){
    QTemporaryDir scratch;check(scratch.isValid(),"Writing source action owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("direction-source-action-document","composition","board");
    Object text;text.id="text";text.name="Writing action target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing vertical source";other.kind=Kind::text;other.text=default_text("other-source","Retain source");other.text->direction="vertical";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);document.compositions.front().roots={text.id,other.id};
    Session fixture(document);
    if(action!="edit")fixture.apply({LinkTextDirection{{"text","","text.direction"},{"other","","text.direction"},false}},fixture.revision());
    document=fixture.document();window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");auto* drive=window.findChild<QToolButton*>("text-direction-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&menu&&size,"Existing Writing source action controls available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");events();
    check(size->hasFocus()&&size->isModified()&&snapshot(window.host.session)==snapshot(expected),"Writing action scalar draft is neutral");
    scroll->ensureWidgetVisible(drive);events();const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits existing Writing driver");
    bool menu_open=false,neutral=false,dialog_open=false,choice=false;
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        menu_open=menu&&menu->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(menu_open)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:0)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        if(action=="unlink")return;
        auto* dialog=window.findChild<QDialog*>("text-direction-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* editor=dialog->findChild<QComboBox*>("text-direction-editor");auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!editor||!buttons){dialog->reject();return;}
        if(action=="linked-edit"){
            auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-direction-driver");
            if(!unlink||!unlink->isVisible()){dialog->reject();return;}
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,unlink->mapTo(dialog,unlink->rect().center()));
            if(!unlink->isChecked()){dialog->reject();return;}
        }
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,editor->mapTo(dialog,QPoint(editor->width()-8,editor->height()/2)));
        QTest::qWait(QApplication::doubleClickInterval()+20);
        auto* view=editor->view();const auto index=editor->model()->index(action=="edit"?1:0,0);
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());
        choice=editor->currentIndex()==(action=="edit"?1:0)&&snapshot(window.host.session)==snapshot(expected);
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(dialog,apply->rect().center()));
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1100);events();
    check(menu_open&&neutral&&(action=="unlink"||(dialog_open&&choice)),"First Writing driver reaches neutral existing Edit or Unlink action");
    if(action=="unlink")expected.apply({UnlinkTextDirection{{"text","","text.direction"}}},expected.revision());
    else{
        auto next=*expected.document().objects.at("text").text;next.direction=action=="edit"?"vertical":"horizontal";next.direction_driver.reset();
        std::vector<Command> commands;if(action=="linked-edit")commands.push_back(UnlinkTextDirection{{"text","","text.direction"}});commands.push_back(UpdateText{"text",next});
        expected.apply(commands,expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Existing Writing source action equals canonical command batch and full source/history");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Writing action Undo restores source/driver and preserves independent size");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original fixture and history");
    std::cout<<"text_direction_source_action_pending_pointer "<<action<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void layout_source_action_pending_pointer(const std::string& action){
    QTemporaryDir scratch;check(scratch.isValid(),"Sizing source action owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("layout-source-action-document","composition","board");
    Object text;text.id="text";text.name="Sizing action target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing frame source";other.kind=Kind::text;other.text=default_text("other-source","Retain source");other.text->layout="frame";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);document.compositions.front().roots={text.id,other.id};
    Session fixture(document);
    if(action!="edit")fixture.apply({LinkTextLayout{{"text","","text.layout"},{"other","","text.layout"},false}},fixture.revision());
    document=fixture.document();window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");auto* drive=window.findChild<QToolButton*>("text-layout-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&menu&&size,"Existing Sizing source action controls available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");events();
    check(size->hasFocus()&&size->isModified()&&snapshot(window.host.session)==snapshot(expected),"Sizing action scalar draft is neutral");
    scroll->ensureWidgetVisible(drive);events();const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits existing Sizing driver");
    bool menu_open=false,neutral=false,dialog_open=false,choice=false;
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        menu_open=menu&&menu->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(menu_open)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:0)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        if(action=="unlink")return;
        auto* dialog=window.findChild<QDialog*>("text-layout-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* editor=dialog->findChild<QComboBox*>("text-layout-editor");auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!editor||!buttons){dialog->reject();return;}
        if(action=="linked-edit"){
            auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-layout-driver");
            if(!unlink||!unlink->isVisible()){dialog->reject();return;}
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,unlink->mapTo(dialog,unlink->rect().center()));
            if(!unlink->isChecked()){dialog->reject();return;}
        }
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,editor->mapTo(dialog,QPoint(editor->width()-8,editor->height()/2)));
        QTest::qWait(QApplication::doubleClickInterval()+20);
        auto* view=editor->view();const auto index=editor->model()->index(action=="edit"?1:0,0);
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());
        choice=editor->currentIndex()==(action=="edit"?1:0)&&snapshot(window.host.session)==snapshot(expected);
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(dialog,apply->rect().center()));
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1100);events();
    check(menu_open&&neutral&&(action=="unlink"||(dialog_open&&choice)),"First Sizing driver reaches neutral existing Edit or Unlink action");
    if(action=="unlink")expected.apply({UnlinkTextLayout{{"text","","text.layout"}}},expected.revision());
    else{
        auto next=*expected.document().objects.at("text").text;next.layout=action=="edit"?"frame":"auto";next.layout_driver.reset();
        std::vector<Command> commands;if(action=="linked-edit")commands.push_back(UnlinkTextLayout{{"text","","text.layout"}});commands.push_back(UpdateText{"text",next});
        expected.apply(commands,expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Existing Sizing source action equals canonical command batch and full source/history");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Sizing action Undo restores source/driver and preserves independent size");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original fixture and history");
    std::cout<<"text_layout_source_action_pending_pointer "<<action<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void alignment_source_action_pending_pointer(const std::string& action){
    QTemporaryDir scratch;check(scratch.isValid(),"Alignment source action owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("alignment-source-action-document","composition","board");
    Object text;text.id="text";text.name="Alignment action target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    Object other;other.id="other";other.name="Existing centered source";other.kind=Kind::text;other.text=default_text("other-source","Retain source");other.text->alignment="center";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);document.compositions.front().roots={text.id,other.id};
    Session fixture(document);
    if(action!="edit")fixture.apply({LinkTextAlignment{{"text","","text.alignment"},{"other","","text.alignment"},false}},fixture.revision());
    document=fixture.document();window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");auto* drive=window.findChild<QToolButton*>("text-alignment-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&menu&&size,"Existing Alignment source action controls available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");events();
    check(size->hasFocus()&&size->isModified()&&snapshot(window.host.session)==snapshot(expected),"Alignment action scalar draft is neutral");
    scroll->ensureWidgetVisible(drive);events();const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits existing Alignment driver");
    bool menu_open=false,neutral=false,dialog_open=false,choice=false;
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        menu_open=menu&&menu->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(menu_open)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:0)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        if(action=="unlink")return;
        auto* dialog=window.findChild<QDialog*>("text-alignment-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* editor=dialog->findChild<QComboBox*>("text-alignment-editor");auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!editor||!buttons){dialog->reject();return;}
        if(action=="linked-edit"){
            auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-alignment-driver");
            if(!unlink||!unlink->isVisible()){dialog->reject();return;}
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,unlink->mapTo(dialog,unlink->rect().center()));
            if(!unlink->isChecked()){dialog->reject();return;}
        }
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,editor->mapTo(dialog,QPoint(editor->width()-8,editor->height()/2)));
        QTest::qWait(QApplication::doubleClickInterval()+20);
        auto* view=editor->view();const auto index=editor->model()->index(action=="edit"?1:0,0);
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());
        choice=editor->currentIndex()==(action=="edit"?1:0)&&snapshot(window.host.session)==snapshot(expected);
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(dialog,apply->rect().center()));
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1100);events();
    check(menu_open&&neutral&&(action=="unlink"||(dialog_open&&choice)),"First Alignment driver reaches neutral existing Edit or Unlink action");
    if(action=="unlink")expected.apply({UnlinkTextAlignment{{"text","","text.alignment"}}},expected.revision());
    else{
        auto next=*expected.document().objects.at("text").text;next.alignment=action=="edit"?"center":"start";next.alignment_driver.reset();
        std::vector<Command> commands;if(action=="linked-edit")commands.push_back(UnlinkTextAlignment{{"text","","text.alignment"}});commands.push_back(UpdateText{"text",next});
        expected.apply(commands,expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Existing Alignment source action equals canonical command batch and full source/history");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Alignment action Undo restores source/driver and preserves independent size");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original fixture and history");
    std::cout<<"text_alignment_source_action_pending_pointer "<<action<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void locale_source_action_pending_pointer(const std::string& action){
    QTemporaryDir scratch;check(scratch.isValid(),"Locale source action owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("locale-source-action-document","composition","board");
    Object text;text.id="text";text.name="Locale action target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");text.text->locale="ja-JP";
    Object other;other.id="other";other.name="Existing locale source";other.kind=Kind::text;other.text=default_text("other-source","Retain source");other.text->locale="en-US";
    document.objects.emplace(text.id,text);document.objects.emplace(other.id,other);document.compositions.front().roots={text.id,other.id};
    Session fixture(document);
    if(action!="edit")fixture.apply({LinkTextLocale{{"text","","text.locale"},{"other","","text.locale"},false}},fixture.revision());
    document=fixture.document();window.host.session=Session(document);window.host.edited();window.resize(1100,750);
    window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");auto* drive=window.findChild<QToolButton*>("text-locale-driver");QPointer<QMenu> menu=drive?drive->menu():nullptr;
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&drive&&menu&&size,"Existing Locale source action controls available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,"65");events();
    check(size->hasFocus()&&size->isModified()&&snapshot(window.host.session)==snapshot(expected),"Locale action scalar draft is neutral");
    scroll->ensureWidgetVisible(drive);events();const auto position=drive->mapTo(&window,drive->rect().center());
    check(window.childAt(position)==drive,"Actual Window pointer hits existing Locale driver");
    bool menu_open=false,neutral=false,dialog_open=false,choice=false;
    expected.apply({EditProperties{{{"text","","text.font_size"}},65,false}},expected.revision());
    QTimer::singleShot(QApplication::doubleClickInterval()+20,&window,[&]{
        menu_open=menu&&menu->isVisible();neutral=snapshot(window.host.session)==snapshot(expected);
        if(menu_open)QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().at(action=="unlink"?2:0)).center());
    });
    QTimer::singleShot(QApplication::doubleClickInterval()+1000,&window,[&]{
        if(action=="unlink")return;
        auto* dialog=window.findChild<QDialog*>("text-locale-dialog");dialog_open=dialog&&dialog->isVisible();
        if(!dialog_open){if(menu)menu->close();return;}
        auto* editor=dialog->findChild<QLineEdit*>("text-locale-editor");auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if(!editor||!buttons){dialog->reject();return;}
        if(action=="linked-edit"){
            auto* unlink=dialog->findChild<QCheckBox*>("unlink-text-locale-driver");
            if(!unlink||!unlink->isVisible()){dialog->reject();return;}
            QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,unlink->mapTo(dialog,unlink->rect().center()));
            if(!unlink->isChecked()){dialog->reject();return;}
        }
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,editor->mapTo(dialog,editor->rect().center()));
        QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,action=="edit"?"en-US":"fr-FR");events();
        choice=editor->text()==(action=="edit"?"en-US":"fr-FR")&&snapshot(window.host.session)==snapshot(expected);
        auto* apply=buttons->button(QDialogButtonBox::Apply);
        QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(dialog,apply->rect().center()));
        if(dialog->isVisible())dialog->reject();
    });
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);QTest::qWait(QApplication::doubleClickInterval()+1100);events();
    check(menu_open&&neutral&&(action=="unlink"||(dialog_open&&choice)),"First Locale driver reaches neutral existing Edit or Unlink action");
    if(action=="unlink")expected.apply({UnlinkTextLocale{{"text","","text.locale"}}},expected.revision());
    else{
        auto next=*expected.document().objects.at("text").text;next.locale=action=="edit"?"en-US":"fr-FR";next.locale_driver.reset();
        std::vector<Command> commands;if(action=="linked-edit")commands.push_back(UnlinkTextLocale{{"text","","text.locale"}});commands.push_back(UpdateText{"text",next});
        expected.apply(commands,expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Existing Locale source action equals canonical command batch and full source/history");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Locale action Undo restores source/driver and preserves independent size");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original fixture and history");
    std::cout<<"text_locale_source_action_pending_pointer "<<action<<": "<<checks<<" checks passed; Qt Window pointer route\n";
}
void seed_scalar_group(Document& document){
    for(const auto* id:{"text","source"}){
        if(!document.objects.contains(id))continue;
        Object child;child.id=std::string(id)+"-child";child.name=child.id;
        child.source=default_primitive(child.id+"-source","nect.shape.circle");child.source->parameters.at("radius").literal=60;
        document.objects.emplace(child.id,child);
    }
    Session seed(document);
    for(const auto* id:{"text","source"}){
        if(!document.objects.contains(id))continue;
        const auto& child=document.objects.at(std::string(id)+"-child");
        seed.apply({AddOperation{child.id,default_operation(child.id+"-fill","nect.paint.fill"),0},Set{{child.id,child.source->id+"-north","x"},10}},seed.revision());
    }
    auto incoming=seed.document();Object guard;guard.id="group-ref-guard";guard.name="Group child incoming stable point Ref";
    guard.source=default_primitive("group-ref-guard-source","nect.shape.rectangle");incoming.objects.emplace(guard.id,guard);incoming.compositions.front().roots.push_back(guard.id);seed=Session(incoming);
    seed.apply({Link{{guard.id,"","generator.width"},{{"text-child","text-child-source-east","x"},0.1,0,"copy_local_value"}}},seed.revision());
    document=seed.document();
}

void make_scalar_path(Object& object){
    object.kind=Kind::path;object.text.reset();object.source.reset();
    Contour contour;contour.id=object.id+"-contour";contour.closed=true;
    Point first;first.id=object.id+"-first";first.out_angle.literal=0.7;first.out_length.literal=25;
    Point second;second.id=object.id+"-second";second.x.literal=80;second.in_angle.literal=-0.4;second.in_length.literal=18;
    Point third;third.id=object.id+"-third";third.y.literal=60;contour.points={first,second,third};object.contours={contour};
}
void seed_scalar_path(Document& document){
    Session seeded(document);seeded.apply({AddOperation{"text",default_operation("path-target-fill","nect.paint.fill"),0}},seeded.revision());
    auto incoming=seeded.document();Object guard;guard.id="path-ref-guard";guard.name="Authored curve point incoming Ref";
    guard.source=default_primitive("path-ref-guard-source","nect.shape.rectangle");incoming.objects.emplace(guard.id,guard);incoming.compositions.front().roots.push_back(guard.id);seeded=Session(incoming);
    seeded.apply({Link{{guard.id,"","generator.width"},{{"text","text-second","x"},0.1,0,"copy_local_value"}}},seeded.revision());document=seeded.document();
}

void make_scalar_image(Object& object){
    object.kind=Kind::image;object.text.reset();object.source.reset();object.contours.clear();object.children.clear();
    object.image=ImageSource{"image-shared-asset",{120},{60}};
}
void seed_scalar_image(Document& document,const std::string& target_field="transform.tx"){
    RasterPixels pixels{2,1,{240,30,10,255,20,60,220,128}};
    document.raster_assets.emplace("image-shared-asset",RasterAsset{"image-shared-asset","Accepted two-pixel original","embedded","",make_raster(encode_raster_png(pixels))});
    if(!document.objects.contains("source")){Object shared;shared.id="image-shared-copy";shared.name="Untouched shared image placement";make_scalar_image(shared);shared.image->width.literal=80;shared.image->height.literal=50;shared.transform[4].literal=300;document.objects.emplace(shared.id,shared);document.compositions.front().roots.push_back(shared.id);}
    Object guard;guard.id="image-ref-guard";guard.name="Image transform incoming Ref";guard.source=default_primitive("image-ref-guard-source","nect.shape.rectangle");document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
    Session seeded(document);seeded.apply({Link{{guard.id,"","generator.width"},{{"text","",target_field},0.1,20,"copy_local_value"}}},seeded.revision());document=seeded.document();
}

void make_scalar_instance(Object& object){
    object.kind=Kind::instance;object.text.reset();object.source.reset();object.contours.clear();object.children.clear();
    object.instance=DefinitionInstance{"instance-definition"};
}
void seed_scalar_instance(Document& document){
    Object root;root.id="instance-root";root.name="Shared authored source";root.kind=Kind::group;
    root.children={"instance-curve","instance-text","instance-image"};
    Object curve;curve.id="instance-curve";make_scalar_path(curve);curve.stack={default_operation("instance-fill","nect.paint.fill")};
    Object text;text.id="instance-text";text.kind=Kind::text;text.text=default_text("instance-text-source","Shared 日本語 source");
    text.text->parameters.at("font_size").expression=Expression{"24 + 2",1};
    Object image;image.id="instance-image";make_scalar_image(image);image.transform[4].literal=130;
    document.raster_assets.emplace("image-shared-asset",RasterAsset{"image-shared-asset","Accepted source raster","embedded","",make_raster(encode_raster_png(RasterPixels{2,1,{240,30,10,255,20,60,220,128}}))});
    for(const auto& object:{root,curve,text,image})document.objects.emplace(object.id,object);
    document.compositions.front().roots.push_back(root.id);
    document.definitions.emplace("instance-definition",Definition{"instance-definition","Shared",root.id});
    document.definitions.emplace("instance-alternate",Definition{"instance-alternate","Alternate identity",root.id});
    if(!document.objects.contains("source")){Object shared=document.objects.at("text");shared.id="source";shared.name="Untouched second Instance";shared.transform[4].literal=300;document.objects.emplace(shared.id,shared);document.compositions.front().roots.push_back(shared.id);}
    Object guard;guard.id="instance-ref-guard";guard.source=default_primitive("instance-ref-guard-source","nect.shape.rectangle");
    document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
    Session seeded(document);seeded.apply({DefinitionCommand{SetInstanceOverride{"text",{"instance-text","","text.font_size"},31}},
        DefinitionCommand{SetInstanceVisibilityOverride{"text","instance-image",false}},
        DefinitionCommand{SetInstanceColorOverride{"text",operation_ref("instance-curve","instance-fill","color"),ColorValue{"srgb","srgb","straight",{0.2,0.4,0.6,0.8}}}},
        DefinitionCommand{SetInstanceTextContentOverride{"text","instance-text","Local 日本語 content"}},
        Link{{guard.id,"","generator.width"},{{"text","","transform.tx"},0.1,20,"copy_local_value"}}},seeded.revision());
    document=seeded.document();
}
void replace_scalar_instance_context(Document& incoming,const std::string& mode){
    auto& object=incoming.objects.at("text");
    if(mode=="entry-type"||mode=="apply-type"){object.instance.reset();make_scalar_path(object);}
    else if(mode=="entry-source"||mode=="apply-source")object.instance->definition="instance-alternate";
    else if(mode.ends_with("root")){
        auto replacement=incoming.objects.at("instance-root");replacement.id="replacement-instance-root";
        incoming.objects.at("instance-root").children.clear();incoming.objects.emplace(replacement.id,replacement);
        incoming.compositions.front().roots.push_back(replacement.id);incoming.definitions.at("instance-definition").root=replacement.id;
        incoming.definitions.at("instance-alternate").root=replacement.id;
    }else if(mode.ends_with("subtree"))incoming.objects.at("instance-curve").contours.front().points.at(1).x.literal=91;
    else if(mode.ends_with("asset"))incoming.raster_assets.at("image-shared-asset").payload=make_raster(encode_raster_png(RasterPixels{2,1,{10,240,30,255,220,20,60,128}}));
    else if(mode.ends_with("visibility"))object.instance->visibility_overrides.at("instance-image")=true;
    else if(mode.ends_with("color"))object.instance->color_overrides.begin()->second.rgba[0]=0.7;
    else if(mode.ends_with("content"))object.instance->text_content_overrides.at("instance-text")="Changed local content";
    else if(mode.ends_with("override"))object.instance->overrides.begin()->second=33;
}

std::optional<Bounds> scalar_world_bounds(const Document& document,const Id& id,
    const std::map<Ref,double>& values,const std::map<Id,EvaluatedTransform>& transforms){
    if(document.objects.at(id).kind!=Kind::instance)return object_bounds(document,id,values,transforms,true);
    const auto projected=project_definition_instances(document,document.compositions.front().id,values,transforms);
    return object_bounds(*projected.document,id,*projected.values,*projected.transforms,true);
}

void scalar_pick_pending_pointer(const std::string& mode,const std::string& scalar_field="text.font_size",bool rectangle_centers=false,bool polygon_source=false,bool star_source=false,bool circle_transform=false,bool group_source=false,bool authored_path=false,bool image_source=false,bool instance_source=false){
    const bool polystar_source=polygon_source||star_source;
    const bool count_source=scalar_field=="generator.points";
    const bool image_dimension=scalar_field=="image.width"||scalar_field=="image.height";
    const bool opacity_source=scalar_field=="composite.opacity";
    const bool anchor_source=scalar_field=="transform.anchor_x"||scalar_field=="transform.anchor_y";
    const bool affine_linear=scalar_field=="transform.a"||scalar_field=="transform.b"||scalar_field=="transform.c"||scalar_field=="transform.d";
    const bool affine_source=scalar_field=="transform.tx"||scalar_field=="transform.ty"||affine_linear;
    const std::size_t affine_index=scalar_field=="transform.a"?0:scalar_field=="transform.b"?1:scalar_field=="transform.c"?2:scalar_field=="transform.d"?3:scalar_field=="transform.ty"?5:4;
    const double pending_value=opacity_source?0.6:affine_linear?1.25:count_source?10.0:64.0;
    const char* pending_text=opacity_source?"0.6":affine_linear?"1.25":count_source?"10":"64";
    const bool rectangle_source=rectangle_centers||scalar_field=="generator.width"||scalar_field=="generator.height";
    const bool vertical=scalar_field=="generator.height"||((rectangle_centers||polystar_source)&&scalar_field=="generator.center_y");
    const std::string field=mode=="generic"?"text.tracking":scalar_field;
    const QString label=opacity_source?"Object opacity":anchor_source?(field=="transform.anchor_y"?"Anchor Y":"Anchor X"):affine_linear?QString::fromStdString(field.substr(10)):field=="transform.tx"?"tx":field=="transform.ty"?"ty":field=="generator.points"?"Points":field=="generator.inner_radius"?"Inner radius":field=="generator.outer_radius"?"Outer radius":field=="generator.rotation"?"Rotation":(field=="generator.height"||field=="image.height")?"Height":(field=="generator.width"||field=="image.width")?"Width":field=="generator.center_x"?"Center X":field=="generator.center_y"?"Center Y":field=="generator.radius"?"Radius":field=="text.origin_x"?"Origin X":field=="text.origin_y"?"Origin Y":field=="text.tracking"?"Tracking":field=="text.line_spacing"?"Line advance · 0 = auto":field=="text.frame_width"?"Frame width":field=="text.frame_height"?"Frame height":"Font size";
    QTemporaryDir scratch;check(scratch.isValid(),"Text numeric picker owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("scalar-pick-document","composition","board");
    Object text;text.id="text";text.name="Numeric picker target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    text.text->font_features={{"KERN",1,"whole_text"},{"lig ",4,"whole_text"}};text.text->additional_axis_values={{"wdth",87.1234567890123}};
    if(image_dimension||affine_source||anchor_source||opacity_source)text.transform={{{0.8,{}},{0.2,{}},{-0.3,{}},{1.1,{}},{60,{}},{50,{}}}};
    if(image_dimension||affine_linear||anchor_source||opacity_source)text.anchor={{{17,{}},{23,{}}}};
    if(opacity_source)text.compositing.opacity.literal=0.9;
    if((circle_transform||scalar_field.rfind("generator.",0)==0)){
        text.name=rectangle_source?"Retained Rectangle Width target":"Retained Circle scalar target";text.kind=Kind::path;text.text.reset();
        text.source=default_primitive("circle-target-source",rectangle_source?"nect.shape.rectangle":star_source?"nect.shape.star":polygon_source?"nect.shape.polygon":"nect.shape.circle");text.source->parameters.at(rectangle_source?((rectangle_centers&&!circle_transform)?scalar_field.substr(10):(vertical?"height":"width")):polystar_source?(circle_transform?(star_source?"outer_radius":"radius"):scalar_field.substr(10)):"radius").literal=count_source?5:60;
    }
    if(authored_path)make_scalar_path(text);
    if(image_source)make_scalar_image(text);
    if(instance_source)make_scalar_instance(text);
    if(group_source){text.kind=Kind::group;text.text.reset();text.children={"text-child"};}
    Object source=text;source.id="source";source.name="Numeric picker source";if(instance_source){make_scalar_instance(source);if(opacity_source)source.compositing.opacity.literal=0.8;else if(anchor_source)source.anchor.at(field=="transform.anchor_y"?1:0).literal=72;else source.transform.at(affine_index).literal=affine_linear?0.75:72;}else if(image_source){make_scalar_image(source);source.image->width.literal=80;source.image->height.literal=50;if(opacity_source)source.compositing.opacity.literal=0.8;else if(anchor_source)source.anchor.at(field=="transform.anchor_y"?1:0).literal=72;else if(!image_dimension)source.transform.at(affine_index).literal=affine_linear?0.75:72;}else if(authored_path){make_scalar_path(source);if(opacity_source)source.compositing.opacity.literal=0.8;else if(anchor_source)source.anchor.at(field=="transform.anchor_y"?1:0).literal=72;else source.transform.at(affine_index).literal=affine_linear?0.75:72;}else if(group_source){source.children={"source-child"};if(opacity_source)source.compositing.opacity.literal=0.8;else if(anchor_source)source.anchor.at(field=="transform.anchor_y"?1:0).literal=72;else source.transform.at(affine_index).literal=affine_linear?0.75:72;}else if(circle_transform){source.source->id="circle-pick-source";if(opacity_source)source.compositing.opacity.literal=0.8;else if(anchor_source)source.anchor.at(field=="transform.anchor_y"?1:0).literal=72;else source.transform.at(affine_index).literal=affine_linear?0.75:72;}else if(scalar_field.rfind("generator.",0)==0){source.source->id="circle-pick-source";source.source->parameters.at(field.substr(10)).literal=count_source?20:72;}else if(opacity_source){source.text->id="source-text";source.compositing.opacity.literal=0.8;}else if(anchor_source){source.text->id="source-text";source.anchor.at(field=="transform.anchor_y"?1:0).literal=72;}else if(affine_source){source.text->id="source-text";source.transform.at(affine_index).literal=affine_linear?0.75:72;}else{source.text->id="source-text";source.text->parameters.at(mode=="generic"?"font_size":field.substr(5)).literal=count_source?20:72;}
    document.objects.emplace(text.id,text);document.objects.emplace(source.id,source);document.compositions.front().roots={text.id,source.id};
    if((circle_transform||scalar_field.rfind("generator.",0)==0)){
        if((circle_transform||polystar_source||scalar_field!="generator.radius")&&document.objects.contains("source")){
            Object guard;guard.id="circle-ref-guard";guard.name="Incoming stable point Ref guard";
            guard.source=default_primitive("circle-ref-guard-source","nect.shape.rectangle");
            document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
        }
        Session seeded(document);seeded.apply({AddOperation{"text",default_operation("circle-target-fill","nect.paint.fill"),0},Set{{"text",rectangle_source?"circle-target-source-top-left":polystar_source?"circle-target-source-outer-0-1":"circle-target-source-north",vertical?"y":"x"},10}},seeded.revision());
        if(document.objects.contains("source"))seeded.apply({Link{{(scalar_field=="generator.radius"&&!polystar_source)?"source":"circle-ref-guard","",(scalar_field=="generator.radius"&&!polystar_source)?"generator.center_x":"generator.width"},{{"text",rectangle_source?(vertical?"circle-target-source-bottom-right":"circle-target-source-top-right"):polystar_source?(vertical?(star_source?"circle-target-source-outer-2-5":"circle-target-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"circle-target-source-outer-4-5":"circle-target-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"circle-target-source-inner-1-10":"circle-target-source-outer-1-5"):count_source?"circle-target-source-outer-1-5":"circle-target-source-outer-1-6"):"circle-target-source-east",vertical?"y":"x"},0.1,0,"copy_local_value"}}},seeded.revision());
        document=seeded.document();
    }
    if(group_source)seed_scalar_group(document);
    if(authored_path)seed_scalar_path(document);
    if(image_source)seed_scalar_image(document,image_dimension?scalar_field:"transform.tx");
    if(instance_source)seed_scalar_instance(document);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    if(affine_source){auto* matrix=window.findChild<QPushButton*>("transform-matrix-toggle");check(matrix,"Existing Affine matrix view toggle available");matrix->setChecked(true);events();}
    const Ref target{text.id,"",field}, source_ref{source.id,"",field};
    auto verify_anchor_placement=[&]{
        if(!anchor_source&&!opacity_source)return;
        const auto original_values=evaluate(document),current_values=evaluate(window.host.session.document());
        const auto original_transforms=evaluate_transforms(document,original_values),current_transforms=evaluate_transforms(window.host.session.document(),current_values);
        const auto original_bounds=scalar_world_bounds(document,text.id,original_values,original_transforms),current_bounds=scalar_world_bounds(window.host.session.document(),text.id,current_values,current_transforms);
        check(window.host.session.document().objects.at(text.id).transform==text.transform&&current_transforms.at(text.id).world==original_transforms.at(text.id).world,"Anchor edits preserve six authored Scalars and actual world matrix");
        check(original_bounds&&current_bounds&&std::tuple{original_bounds->left,original_bounds->top,original_bounds->right,original_bounds->bottom}==std::tuple{current_bounds->left,current_bounds->top,current_bounds->right,current_bounds->bottom},"Anchor edits preserve actual world geometric bounds");
    };
    if(instance_source&&mode=="valid"){auto unrelated=window.host.session.document();unrelated.objects.at("source").name="Independent same-revision occurrence edit";window.host.session=Session(unrelated);document=unrelated;}
    Session expected=window.host.session;auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");QLineEdit* size=nullptr;QPushButton* pick=nullptr;
    auto verify_circle_affine=[&]{
        if(!circle_transform&&!group_source&&!authored_path&&!image_source&&!instance_source)return;
        const auto actual_values=evaluate(window.host.session.document()),expected_values=evaluate(expected.document());
        const auto actual_transforms=evaluate_transforms(window.host.session.document(),actual_values),expected_transforms=evaluate_transforms(expected.document(),expected_values);
        const auto actual_bounds=scalar_world_bounds(window.host.session.document(),text.id,actual_values,actual_transforms),expected_bounds=scalar_world_bounds(expected.document(),text.id,expected_values,expected_transforms);
        check(actual_transforms.at(text.id).world==expected_transforms.at(text.id).world,"Circle authored translation world matrix equals canonical Session");
        check(actual_bounds&&expected_bounds&&std::tuple{actual_bounds->left,actual_bounds->top,actual_bounds->right,actual_bounds->bottom}==std::tuple{expected_bounds->left,expected_bounds->top,expected_bounds->right,expected_bounds->bottom},"Circle corrected world bounds equal canonical Session");
    };
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()==QString::fromStdString(field))size=input;
    }
    for(auto* button:window.findChildren<QPushButton*>("property-source-pick"))if(button->accessibleName()=="Pick source for "+label)pick=button;
    check(scroll&&size&&pick,"Existing Font size and numeric source picker available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    const bool pending=mode!="plain"&&mode!="generic";
    if(pending){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="out-of-range"?(image_dimension?"0":"1.2"):mode=="invalid"?"not-a-number":(mode=="topology-drop"?"6":pending_text));events();}
    check(size->hasFocus()&&size->isModified()==pending&&snapshot(window.host.session)==snapshot(expected),"Pending Font size remains full-state neutral before source picker");
    if(mode=="entry-revision"){window.host.session.apply({EditProperties{{{"text","",(group_source||authored_path||image_source||instance_source)?"transform.tx":(circle_transform||scalar_field.rfind("generator.",0)==0)?((polystar_source&&vertical)?"generator.center_x":"generator.center_y"):"text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;}
    if(mode=="entry-document"){auto incoming=document;incoming.id="incoming-pick-entry-document";window.host.session=Session(incoming);expected=window.host.session;}
    if((instance_source&&(mode=="entry-root"||mode=="entry-subtree"||mode=="entry-asset"||mode=="entry-override"||mode=="entry-visibility"||mode=="entry-color"||mode=="entry-content"))||mode=="entry-source"||mode=="entry-type"||(image_source&&(mode=="entry-asset"||(mode=="entry-dimensions"||mode=="entry-target-dimension")))){
        auto incoming=expected.document();auto& circle=incoming.objects.at("text");
        if(instance_source)replace_scalar_instance_context(incoming,mode);else if(image_source){if(mode=="entry-asset"||mode=="apply-asset"){auto& asset=incoming.raster_assets.at("image-shared-asset");asset.payload=make_raster(encode_raster_png(RasterPixels{2,1,{10,240,30,255,220,20,60,128}}));}else if((mode=="entry-dimensions"||mode=="entry-target-dimension")||(mode=="apply-dimensions"||mode=="apply-target-dimension"))(mode=="entry-target-dimension"||mode=="apply-target-dimension"?(scalar_field=="image.height"?circle.image->height:circle.image->width):(scalar_field=="image.width"?circle.image->height:circle.image->width)).literal=121;else if(mode=="entry-type"||mode=="apply-type"){circle.image.reset();make_scalar_path(circle);if(image_dimension)incoming.objects.at("image-ref-guard").source->parameters.at("width").binding->source={"text","","transform.tx"};}else {auto asset=incoming.raster_assets.at("image-shared-asset");asset.id="replacement-image-asset";incoming.raster_assets.emplace(asset.id,asset);circle.image->asset=asset.id;}}else if(authored_path){if(mode=="entry-type"||mode=="apply-type"){circle.contours.clear();circle.source=default_primitive("replacement-authored-path-source","nect.shape.circle");incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text","replacement-authored-path-source-east","x"};}else circle.contours.front().id="replacement-authored-contour";}else if(group_source){circle.children.clear();incoming.compositions.front().roots.push_back("text-child");if(mode=="entry-type"||mode=="apply-type"){circle.kind=Kind::path;circle.source=default_primitive("replacement-group-path-source","nect.shape.circle");}}else {
        if(mode=="entry-type"||mode=="apply-type")circle.source=default_primitive(circle.source->id,"nect.shape.circle");
        else circle.source->id="replacement-circle-source";
        circle.point_edit.reset();
        if(scalar_field=="generator.radius"&&!polystar_source&&incoming.objects.contains("source"))incoming.objects.at("source").source->parameters.at("center_x").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        if(incoming.objects.contains("circle-ref-guard"))incoming.objects.at("circle-ref-guard").source->parameters.at("width").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        }
        Session replacement(incoming);
        if(std::string("entry")=="apply")replacement.apply({EditProperties{{{"text","",(image_source&&image_dimension&&(mode=="apply-type"||mode=="apply-target-dimension"))?"transform.tx":(instance_source||image_source||((authored_path||group_source||star_source||count_source)&&mode!="apply-type"))?scalar_field:(mode=="apply-type"||(!rectangle_source))?"generator.radius":vertical?"generator.width":"generator.height"}},pending_value,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"incoming Circle source collides at exact revision");
        window.host.session=replacement;expected=replacement;
    }
    if(mode=="entry-session")window.host.session_id="incoming-pick-entry-session";
    const auto selection=window.canvas->selections();const auto position=pick->mapTo(&window,pick->rect().center());
    check(window.childAt(position)==pick,"Actual Window pointer hits numeric source picker");
    if(mode=="drag"||mode=="drag-cancel"){
        QTest::mousePress(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();expected.apply({EditProperties{{target},pending_value,false}},expected.revision());
        check(snapshot(window.host.session)==snapshot(expected),"Whip press prepares only its own scalar before target freeze");
        if(mode=="drag-cancel"){
            QTest::keyClick(&window,Qt::Key_Escape);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Whip Escape restores target with only independent scalar");
        }else{
            QTreeWidget* tree=nullptr;QTreeWidgetItem* source_item=nullptr;
            for(auto* candidate:window.findChildren<QTreeWidget*>())for(QTreeWidgetItemIterator it(candidate);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="source"){tree=candidate;source_item=*it;}
            check(tree&&source_item,"Exact other Text source row available for whip browsing");tree->scrollToItem(source_item);events();
            const auto hover=tree->viewport()->mapTo(&window,tree->visualItemRect(source_item).center());
            QTest::mouseMove(window.windowHandle(),hover,10);events();
            check(window.canvas->selected_object=="source"&&snapshot(window.host.session)==snapshot(expected),"Whip source browsing changes only view while target remains frozen");
            QLineEdit* source_field=nullptr;for(auto* input:window.findChildren<QLineEdit*>()){
                const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
                if(input->isVisible()&&ref.value("object").toString()=="source"&&ref.value("field").toString()==QString::fromStdString(field))source_field=input;
            }
            check(source_field,"Existing compatible other Text Font size source available for whip");scroll->ensureWidgetVisible(source_field);events();
            const auto drop=source_field->mapTo(&window,source_field->rect().center());check(window.childAt(drop)==source_field,"Actual Window drag reaches visible source field");
            QTest::mouseMove(window.windowHandle(),drop,10);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,drop);events();
            expected.apply({LinkProperties{{target},source_ref,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Prepared Text whip links exact compatible Ref and restores target");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Whip link Undo retains independent scalar");
        }
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Whip scalar Undo restores complete original source/history");return;
    }
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    QPointer<QDialog> picker=window.findChild<QDialog*>("property-source-picker");
    if(mode=="out-of-range"||mode=="invalid"||mode=="topology-drop"||mode.rfind("entry-",0)==0){
        check(!picker&&snapshot(window.host.session)==snapshot(expected),"Refused picker entry preserves incoming full source/history");
        check(window.statusBar()->currentMessage().contains(((instance_source&&(mode=="entry-root"||mode=="entry-subtree"||mode=="entry-asset"||mode=="entry-override"||mode=="entry-visibility"||mode=="entry-color"||mode=="entry-content"))||mode=="entry-source"||mode=="entry-type"||(image_source&&(mode=="entry-asset"||(mode=="entry-dimensions"||mode=="entry-target-dimension"))))?"PROPERTY_CONFLICT":mode=="topology-drop"?"MISSING_REFERENCE":mode=="out-of-range"?"OUT_OF_RANGE":mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),"Refused picker entry identifies exact cause");return;
    }
    if(pending)expected.apply({EditProperties{{target},pending_value,false}},expected.revision());
    std::cerr<<field<<" numeric picker mode="<<mode<<" picker="<<bool(picker)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
    check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"First source-picker pointer commits scalar and opens neutral picker");verify_anchor_placement();verify_circle_affine();
    auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();QListWidgetItem* item=nullptr;
    if(list)for(int i=0;i<list->count();++i){const auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(ref.value("object").toString()=="source"&&ref.value("field").toString()==QString::fromStdString(field))item=list->item(i);}
    check(list&&buttons&&item,"Numeric picker retains exact compatible source Ref");list->setCurrentItem(item);events();
    check(snapshot(window.host.session)==snapshot(expected),"Numeric source selection remains authored-state neutral");
    if(mode=="cancel"){
        buttons->button(QDialogButtonBox::Cancel)->click();events();check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Picker Cancel restores target and preserves only scalar");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Cancelled picker scalar independently restores original source");return;
    }
    if(mode=="apply-revision"){window.host.session.apply({EditProperties{{{"text","",(group_source||authored_path||image_source||instance_source)?"transform.tx":(circle_transform||scalar_field.rfind("generator.",0)==0)?((polystar_source&&vertical)?"generator.center_x":"generator.center_y"):"text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;}
    if(mode=="apply-document"){
        auto incoming=document;incoming.id="incoming-pick-apply-document";Session replacement(incoming);replacement.apply({EditProperties{{target},pending_value,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"Incoming picker document collides at exact revision");window.host.session=replacement;expected=replacement;
    }
    if((instance_source&&(mode=="apply-root"||mode=="apply-subtree"||mode=="apply-asset"||mode=="apply-override"||mode=="apply-visibility"||mode=="apply-color"||mode=="apply-content"))||mode=="apply-source"||mode=="apply-type"||(image_source&&(mode=="apply-asset"||(mode=="apply-dimensions"||mode=="apply-target-dimension")))){
        auto incoming=expected.document();auto& circle=incoming.objects.at("text");
        if(instance_source)replace_scalar_instance_context(incoming,mode);else if(image_source){if(mode=="entry-asset"||mode=="apply-asset"){auto& asset=incoming.raster_assets.at("image-shared-asset");asset.payload=make_raster(encode_raster_png(RasterPixels{2,1,{10,240,30,255,220,20,60,128}}));}else if((mode=="entry-dimensions"||mode=="entry-target-dimension")||(mode=="apply-dimensions"||mode=="apply-target-dimension"))(mode=="entry-target-dimension"||mode=="apply-target-dimension"?(scalar_field=="image.height"?circle.image->height:circle.image->width):(scalar_field=="image.width"?circle.image->height:circle.image->width)).literal=121;else if(mode=="entry-type"||mode=="apply-type"){circle.image.reset();make_scalar_path(circle);if(image_dimension)incoming.objects.at("image-ref-guard").source->parameters.at("width").binding->source={"text","","transform.tx"};}else {auto asset=incoming.raster_assets.at("image-shared-asset");asset.id="replacement-image-asset";incoming.raster_assets.emplace(asset.id,asset);circle.image->asset=asset.id;}}else if(authored_path){if(mode=="entry-type"||mode=="apply-type"){circle.contours.clear();circle.source=default_primitive("replacement-authored-path-source","nect.shape.circle");incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text","replacement-authored-path-source-east","x"};}else circle.contours.front().id="replacement-authored-contour";}else if(group_source){circle.children.clear();incoming.compositions.front().roots.push_back("text-child");if(mode=="entry-type"||mode=="apply-type"){circle.kind=Kind::path;circle.source=default_primitive("replacement-group-path-source","nect.shape.circle");}}else {
        if(mode=="entry-type"||mode=="apply-type")circle.source=default_primitive(circle.source->id,"nect.shape.circle");
        else circle.source->id="replacement-circle-source";
        circle.point_edit.reset();
        if(scalar_field=="generator.radius"&&!polystar_source&&incoming.objects.contains("source"))incoming.objects.at("source").source->parameters.at("center_x").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        if(incoming.objects.contains("circle-ref-guard"))incoming.objects.at("circle-ref-guard").source->parameters.at("width").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        }
        Session replacement(incoming);
        if(std::string("apply")=="apply")replacement.apply({EditProperties{{{"text","",(image_source&&image_dimension&&(mode=="apply-type"||mode=="apply-target-dimension"))?"transform.tx":(instance_source||image_source||((authored_path||group_source||star_source||count_source)&&mode!="apply-type"))?scalar_field:(mode=="apply-type"||(!rectangle_source))?"generator.radius":vertical?"generator.width":"generator.height"}},pending_value,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"incoming Circle source collides at exact revision");
        window.host.session=replacement;expected=replacement;
    }
    if(mode=="apply-session")window.host.session_id="incoming-pick-apply-session";
    if(mode=="apply-gesture"||mode=="apply-cancelled-gesture"){window.host.session.begin_gesture(window.host.session.revision());if(mode=="apply-cancelled-gesture")window.host.session.cancel_gesture();}
    if((circle_transform||scalar_field.rfind("generator.",0)==0)&&mode=="apply-gesture")window.host.session.update_gesture({EditProperties{{{"text","","generator.center_y"}},13,false}});
    if(group_source&&mode=="apply-gesture")window.host.session.update_gesture({EditProperties{{{"text-child","","generator.center_y"}},13,false}});
    if(authored_path&&mode=="apply-gesture")window.host.session.update_gesture({Set{{"text","text-first","y"},13}});
    if(instance_source&&mode=="apply-gesture")window.host.session.update_gesture({Set{{"instance-curve","instance-curve-first","y"},13}});
    if(image_source&&mode=="apply-gesture")window.host.session.update_gesture({Set{{"text","","image.width"},130}});
    if(!instance_source&&!image_source&&!authored_path&&!group_source&&!circle_transform&&(affine_linear||anchor_source||opacity_source)&&mode=="apply-gesture")window.host.session.update_gesture({EditProperties{{{"text","","text.tracking"}},13,false}});
    const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};

    if(mode.rfind("apply-",0)==0){
        const auto current_selection=window.canvas->selections();buttons->button(QDialogButtonBox::Ok)->click();events();
        check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Refused picker Apply preserves exact incoming full source/history");
        check(window.statusBar()->currentMessage().contains(((instance_source&&(mode=="apply-root"||mode=="apply-subtree"||mode=="apply-asset"||mode=="apply-override"||mode=="apply-visibility"||mode=="apply-color"||mode=="apply-content"))||mode=="apply-source"||mode=="apply-type"||(image_source&&(mode=="apply-asset"||(mode=="apply-dimensions"||mode=="apply-target-dimension"))))?"PROPERTY_CONFLICT":mode=="apply-gesture"?"GESTURE_ACTIVE":mode=="apply-revision"||mode=="apply-cancelled-gesture"?"REVISION_CONFLICT":"SESSION_CONFLICT"),"Refused picker Apply identifies exact cause");
        if(mode=="apply-document"||mode=="apply-session"){buttons->button(QDialogButtonBox::Cancel)->click();events();check(window.canvas->selections()==current_selection&&snapshot(window.host.session)==snapshot(expected),"Stale picker Cancel does not restore selection into replacement document");}
        if((circle_transform||scalar_field.rfind("generator.",0)==0)||image_dimension||affine_linear||anchor_source||opacity_source)check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Circle refusal preserves complete preview/gesture state");
        if(mode=="apply-gesture"){check(window.host.session.gesture_active(),"Picker refusal preserves active gesture");window.host.session.cancel_gesture();}return;
    }
    buttons->button(QDialogButtonBox::Ok)->click();events();expected.apply({LinkProperties{{target},source_ref,false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Numeric picker Apply equals canonical LinkProperties and restores frozen target");verify_anchor_placement();verify_circle_affine();
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Link Undo retains independent scalar");
    if(pending){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();}check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Scalar Undo restores full original Text source/style/history");
}

void scalar_fx_pending_pointer(const std::string& mode,const std::string& scalar_field="text.font_size",bool rectangle_centers=false,bool polygon_source=false,bool star_source=false,bool circle_transform=false,bool group_source=false,bool authored_path=false,bool image_source=false,bool instance_source=false){
    const bool polystar_source=polygon_source||star_source;
    const bool count_source=scalar_field=="generator.points";
    const bool image_dimension=scalar_field=="image.width"||scalar_field=="image.height";
    const bool opacity_source=scalar_field=="composite.opacity";
    const bool anchor_source=scalar_field=="transform.anchor_x"||scalar_field=="transform.anchor_y";
    const bool affine_linear=scalar_field=="transform.a"||scalar_field=="transform.b"||scalar_field=="transform.c"||scalar_field=="transform.d";
    const bool affine_source=scalar_field=="transform.tx"||scalar_field=="transform.ty"||affine_linear;
    const double pending_value=opacity_source?0.6:affine_linear?1.25:count_source?10.0:64.0;
    const char* pending_text=opacity_source?"0.6":affine_linear?"1.25":count_source?"10":"64";
    const bool rectangle_source=rectangle_centers||scalar_field=="generator.width"||scalar_field=="generator.height";
    const bool vertical=scalar_field=="generator.height"||((rectangle_centers||polystar_source)&&scalar_field=="generator.center_y");
    QTemporaryDir scratch;check(scratch.isValid(),"Text scalar fx entry owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("scalar-fx-document","composition","board");
    Object text;text.id="text";text.name="Font size fx target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    text.text->font_features={{"KERN",1,"whole_text"},{"lig ",4,"whole_text"}};text.text->additional_axis_values={{"wdth",87.1234567890123}};
    if(image_dimension||affine_source||anchor_source||opacity_source)text.transform={{{0.8,{}},{0.2,{}},{-0.3,{}},{1.1,{}},{60,{}},{50,{}}}};
    if(image_dimension||affine_linear||anchor_source||opacity_source)text.anchor={{{17,{}},{23,{}}}};
    if(opacity_source)text.compositing.opacity.literal=0.9;
    if((circle_transform||scalar_field.rfind("generator.",0)==0)){
        text.name=rectangle_source?"Retained Rectangle Width target":"Retained Circle scalar target";text.kind=Kind::path;text.text.reset();
        text.source=default_primitive("circle-target-source",rectangle_source?"nect.shape.rectangle":star_source?"nect.shape.star":polygon_source?"nect.shape.polygon":"nect.shape.circle");text.source->parameters.at(rectangle_source?((rectangle_centers&&!circle_transform)?scalar_field.substr(10):(vertical?"height":"width")):polystar_source?(circle_transform?(star_source?"outer_radius":"radius"):scalar_field.substr(10)):"radius").literal=count_source?5:60;
    }
    if(authored_path)make_scalar_path(text);
    if(image_source)make_scalar_image(text);
    if(instance_source)make_scalar_instance(text);
    if(group_source){text.kind=Kind::group;text.text.reset();text.children={"text-child"};}
    document.objects.emplace(text.id,text);document.compositions.front().roots={text.id};
    if((circle_transform||scalar_field.rfind("generator.",0)==0)){
        if(circle_transform||rectangle_source||polystar_source||(scalar_field!="generator.radius"&&document.objects.contains("source"))){
            Object guard;guard.id="circle-ref-guard";guard.name="Incoming stable point Ref guard";
            guard.source=default_primitive("circle-ref-guard-source","nect.shape.rectangle");
            document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
        }
        Session seeded(document);seeded.apply({AddOperation{"text",default_operation("circle-target-fill","nect.paint.fill"),0},Set{{"text",rectangle_source?"circle-target-source-top-left":polystar_source?"circle-target-source-outer-0-1":"circle-target-source-north",vertical?"y":"x"},10}},seeded.revision());
        if(circle_transform||rectangle_source||polystar_source||document.objects.contains("source"))seeded.apply({Link{{(scalar_field=="generator.radius"&&!polystar_source)?"source":"circle-ref-guard","",(scalar_field=="generator.radius"&&!polystar_source)?"generator.center_x":"generator.width"},{{"text",rectangle_source?(vertical?"circle-target-source-bottom-right":"circle-target-source-top-right"):polystar_source?(vertical?(star_source?"circle-target-source-outer-2-5":"circle-target-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"circle-target-source-outer-4-5":"circle-target-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"circle-target-source-inner-1-10":"circle-target-source-outer-1-5"):count_source?"circle-target-source-outer-1-5":"circle-target-source-outer-1-6"):"circle-target-source-east",vertical?"y":"x"},0.1,0,"copy_local_value"}}},seeded.revision());
        document=seeded.document();
    }
    if(group_source)seed_scalar_group(document);
    if(authored_path)seed_scalar_path(document);
    if(image_source)seed_scalar_image(document,image_dimension?scalar_field:"transform.tx");
    if(instance_source)seed_scalar_instance(document);
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    if(affine_source){auto* matrix=window.findChild<QPushButton*>("transform-matrix-toggle");check(matrix,"Existing Affine matrix view toggle available");matrix->setChecked(true);events();}
    const std::string field=mode=="other-field"?"text.tracking":scalar_field;
    const QString label=opacity_source?"Object opacity":anchor_source?(field=="transform.anchor_y"?"Anchor Y":"Anchor X"):affine_linear?QString::fromStdString(field.substr(10)):field=="transform.tx"?"tx":field=="transform.ty"?"ty":field=="generator.points"?"Points":field=="generator.inner_radius"?"Inner radius":field=="generator.outer_radius"?"Outer radius":field=="generator.rotation"?"Rotation":(field=="generator.height"||field=="image.height")?"Height":(field=="generator.width"||field=="image.width")?"Width":field=="generator.center_x"?"Center X":field=="generator.center_y"?"Center Y":field=="generator.radius"?"Radius":field=="text.origin_x"?"Origin X":field=="text.origin_y"?"Origin Y":field=="text.tracking"?"Tracking":field=="text.line_spacing"?"Line advance · 0 = auto":field=="text.frame_width"?"Frame width":field=="text.frame_height"?"Frame height":"Font size";
    auto verify_anchor_placement=[&]{
        if(!anchor_source&&!opacity_source)return;
        const auto original_values=evaluate(document),current_values=evaluate(window.host.session.document());
        const auto original_transforms=evaluate_transforms(document,original_values),current_transforms=evaluate_transforms(window.host.session.document(),current_values);
        const auto original_bounds=scalar_world_bounds(document,text.id,original_values,original_transforms),current_bounds=scalar_world_bounds(window.host.session.document(),text.id,current_values,current_transforms);
        check(window.host.session.document().objects.at(text.id).transform==text.transform&&current_transforms.at(text.id).world==original_transforms.at(text.id).world,"Anchor edits preserve six authored Scalars and actual world matrix");
        check(original_bounds&&current_bounds&&std::tuple{original_bounds->left,original_bounds->top,original_bounds->right,original_bounds->bottom}==std::tuple{current_bounds->left,current_bounds->top,current_bounds->right,current_bounds->bottom},"Anchor edits preserve actual world geometric bounds");
    };
    if(instance_source&&mode=="valid"){auto unrelated=window.host.session.document();unrelated.objects.at("source").name="Independent same-revision occurrence edit";window.host.session=Session(unrelated);document=unrelated;}
    Session expected=window.host.session;auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");QLineEdit* size=nullptr;QPointer<QPushButton> fx;
    auto verify_circle_affine=[&]{
        if(!circle_transform&&!group_source&&!authored_path&&!image_source&&!instance_source)return;
        const auto actual_values=evaluate(window.host.session.document()),expected_values=evaluate(expected.document());
        const auto actual_transforms=evaluate_transforms(window.host.session.document(),actual_values),expected_transforms=evaluate_transforms(expected.document(),expected_values);
        const auto actual_bounds=scalar_world_bounds(window.host.session.document(),text.id,actual_values,actual_transforms),expected_bounds=scalar_world_bounds(expected.document(),text.id,expected_values,expected_transforms);
        check(actual_transforms.at(text.id).world==expected_transforms.at(text.id).world,"Circle authored translation world matrix equals canonical Session");
        check(actual_bounds&&expected_bounds&&std::tuple{actual_bounds->left,actual_bounds->top,actual_bounds->right,actual_bounds->bottom}==std::tuple{expected_bounds->left,expected_bounds->top,expected_bounds->right,expected_bounds->bottom},"Circle corrected world bounds equal canonical Session");
    };
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()==QString::fromStdString(field))size=input;
    }
    for(auto* button:window.findChildren<QPushButton*>("property-expression"))if(button->accessibleName()==label+" expression editor")fx=button;
    check(scroll&&size&&fx,"Existing same-object Font size and inline fx controls available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));if((mode!="plain"&&mode!="other-field")){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="out-of-range"?(image_dimension?"0":"1.2"):mode=="invalid"?"not-a-number":mode=="expression-scalar"?(opacity_source?"=0.5 + 0.1":affine_linear?"=1 + 0.25":count_source?"=5 + 5":"=40 + 2"):(mode=="topology-drop"?"6":pending_text));events();}
    check(size->hasFocus()&&size->isModified()==((mode!="plain"&&mode!="other-field"))&&snapshot(window.host.session)==snapshot(expected),"Font size draft remains fully authored-state neutral");
    if(mode=="entry-revision"){window.host.session.apply({EditProperties{{{"text","",(group_source||authored_path||image_source||instance_source)?"transform.tx":(circle_transform||scalar_field.rfind("generator.",0)==0)?((polystar_source&&vertical)?"generator.center_x":"generator.center_y"):"text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;}
    if(mode=="entry-document"){auto incoming=document;incoming.id="incoming-fx-entry-document";window.host.session=Session(incoming);expected=window.host.session;}
    if((instance_source&&(mode=="entry-root"||mode=="entry-subtree"||mode=="entry-asset"||mode=="entry-override"||mode=="entry-visibility"||mode=="entry-color"||mode=="entry-content"))||mode=="entry-source"||mode=="entry-type"||(image_source&&(mode=="entry-asset"||(mode=="entry-dimensions"||mode=="entry-target-dimension")))){
        auto incoming=expected.document();auto& circle=incoming.objects.at("text");
        if(instance_source)replace_scalar_instance_context(incoming,mode);else if(image_source){if(mode=="entry-asset"||mode=="apply-asset"){auto& asset=incoming.raster_assets.at("image-shared-asset");asset.payload=make_raster(encode_raster_png(RasterPixels{2,1,{10,240,30,255,220,20,60,128}}));}else if((mode=="entry-dimensions"||mode=="entry-target-dimension")||(mode=="apply-dimensions"||mode=="apply-target-dimension"))(mode=="entry-target-dimension"||mode=="apply-target-dimension"?(scalar_field=="image.height"?circle.image->height:circle.image->width):(scalar_field=="image.width"?circle.image->height:circle.image->width)).literal=121;else if(mode=="entry-type"||mode=="apply-type"){circle.image.reset();make_scalar_path(circle);if(image_dimension)incoming.objects.at("image-ref-guard").source->parameters.at("width").binding->source={"text","","transform.tx"};}else {auto asset=incoming.raster_assets.at("image-shared-asset");asset.id="replacement-image-asset";incoming.raster_assets.emplace(asset.id,asset);circle.image->asset=asset.id;}}else if(authored_path){if(mode=="entry-type"||mode=="apply-type"){circle.contours.clear();circle.source=default_primitive("replacement-authored-path-source","nect.shape.circle");incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text","replacement-authored-path-source-east","x"};}else circle.contours.front().id="replacement-authored-contour";}else if(group_source){circle.children.clear();incoming.compositions.front().roots.push_back("text-child");if(mode=="entry-type"||mode=="apply-type"){circle.kind=Kind::path;circle.source=default_primitive("replacement-group-path-source","nect.shape.circle");}}else {
        if(mode=="entry-type"||mode=="apply-type")circle.source=default_primitive(circle.source->id,"nect.shape.circle");
        else circle.source->id="replacement-circle-source";
        circle.point_edit.reset();
        if(scalar_field=="generator.radius"&&!polystar_source&&incoming.objects.contains("source"))incoming.objects.at("source").source->parameters.at("center_x").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        if(incoming.objects.contains("circle-ref-guard"))incoming.objects.at("circle-ref-guard").source->parameters.at("width").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        }
        Session replacement(incoming);
        if(std::string("entry")=="apply")replacement.apply({EditProperties{{{"text","",(image_source&&image_dimension&&(mode=="apply-type"||mode=="apply-target-dimension"))?"transform.tx":(instance_source||image_source||((authored_path||group_source||star_source||count_source)&&mode!="apply-type"))?scalar_field:(mode=="apply-type"||(!rectangle_source))?"generator.radius":vertical?"generator.width":"generator.height"}},pending_value,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"incoming Circle source collides at exact revision");
        window.host.session=replacement;expected=replacement;
    }
    if(mode=="entry-session")window.host.session_id="incoming-fx-entry-session";
    check(bool(fx),"Original fx control remains alive before the bound pointer entry");
    const auto position=fx->mapTo(&window,fx->rect().center());check(window.childAt(position)==fx,"Actual Window pointer hits existing Font size fx");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="out-of-range"||mode=="invalid"||mode=="topology-drop"||mode.rfind("entry-",0)==0){
        check(snapshot(window.host.session)==snapshot(expected),"Rejected fx entry preserves full incoming source/history without scalar mutation");
        if(opacity_source)std::cerr<<"Opacity fx refusal mode="<<mode<<" status="<<window.statusBar()->currentMessage().toStdString()<<"\n";
        check(window.statusBar()->currentMessage().contains(((instance_source&&(mode=="entry-root"||mode=="entry-subtree"||mode=="entry-asset"||mode=="entry-override"||mode=="entry-visibility"||mode=="entry-color"||mode=="entry-content"))||mode=="entry-source"||mode=="entry-type"||(image_source&&(mode=="entry-asset"||(mode=="entry-dimensions"||mode=="entry-target-dimension"))))?"PROPERTY_CONFLICT":mode=="topology-drop"?"MISSING_REFERENCE":mode=="out-of-range"?"OUT_OF_RANGE":mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),"Rejected fx entry reports exact bound cause");return;
    }
    if(mode=="expression-scalar")expected.apply({SetExpression{{{"text","",field}},{(opacity_source?"0.5 + 0.1":affine_linear?"1 + 0.25":count_source?"5 + 5":"40 + 2"),1},false}},expected.revision());
    else if((mode!="plain"&&mode!="other-field"))expected.apply({EditProperties{{{"text","",field}},pending_value,false}},expected.revision());
    QPlainTextEdit* editor=nullptr;for(auto* candidate:window.findChildren<QPlainTextEdit*>())if(candidate->accessibleName()==label+" expression")editor=candidate;
    std::cerr<<field<<" fx panel="<<(editor!=nullptr)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
    check(editor&&editor->isVisible()&&snapshot(window.host.session)==snapshot(expected),"First fx pointer commits only scalar and opens existing inline neutral expression editor");verify_anchor_placement();verify_circle_affine();
    auto* panel=window.findChild<QWidget*>("nect-expression-panel");QPushButton* apply=nullptr;
    for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;
    check(apply,"Existing inline Apply available");
    check(editor->toPlainText()==(mode=="expression-scalar"?(opacity_source?"0.5 + 0.1":affine_linear?"1 + 0.25":count_source?"5 + 5":"40 + 2"):(mode=="plain"||mode=="other-field")?QString::number(evaluate(expected.document()).at({"text","",field}),'g',17):(mode=="topology-drop"?QString("6"):opacity_source?QString::number(pending_value,'g',17):QString(pending_text))),"Inline initial source follows exactly its own committed scalar or expression");
    editor->setPlainText(mode=="out-of-range-expression"?(image_dimension?"-1":"1.2"):mode=="invalid-expression"?"broken(":(opacity_source?"0.25 + 0.1":affine_linear?"0.75 + 0.125":count_source?"10 + 5":"32 + 3"));events();
    check(snapshot(window.host.session)==snapshot(expected),"Inline expression draft is fully source/history neutral");scroll->ensureWidgetVisible(apply);events();
    if(mode=="apply-revision"){window.host.session.apply({EditProperties{{{"text","",(group_source||authored_path||image_source||instance_source)?"transform.tx":(circle_transform||scalar_field.rfind("generator.",0)==0)?((polystar_source&&vertical)?"generator.center_x":"generator.center_y"):"text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;}
    if(mode=="apply-document"){
        auto incoming=document;incoming.id="incoming-fx-apply-document";Session replacement(incoming);replacement.apply({EditProperties{{{"text","",field}},pending_value,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"Incoming fx Document collides at exact revision");window.host.session=replacement;expected=replacement;
    }
    if((instance_source&&(mode=="apply-root"||mode=="apply-subtree"||mode=="apply-asset"||mode=="apply-override"||mode=="apply-visibility"||mode=="apply-color"||mode=="apply-content"))||mode=="apply-source"||mode=="apply-type"||(image_source&&(mode=="apply-asset"||(mode=="apply-dimensions"||mode=="apply-target-dimension")))){
        auto incoming=expected.document();auto& circle=incoming.objects.at("text");
        if(instance_source)replace_scalar_instance_context(incoming,mode);else if(image_source){if(mode=="entry-asset"||mode=="apply-asset"){auto& asset=incoming.raster_assets.at("image-shared-asset");asset.payload=make_raster(encode_raster_png(RasterPixels{2,1,{10,240,30,255,220,20,60,128}}));}else if((mode=="entry-dimensions"||mode=="entry-target-dimension")||(mode=="apply-dimensions"||mode=="apply-target-dimension"))(mode=="entry-target-dimension"||mode=="apply-target-dimension"?(scalar_field=="image.height"?circle.image->height:circle.image->width):(scalar_field=="image.width"?circle.image->height:circle.image->width)).literal=121;else if(mode=="entry-type"||mode=="apply-type"){circle.image.reset();make_scalar_path(circle);if(image_dimension)incoming.objects.at("image-ref-guard").source->parameters.at("width").binding->source={"text","","transform.tx"};}else {auto asset=incoming.raster_assets.at("image-shared-asset");asset.id="replacement-image-asset";incoming.raster_assets.emplace(asset.id,asset);circle.image->asset=asset.id;}}else if(authored_path){if(mode=="entry-type"||mode=="apply-type"){circle.contours.clear();circle.source=default_primitive("replacement-authored-path-source","nect.shape.circle");incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text","replacement-authored-path-source-east","x"};}else circle.contours.front().id="replacement-authored-contour";}else if(group_source){circle.children.clear();incoming.compositions.front().roots.push_back("text-child");if(mode=="entry-type"||mode=="apply-type"){circle.kind=Kind::path;circle.source=default_primitive("replacement-group-path-source","nect.shape.circle");}}else {
        if(mode=="entry-type"||mode=="apply-type")circle.source=default_primitive(circle.source->id,"nect.shape.circle");
        else circle.source->id="replacement-circle-source";
        circle.point_edit.reset();
        if(scalar_field=="generator.radius"&&!polystar_source&&incoming.objects.contains("source"))incoming.objects.at("source").source->parameters.at("center_x").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        if(incoming.objects.contains("circle-ref-guard"))incoming.objects.at("circle-ref-guard").source->parameters.at("width").binding->source={"text",(mode=="entry-type"||mode=="apply-type")?(vertical?"circle-target-source-south":"circle-target-source-east"):rectangle_source?(vertical?"replacement-circle-source-bottom-right":"replacement-circle-source-top-right"):polystar_source?(vertical?(star_source?"replacement-circle-source-outer-2-5":"replacement-circle-source-outer-1-3"):scalar_field=="generator.rotation"?(star_source?"replacement-circle-source-outer-4-5":"replacement-circle-source-outer-5-6"):star_source?(scalar_field=="generator.inner_radius"?"replacement-circle-source-inner-1-10":"replacement-circle-source-outer-1-5"):count_source?"replacement-circle-source-outer-1-5":"replacement-circle-source-outer-1-6"):"replacement-circle-source-east",vertical?"y":"x"};
        }
        Session replacement(incoming);
        if(std::string("apply")=="apply")replacement.apply({EditProperties{{{"text","",(image_source&&image_dimension&&(mode=="apply-type"||mode=="apply-target-dimension"))?"transform.tx":(instance_source||image_source||((authored_path||group_source||star_source||count_source)&&mode!="apply-type"))?scalar_field:(mode=="apply-type"||(!rectangle_source))?"generator.radius":vertical?"generator.width":"generator.height"}},pending_value,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"incoming Circle source collides at exact revision");
        window.host.session=replacement;expected=replacement;
    }
    if(mode=="apply-session")window.host.session_id="incoming-fx-apply-session";
    if(mode=="apply-gesture"||mode=="apply-cancelled-gesture"){
        window.host.session.begin_gesture(window.host.session.revision());if(mode=="apply-cancelled-gesture")window.host.session.cancel_gesture();
    }
    if((circle_transform||scalar_field.rfind("generator.",0)==0)&&mode=="apply-gesture")window.host.session.update_gesture({EditProperties{{{"text","","generator.center_y"}},13,false}});
    if(group_source&&mode=="apply-gesture")window.host.session.update_gesture({EditProperties{{{"text-child","","generator.center_y"}},13,false}});
    if(authored_path&&mode=="apply-gesture")window.host.session.update_gesture({Set{{"text","text-first","y"},13}});
    if(instance_source&&mode=="apply-gesture")window.host.session.update_gesture({Set{{"instance-curve","instance-curve-first","y"},13}});
    if(image_source&&mode=="apply-gesture")window.host.session.update_gesture({Set{{"text","","image.width"},130}});
    if(!instance_source&&!image_source&&!authored_path&&!group_source&&!circle_transform&&(affine_linear||anchor_source||opacity_source)&&mode=="apply-gesture")window.host.session.update_gesture({EditProperties{{{"text","","text.tracking"}},13,false}});
    const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};

    if(mode=="apply-selection"){
        window.canvas->set_selections({});events();window.canvas->set_selection(text.id);events();
        panel=window.findChild<QWidget*>("nect-expression-panel");check(panel,"Retained draft reappears after exact whole selection roundtrip");
        apply=nullptr;for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;
        check(apply,"Recreated inline Apply exists at selection boundary");scroll->ensureWidgetVisible(apply);events();
    }
    if(mode=="cancel")for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Cancel")apply=button;
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(&window,apply->rect().center()));events();
    if(mode.rfind("apply-",0)==0||mode=="invalid-expression"||mode=="out-of-range-expression"){
        check(snapshot(window.host.session)==snapshot(expected),"Refused inline Apply preserves full incoming source/history and neutral draft");
        auto* result=window.findChild<QLabel*>("nect-expression-result");check(result&&result->text().contains("Committed result is unchanged"),"Refused inline Apply keeps existing visible recovery status");
        if((circle_transform||scalar_field.rfind("generator.",0)==0)||image_dimension||affine_linear||anchor_source||opacity_source)check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Circle refusal preserves complete preview/gesture state");
        if(mode=="apply-gesture"){check(window.host.session.gesture_active(),"Inline rejection retains active gesture");window.host.session.cancel_gesture();}
        return;
    }
    if(mode=="cancel"){
        check(snapshot(window.host.session)==snapshot(expected)&&!window.findChild<QWidget*>("nect-expression-panel"),"Cancel discards expression draft and preserves only independent scalar");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Cancelled expression leaves scalar independently undoable");return;
    }
    expected.apply({SetExpression{{{"text","",field}},{(opacity_source?"0.25 + 0.1":affine_linear?"0.75 + 0.125":count_source?"10 + 5":"32 + 3"),1},false}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Inline Apply equals canonical scalar then expression source/history");verify_anchor_placement();verify_circle_affine();
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Expression Undo retains independent scalar");
    if((mode!="plain"&&mode!="other-field")){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();}check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo restores exact original Text source/style/history");
}


void authored_point_coordinate_pointer(const std::string& field,bool picking,const std::string& mode="valid",bool generated_circle=false,bool canvas_selection=false){
    const Id target_point=generated_circle?"circle-point-source-east":"text-second",first_point=generated_circle?"circle-point-source-north":"text-first";
    const bool handle=field.starts_with("in.")||field.starts_with("out.");
    auto point_scalar=[&](Point& point)->Scalar&{
        if(field=="x")return point.x;if(field=="y")return point.y;
        if(field=="in.angle")return point.in_angle;if(field=="in.length")return point.in_length;
        if(field=="out.angle")return point.out_angle;if(field=="out.length")return point.out_length;
        throw std::runtime_error("Unexpected canonical point scalar");
    };
    QTemporaryDir scratch;check(scratch.isValid(),"Point coordinate owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("authored-point-entry","composition","board");
    Object target;target.id="text";target.name="Authored target";if(generated_circle){target.kind=Kind::path;target.source=default_primitive("circle-point-source","nect.shape.circle");}else make_scalar_path(target);
    target.transform={{{0.8,{}},{0.2,{}},{-0.3,{}},{1.1,{}},{60,{}},{50,{}}}};target.anchor={{{17,{}},{23,{}}}};
    Object source;source.id="source";source.name="Other authored source";make_scalar_path(source);source.contours.front().points.at(1).x.literal=72;source.contours.front().points.at(1).y.literal=90;
    if(handle)point_scalar(source.contours.front().points.at(1)).literal=field=="in.angle"?450.5:field=="out.angle"?-725.25:field=="in.length"?36:28;
    document.objects.emplace(target.id,target);document.objects.emplace(source.id,source);document.compositions.front().roots={target.id,source.id};if(generated_circle){
        Object guard;guard.id="path-ref-guard";guard.source=default_primitive("circle-point-guard-source","nect.shape.rectangle");document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
        Session setup(document);setup.apply({AddOperation{"text",default_operation("circle-point-fill","nect.paint.fill"),0},AddOperation{"text",default_operation("circle-point-stroke","nect.paint.stroke"),1}},setup.revision());
        if(mode!="fresh")setup.apply({Set{{"text",first_point,"x"},87},Set{{"text",first_point,"out.length"},31}},setup.revision());
        if(mode=="point-edit"||mode=="bypassed")setup.apply({Set{{"text",target_point,field},55}},setup.revision());
        setup.apply({Link{{"path-ref-guard","","generator.width"},{{"text",target_point,"x"},0.1,20,"copy_local_value"}}},setup.revision());document=setup.document();
        if(mode=="bypassed")document.objects.at("text").point_edit->enabled=false;
    }else seed_scalar_path(document);
    Session seed(document);seed.apply({Link{{"path-ref-guard","","generator.height"},{{"text",target_point,"y"},0.1,20,"copy_local_value"}}},seed.revision());document=seed.document();
    if(handle){
        Object guard;guard.id="path-handle-guard";guard.name="Incoming handle Ref guard";make_scalar_path(guard);
        document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
        Session linked(document);linked.apply({Link{{guard.id,guard.id+"-first",field},{{"text",target_point,field},0.1,20,"copy_local_value"}}},linked.revision());document=linked.document();
    }
    const Ref ref{"text",target_point,field};Ref source_ref{"source","source-second",field};
    if(mode=="cycle"){Session cycle(document);cycle.apply({Link{{"source","source-second",field},{{"text",target_point,field},1,0,"copy_local_value"}}},cycle.revision());document=cycle.document();}
    if(mode=="unit")source_ref={"source","","composite.opacity"};
    if(generated_circle&&(mode=="entry-point-edit-reset"||mode=="apply-point-edit-reset")){
        // A correction ID cannot be replaced independently of its Source. Test
        // that invalid fixture explicitly, then use a valid reset for UI refusal.
        auto invalid=document;invalid.objects.at("text").point_edit->id="replacement-point-edit";
        bool rejected=false;try{Session replacement(invalid);}catch(const Error& error){rejected=error.code=="INVALID_POINT_EDIT";}
        check(rejected,"Canonical Session rejects Point Edit identity detached from retained Source");
    }
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(ref.object,ref.point);events();
    Session expected=window.host.session;
    if(mode=="canvas-selection"||canvas_selection){
        window.canvas->set_selection(ref.object);window.canvas->fit_artboard();events();click(window,"tool-direct-selection");
        const auto values=evaluate(document);const auto world=evaluate_transforms(document,values).at(ref.object).world;
        const auto px=values.at({ref.object,ref.point,"x"}),py=values.at({ref.object,ref.point,"y"});
        const auto& board=document.compositions.front().artboards.front();
        const QPoint at(qRound(window.canvas->width()/2.0+(world[0]*px+world[2]*py+world[4]-board.width/2.0)*window.canvas->zoom()),
            qRound(window.canvas->height()/2.0+(world[1]*px+world[3]*py+world[5]-board.height/2.0)*window.canvas->zoom()));
        QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,window.canvas->mapTo(&window,at));events();
        check(window.canvas->selected_object==ref.object&&window.canvas->selected_point==ref.point,"Actual Canvas click/release selects exact stable point");
        check(!window.host.session.gesture_active()&&snapshot(window.host.session)==snapshot(expected),"Empty selection gesture leaves full authored state/history neutral");
    }
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");QLineEdit* input=nullptr;QPointer<QPushButton> action;
    for(auto* candidate:window.findChildren<QLineEdit*>()){
        const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
        if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString()==QString::fromStdString(target_point)&&data.value("field").toString()==QString::fromStdString(field))input=candidate;
    }
    for(auto* button:window.findChildren<QPushButton*>(picking?"property-source-pick":"property-expression"))
        if(button->accessibleName()==(picking?"Pick source for "+QString::fromStdString(field):QString::fromStdString(field)+" expression editor"))action=button;
    check(scroll&&input&&action,"Exact stable point coordinate input/action exists");scroll->ensureWidgetVisible(input);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,input->mapTo(&window,input->rect().center()));
    const double pending_value=mode=="zero"?0:mode=="signed"?-64:mode=="unwrapped"?450.5:64;
    const QString pending_text=mode=="zero"?"0":mode=="signed"?"-64":mode=="unwrapped"?"450.5":"64";
    const QString expression_text=mode=="signed"?"-32 - 3":mode=="unwrapped"?"360 + 45":"32 + 3";
    QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,mode=="invalid"?QString("not-a-number"):mode=="negative"?QString("-1"):mode=="expression-scalar"?QString("=40 + 2"):pending_text);events();
    check(input->hasFocus()&&input->isModified()&&snapshot(window.host.session)==snapshot(expected),"Selected point draft is full-state neutral");
    auto change_context=[&](bool applying){
        const auto suffix=mode.substr(mode.find('-')+1);
        if(suffix=="session"){window.host.session_id="replacement-point-session";return;}
        if(suffix=="revision")window.host.session.apply({Set{{"text","","transform.tx"},61}},window.host.session.revision());
        else if(suffix=="gesture"||suffix=="cancelled-gesture"){
            window.host.session.begin_gesture(window.host.session.revision());
            window.host.session.update_gesture({Set{{"text",first_point,"y"},13}});
            if(suffix=="cancelled-gesture")window.host.session.cancel_gesture();
        }else if(suffix=="selection"){
            window.canvas->set_selection("text",first_point);events();window.canvas->set_selection(ref.object,ref.point);events();
        }else{
            auto incoming=window.host.session.document();auto& object=incoming.objects.at("text");
            if(generated_circle&&suffix!="document") {
                if(suffix=="source")object.source->parameters.at("radius").literal=76;
                else if(suffix=="source-id"||suffix=="type"||suffix=="missing-point") {
                    const bool circle=suffix=="source-id";object.source=default_primitive("replacement-point-source",circle?"nect.shape.circle":"nect.shape.rectangle");object.point_edit.reset();
                    const Id replacement=circle?"replacement-point-source-east":"replacement-point-source-top-right";
                    auto& guard=*incoming.objects.at("path-ref-guard").source;guard.parameters.at("width").binding->source.point=replacement;guard.parameters.at("height").binding->source.point=replacement;
                }else if(suffix=="coordinate"||suffix=="handle") {
                    Session edit(incoming);edit.apply({Set{{"text",suffix=="coordinate"?target_point:first_point,suffix=="coordinate"?field:"out.length"},suffix=="coordinate"?65.0:26.0}},edit.revision());incoming=edit.document();
                }else if(suffix=="point-edit-reset")object.point_edit.reset();
                else if(suffix=="point-edit-enabled")object.point_edit->enabled=!object.point_edit->enabled;
                else throw std::runtime_error("Unknown generated Circle point context fixture");
            }else if(suffix=="document")incoming.id="replacement-point-document";
            else if(suffix=="source")object.contours.front().id="replacement-point-contour";
            else if(suffix=="coordinate")point_scalar(object.contours.front().points.at(1)).literal=65;
            else if(suffix=="handle")object.contours.front().points.front().out_length.literal=26;
            else if(suffix=="type"||suffix=="missing-point"){
                const Id replacement=suffix=="type"?"replacement-point-source-east":"replacement-second";
                if(suffix=="type"){object.contours.clear();object.source=default_primitive("replacement-point-source","nect.shape.circle");}
                else object.contours.front().points.at(1).id=replacement;
                auto& guard=*incoming.objects.at("path-ref-guard").source;
                guard.parameters.at("width").binding->source={"text",replacement,"x"};guard.parameters.at("height").binding->source={"text",replacement,"y"};
                if(handle)point_scalar(incoming.objects.at("path-handle-guard").contours.front().points.front()).binding->source.point=replacement;
            }else throw std::runtime_error("Unknown point context fixture");
            Session replacement(incoming);
            if(applying)replacement.apply({Set{{"text","","transform.tx"},61}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Point replacement collides at captured revision without retargeting Ref");
            window.host.session=replacement;
        }
        expected=window.host.session;
    };
    if(mode.rfind("entry-",0)==0)change_context(false);
    const auto preview_before_entry=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
    const auto selection=window.canvas->selections();
    const auto position=action->mapTo(&window,action->rect().center());check(window.childAt(position)==action,"Actual Window pointer hits selected point action");
    auto scalar_expected=[&]{if(mode=="expression-scalar")expected.apply({SetExpression{{ref},{"40 + 2",1},false}},expected.revision());else expected.apply({EditProperties{{ref},pending_value,false}},expected.revision());};
    auto undo_scalar=[&]{window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate point scalar Undo restores exact curve/IDs/handles/paint/Refs");};
    if(mode=="drag"||mode=="drag-cancel"){
        QTest::mousePress(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();scalar_expected();
        check(snapshot(window.host.session)==snapshot(expected),"Point whip press commits only independent coordinate before freezing target");
        if(mode=="drag-cancel"){
            QTest::keyClick(&window,Qt::Key_Escape);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Point whip Escape retains scalar and exact target selection");
        }else{
            QTreeWidget* tree=nullptr;QTreeWidgetItem* source_item=nullptr;
            for(auto* candidate:window.findChildren<QTreeWidget*>())for(QTreeWidgetItemIterator it(candidate);*it;++it)
                if((*it)->data(0,Qt::UserRole).toString()=="source"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty()){tree=candidate;source_item=*it;}
            check(tree&&source_item,"Point whip finds exact other authored Path tree row");tree->scrollToItem(source_item);events();
            QTest::mouseMove(window.windowHandle(),tree->viewport()->mapTo(&window,tree->visualItemRect(source_item).center()),10);events();
            check(window.canvas->selected_object=="source"&&window.canvas->selected_point=="source-first"&&snapshot(window.host.session)==snapshot(expected),"Point whip browsing selects stable first source point with no authored mutation");
            QLineEdit* source_field=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
                const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
                if(candidate->isVisible()&&data.value("object").toString()=="source"&&data.value("point").toString()=="source-first"&&data.value("field").toString()==QString::fromStdString(field))source_field=candidate;
            }
            check(source_field,"Whip drop has exact stable source point coordinate");scroll->ensureWidgetVisible(source_field);events();
            const auto drop=source_field->mapTo(&window,source_field->rect().center());check(window.childAt(drop)==source_field,"Actual point whip drop hits visible coordinate");
            QTest::mouseMove(window.windowHandle(),drop,10);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,drop);events();
            source_ref.point="source-first";expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Point whip links exact Ref and restores frozen target");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Point whip link Undo retains independent scalar");
        }
        undo_scalar();return;
    }
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode=="negative"||mode.rfind("entry-",0)==0){
        check(!window.findChild<QDialog*>("property-source-picker")&&!window.findChild<QWidget*>("nect-expression-panel")&&snapshot(window.host.session)==snapshot(expected),"Refused point entry preserves complete incoming authored state and opens no editor");
        const auto message=window.statusBar()->currentMessage();
        const auto reason=mode=="negative"?"OUT_OF_RANGE":mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"||mode=="entry-cancelled-gesture"?"REVISION_CONFLICT":mode=="entry-session"||mode=="entry-document"?"SESSION_CONFLICT":mode=="entry-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
        check(message.contains(reason),"Point entry refusal identifies exact cause");
        check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before_entry,"Point entry refusal preserves preview and gesture state");
        if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
    }
    scalar_expected();
    if(picking){
        QPointer<QDialog> picker=window.findChild<QDialog*>("property-source-picker");
        std::cerr<<field<<" point pick="<<bool(picker)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"First point picker commits only selected coordinate and opens neutral picker");
        auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();QListWidgetItem* item=nullptr;
        if(list)for(int i=0;i<list->count();++i){const auto data=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(data.value("object").toString()==QString::fromStdString(source_ref.object)&&data.value("point").toString()==QString::fromStdString(source_ref.point)&&data.value("field").toString()==QString::fromStdString(source_ref.field))item=list->item(i);}
        check(list&&buttons&&item,"Picker retains exact other stable point coordinate Ref");list->setCurrentItem(item);events();
        check(snapshot(window.host.session)==snapshot(expected),"Choosing other stable point is authored-state neutral");
        if(mode=="cancel"){buttons->button(QDialogButtonBox::Cancel)->click();events();check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Point picker Cancel discards only link draft");undo_scalar();return;}
        if(mode.rfind("apply-",0)==0)change_context(true);
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        buttons->button(QDialogButtonBox::Ok)->click();events();
        if(mode=="cycle"||mode=="unit") {
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Generated point cycle/unit refusal is full-state atomic");
            check(window.statusBar()->currentMessage().contains(mode=="cycle"?"CYCLE":"NO_SOURCE"),"Generated point cycle/unit refusal identifies cause");
            buttons->button(QDialogButtonBox::Cancel)->click();events();undo_scalar();return;
        }
        if(mode.rfind("apply-",0)==0){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Refused point picker Apply preserves incoming source and complete history");
            const auto reason=mode=="apply-revision"||mode=="apply-cancelled-gesture"?"REVISION_CONFLICT":mode=="apply-session"||mode=="apply-document"?"SESSION_CONFLICT":mode=="apply-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
            check(window.statusBar()->currentMessage().contains(reason),"Point picker refusal identifies exact cause");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Point picker refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
    }else{
        QPlainTextEdit* editor=nullptr;for(auto* candidate:window.findChildren<QPlainTextEdit*>())if(candidate->accessibleName()==QString::fromStdString(field)+" expression")editor=candidate;
        std::cerr<<field<<" point fx="<<bool(editor)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(editor&&editor->isVisible()&&editor->toPlainText()==(mode=="expression-scalar"?QString("40 + 2"):pending_text)&&snapshot(window.host.session)==snapshot(expected),"First point fx commits coordinate and opens exact neutral editor");
        editor->setPlainText(mode=="invalid-expression"?QString("broken("):mode=="negative-expression"?QString("-1"):expression_text);events();check(snapshot(window.host.session)==snapshot(expected),"Point expression draft remains neutral");
        QPushButton* apply=nullptr;for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;
        check(apply,"Point inline Apply exists");
        if(mode.rfind("apply-",0)==0)change_context(true);
        if(mode=="apply-selection"){
            auto* panel=window.findChild<QWidget*>("nect-expression-panel");check(panel,"Point expression draft survives exact stable selection roundtrip");
            apply=nullptr;for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;check(apply,"Point selection roundtrip recreated Apply");
        }
        if(mode=="cancel")for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Cancel")apply=button;
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        scroll->ensureWidgetVisible(apply);events();QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(&window,apply->rect().center()));events();
        if(mode.rfind("apply-",0)==0||mode=="invalid-expression"||mode=="negative-expression"){
            check(snapshot(window.host.session)==snapshot(expected),"Refused point fx Apply preserves incoming source and complete history");
            auto* result=window.findChild<QLabel*>("nect-expression-result");check(result&&result->text().contains("Committed result is unchanged"),"Point expression refusal retains visible recovery status");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Point fx refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        if(mode=="cancel"){check(snapshot(window.host.session)==snapshot(expected)&&!window.findChild<QWidget*>("nect-expression-panel"),"Point expression Cancel discards only expression draft");undo_scalar();return;}
        expected.apply({SetExpression{{ref},{expression_text.toStdString(),1},false}},expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Point operation exactly matches canonical Document/encode/history/revision");
    if(handle){
        QLineEdit* result=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
            const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
            if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString()==QString::fromStdString(target_point)&&data.value("field").toString()==QString::fromStdString(field))result=candidate;
        }
        check(result&&result->text().toDouble()==evaluate(expected.document()).at(ref),"Handle numeric field retains raw unwrapped canonical value without dial normalization");
    }
    const auto actual_values=evaluate(window.host.session.document()),expected_values=evaluate(expected.document());
    const auto actual_transforms=evaluate_transforms(window.host.session.document(),actual_values),expected_transforms=evaluate_transforms(expected.document(),expected_values);
    const auto actual_bounds=object_bounds(window.host.session.document(),"text",actual_values,actual_transforms,true),expected_bounds=object_bounds(expected.document(),"text",expected_values,expected_transforms,true);
    check(actual_transforms.at("text").world==expected_transforms.at("text").world,"Point edit retains canonical nonidentity world matrix");
    check(actual_bounds&&expected_bounds&&std::tuple{actual_bounds->left,actual_bounds->top,actual_bounds->right,actual_bounds->bottom}==std::tuple{expected_bounds->left,expected_bounds->top,expected_bounds->right,expected_bounds->bottom},"Point edit bounds match canonical curve geometry");
    for(int i=0;i<2;++i){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Point scalar and later operation undo independently");}
    check(expected.document()==document,"Two Undo restore exact authored curve/IDs/handles/paint/Ref and both sources");
}

void authored_stroke_width_pointer(bool picking,const std::string& mode="valid"){
    const bool handle=false;const std::string field="op.path-target-stroke.width";const QString label="Width";
    QTemporaryDir scratch;check(scratch.isValid(),"Stroke width owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("authored-stroke-entry","composition","board");
    Object target;target.id="text";target.name="Authored target";make_scalar_path(target);
    target.transform={{{0.8,{}},{0.2,{}},{-0.3,{}},{1.1,{}},{60,{}},{50,{}}}};target.anchor={{{17,{}},{23,{}}}};
    Object source;source.id="source";source.name="Other authored source";make_scalar_path(source);source.contours.front().points.at(1).x.literal=72;source.contours.front().points.at(1).y.literal=90;
    document.objects.emplace(target.id,target);document.objects.emplace(source.id,source);document.compositions.front().roots={target.id,source.id};seed_scalar_path(document);
    auto target_stroke=default_operation("path-target-stroke","nect.paint.stroke");target_stroke.parameters.at("width").literal=4;
    auto source_stroke=default_operation("source-stroke","nect.paint.stroke");source_stroke.parameters.at("width").literal=12;
    Session seeded(document);seeded.apply({AddOperation{"text",target_stroke,1},AddOperation{"source",source_stroke,0},
        Link{{"path-ref-guard","","generator.height"},{operation_ref("text","path-target-stroke","width"),0.1,20,"copy_local_value"}}},seeded.revision());document=seeded.document();
    if(mode=="cycle"){seeded=Session(document);seeded.apply({Link{operation_ref("source","source-stroke","width"),{operation_ref("text","path-target-stroke","width"),1,0,"copy_local_value"}}},seeded.revision());document=seeded.document();}
    const Ref ref=operation_ref("text","path-target-stroke","width");Ref source_ref=operation_ref("source","source-stroke","width");if(mode=="unit")source_ref=operation_ref("source","source-stroke","a");
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(ref.object);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");QLineEdit* input=nullptr;QPointer<QPushButton> action;
    for(auto* candidate:window.findChildren<QLineEdit*>()){
        const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
        if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(field))input=candidate;
    }
    for(auto* button:window.findChildren<QPushButton*>(picking?"property-source-pick":"property-expression"))
        if(button->accessibleName()==(picking?"Pick source for "+label:label+" expression editor"))action=button;
    check(scroll&&input&&action,"Exact stable point width input/action exists");scroll->ensureWidgetVisible(input);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,input->mapTo(&window,input->rect().center()));
    const double pending_value=mode=="zero"?0:mode=="unwrapped"?450.5:64;
    const QString pending_text=mode=="zero"?"0":mode=="unwrapped"?"450.5":"64";
    const QString expression_text=mode=="unwrapped"?"360 + 45":"32 + 3";
    QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,mode=="invalid"?QString("not-a-number"):mode=="negative"?QString("-1"):mode=="expression-scalar"?QString("=40 + 2"):pending_text);events();
    check(input->hasFocus()&&input->isModified()&&snapshot(window.host.session)==snapshot(expected),"Selected Stroke draft is full-state neutral");
    auto change_context=[&](bool applying){
        const auto suffix=mode.substr(mode.find('-')+1);
        if(suffix=="session"){window.host.session_id="replacement-stroke-session";return;}
        if(suffix=="revision")window.host.session.apply({Set{{"source","","transform.tx"},61}},window.host.session.revision());
        else if(suffix=="gesture"||suffix=="cancelled-gesture"){
            window.host.session.begin_gesture(window.host.session.revision());window.host.session.update_gesture({Set{{"text","text-first","y"},13}});
            if(suffix=="cancelled-gesture")window.host.session.cancel_gesture();
        }else if(suffix=="selection"){
            window.canvas->set_selection("source");events();window.canvas->set_selection(ref.object);events();
        }else{
            auto incoming=window.host.session.document();auto& object=incoming.objects.at("text");
            if(suffix=="document")incoming.id="replacement-stroke-document";
            else if(suffix=="source")object.contours.front().id="replacement-stroke-contour";
            else if(suffix=="coordinate")object.stack.at(1).parameters.at("width").literal=65;
            else if(suffix=="handle")object.contours.front().points.front().out_length.literal=26;
            else if(suffix=="color")object.stack.at(1).parameters.at("r").literal=0.7;
            else if(suffix=="order")std::reverse(object.stack.begin(),object.stack.end());
            else if(suffix=="operation"){object.stack.at(1).id="replacement-stroke";incoming.objects.at("path-ref-guard").source->parameters.at("height").binding->source=operation_ref("text","replacement-stroke","width");}
            else if(suffix=="type"){object.contours.clear();object.source=default_primitive("replacement-stroke-source","nect.shape.circle");incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text","replacement-stroke-source-east","x"};}
            else throw std::runtime_error("Unknown Stroke context fixture");
            Session replacement(incoming);if(applying)replacement.apply({Set{{"source","","transform.tx"},61}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Stroke replacement collides at captured revision without retargeting Ref");window.host.session=replacement;
        }
        expected=window.host.session;
    };
    if(mode.rfind("entry-",0)==0)change_context(false);
    const auto preview_before_entry=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
    const auto selection=window.canvas->selections();
    const auto position=action->mapTo(&window,action->rect().center());check(window.childAt(position)==action,"Actual Window pointer hits selected Stroke width action");
    auto scalar_expected=[&]{if(mode=="expression-scalar")expected.apply({SetExpression{{ref},{"40 + 2",1},false}},expected.revision());else expected.apply({EditProperties{{ref},pending_value,false}},expected.revision());};
    auto undo_scalar=[&]{window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate Stroke scalar Undo restores exact curve/IDs/handles/paint/Refs");};
    if(mode=="drag"||mode=="drag-cancel"){
        QTest::mousePress(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();scalar_expected();
        check(snapshot(window.host.session)==snapshot(expected),"Stroke whip press commits only independent width before freezing target");
        if(mode=="drag-cancel"){
            QTest::keyClick(&window,Qt::Key_Escape);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Stroke whip Escape retains scalar and exact target selection");
        }else{
            QTreeWidget* tree=nullptr;QTreeWidgetItem* source_item=nullptr;
            for(auto* candidate:window.findChildren<QTreeWidget*>())for(QTreeWidgetItemIterator it(candidate);*it;++it)
                if((*it)->data(0,Qt::UserRole).toString()=="source"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty()){tree=candidate;source_item=*it;}
            check(tree&&source_item,"Stroke whip finds exact other authored Path tree row");tree->scrollToItem(source_item);events();
            QTest::mouseMove(window.windowHandle(),tree->viewport()->mapTo(&window,tree->visualItemRect(source_item).center()),10);events();
            check(window.canvas->selected_object=="source"&&snapshot(window.host.session)==snapshot(expected),"Stroke whip browsing selects stable first source point with no authored mutation");
            QLineEdit* source_field=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
                const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
                if(candidate->isVisible()&&data.value("object").toString()=="source"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(source_ref.field))source_field=candidate;
            }
            check(source_field,"Whip drop has exact stable source point width");scroll->ensureWidgetVisible(source_field);events();
            const auto drop=source_field->mapTo(&window,source_field->rect().center());check(window.childAt(drop)==source_field,"Actual point whip drop hits visible width");
            QTest::mouseMove(window.windowHandle(),drop,10);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,drop);events();
            expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Stroke whip links exact Ref and restores frozen target");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Stroke whip link Undo retains independent scalar");
        }
        undo_scalar();return;
    }
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode=="negative"||mode.rfind("entry-",0)==0){
        check(!window.findChild<QDialog*>("property-source-picker")&&!window.findChild<QWidget*>("nect-expression-panel")&&snapshot(window.host.session)==snapshot(expected),"Refused point entry preserves complete incoming authored state and opens no editor");
        const auto message=window.statusBar()->currentMessage();
        const auto reason=mode=="negative"?"OUT_OF_RANGE":mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"||mode=="entry-cancelled-gesture"?"REVISION_CONFLICT":mode=="entry-session"||mode=="entry-document"?"SESSION_CONFLICT":mode=="entry-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
        check(message.contains(reason),"Stroke entry refusal identifies exact cause");
        check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before_entry,"Stroke entry refusal preserves preview and gesture state");
        if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
    }
    scalar_expected();
    if(picking){
        QPointer<QDialog> picker=window.findChild<QDialog*>("property-source-picker");
        std::cerr<<field<<" stroke pick="<<bool(picker)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"First Stroke picker commits only selected width and opens neutral picker");
        auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();QListWidgetItem* item=nullptr;
        if(list)for(int i=0;i<list->count();++i){const auto data=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(data.value("object").toString()=="source"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(source_ref.field))item=list->item(i);}
        check(list&&buttons&&item,"Picker retains exact other stable point width Ref");list->setCurrentItem(item);events();
        check(snapshot(window.host.session)==snapshot(expected),"Choosing other stable point is authored-state neutral");
        if(mode=="cancel"){buttons->button(QDialogButtonBox::Cancel)->click();events();check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Stroke picker Cancel discards only link draft");undo_scalar();return;}
        if(mode.rfind("apply-",0)==0)change_context(true);
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        buttons->button(QDialogButtonBox::Ok)->click();events();
        if(mode=="cycle"||mode=="unit"){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Cycle/unit picker refusal is full-state atomic");
            check(window.statusBar()->currentMessage().contains(mode=="cycle"?"CYCLE":"NO_SOURCE"),"Cycle/unit refusal identifies cause");
            buttons->button(QDialogButtonBox::Cancel)->click();events();undo_scalar();return;
        }
        if(mode.rfind("apply-",0)==0&&mode!="apply-selection"){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Refused point picker Apply preserves incoming source and complete history");
            const auto reason=mode=="apply-revision"||mode=="apply-cancelled-gesture"?"REVISION_CONFLICT":mode=="apply-session"||mode=="apply-document"?"SESSION_CONFLICT":mode=="apply-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
            check(window.statusBar()->currentMessage().contains(reason),"Stroke picker refusal identifies exact cause");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Stroke picker refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
    }else{
        QPlainTextEdit* editor=nullptr;for(auto* candidate:window.findChildren<QPlainTextEdit*>())if(candidate->accessibleName()==label+" expression")editor=candidate;
        std::cerr<<field<<" stroke fx="<<bool(editor)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(editor&&editor->isVisible()&&editor->toPlainText()==(mode=="expression-scalar"?QString("40 + 2"):pending_text)&&snapshot(window.host.session)==snapshot(expected),"First Stroke fx commits width and opens exact neutral editor");
        editor->setPlainText(mode=="invalid-expression"?QString("broken("):mode=="negative-expression"?QString("-1"):expression_text);events();check(snapshot(window.host.session)==snapshot(expected),"Stroke expression draft remains neutral");
        QPushButton* apply=nullptr;for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;
        check(apply,"Stroke inline Apply exists");
        if(mode.rfind("apply-",0)==0)change_context(true);
        if(mode=="apply-selection"){
            auto* panel=window.findChild<QWidget*>("nect-expression-panel");check(panel,"Stroke expression draft survives exact stable selection roundtrip");
            apply=nullptr;for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;check(apply,"Stroke selection roundtrip recreated Apply");
        }
        if(mode=="cancel")for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Cancel")apply=button;
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        scroll->ensureWidgetVisible(apply);events();QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(&window,apply->rect().center()));events();
        if(mode.rfind("apply-",0)==0||mode=="invalid-expression"||mode=="negative-expression"){
            check(snapshot(window.host.session)==snapshot(expected),"Refused point fx Apply preserves incoming source and complete history");
            auto* result=window.findChild<QLabel*>("nect-expression-result");check(result&&result->text().contains("Committed result is unchanged"),"Stroke expression refusal retains visible recovery status");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Stroke fx refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        if(mode=="cancel"){check(snapshot(window.host.session)==snapshot(expected)&&!window.findChild<QWidget*>("nect-expression-panel"),"Stroke expression Cancel discards only expression draft");undo_scalar();return;}
        expected.apply({SetExpression{{ref},{expression_text.toStdString(),1},false}},expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Stroke operation exactly matches canonical Document/encode/history/revision");
    if(handle){
        QLineEdit* result=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
            const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
            if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(field))result=candidate;
        }
        check(result&&result->text().toDouble()==evaluate(expected.document()).at(ref),"Handle numeric field retains raw unwrapped canonical value without dial normalization");
    }
    const auto actual_values=evaluate(window.host.session.document()),expected_values=evaluate(expected.document());
    const auto actual_transforms=evaluate_transforms(window.host.session.document(),actual_values),expected_transforms=evaluate_transforms(expected.document(),expected_values);
    const auto actual_bounds=object_bounds(window.host.session.document(),"text",actual_values,actual_transforms,true),expected_bounds=object_bounds(expected.document(),"text",expected_values,expected_transforms,true);
    check(actual_transforms.at("text").world==expected_transforms.at("text").world,"Stroke edit retains canonical nonidentity world matrix");
    check(actual_bounds&&expected_bounds&&std::tuple{actual_bounds->left,actual_bounds->top,actual_bounds->right,actual_bounds->bottom}==std::tuple{expected_bounds->left,expected_bounds->top,expected_bounds->right,expected_bounds->bottom},"Stroke edit bounds match canonical curve geometry");
    for(int i=0;i<2;++i){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Stroke scalar and later operation undo independently");}
    check(expected.document()==document,"Two Undo restore exact authored curve/IDs/handles/paint/Ref and both sources");
}

void circle_stroke_width_pointer(bool picking,const std::string& mode="valid",bool rectangle_source=false,bool polygon_source=false,bool star_source=false){
    const bool handle=false;const std::string field="op.path-target-stroke.width";const QString label="Width";const Id generated_point=rectangle_source?"circle-stroke-source-top-right":polygon_source?"circle-stroke-source-outer-1-6":star_source?"circle-stroke-source-outer-1-5":"circle-stroke-source-east";
    QTemporaryDir scratch;check(scratch.isValid(),"Stroke width owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("circle-stroke-entry","composition","board");
    Object target;target.id="text";target.name="Authored target";target.kind=Kind::path;target.source=default_primitive("circle-stroke-source",rectangle_source?"nect.shape.rectangle":polygon_source?"nect.shape.polygon":star_source?"nect.shape.star":"nect.shape.circle");
    target.transform={{{0.8,{}},{0.2,{}},{-0.3,{}},{1.1,{}},{60,{}},{50,{}}}};target.anchor={{{17,{}},{23,{}}}};
    Object source;source.id="source";source.name="Other authored source";make_scalar_path(source);source.contours.front().points.at(1).x.literal=72;source.contours.front().points.at(1).y.literal=90;
    document.objects.emplace(target.id,target);document.objects.emplace(source.id,source);document.compositions.front().roots={target.id,source.id};Session paint_seed(document);paint_seed.apply({AddOperation{"text",default_operation("path-target-fill","nect.paint.fill"),0},Set{{"text",generated_point,"x"},87},Set{{"text",generated_point,"out.length"},31}},paint_seed.revision());document=paint_seed.document();
    Object guard;guard.id="path-ref-guard";guard.source=default_primitive("circle-stroke-guard-source","nect.shape.rectangle");document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
    paint_seed=Session(document);paint_seed.apply({Link{{guard.id,"","generator.width"},{{"text",generated_point,"x"},0.1,0,"copy_local_value"}}},paint_seed.revision());document=paint_seed.document();
    auto target_stroke=default_operation("path-target-stroke","nect.paint.stroke");target_stroke.parameters.at("width").literal=4;
    auto source_stroke=default_operation("source-stroke","nect.paint.stroke");source_stroke.parameters.at("width").literal=12;
    Session seeded(document);seeded.apply({AddOperation{"text",target_stroke,1},AddOperation{"source",source_stroke,0},
        Link{{"path-ref-guard","","generator.height"},{operation_ref("text","path-target-stroke","width"),0.1,20,"copy_local_value"}}},seeded.revision());document=seeded.document();
    if(mode=="cycle"){seeded=Session(document);seeded.apply({Link{operation_ref("source","source-stroke","width"),{operation_ref("text","path-target-stroke","width"),1,0,"copy_local_value"}}},seeded.revision());document=seeded.document();}
    const Ref ref=operation_ref("text","path-target-stroke","width");Ref source_ref=operation_ref("source","source-stroke","width");if(mode=="unit")source_ref=operation_ref("source","source-stroke","a");
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(ref.object);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");QLineEdit* input=nullptr;QPointer<QPushButton> action;
    for(auto* candidate:window.findChildren<QLineEdit*>()){
        const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
        if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(field))input=candidate;
    }
    for(auto* button:window.findChildren<QPushButton*>(picking?"property-source-pick":"property-expression"))
        if(button->accessibleName()==(picking?"Pick source for "+label:label+" expression editor"))action=button;
    check(scroll&&input&&action,"Exact stable point width input/action exists");scroll->ensureWidgetVisible(input);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,input->mapTo(&window,input->rect().center()));
    const double pending_value=mode=="zero"?0:mode=="unwrapped"?450.5:64;
    const QString pending_text=mode=="zero"?"0":mode=="unwrapped"?"450.5":"64";
    const QString expression_text=mode=="unwrapped"?"360 + 45":"32 + 3";
    QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,mode=="invalid"?QString("not-a-number"):mode=="negative"?QString("-1"):mode=="expression-scalar"?QString("=40 + 2"):pending_text);events();
    check(input->hasFocus()&&input->isModified()&&snapshot(window.host.session)==snapshot(expected),"Selected Stroke draft is full-state neutral");
    auto change_context=[&](bool applying){
        const auto suffix=mode.substr(mode.find('-')+1);
        if(suffix=="session"){window.host.session_id="replacement-stroke-session";return;}
        if(suffix=="revision")window.host.session.apply({Set{{"source","","transform.tx"},61}},window.host.session.revision());
        else if(suffix=="gesture"||suffix=="cancelled-gesture"){
            window.host.session.begin_gesture(window.host.session.revision());window.host.session.update_gesture({Set{{"text",generated_point,"y"},13}});
            if(suffix=="cancelled-gesture")window.host.session.cancel_gesture();
        }else if(suffix=="selection"){
            window.canvas->set_selection("source");events();window.canvas->set_selection(ref.object);events();
        }else{
            auto incoming=window.host.session.document();auto& object=incoming.objects.at("text");
            if(suffix=="document")incoming.id="replacement-stroke-document";
            else if(suffix=="source")object.source->parameters.at(rectangle_source?"width":star_source?"outer_radius":"radius").literal=rectangle_source?221:76;
            else if(suffix=="inner-radius")object.source->parameters.at("inner_radius").literal=36;
            else if(suffix=="topology")object.source->parameters.at("points").literal=star_source?10:12;
            else if(suffix=="coordinate")object.stack.at(1).parameters.at("width").literal=65;
            else if(suffix=="handle")object.point_edit->overrides.at(generated_point).at("out.length").literal=26;
            else if(suffix=="color")object.stack.at(1).parameters.at("r").literal=0.7;
            else if(suffix=="order")std::reverse(object.stack.begin(),object.stack.end());
            else if(suffix=="operation"){object.stack.at(1).id="replacement-stroke";incoming.objects.at("path-ref-guard").source->parameters.at("height").binding->source=operation_ref("text","replacement-stroke","width");}
            else if(suffix=="type"){object.source=default_primitive("replacement-stroke-source",rectangle_source?"nect.shape.circle":"nect.shape.rectangle");object.point_edit.reset();incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text",rectangle_source?"replacement-stroke-source-east":"replacement-stroke-source-top-right","x"};}
            else if(suffix=="identity"){object.source->id="replacement-stroke-source";object.point_edit.reset();incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text",rectangle_source?"replacement-stroke-source-top-right":polygon_source?"replacement-stroke-source-outer-1-6":star_source?"replacement-stroke-source-outer-1-5":"replacement-stroke-source-east","x"};}
            else if(suffix=="point-edit")object.point_edit->enabled=false;
            else throw std::runtime_error("Unknown Stroke context fixture");
            Session replacement(incoming);if(applying)replacement.apply({Set{{"source","","transform.tx"},61}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Stroke replacement collides at captured revision without retargeting Ref");window.host.session=replacement;
        }
        expected=window.host.session;
    };
    if(mode.rfind("entry-",0)==0)change_context(false);
    const auto preview_before_entry=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
    const auto selection=window.canvas->selections();
    const auto position=action->mapTo(&window,action->rect().center());check(window.childAt(position)==action,"Actual Window pointer hits selected Stroke width action");
    auto scalar_expected=[&]{if(mode=="expression-scalar")expected.apply({SetExpression{{ref},{"40 + 2",1},false}},expected.revision());else expected.apply({EditProperties{{ref},pending_value,false}},expected.revision());};
    auto undo_scalar=[&]{window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate Stroke scalar Undo restores exact curve/IDs/handles/paint/Refs");};
    if(mode=="drag"||mode=="drag-cancel"){
        QTest::mousePress(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();scalar_expected();
        check(snapshot(window.host.session)==snapshot(expected),"Stroke whip press commits only independent width before freezing target");
        if(mode=="drag-cancel"){
            QTest::keyClick(&window,Qt::Key_Escape);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Stroke whip Escape retains scalar and exact target selection");
        }else{
            QTreeWidget* tree=nullptr;QTreeWidgetItem* source_item=nullptr;
            for(auto* candidate:window.findChildren<QTreeWidget*>())for(QTreeWidgetItemIterator it(candidate);*it;++it)
                if((*it)->data(0,Qt::UserRole).toString()=="source"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty()){tree=candidate;source_item=*it;}
            check(tree&&source_item,"Stroke whip finds exact other authored Path tree row");tree->scrollToItem(source_item);events();
            QTest::mouseMove(window.windowHandle(),tree->viewport()->mapTo(&window,tree->visualItemRect(source_item).center()),10);events();
            check(window.canvas->selected_object=="source"&&snapshot(window.host.session)==snapshot(expected),"Stroke whip browsing selects stable first source point with no authored mutation");
            QLineEdit* source_field=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
                const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
                if(candidate->isVisible()&&data.value("object").toString()=="source"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(source_ref.field))source_field=candidate;
            }
            check(source_field,"Whip drop has exact stable source point width");scroll->ensureWidgetVisible(source_field);events();
            const auto drop=source_field->mapTo(&window,source_field->rect().center());check(window.childAt(drop)==source_field,"Actual point whip drop hits visible width");
            QTest::mouseMove(window.windowHandle(),drop,10);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,drop);events();
            expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Stroke whip links exact Ref and restores frozen target");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Stroke whip link Undo retains independent scalar");
        }
        undo_scalar();return;
    }
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode=="negative"||mode.rfind("entry-",0)==0){
        check(!window.findChild<QDialog*>("property-source-picker")&&!window.findChild<QWidget*>("nect-expression-panel")&&snapshot(window.host.session)==snapshot(expected),"Refused point entry preserves complete incoming authored state and opens no editor");
        const auto message=window.statusBar()->currentMessage();
        const auto reason=mode=="negative"?"OUT_OF_RANGE":mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"||mode=="entry-cancelled-gesture"?"REVISION_CONFLICT":mode=="entry-session"||mode=="entry-document"?"SESSION_CONFLICT":mode=="entry-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
        check(message.contains(reason),"Stroke entry refusal identifies exact cause");
        check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before_entry,"Stroke entry refusal preserves preview and gesture state");
        if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
    }
    scalar_expected();
    if(picking){
        QPointer<QDialog> picker=window.findChild<QDialog*>("property-source-picker");
        std::cerr<<field<<" stroke pick="<<bool(picker)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"First Stroke picker commits only selected width and opens neutral picker");
        auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();QListWidgetItem* item=nullptr;
        if(list)for(int i=0;i<list->count();++i){const auto data=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(data.value("object").toString()=="source"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(source_ref.field))item=list->item(i);}
        check(list&&buttons&&item,"Picker retains exact other stable point width Ref");list->setCurrentItem(item);events();
        check(snapshot(window.host.session)==snapshot(expected),"Choosing other stable point is authored-state neutral");
        if(mode=="cancel"){buttons->button(QDialogButtonBox::Cancel)->click();events();check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Stroke picker Cancel discards only link draft");undo_scalar();return;}
        if(mode.rfind("apply-",0)==0)change_context(true);
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        buttons->button(QDialogButtonBox::Ok)->click();events();
        if(mode=="cycle"||mode=="unit"){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Cycle/unit picker refusal is full-state atomic");
            check(window.statusBar()->currentMessage().contains(mode=="cycle"?"CYCLE":"NO_SOURCE"),"Cycle/unit refusal identifies cause");
            buttons->button(QDialogButtonBox::Cancel)->click();events();undo_scalar();return;
        }
        if(mode.rfind("apply-",0)==0&&mode!="apply-selection"){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Refused point picker Apply preserves incoming source and complete history");
            const auto reason=mode=="apply-revision"||mode=="apply-cancelled-gesture"?"REVISION_CONFLICT":mode=="apply-session"||mode=="apply-document"?"SESSION_CONFLICT":mode=="apply-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
            check(window.statusBar()->currentMessage().contains(reason),"Stroke picker refusal identifies exact cause");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Stroke picker refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
    }else{
        QPlainTextEdit* editor=nullptr;for(auto* candidate:window.findChildren<QPlainTextEdit*>())if(candidate->accessibleName()==label+" expression")editor=candidate;
        std::cerr<<field<<" stroke fx="<<bool(editor)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(editor&&editor->isVisible()&&editor->toPlainText()==(mode=="expression-scalar"?QString("40 + 2"):pending_text)&&snapshot(window.host.session)==snapshot(expected),"First Stroke fx commits width and opens exact neutral editor");
        editor->setPlainText(mode=="invalid-expression"?QString("broken("):mode=="negative-expression"?QString("-1"):expression_text);events();check(snapshot(window.host.session)==snapshot(expected),"Stroke expression draft remains neutral");
        QPushButton* apply=nullptr;for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;
        check(apply,"Stroke inline Apply exists");
        if(mode.rfind("apply-",0)==0)change_context(true);
        if(mode=="apply-selection"){
            auto* panel=window.findChild<QWidget*>("nect-expression-panel");check(panel,"Stroke expression draft survives exact stable selection roundtrip");
            apply=nullptr;for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;check(apply,"Stroke selection roundtrip recreated Apply");
        }
        if(mode=="cancel")for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Cancel")apply=button;
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        scroll->ensureWidgetVisible(apply);events();QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(&window,apply->rect().center()));events();
        if(mode.rfind("apply-",0)==0||mode=="invalid-expression"||mode=="negative-expression"){
            check(snapshot(window.host.session)==snapshot(expected),"Refused point fx Apply preserves incoming source and complete history");
            auto* result=window.findChild<QLabel*>("nect-expression-result");check(result&&result->text().contains("Committed result is unchanged"),"Stroke expression refusal retains visible recovery status");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Stroke fx refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        if(mode=="cancel"){check(snapshot(window.host.session)==snapshot(expected)&&!window.findChild<QWidget*>("nect-expression-panel"),"Stroke expression Cancel discards only expression draft");undo_scalar();return;}
        expected.apply({SetExpression{{ref},{expression_text.toStdString(),1},false}},expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Stroke operation exactly matches canonical Document/encode/history/revision");
    if(handle){
        QLineEdit* result=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
            const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
            if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(field))result=candidate;
        }
        check(result&&result->text().toDouble()==evaluate(expected.document()).at(ref),"Handle numeric field retains raw unwrapped canonical value without dial normalization");
    }
    const auto actual_values=evaluate(window.host.session.document()),expected_values=evaluate(expected.document());
    const auto actual_transforms=evaluate_transforms(window.host.session.document(),actual_values),expected_transforms=evaluate_transforms(expected.document(),expected_values);
    const auto actual_bounds=object_bounds(window.host.session.document(),"text",actual_values,actual_transforms,true),expected_bounds=object_bounds(expected.document(),"text",expected_values,expected_transforms,true);
    check(actual_transforms.at("text").world==expected_transforms.at("text").world,"Stroke edit retains canonical nonidentity world matrix");
    check(actual_bounds&&expected_bounds&&std::tuple{actual_bounds->left,actual_bounds->top,actual_bounds->right,actual_bounds->bottom}==std::tuple{expected_bounds->left,expected_bounds->top,expected_bounds->right,expected_bounds->bottom},"Stroke edit bounds match canonical curve geometry");
    for(int i=0;i<2;++i){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Stroke scalar and later operation undo independently");}
    check(expected.document()==document,"Two Undo restore exact Circle Source/Point Edit/stable IDs/handles/paint/Ref and both sources");
}

void authored_paint_scalar_pointer(const std::string& paint,const std::string& parameter,bool picking,const std::string& mode="valid",bool generated_circle=false,bool generated_rectangle=false,bool generated_polygon=false,bool generated_star=false){
    const bool generated=generated_circle||generated_rectangle||generated_polygon||generated_star;const Id generated_point=generated_rectangle?"circle-paint-source-top-right":generated_polygon?"circle-paint-source-outer-1-6":generated_star?"circle-paint-source-outer-1-5":"circle-paint-source-east";
    const bool handle=false;const Id target_op="path-target-"+paint;const Id source_op="source-"+paint;const auto field=operation_ref("text",target_op,parameter).field;const QString label=parameter=="r"?"Red":parameter=="g"?"Green":parameter=="b"?"Blue":"Paint opacity";
    QTemporaryDir scratch;check(scratch.isValid(),"Paint width owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("authored-stroke-entry","composition","board");
    Object target;target.id="text";target.name="Authored target";if(generated){target.kind=Kind::path;target.source=default_primitive("circle-paint-source",generated_rectangle?"nect.shape.rectangle":generated_polygon?"nect.shape.polygon":generated_star?"nect.shape.star":"nect.shape.circle");}else make_scalar_path(target);
    target.transform={{{0.8,{}},{0.2,{}},{-0.3,{}},{1.1,{}},{60,{}},{50,{}}}};target.anchor={{{17,{}},{23,{}}}};
    Object source;source.id="source";source.name="Other authored source";make_scalar_path(source);source.contours.front().points.at(1).x.literal=72;source.contours.front().points.at(1).y.literal=90;
    document.objects.emplace(target.id,target);document.objects.emplace(source.id,source);document.compositions.front().roots={target.id,source.id};if(generated){
        Session paint_seed(document);paint_seed.apply({AddOperation{"text",default_operation("path-target-fill","nect.paint.fill"),0},Set{{"text",generated_point,"x"},87},Set{{"text",generated_point,"out.length"},31}},paint_seed.revision());document=paint_seed.document();
        Object guard;guard.id="path-ref-guard";guard.source=default_primitive("circle-paint-guard-source","nect.shape.rectangle");document.objects.emplace(guard.id,guard);document.compositions.front().roots.push_back(guard.id);
        paint_seed=Session(document);paint_seed.apply({Link{{guard.id,"","generator.width"},{{"text",generated_point,"x"},0.1,0,"copy_local_value"}}},paint_seed.revision());document=paint_seed.document();
    }else seed_scalar_path(document);
    auto target_stroke=default_operation("path-target-stroke","nect.paint.stroke");target_stroke.parameters.at("width").literal=4;
    auto source_stroke=default_operation("source-stroke","nect.paint.stroke");source_stroke.parameters.at("width").literal=12;
    Session seeded(document);seeded.apply({AddOperation{"text",target_stroke,1},AddOperation{"source",source_stroke,0},AddOperation{"source",default_operation("source-fill","nect.paint.fill"),1},
        Set{operation_ref("text",target_op,parameter),0.4},Set{operation_ref("source",source_op,parameter),0.2},
        Link{{"path-ref-guard","","composite.opacity"},{operation_ref("text",target_op,parameter),0.1,0.2,"copy_local_value"}}},seeded.revision());document=seeded.document();
    if(mode=="cycle"){seeded=Session(document);seeded.apply({Link{operation_ref("source",source_op,parameter),{operation_ref("text",target_op,parameter),1,0,"copy_local_value"}}},seeded.revision());document=seeded.document();}
    const Ref ref=operation_ref("text",target_op,parameter);Ref source_ref=operation_ref("source",source_op,parameter);if(mode=="unit")source_ref=operation_ref("source","source-stroke","width");
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(ref.object);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");QLineEdit* input=nullptr;QPointer<QPushButton> action;
    for(auto* candidate:window.findChildren<QLineEdit*>()){
        const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
        if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(field))input=candidate;
    }
    if(input)action=input->parentWidget()->findChild<QPushButton*>(picking?"property-source-pick":"property-expression");
    check(scroll&&input&&action,"Exact paint scalar input/action exists");scroll->ensureWidgetVisible(input);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,input->mapTo(&window,input->rect().center()));
    const double pending_value=mode=="zero"?0:mode=="one"?1:0.6;
    const QString pending_text=mode=="zero"?"0":mode=="one"?"1":"0.6";
    const QString expression_text="0.2 + 0.1";
    QTest::keyClick(input,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(input,mode=="invalid"?QString("not-a-number"):mode=="negative"?QString("-1"):mode=="above"?QString("1.01"):mode=="expression-scalar"?QString("=0.4 + 0.2"):pending_text);events();
    check(input->hasFocus()&&input->isModified()&&snapshot(window.host.session)==snapshot(expected),"Selected paint draft is full-state neutral");
    auto change_context=[&](bool applying){
        const auto suffix=mode.substr(mode.find('-')+1);
        if(suffix=="session"){window.host.session_id="replacement-paint-session";return;}
        if(suffix=="revision")window.host.session.apply({Set{{"source","","transform.tx"},61}},window.host.session.revision());
        else if(suffix=="gesture"||suffix=="cancelled-gesture"){
            window.host.session.begin_gesture(window.host.session.revision());window.host.session.update_gesture({Set{{"text","text-first","y"},13}});
            if(suffix=="cancelled-gesture")window.host.session.cancel_gesture();
        }else if(suffix=="selection"){
            window.canvas->set_selection("source");events();window.canvas->set_selection(ref.object);events();
        }else{
            auto incoming=window.host.session.document();auto& object=incoming.objects.at("text");auto op=std::find_if(object.stack.begin(),object.stack.end(),[&](const auto& o){return o.id==target_op;});check(op!=object.stack.end(),"Exact paint operation survives fixture context");
            if(suffix=="document")incoming.id="replacement-paint-document";
            else if(suffix=="source")object.contours.front().id="replacement-paint-contour";
            else if(suffix=="coordinate")op->parameters.at(parameter).literal=0.65;
            else if(suffix=="handle")object.contours.front().points.front().out_length.literal=26;
            else if(suffix=="color")op->parameters.at(parameter=="r"?"g":"r").literal=0.7;
            else if(suffix=="order")std::reverse(object.stack.begin(),object.stack.end());
            else if(suffix=="operation"){op->id="replacement-paint";incoming.objects.at("path-ref-guard").compositing.opacity.binding->source=operation_ref("text","replacement-paint",parameter);}
            else if(suffix=="type"){object.contours.clear();object.source=default_primitive("replacement-paint-source","nect.shape.circle");incoming.objects.at("path-ref-guard").source->parameters.at("width").binding->source={"text","replacement-paint-source-east","x"};}
            else throw std::runtime_error("Unknown Paint context fixture");
            Session replacement(incoming);if(applying)replacement.apply({Set{{"source","","transform.tx"},61}},replacement.revision());
            check(replacement.revision()==window.host.session.revision(),"Paint replacement collides at captured revision without retargeting Ref");window.host.session=replacement;
        }
        expected=window.host.session;
    };
    if(mode.rfind("entry-",0)==0)change_context(false);
    const auto preview_before_entry=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
    const auto selection=window.canvas->selections();
    const auto position=action->mapTo(&window,action->rect().center());check(window.childAt(position)==action,"Actual Window pointer hits selected Paint width action");
    auto scalar_expected=[&]{if(mode=="expression-scalar")expected.apply({SetExpression{{ref},{"0.4 + 0.2",1},false}},expected.revision());else expected.apply({EditProperties{{ref},pending_value,false}},expected.revision());};
    auto undo_scalar=[&]{window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate Paint scalar Undo restores exact curve/IDs/handles/paint/Refs");};
    if(mode=="drag"||mode=="drag-cancel"){
        QTest::mousePress(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();scalar_expected();
        check(snapshot(window.host.session)==snapshot(expected),"Paint whip press commits only independent paint scalar before freezing target");
        if(mode=="drag-cancel"){
            QTest::keyClick(&window,Qt::Key_Escape);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Paint whip Escape retains scalar and exact target selection");
        }else{
            QTreeWidget* tree=nullptr;QTreeWidgetItem* source_item=nullptr;
            for(auto* candidate:window.findChildren<QTreeWidget*>())for(QTreeWidgetItemIterator it(candidate);*it;++it)
                if((*it)->data(0,Qt::UserRole).toString()=="source"&&(*it)->data(0,Qt::UserRole+1).toString().isEmpty()){tree=candidate;source_item=*it;}
            check(tree&&source_item,"Paint whip finds exact other authored Path tree row");tree->scrollToItem(source_item);events();
            QTest::mouseMove(window.windowHandle(),tree->viewport()->mapTo(&window,tree->visualItemRect(source_item).center()),10);events();
            check(window.canvas->selected_object=="source"&&snapshot(window.host.session)==snapshot(expected),"Paint whip browsing selects stable first source operation with no authored mutation");
            QLineEdit* source_field=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
                const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
                if(candidate->isVisible()&&data.value("object").toString()=="source"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(source_ref.field))source_field=candidate;
            }
            check(source_field,"Whip drop has exact stable source paint scalar");scroll->ensureWidgetVisible(source_field);events();
            const auto drop=source_field->mapTo(&window,source_field->rect().center());check(window.childAt(drop)==source_field,"Actual point whip drop hits visible paint scalar");
            QTest::mouseMove(window.windowHandle(),drop,10);QTest::mouseRelease(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,drop);events();
            expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
            check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Paint whip links exact Ref and restores frozen target");
            window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Paint whip link Undo retains independent scalar");
        }
        undo_scalar();return;
    }
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode=="negative"||mode=="above"||mode.rfind("entry-",0)==0){
        check(!window.findChild<QDialog*>("property-source-picker")&&!window.findChild<QWidget*>("nect-expression-panel")&&snapshot(window.host.session)==snapshot(expected),"Refused point entry preserves complete incoming authored state and opens no editor");
        const auto message=window.statusBar()->currentMessage();
        const auto reason=(mode=="negative"||mode=="above")?"OUT_OF_RANGE":mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"||mode=="entry-cancelled-gesture"?"REVISION_CONFLICT":mode=="entry-session"||mode=="entry-document"?"SESSION_CONFLICT":mode=="entry-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
        check(message.contains(reason),"Paint entry refusal identifies exact cause");
        check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before_entry,"Paint entry refusal preserves preview and gesture state");
        if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
    }
    scalar_expected();
    if(picking){
        QPointer<QDialog> picker=window.findChild<QDialog*>("property-source-picker");
        std::cerr<<field<<" paint pick="<<bool(picker)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"First Paint picker commits only selected paint scalar and opens neutral picker");
        auto* list=picker->findChild<QListWidget*>("property-source-picker-list");auto* buttons=picker->findChild<QDialogButtonBox*>();QListWidgetItem* item=nullptr;
        if(list)for(int i=0;i<list->count();++i){const auto data=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();if(data.value("object").toString()=="source"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(source_ref.field))item=list->item(i);}
        check(list&&buttons&&item,"Picker retains exact other paint scalar Ref");list->setCurrentItem(item);events();
        check(snapshot(window.host.session)==snapshot(expected),"Choosing other stable point is authored-state neutral");
        if(mode=="cancel"){buttons->button(QDialogButtonBox::Cancel)->click();events();check(snapshot(window.host.session)==snapshot(expected)&&window.canvas->selections()==selection,"Paint picker Cancel discards only link draft");undo_scalar();return;}
        if(mode.rfind("apply-",0)==0)change_context(true);
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        buttons->button(QDialogButtonBox::Ok)->click();events();
        if(mode=="cycle"||mode=="unit"){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Cycle/unit picker refusal is full-state atomic");
            check(window.statusBar()->currentMessage().contains(mode=="cycle"?"CYCLE":"NO_SOURCE"),"Cycle/unit refusal identifies cause");
            buttons->button(QDialogButtonBox::Cancel)->click();events();undo_scalar();return;
        }
        if(mode.rfind("apply-",0)==0&&mode!="apply-selection"){
            check(picker&&picker->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Refused point picker Apply preserves incoming source and complete history");
            const auto reason=mode=="apply-revision"||mode=="apply-cancelled-gesture"?"REVISION_CONFLICT":mode=="apply-session"||mode=="apply-document"?"SESSION_CONFLICT":mode=="apply-gesture"?"GESTURE_ACTIVE":"PROPERTY_CONFLICT";
            check(window.statusBar()->currentMessage().contains(reason),"Paint picker refusal identifies exact cause");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Paint picker refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        expected.apply({LinkProperties{{ref},source_ref,false}},expected.revision());
    }else{
        QPlainTextEdit* editor=nullptr;for(auto* candidate:window.findChildren<QPlainTextEdit*>())if(candidate->accessibleName()==label+" expression")editor=candidate;
        std::cerr<<field<<" paint fx="<<bool(editor)<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<"\n";
        check(editor&&editor->isVisible()&&editor->toPlainText()==(mode=="expression-scalar"?QString("0.4 + 0.2"):QString::number(pending_value,'g',17))&&snapshot(window.host.session)==snapshot(expected),"First Paint fx commits paint scalar and opens exact neutral editor");
        editor->setPlainText(mode=="invalid-expression"?QString("broken("):mode=="negative-expression"?QString("-1"):mode=="above-expression"?QString("1.01"):expression_text);events();check(snapshot(window.host.session)==snapshot(expected),"Paint expression draft remains neutral");
        QPushButton* apply=nullptr;for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;
        check(apply,"Paint inline Apply exists");
        if(mode.rfind("apply-",0)==0)change_context(true);
        if(mode=="apply-selection"){
            auto* panel=window.findChild<QWidget*>("nect-expression-panel");check(panel,"Paint expression draft survives exact stable selection roundtrip");
            apply=nullptr;for(auto* button:panel->findChildren<QPushButton*>())if(button->text()=="Apply")apply=button;check(apply,"Paint selection roundtrip recreated Apply");
        }
        if(mode=="cancel")for(auto* button:editor->parentWidget()->findChildren<QPushButton*>())if(button->text()=="Cancel")apply=button;
        const auto preview_before=std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()};
        scroll->ensureWidgetVisible(apply);events();QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,apply->mapTo(&window,apply->rect().center()));events();
        if(mode.rfind("apply-",0)==0||mode=="invalid-expression"||mode=="negative-expression"||mode=="above-expression"){
            check(snapshot(window.host.session)==snapshot(expected),"Refused point fx Apply preserves incoming source and complete history");
            auto* result=window.findChild<QLabel*>("nect-expression-result");check(result&&result->text().contains("Committed result is unchanged"),"Paint expression refusal retains visible recovery status");
            check(std::tuple{window.host.session.preview_document(),window.host.session.gesture_generation(),window.host.session.gesture_active()}==preview_before,"Paint fx refusal preserves preview and gesture state");
            if(window.host.session.gesture_active())window.host.session.cancel_gesture();return;
        }
        if(mode=="cancel"){check(snapshot(window.host.session)==snapshot(expected)&&!window.findChild<QWidget*>("nect-expression-panel"),"Paint expression Cancel discards only expression draft");undo_scalar();return;}
        expected.apply({SetExpression{{ref},{expression_text.toStdString(),1},false}},expected.revision());
    }
    check(snapshot(window.host.session)==snapshot(expected),"Paint operation exactly matches canonical Document/encode/history/revision");
    if(handle){
        QLineEdit* result=nullptr;for(auto* candidate:window.findChildren<QLineEdit*>()){
            const auto data=QJsonDocument::fromJson(candidate->property("nect-reference").toByteArray()).object();
            if(candidate->isVisible()&&data.value("object").toString()=="text"&&data.value("point").toString().isEmpty()&&data.value("field").toString()==QString::fromStdString(field))result=candidate;
        }
        check(result&&result->text().toDouble()==evaluate(expected.document()).at(ref),"Handle numeric field retains raw unwrapped canonical value without dial normalization");
    }
    const auto actual_values=evaluate(window.host.session.document()),expected_values=evaluate(expected.document());
    const auto actual_transforms=evaluate_transforms(window.host.session.document(),actual_values),expected_transforms=evaluate_transforms(expected.document(),expected_values);
    const auto actual_bounds=object_bounds(window.host.session.document(),"text",actual_values,actual_transforms,true),expected_bounds=object_bounds(expected.document(),"text",expected_values,expected_transforms,true);
    check(actual_transforms.at("text").world==expected_transforms.at("text").world,"Paint edit retains canonical nonidentity world matrix");
    check(actual_bounds&&expected_bounds&&std::tuple{actual_bounds->left,actual_bounds->top,actual_bounds->right,actual_bounds->bottom}==std::tuple{expected_bounds->left,expected_bounds->top,expected_bounds->right,expected_bounds->bottom},"Paint edit bounds match canonical curve geometry");
    for(int i=0;i<2;++i){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();check(snapshot(window.host.session)==snapshot(expected),"Paint scalar and later operation undo independently");}
    check(expected.document()==document,"Two Undo restore exact authored curve/IDs/handles/paint/Ref and both sources");
}

void path_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Text Path entry owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("path-pointer-document","composition","board");
    Object text;text.id="text";text.name="Path pointer target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    text.text->font_features={{"KERN",1,"whole_text"},{"lig ",4,"whole_text"}};text.text->additional_axis_values={{"wdth",87.1234567890123}};
    Object path;path.id="path";path.name="Stable guide";path.kind=Kind::path;Contour contour;contour.id="contour";
    Point first;first.id="first";first.y.literal=100;Point last;last.id="last";last.x.literal=2000;last.y.literal=100;contour.points={first,last};path.contours={contour};
    if(mode=="update"||mode=="detach")text.text->path_attachment=TextPathAttachment{"path","contour","distance",12,3,false};
    document.objects.emplace(text.id,text);document.objects.emplace(path.id,path);document.compositions.front().roots={text.id,path.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");auto* action=window.findChild<QPushButton*>(mode=="detach"?"text-path-detach":"text-path-apply");
    auto* choice=window.findChild<QComboBox*>("text-path-contour");auto* start=window.findChild<QLineEdit*>("text-path-start");auto* spacing=window.findChild<QLineEdit*>("text-path-spacing");
    check(scroll&&action&&choice&&start&&spacing,"Existing Path attachment controls available");
    choice->setCurrentIndex(choice->findData("path\ncontour"));start->setText("23.125");spacing->setText("4.75");
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(size,"Visible same-object Font size available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain"){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Path scalar and attachment drafts are fully neutral");
    if(mode=="invalid-start")start->setText("bad");
    if(mode=="invalid-spacing")spacing->setText("-1");
    if(mode=="missing")choice->setCurrentIndex(0);
    if(mode=="entry-revision"){window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;}
    if(mode=="entry-document"){auto incoming=document;incoming.id="incoming-path-document";window.host.session=Session(incoming);expected=window.host.session;}
    if(mode=="entry-session")window.host.session_id="incoming-path-session";
    if(mode=="entry-gesture"||mode=="entry-cancelled-gesture"){
        window.host.session.begin_gesture(window.host.session.revision());if(mode=="entry-cancelled-gesture")window.host.session.cancel_gesture();
    }
    scroll->ensureWidgetVisible(action);events();const auto position=action->mapTo(&window,action->rect().center());
    check(window.childAt(position)==action,"Actual Window pointer hits existing Path action");
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    if(mode=="invalid"||mode.rfind("entry-",0)==0){
        check(snapshot(window.host.session)==snapshot(expected),"Rejected Path entry preserves full incoming source/history without scalar mutation");
        const auto cause=mode=="invalid"?"INVALID_VALUE":mode=="entry-document"||mode=="entry-session"?"SESSION_CONFLICT":mode=="entry-gesture"?"GESTURE_ACTIVE":"REVISION_CONFLICT";
        check(window.statusBar()->currentMessage().contains(cause),"Rejected Path entry reports exact bound context cause");
        if(mode=="entry-gesture"){check(window.host.session.gesture_active(),"Path refusal retains actual active gesture");window.host.session.cancel_gesture();}
        return;
    }
    if(mode!="plain")expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    if(mode=="invalid-start"||mode=="invalid-spacing"||mode=="missing"){
        check(snapshot(window.host.session)==snapshot(expected),"Rejected attachment draft retains only its independent scalar command");
        check(window.statusBar()->currentMessage().contains(mode=="missing"?"MISSING_PATH_ATTACHMENT":mode=="invalid-start"?"TEXT_PATH_START_INVALID":"TEXT_PATH_SPACING"),"Invalid Path intent reports exact existing cause");
        window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
        check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Undo after refused attachment restores exact original scalar/source/history");return;
    }
    auto next=*expected.document().objects.at("text").text;
    if(mode=="detach")next.path_attachment.reset();else next.path_attachment=TextPathAttachment{"path","contour","distance",23.125,4.75,false};
    expected.apply({UpdateText{"text",next}},expected.revision());
    std::cerr<<"Path "<<mode<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<" status="<<window.statusBar()->currentMessage().toStdString()<<"\n";
    check(snapshot(window.host.session)==snapshot(expected),"First Path action commits exact scalar then attachment intent with canonical full source/history");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One Path action Undo restores attachment and retains size");
    if(mode!="plain"){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();}
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate size Undo restores exact original Path/Text/style/history");
}

void typography_pending_pointer(const std::string& mode){
    QTemporaryDir scratch;check(scratch.isValid(),"Typography entry owns temporary state");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    auto document=empty_document("typography-pointer-document","composition","board");
    Object text;text.id="text";text.name="Typography pointer target";text.kind=Kind::text;text.text=default_text("text-source","Retain 日本語 and style");
    text.text->font_features={{"KERN",1,"whole_text"},{"lig ",4,"whole_text"}};text.text->additional_axis_values={{"wdth",87.1234567890123}};
    document.objects.emplace(text.id,text);document.compositions.front().roots={text.id};
    window.host.session=Session(document);window.host.edited();window.resize(1100,750);window.show();window.activateWindow();events();window.canvas->set_selection(text.id);events();
    Session expected=window.host.session;
    const bool axis=mode=="axis-add"||mode=="axis-edit"||mode=="axis-remove";
    const bool edit=mode=="feature-edit"||mode=="axis-edit",remove=mode=="feature-remove"||mode=="axis-remove";
    const char* name=axis?(remove?"text-font-axis-remove":edit?"text-font-axis-edit":"text-font-axis-add"):(remove?"text-font-feature-remove":edit?"text-font-feature-edit":"text-font-feature-add");
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");auto* action=window.findChild<QPushButton*>(name);
    QLineEdit* size=nullptr;
    for(auto* input:window.findChildren<QLineEdit*>()){
        const auto ref=QJsonDocument::fromJson(input->property("nect-reference").toByteArray()).object();
        if(input->isVisible()&&ref.value("object").toString()=="text"&&ref.value("field").toString()=="text.font_size")size=input;
    }
    check(scroll&&action&&size,"Existing Add feature and scalar controls available");scroll->ensureWidgetVisible(size);events();
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,size->mapTo(&window,size->rect().center()));
    if(mode!="plain"){QTest::keyClick(size,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(size,mode=="invalid"?"not-a-number":"64");events();}
    check(size->hasFocus()&&size->isModified()==(mode!="plain")&&snapshot(window.host.session)==snapshot(expected),"Typography scalar draft is authored-state neutral");
    scroll->ensureWidgetVisible(action);events();
    if(mode=="entry-revision"){
        window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;
    }
    if(mode=="entry-document"){auto incoming=document;incoming.id="incoming-entry-document";window.host.session=Session(incoming);expected=window.host.session;}
    const auto position=action->mapTo(&window,action->rect().center());check(window.childAt(position)==action,"Actual Window pointer hits existing Add feature button");
    const bool rejected=mode=="invalid"||mode=="entry-revision"||mode=="entry-document";
    if(mode!="plain"&&!rejected)expected.apply({EditProperties{{{"text","","text.font_size"}},64,false}},expected.revision());
    QTest::mouseClick(window.windowHandle(),Qt::LeftButton,Qt::NoModifier,position);events();
    QPointer<QDialog> dialog=window.findChild<QDialog*>("text-typography-dialog");
    std::cout<<"Typography "<<mode<<" dialog="<<bool(dialog&&dialog->isVisible())<<" actual="<<window.host.session.revision()<<" expected="<<expected.revision()<<" status="<<window.statusBar()->currentMessage().toStdString()<<std::endl;
    check(snapshot(window.host.session)==snapshot(expected),"Typography first pointer commits only independent scalar or refuses atomically");
    if(rejected){
        check(!dialog||!dialog->isVisible(),"Invalid or stale Typography entry has no writable dialog");
        check(window.statusBar()->currentMessage().contains(mode=="invalid"?"INVALID_VALUE":mode=="entry-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),"Typography entry reports exact cause");return;
    }
    check(dialog&&dialog->isVisible(),"Actual first Typography pointer opens existing Window-owned dialog");
    auto* tag=dialog->findChild<QLineEdit*>("text-typography-tag");auto* value=dialog->findChild<QLineEdit*>("text-typography-value");auto* apply=dialog->findChild<QPushButton*>("text-typography-apply");auto* cancel=dialog->findChild<QPushButton*>("text-typography-cancel");
    check(tag&&value&&apply&&cancel,"Existing exact-tag draft fields available");
    const QString exact=axis?(edit||remove?"wdth":" A  "):(edit||remove?"KERN":"kern");
    const QString numeric=axis?(edit?"82.5":"83.75"):"29";
    if(!edit&&!remove){QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,tag->mapTo(dialog,tag->rect().center()));QTest::keyClicks(tag,exact);}
    if(!remove){QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,value->mapTo(dialog,value->rect().center()));QTest::keyClick(value,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(value,numeric);events();}
    check(tag->text()==exact&&(remove||value->text()==numeric)&&snapshot(window.host.session)==snapshot(expected),"Typography exact draft is neutral with lowercase/uppercase/spaced tag boundaries preserved");
    if(mode=="apply-revision"){window.host.session.apply({EditProperties{{{"text","","text.tracking"}},2,false}},window.host.session.revision());expected=window.host.session;}
    if(mode=="apply-document"){
        auto incoming=document;incoming.id="incoming-apply-document";Session replacement(incoming);replacement.apply({EditProperties{{{"text","","text.font_size"}},64,false}},replacement.revision());
        check(replacement.revision()==window.host.session.revision(),"Incoming typography Document has same revision");window.host.session=replacement;expected=replacement;
    }
    if(mode=="apply-session")window.host.session_id="incoming-typography-session";
    if(mode=="apply-gesture"||mode=="apply-cancelled-gesture"){
        window.host.session.begin_gesture(window.host.session.revision());
        if(mode=="apply-cancelled-gesture")window.host.session.cancel_gesture();
    }
    if(mode=="apply-selection")window.canvas->set_selections({});
    auto* button=mode=="cancel"?cancel:apply;QTest::mouseClick(dialog->windowHandle(),Qt::LeftButton,Qt::NoModifier,button->mapTo(dialog,button->rect().center()));events();
    if(mode=="apply-revision"||mode=="apply-document"||mode=="apply-session"){
        check(dialog&&dialog->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Stale typography Apply keeps full incoming source/history");
        check(dialog->findChild<QLabel*>("text-typography-status")->text().contains(mode=="apply-revision"?"REVISION_CONFLICT":"SESSION_CONFLICT"),"Typography dialog reports exact context cause");dialog->reject();events();return;
    }
    if(mode=="apply-gesture"||mode=="apply-cancelled-gesture"||mode=="apply-selection"){
        check(dialog&&dialog->isVisible()&&snapshot(window.host.session)==snapshot(expected),"Gesture or selection conflict preserves complete authored state/history");
        const auto code=mode=="apply-gesture"?"GESTURE_ACTIVE":mode=="apply-selection"?"SELECTION_CONFLICT":"REVISION_CONFLICT";
        check(dialog->findChild<QLabel*>("text-typography-status")->text().contains(code),"Typography guard retains exact active/cancelled gesture or selection code");
        if(mode=="apply-gesture"){check(window.host.session.gesture_active(),"Rejected dialog retains actual active gesture");window.host.session.cancel_gesture();}
        dialog->reject();events();return;
    }
    if(mode=="feature-edit")expected.apply({UpdateTextFontFeature{"text","KERN",29}},expected.revision());
    else if(mode=="feature-remove")expected.apply({RemoveTextFontFeature{"text","KERN"}},expected.revision());
    else if(mode=="axis-add")expected.apply({SetTextAdditionalAxis{"text"," A  ",83.75}},expected.revision());
    else if(mode=="axis-edit")expected.apply({SetTextAdditionalAxis{"text","wdth",82.5}},expected.revision());
    else if(mode=="axis-remove")expected.apply({RemoveTextAdditionalAxis{"text","wdth"}},expected.revision());
    else if(mode!="cancel")expected.apply({AddTextFontFeature{"text",{"kern",29,"whole_text"}}},expected.revision());
    check(snapshot(window.host.session)==snapshot(expected),"Typography Apply or Cancel equals independent canonical command and complete source/history");
    window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();
    check(snapshot(window.host.session)==snapshot(expected),"One typography or cancelled scalar Undo preserves canonical source/history");
    if(mode!="plain"&&mode!="cancel"){window.host.session.undo(window.host.session.revision());expected.undo(expected.revision());window.host.edited();events();}
    check(snapshot(window.host.session)==snapshot(expected)&&expected.document()==document,"Separate scalar Undo restores exact original tags/style/source");
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
void keyboard_popup_focus_help(){
    QTemporaryDir scratch;check(scratch.isValid(),"Popup focus check owns preferences and recovery");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.resize(1100,750);window.show();window.activateWindow();events();
    auto* pen=window.findChild<QToolButton*>("tool-pen");
    auto* text=window.findChild<QToolButton*>("tool-text");
    check(pen&&text&&text->menu(),"Existing Text Tool and variant menu are present");
    struct FocusProbe final:QObject {
        bool popup_out=false,popup_in=false;
        bool eventFilter(QObject*,QEvent* event)override {
            if(event->type()==QEvent::FocusIn||event->type()==QEvent::FocusOut) {
                if(static_cast<QFocusEvent*>(event)->reason()==Qt::PopupFocusReason) {
                    if(event->type()==QEvent::FocusOut)popup_out=true;else popup_in=true;
                }
            }
            return false;
        }
    } probe;
    text->installEventFilter(&probe);
    const auto before=snapshot(window.host.session);
    const auto preference=settings.value("workspace/tools/textCreationDirection");
    const auto tool_state=std::tuple{text->isChecked(),window.canvas->text_mode()};
    QCursor::setPos(window.canvas->mapToGlobal(window.canvas->rect().center()));
    pen->setFocus(Qt::OtherFocusReason);events();QTest::keyClick(pen,Qt::Key_Tab);events();
    check(text->hasFocus()&&QToolTip::isVisible()&&QToolTip::text()==text->toolTip(),
        "Keyboard-origin Text focus begins with visible current help");
    auto* menu=text->menu();
    const auto cancel_popup=[&](bool pointer,bool leave_after_cancel){
        bool opened=false,neutral=false;probe.popup_out=false;probe.popup_in=false;
        QTimer::singleShot(1000,menu,[&]{
            opened=menu->isVisible();neutral=snapshot(window.host.session)==before;
            if(opened){
                if(!pointer)QTest::keyRelease(menu,Qt::Key_Space);
                QTest::keyClick(menu,Qt::Key_Escape);
                if(leave_after_cancel)window.canvas->setFocus();
            }
        });
        if(pointer)QTest::mousePress(text,Qt::LeftButton);else QTest::keyPress(text,Qt::Key_Space);
        QTest::qWait(1200);events();
        if(pointer){QTest::mouseRelease(text,Qt::LeftButton);events();}
        check(opened&&!menu->isVisible(),"Existing delayed popup opens and Escape cancels it");
        check(probe.popup_out&&probe.popup_in,"Actual Qt popup uses PopupFocusReason on departure and return");
        check(neutral,"Open variant menu preserves the complete authored state and History");
        unchanged(window.host.session,before,"Popup cancel preserves document, revision and History");
        check(settings.value("workspace/tools/textCreationDirection")==preference,
            "Popup cancel preserves the creation preference");
        check(std::tuple{text->isChecked(),window.canvas->text_mode()}==tool_state,
            "Popup cancel activates no Tool or variant");
    };
    cancel_popup(false,false);
    check(text->hasFocus(),"Qt PopupFocusReason returns focus to Text");
    check(QToolTip::isVisible()&&QToolTip::text()==text->toolTip(),
        "Keyboard popup cancel restores visible Text name, current variant and hold hint");
    cancel_popup(false,true);QTest::qWait(350);events();
    check(window.canvas->hasFocus()&&!QToolTip::isVisible(),
        "Leaving on popup cancel refuses queued old Text help");
    pen->setFocus(Qt::OtherFocusReason);events();QTest::keyClick(pen,Qt::Key_Tab);events();
    check(text->hasFocus()&&QToolTip::isVisible(),"Keyboard help is available before changing to pointer input");
    cancel_popup(true,false);QTest::qWait(350);events();
    check(text->hasFocus()&&!QToolTip::isVisible(),
        "Pointer popup cancel does not revive earlier keyboard help");
    std::cout<<"rail_keyboard_popup_focus_help: "<<checks<<" checks passed; Qt event route, not native input\n";
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
        if(app.arguments().contains("--scalar-fx-affected-pending-pointer")){for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode);std::cout<<"text_scalar_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--scalar-fx-generic-pending-pointer")){scalar_fx_pending_pointer("other-field");std::cout<<"text_scalar_fx_generic_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--scalar-fx-pending-pointer")){scalar_fx_pending_pointer("valid");std::cout<<"text_scalar_fx_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--scalar-pick-pending-pointer")){scalar_pick_pending_pointer("valid");std::cout<<"text_scalar_pick_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--scalar-pick-affected-pending-pointer")){for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","generic"})scalar_pick_pending_pointer(mode);std::cout<<"text_scalar_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--scalar-pick-whip-pending-pointer")){scalar_pick_pending_pointer("drag");scalar_pick_pending_pointer("drag-cancel");std::cout<<"text_scalar_pick_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--frame-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"text.frame_width","text.frame_height"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Frame scalar first-pointer entries satisfy the existing source contract");
            std::cout<<"text_frame_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--frame-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"text.frame_width","text.frame_height"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_frame_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--frame-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"text.frame_width","text.frame_height"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_frame_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--frame-scalars-whip-pending-pointer")){
            for(const auto* field:{"text.frame_width","text.frame_height"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_frame_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--spacing-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"text.tracking","text.line_spacing"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Text spacing first-pointer entries satisfy the existing source contract");
            std::cout<<"text_spacing_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--spacing-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"text.tracking","text.line_spacing"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_spacing_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--spacing-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"text.tracking","text.line_spacing"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_spacing_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--spacing-scalars-whip-pending-pointer")){
            for(const auto* field:{"text.tracking","text.line_spacing"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_spacing_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--origin-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"text.origin_x","text.origin_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Text origin first-pointer entries satisfy the existing source contract");
            std::cout<<"text_origin_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--origin-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"text.origin_x","text.origin_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_origin_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--origin-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"text.origin_x","text.origin_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_origin_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--origin-scalars-whip-pending-pointer")){
            for(const auto* field:{"text.origin_x","text.origin_y"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_origin_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-translations-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.tx","transform.ty"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Text Affine matrix translation first-pointer entries satisfy the existing source contract");
            std::cout<<"text_affine_translations_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-translations-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.tx","transform.ty"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_affine_translations_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-translations-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.tx","transform.ty"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_affine_translations_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-translations-whip-pending-pointer")){
            for(const auto* field:{"transform.tx","transform.ty"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_affine_translations_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-translations-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.tx","transform.ty"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Circle authored affine translation first-pointer entries satisfy the existing source contract");
            std::cout<<"circle_affine_translations_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-translations-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.tx","transform.ty"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_affine_translations_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-translations-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.tx","transform.ty"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_affine_translations_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-translations-whip-pending-pointer")){
            for(const auto* field:{"transform.tx","transform.ty"}){scalar_pick_pending_pointer("drag",field,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,true);}
            std::cout<<"circle_affine_translations_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-anchor-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Circle authored Anchor first-pointer entries satisfy the existing source contract");
            std::cout<<"circle_anchor_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-anchor-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_anchor_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-anchor-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_anchor_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-anchor-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"}){scalar_pick_pending_pointer("drag",field,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,true);}
            std::cout<<"circle_anchor_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-linear-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Circle authored affine linear first-pointer entries satisfy the existing source contract");
            std::cout<<"circle_affine_linear_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-linear-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_affine_linear_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-linear-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_affine_linear_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-affine-linear-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"}){scalar_pick_pending_pointer("drag",field,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,true);}
            std::cout<<"circle_affine_linear_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-anchor-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Text authored Anchor first-pointer entries satisfy the existing source contract");
            std::cout<<"text_anchor_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-anchor-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_anchor_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-anchor-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_anchor_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-anchor-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.anchor_x","transform.anchor_y"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_anchor_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-dimension-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"image.width","image.height"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Image dimension property first-pointer entries satisfy the existing source contract");
            std::cout<<"image_dimension_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-dimension-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"image.width","image.height"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","entry-asset","entry-dimensions","entry-target-dimension","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-asset","apply-dimensions","apply-target-dimension","apply-gesture","apply-cancelled-gesture","apply-selection"}){scalar_fx_pending_pointer(mode,field,false,false,false,false,false,false,true);}
            std::cout<<"image_dimension_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-dimension-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"image.width","image.height"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","entry-type","entry-asset","entry-dimensions","entry-target-dimension","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-asset","apply-dimensions","apply-target-dimension","apply-gesture","apply-cancelled-gesture"}){scalar_pick_pending_pointer(mode,field,false,false,false,false,false,false,true);}
            std::cout<<"image_dimension_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-dimension-scalars-whip-pending-pointer")){
            for(const auto* field:{"image.width","image.height"}){scalar_pick_pending_pointer("drag",field,false,false,false,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,false,false,false,true);}
            std::cout<<"image_dimension_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-paint-scalars-entry-pending-pointer")){
            bool failed=false;for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(bool picking:{false,true}){
                try{authored_paint_scalar_pointer(paint,parameter,picking,"valid",true);}catch(const std::exception& error){failed=true;std::cerr<<paint<<"."<<parameter<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Sixteen generated Circle paint first-pointer entries satisfy existing semantics");
            std::cout<<"circle_paint_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-paint-scalars-affected-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"}){
                for(bool picking:{false,true})for(const auto* mode:{"zero","one","negative","above","invalid","cancel","expression-scalar"})authored_paint_scalar_pointer(paint,parameter,picking,mode,true);
                for(const auto* mode:{"invalid-expression","negative-expression","above-expression"})authored_paint_scalar_pointer(paint,parameter,false,mode,true);
                for(const auto* mode:{"cycle","unit"})authored_paint_scalar_pointer(paint,parameter,true,mode,true);
            }
            std::cout<<"circle_paint_scalars_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-paint-scalars-whip-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(const auto* mode:{"drag","drag-cancel"})authored_paint_scalar_pointer(paint,parameter,true,mode,true);
            std::cout<<"circle_paint_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-paint-scalars-entry-pending-pointer")){
            bool failed=false;for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(bool picking:{false,true}){
                try{authored_paint_scalar_pointer(paint,parameter,picking,"valid",false,true);}catch(const std::exception& error){failed=true;std::cerr<<paint<<"."<<parameter<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Sixteen generated Rectangle paint first-pointer entries satisfy existing semantics");
            std::cout<<"rectangle_paint_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-paint-scalars-affected-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"}){
                for(bool picking:{false,true})for(const auto* mode:{"zero","one","negative","above","invalid","cancel","expression-scalar"})authored_paint_scalar_pointer(paint,parameter,picking,mode,false,true);
                for(const auto* mode:{"invalid-expression","negative-expression","above-expression"})authored_paint_scalar_pointer(paint,parameter,false,mode,false,true);
                for(const auto* mode:{"cycle","unit"})authored_paint_scalar_pointer(paint,parameter,true,mode,false,true);
            }
            std::cout<<"rectangle_paint_scalars_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-paint-scalars-whip-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(const auto* mode:{"drag","drag-cancel"})authored_paint_scalar_pointer(paint,parameter,true,mode,false,true);
            std::cout<<"rectangle_paint_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-paint-scalars-entry-pending-pointer")){
            bool failed=false;for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(bool picking:{false,true}){
                try{authored_paint_scalar_pointer(paint,parameter,picking,"valid",false,false,true);}catch(const std::exception& error){failed=true;std::cerr<<paint<<"."<<parameter<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Sixteen generated Polygon paint first-pointer entries satisfy existing semantics");
            std::cout<<"polygon_paint_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-paint-scalars-affected-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"}){
                for(bool picking:{false,true})for(const auto* mode:{"zero","one","negative","above","invalid","cancel","expression-scalar"})authored_paint_scalar_pointer(paint,parameter,picking,mode,false,false,true);
                for(const auto* mode:{"invalid-expression","negative-expression","above-expression"})authored_paint_scalar_pointer(paint,parameter,false,mode,false,false,true);
                for(const auto* mode:{"cycle","unit"})authored_paint_scalar_pointer(paint,parameter,true,mode,false,false,true);
            }
            std::cout<<"polygon_paint_scalars_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-paint-scalars-whip-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(const auto* mode:{"drag","drag-cancel"})authored_paint_scalar_pointer(paint,parameter,true,mode,false,false,true);
            std::cout<<"polygon_paint_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-paint-scalars-entry-pending-pointer")){
            bool failed=false;for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(bool picking:{false,true}){
                try{authored_paint_scalar_pointer(paint,parameter,picking,"valid",false,false,false,true);}catch(const std::exception& error){failed=true;std::cerr<<paint<<"."<<parameter<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Sixteen generated Star paint first-pointer entries satisfy existing semantics");
            std::cout<<"star_paint_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-paint-scalars-affected-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"}){
                for(bool picking:{false,true})for(const auto* mode:{"zero","one","negative","above","invalid","cancel","expression-scalar"})authored_paint_scalar_pointer(paint,parameter,picking,mode,false,false,false,true);
                for(const auto* mode:{"invalid-expression","negative-expression","above-expression"})authored_paint_scalar_pointer(paint,parameter,false,mode,false,false,false,true);
                for(const auto* mode:{"cycle","unit"})authored_paint_scalar_pointer(paint,parameter,true,mode,false,false,false,true);
            }
            std::cout<<"star_paint_scalars_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-paint-scalars-whip-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(const auto* mode:{"drag","drag-cancel"})authored_paint_scalar_pointer(paint,parameter,true,mode,false,false,false,true);
            std::cout<<"star_paint_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-stroke-width-entry-pending-pointer")){
            bool failed=false;for(bool picking:{false,true}){try{circle_stroke_width_pointer(picking,"valid",true);}catch(const std::exception& error){failed=true;std::cerr<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}}
            check(!failed,"Both generated Rectangle Stroke first-pointer entries satisfy existing semantics");
            std::cout<<"rectangle_stroke_width_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-stroke-width-affected-pending-pointer")){
            for(bool picking:{false,true})for(const auto* mode:{"valid","zero","invalid","negative","cancel","expression-scalar","entry-session","entry-document","entry-revision","entry-source","entry-identity","entry-point-edit","entry-coordinate","entry-handle","entry-color","entry-order","entry-operation","entry-type","entry-gesture","entry-cancelled-gesture","apply-session","apply-document","apply-revision","apply-source","apply-identity","apply-point-edit","apply-coordinate","apply-handle","apply-color","apply-order","apply-operation","apply-type","apply-selection","apply-gesture","apply-cancelled-gesture"}){std::cerr<<(picking?"pick":"fx")<<" / "<<mode<<"\n";circle_stroke_width_pointer(picking,mode,true);}
            for(const auto* mode:{"invalid-expression","negative-expression"})circle_stroke_width_pointer(false,mode,true);
            for(const auto* mode:{"cycle","unit"})circle_stroke_width_pointer(true,mode,true);
            std::cout<<"rectangle_stroke_width_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-stroke-width-whip-pending-pointer")){
            for(const auto* mode:{"drag","drag-cancel"})circle_stroke_width_pointer(true,mode,true);
            std::cout<<"rectangle_stroke_width_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-stroke-width-entry-pending-pointer")){
            bool failed=false;for(bool picking:{false,true}){try{circle_stroke_width_pointer(picking,"valid",false,true);}catch(const std::exception& error){failed=true;std::cerr<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}}
            check(!failed,"Both generated Polygon Stroke first-pointer entries satisfy existing semantics");
            std::cout<<"polygon_stroke_width_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-stroke-width-affected-pending-pointer")){
            for(bool picking:{false,true})for(const auto* mode:{"valid","zero","invalid","negative","cancel","expression-scalar","entry-session","entry-document","entry-revision","entry-source","entry-topology","entry-identity","entry-point-edit","entry-coordinate","entry-handle","entry-color","entry-order","entry-operation","entry-type","entry-gesture","entry-cancelled-gesture","apply-session","apply-document","apply-revision","apply-source","apply-topology","apply-identity","apply-point-edit","apply-coordinate","apply-handle","apply-color","apply-order","apply-operation","apply-type","apply-selection","apply-gesture","apply-cancelled-gesture"}){std::cerr<<(picking?"pick":"fx")<<" / "<<mode<<"\n";circle_stroke_width_pointer(picking,mode,false,true);}
            for(const auto* mode:{"invalid-expression","negative-expression"})circle_stroke_width_pointer(false,mode,false,true);
            for(const auto* mode:{"cycle","unit"})circle_stroke_width_pointer(true,mode,false,true);
            std::cout<<"polygon_stroke_width_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-stroke-width-whip-pending-pointer")){
            for(const auto* mode:{"drag","drag-cancel"})circle_stroke_width_pointer(true,mode,false,true);
            std::cout<<"polygon_stroke_width_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-stroke-width-entry-pending-pointer")){
            bool failed=false;for(bool picking:{false,true}){try{circle_stroke_width_pointer(picking,"valid",false,false,true);}catch(const std::exception& error){failed=true;std::cerr<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}}
            check(!failed,"Both generated Star Stroke first-pointer entries satisfy existing semantics");
            std::cout<<"star_stroke_width_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-stroke-width-affected-pending-pointer")){
            for(bool picking:{false,true})for(const auto* mode:{"valid","zero","invalid","negative","cancel","expression-scalar","entry-session","entry-document","entry-revision","entry-source","entry-inner-radius","entry-topology","entry-identity","entry-point-edit","entry-coordinate","entry-handle","entry-color","entry-order","entry-operation","entry-type","entry-gesture","entry-cancelled-gesture","apply-session","apply-document","apply-revision","apply-source","apply-inner-radius","apply-topology","apply-identity","apply-point-edit","apply-coordinate","apply-handle","apply-color","apply-order","apply-operation","apply-type","apply-selection","apply-gesture","apply-cancelled-gesture"}){std::cerr<<(picking?"pick":"fx")<<" / "<<mode<<"\n";circle_stroke_width_pointer(picking,mode,false,false,true);}
            for(const auto* mode:{"invalid-expression","negative-expression"})circle_stroke_width_pointer(false,mode,false,false,true);
            for(const auto* mode:{"cycle","unit"})circle_stroke_width_pointer(true,mode,false,false,true);
            std::cout<<"star_stroke_width_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-stroke-width-whip-pending-pointer")){
            for(const auto* mode:{"drag","drag-cancel"})circle_stroke_width_pointer(true,mode,false,false,true);
            std::cout<<"star_stroke_width_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-stroke-width-entry-pending-pointer")){
            bool failed=false;for(bool picking:{false,true}){try{circle_stroke_width_pointer(picking);}catch(const std::exception& error){failed=true;std::cerr<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}}
            check(!failed,"Both generated Circle Stroke first-pointer entries satisfy existing semantics");
            std::cout<<"circle_stroke_width_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-stroke-width-affected-pending-pointer")){
            for(bool picking:{false,true})for(const auto* mode:{"valid","zero","invalid","negative","cancel","expression-scalar","entry-session","entry-document","entry-revision","entry-source","entry-identity","entry-point-edit","entry-coordinate","entry-handle","entry-color","entry-order","entry-operation","entry-type","entry-gesture","entry-cancelled-gesture","apply-session","apply-document","apply-revision","apply-source","apply-identity","apply-point-edit","apply-coordinate","apply-handle","apply-color","apply-order","apply-operation","apply-type","apply-selection","apply-gesture","apply-cancelled-gesture"}){std::cerr<<(picking?"pick":"fx")<<" / "<<mode<<"\n";circle_stroke_width_pointer(picking,mode);}
            for(const auto* mode:{"invalid-expression","negative-expression"})circle_stroke_width_pointer(false,mode);
            for(const auto* mode:{"cycle","unit"})circle_stroke_width_pointer(true,mode);
            std::cout<<"circle_stroke_width_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-stroke-width-whip-pending-pointer")){
            for(const auto* mode:{"drag","drag-cancel"})circle_stroke_width_pointer(true,mode);
            std::cout<<"circle_stroke_width_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-paint-scalars-entry-pending-pointer")){
            bool failed=false;for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(bool picking:{false,true}){
                try{authored_paint_scalar_pointer(paint,parameter,picking);}catch(const std::exception& error){failed=true;std::cerr<<paint<<"."<<parameter<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Sixteen authored paint first-pointer entries satisfy existing semantics");
            std::cout<<"path_paint_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-paint-scalars-affected-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"}){
                for(bool picking:{false,true})for(const auto* mode:{"valid","zero","one","negative","above","invalid","cancel","expression-scalar","entry-revision","entry-document","entry-session","entry-source","entry-coordinate","entry-handle","entry-type","entry-operation","entry-color","entry-order","entry-gesture","entry-cancelled-gesture","apply-revision","apply-document","apply-session","apply-source","apply-coordinate","apply-handle","apply-type","apply-operation","apply-color","apply-order","apply-selection","apply-gesture","apply-cancelled-gesture"}){
                    std::cerr<<paint<<"."<<parameter<<" / "<<(picking?"pick":"fx")<<" / "<<mode<<"\n";authored_paint_scalar_pointer(paint,parameter,picking,mode);
                }
                for(const auto* mode:{"invalid-expression","negative-expression","above-expression"})authored_paint_scalar_pointer(paint,parameter,false,mode);
                for(const auto* mode:{"cycle","unit"})authored_paint_scalar_pointer(paint,parameter,true,mode);
            }
            std::cout<<"path_paint_scalars_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-paint-scalars-whip-pending-pointer")){
            for(const auto* paint:{"fill","stroke"})for(const auto* parameter:{"r","g","b","a"})for(const auto* mode:{"drag","drag-cancel"})authored_paint_scalar_pointer(paint,parameter,true,mode);
            std::cout<<"path_paint_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-stroke-width-entry-pending-pointer")){
            bool failed=false;for(bool picking:{false,true}){try{authored_stroke_width_pointer(picking);}catch(const std::exception& error){failed=true;std::cerr<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}}
            check(!failed,"Two authored Stroke width first-pointer entries satisfy existing semantics");
            std::cout<<"path_stroke_width_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-stroke-width-affected-pending-pointer")){
            for(bool picking:{false,true})for(const auto* mode:{"valid","zero","negative","invalid","cancel","expression-scalar","entry-revision","entry-document","entry-session","entry-source","entry-coordinate","entry-handle","entry-type","entry-operation","entry-color","entry-order","entry-gesture","entry-cancelled-gesture","apply-revision","apply-document","apply-session","apply-source","apply-coordinate","apply-handle","apply-type","apply-operation","apply-color","apply-order","apply-selection","apply-gesture","apply-cancelled-gesture"}){
                std::cerr<<(picking?"pick":"fx")<<" / "<<mode<<"\n";authored_stroke_width_pointer(picking,mode);
            }
            for(const auto* mode:{"invalid-expression","negative-expression"})authored_stroke_width_pointer(false,mode);
            for(const auto* mode:{"cycle","unit"})authored_stroke_width_pointer(true,mode);
            std::cout<<"path_stroke_width_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-stroke-width-whip-pending-pointer")){
            for(const auto* mode:{"drag","drag-cancel"})authored_stroke_width_pointer(true,mode);
            std::cout<<"path_stroke_width_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--point-canvas-entry-pending-pointer")){
            bool failed=false;
            for(bool generated:{true,false})for(const auto* field:{"x","y"})for(bool picking:{false,true}){
                try{authored_point_coordinate_pointer(field,picking,"canvas-selection",generated);}
                catch(const std::exception& error){failed=true;std::cerr<<(generated?"Circle":"Authored")<<" Canvas "<<field<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            for(const auto* field:{"in.angle","out.length"})for(bool picking:{false,true})
                authored_point_coordinate_pointer(field,picking,"canvas-selection");
            for(const auto* field:{"x","y"})for(bool picking:{false,true})for(const auto* mode:{"entry-cancelled-gesture","apply-cancelled-gesture"})
                authored_point_coordinate_pointer(field,picking,mode,true,true);
            if(failed)return 1;std::cout<<"point_canvas_entry_pending_pointer: "<<checks<<" checks passed; Qt Window Canvas route\n";return 0;
        }
        if(app.arguments().contains("--circle-point-coordinates-entry-pending-pointer")){
            bool failed=false;for(const auto* field:{"x","y"})for(bool picking:{false,true}){
                try{authored_point_coordinate_pointer(field,picking,"valid",true);}catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Four generated Circle point coordinate first-pointer entries satisfy existing semantics");
            std::cout<<"circle_point_coordinates_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-point-coordinates-affected-pending-pointer")){
            for(const auto* field:{"x","y"})for(bool picking:{false,true})for(const auto* mode:{"fresh","point-edit","bypassed","zero","signed","cancel","invalid","expression-scalar","entry-revision","entry-session","entry-document","entry-source","entry-source-id","entry-coordinate","entry-handle","entry-type","entry-missing-point","entry-point-edit-reset","entry-point-edit-enabled","entry-gesture","entry-cancelled-gesture","apply-revision","apply-session","apply-document","apply-source","apply-source-id","apply-coordinate","apply-handle","apply-type","apply-missing-point","apply-point-edit-reset","apply-point-edit-enabled","apply-gesture","apply-cancelled-gesture"}){std::cerr<<"Circle point "<<field<<" / "<<(picking?"pick":"fx")<<" / "<<mode<<"\n";authored_point_coordinate_pointer(field,picking,mode,true);}
            for(const auto* field:{"x","y"})for(const auto* mode:{"invalid-expression","apply-selection"})authored_point_coordinate_pointer(field,false,mode,true);
            for(const auto* field:{"x","y"})for(const auto* mode:{"cycle","unit"})authored_point_coordinate_pointer(field,true,mode,true);
            std::cout<<"circle_point_coordinates_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-point-coordinates-whip-pending-pointer")){
            for(const auto* field:{"x","y"})for(const auto* mode:{"drag","drag-cancel"})authored_point_coordinate_pointer(field,true,mode,true);
            std::cout<<"circle_point_coordinates_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-point-coordinates-entry-pending-pointer")){
            bool failed=false;for(const auto* field:{"x","y"})for(bool picking:{false,true}){
                try{authored_point_coordinate_pointer(field,picking);}catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Four authored point coordinate first-pointer entries satisfy existing semantics");
            std::cout<<"path_point_coordinates_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-point-handles-entry-pending-pointer")){
            bool failed=false;for(const auto* field:{"in.angle","in.length","out.angle","out.length"})for(bool picking:{false,true}){
                try{authored_point_coordinate_pointer(field,picking);}catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<(picking?"pick":"fx")<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Eight authored handle first-pointer entries satisfy existing semantics");
            std::cout<<"path_point_handles_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-point-handles-affected-pending-pointer")){
            for(const auto* field:{"in.angle","in.length","out.angle","out.length"})for(bool picking:{false,true})
                for(const auto* mode:{"cancel","invalid","expression-scalar","entry-revision","entry-session","entry-document","entry-source","entry-coordinate","entry-handle","entry-type","entry-missing-point","entry-gesture","entry-cancelled-gesture","apply-revision","apply-session","apply-document","apply-source","apply-coordinate","apply-handle","apply-type","apply-missing-point","apply-gesture","apply-cancelled-gesture"}){
                    std::cerr<<field<<" / "<<(picking?"pick":"fx")<<" / "<<mode<<"\n";authored_point_coordinate_pointer(field,picking,mode);
                }
            for(const auto* field:{"in.angle","out.angle"})for(bool picking:{false,true})authored_point_coordinate_pointer(field,picking,"unwrapped");
            for(const auto* field:{"in.length","out.length"}){
                for(bool picking:{false,true})for(const auto* mode:{"zero","negative"})authored_point_coordinate_pointer(field,picking,mode);
                authored_point_coordinate_pointer(field,false,"negative-expression");
            }
            for(const auto* field:{"in.angle","in.length","out.angle","out.length"})for(const auto* mode:{"invalid-expression","apply-selection"})authored_point_coordinate_pointer(field,false,mode);
            std::cout<<"path_point_handles_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-point-handles-whip-pending-pointer")){
            for(const auto* field:{"in.angle","in.length","out.angle","out.length"})for(const auto* mode:{"drag","drag-cancel"})authored_point_coordinate_pointer(field,true,mode);
            std::cout<<"path_point_handles_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-point-coordinates-affected-pending-pointer")){
            for(const auto* field:{"x","y"})for(bool picking:{false,true})
                for(const auto* mode:{"cancel","invalid","expression-scalar","entry-revision","entry-session","entry-document","entry-source","entry-coordinate","entry-handle","entry-type","entry-missing-point","entry-gesture","entry-cancelled-gesture","apply-revision","apply-session","apply-document","apply-source","apply-coordinate","apply-handle","apply-type","apply-missing-point","apply-gesture","apply-cancelled-gesture"}){
                    std::cerr<<field<<" / "<<(picking?"pick":"fx")<<" / "<<mode<<"\n";authored_point_coordinate_pointer(field,picking,mode);
                }
            for(const auto* field:{"x","y"})for(const auto* mode:{"invalid-expression","apply-selection"})authored_point_coordinate_pointer(field,false,mode);
            std::cout<<"path_point_coordinates_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-point-coordinates-whip-pending-pointer")){
            for(const auto* field:{"x","y"})for(const auto* mode:{"drag","drag-cancel"})authored_point_coordinate_pointer(field,true,mode);
            std::cout<<"path_point_coordinates_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--instance-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,false,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,false,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Instance common property first-pointer entries satisfy the existing source contract");
            std::cout<<"instance_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--instance-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","entry-root","entry-subtree","entry-asset","entry-override","entry-visibility","entry-color","entry-content","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-root","apply-subtree","apply-asset","apply-override","apply-visibility","apply-color","apply-content","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,false,false,false,false,false,false,false,true);}
            std::cout<<"instance_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--instance-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","entry-type","entry-root","entry-subtree","entry-asset","entry-override","entry-visibility","entry-color","entry-content","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-root","apply-subtree","apply-asset","apply-override","apply-visibility","apply-color","apply-content","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,false,false,false,false,false,false,false,true);}
            std::cout<<"instance_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--instance-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,false,false,false,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,false,false,false,false,true);}
            std::cout<<"instance_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Image common property first-pointer entries satisfy the existing source contract");
            std::cout<<"image_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","entry-asset","entry-dimensions","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-asset","apply-dimensions","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,false,false,false,false,false,false,true);}
            std::cout<<"image_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","entry-type","entry-asset","entry-dimensions","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-asset","apply-dimensions","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,false,false,false,false,false,false,true);}
            std::cout<<"image_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--image-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,false,false,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,false,false,false,true);}
            std::cout<<"image_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Authored Path common property first-pointer entries satisfy the existing source contract");
            std::cout<<"path_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,false,false,false,false,false,true);}
            std::cout<<"path_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,false,false,false,false,false,true);}
            std::cout<<"path_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,false,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,false,false,true);}
            std::cout<<"path_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--group-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Group common property first-pointer entries satisfy the existing source contract");
            std::cout<<"group_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--group-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,false,false,false,false,true);}
            std::cout<<"group_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--group-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,false,false,false,false,true);}
            std::cout<<"group_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--group-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,false,true);}
            std::cout<<"group_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,true,true);else scalar_pick_pending_pointer("valid",field,false,false,true,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Star common property first-pointer entries satisfy the existing source contract");
            std::cout<<"star_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,false,false,true,true);}
            std::cout<<"star_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,false,false,true,true);}
            std::cout<<"star_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,false,true,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,true,true);}
            std::cout<<"star_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,true,false,true);else scalar_pick_pending_pointer("valid",field,false,true,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Polygon common property first-pointer entries satisfy the existing source contract");
            std::cout<<"polygon_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,false,true,false,true);}
            std::cout<<"polygon_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,false,true,false,true);}
            std::cout<<"polygon_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,true,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,true,false,true);}
            std::cout<<"polygon_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-common-scalars-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,true,false,false,true);else scalar_pick_pending_pointer("valid",field,true,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Rectangle common property first-pointer entries satisfy the existing source contract");
            std::cout<<"rectangle_common_scalars_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-common-scalars-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"}){if((std::string(mode)=="out-of-range"||std::string(mode)=="out-of-range-expression")&&std::string(field)!="composite.opacity")continue;scalar_fx_pending_pointer(mode,field,true,false,false,true);}
            std::cout<<"rectangle_common_scalars_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-common-scalars-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"}){if(std::string(mode)=="out-of-range"&&std::string(field)!="composite.opacity")continue;scalar_pick_pending_pointer(mode,field,true,false,false,true);}
            std::cout<<"rectangle_common_scalars_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-common-scalars-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d","transform.tx","transform.ty","transform.anchor_x","transform.anchor_y","composite.opacity"}){scalar_pick_pending_pointer("drag",field,true,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,true,false,false,true);}
            std::cout<<"rectangle_common_scalars_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-object-opacity-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Circle Object opacity first-pointer entries satisfy the existing source contract");
            std::cout<<"circle_object_opacity_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-object-opacity-fx-affected-pending-pointer")){
            for(const auto* field:{"composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_object_opacity_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-object-opacity-pick-affected-pending-pointer")){
            for(const auto* field:{"composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,false,true);
            std::cout<<"circle_object_opacity_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-object-opacity-whip-pending-pointer")){
            for(const auto* field:{"composite.opacity"}){scalar_pick_pending_pointer("drag",field,false,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,false,true);}
            std::cout<<"circle_object_opacity_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-object-opacity-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"composite.opacity"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Text Object opacity first-pointer entries satisfy the existing source contract");
            std::cout<<"text_object_opacity_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-object-opacity-fx-affected-pending-pointer")){
            for(const auto* field:{"composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","expression-scalar","invalid-expression","out-of-range-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_object_opacity_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-object-opacity-pick-affected-pending-pointer")){
            for(const auto* field:{"composite.opacity"})for(const auto* mode:{"plain","cancel","invalid","out-of-range","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_object_opacity_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-object-opacity-whip-pending-pointer")){
            for(const auto* field:{"composite.opacity"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_object_opacity_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-linear-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Text Affine matrix linear first-pointer entries satisfy the existing source contract");
            std::cout<<"text_affine_linear_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-linear-fx-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_affine_linear_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-linear-pick-affected-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","apply-revision","apply-document","apply-session","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_affine_linear_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--text-affine-linear-whip-pending-pointer")){
            for(const auto* field:{"transform.a","transform.b","transform.c","transform.d"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_affine_linear_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-scalar-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.radius"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Circle Radius first-pointer entries satisfy the existing source contract");
            std::cout<<"text_circle_scalar_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-scalar-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.radius"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_circle_scalar_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-scalar-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.radius"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_circle_scalar_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-scalar-whip-pending-pointer")){
            for(const auto* field:{"generator.radius"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_circle_scalar_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-width-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.width"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Rectangle Width first-pointer entries satisfy the existing source contract");
            std::cout<<"text_rectangle_width_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-width-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.width"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_rectangle_width_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-width-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.width"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_rectangle_width_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-width-whip-pending-pointer")){
            for(const auto* field:{"generator.width"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_rectangle_width_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-height-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.height"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Rectangle Height first-pointer entries satisfy the existing source contract");
            std::cout<<"text_rectangle_height_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-height-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.height"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_rectangle_height_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-height-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.height"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_rectangle_height_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-height-whip-pending-pointer")){
            for(const auto* field:{"generator.height"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_rectangle_height_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-radius-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.radius"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,true);else scalar_pick_pending_pointer("valid",field,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Polygon Radius first-pointer entries satisfy the existing source contract");
            std::cout<<"text_polygon_radius_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-radius-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.radius"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,true);
            std::cout<<"text_polygon_radius_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-radius-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.radius"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,true);
            std::cout<<"text_polygon_radius_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-radius-whip-pending-pointer")){
            for(const auto* field:{"generator.radius"}){scalar_pick_pending_pointer("drag",field,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,true);}
            std::cout<<"text_polygon_radius_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-outer-radius-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.outer_radius"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Star Outer radius first-pointer entries satisfy the existing source contract");
            std::cout<<"text_star_outer_radius_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-outer-radius-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.outer_radius"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_outer_radius_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-outer-radius-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.outer_radius"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_outer_radius_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-outer-radius-whip-pending-pointer")){
            for(const auto* field:{"generator.outer_radius"}){scalar_pick_pending_pointer("drag",field,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,true);}
            std::cout<<"text_star_outer_radius_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-inner-radius-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.inner_radius"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Star Inner radius first-pointer entries satisfy the existing source contract");
            std::cout<<"text_star_inner_radius_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-inner-radius-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.inner_radius"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_inner_radius_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-inner-radius-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.inner_radius"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_inner_radius_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-inner-radius-whip-pending-pointer")){
            for(const auto* field:{"generator.inner_radius"}){scalar_pick_pending_pointer("drag",field,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,true);}
            std::cout<<"text_star_inner_radius_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-rotation-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.rotation"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Star Rotation first-pointer entries satisfy the existing source contract");
            std::cout<<"text_star_rotation_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-rotation-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.rotation"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_rotation_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-rotation-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.rotation"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_rotation_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-rotation-whip-pending-pointer")){
            for(const auto* field:{"generator.rotation"}){scalar_pick_pending_pointer("drag",field,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,true);}
            std::cout<<"text_star_rotation_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-points-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.points"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Star Points first-pointer entries satisfy the existing source contract");
            std::cout<<"text_star_points_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-points-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.points"})for(const auto* mode:{"plain","cancel","invalid","topology-drop","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_points_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-points-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.points"})for(const auto* mode:{"plain","cancel","invalid","topology-drop","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_points_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-points-whip-pending-pointer")){
            for(const auto* field:{"generator.points"}){scalar_pick_pending_pointer("drag",field,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,true);}
            std::cout<<"text_star_points_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-points-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.points"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,true,false);else scalar_pick_pending_pointer("valid",field,false,true,false);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Polygon Points first-pointer entries satisfy the existing source contract");
            std::cout<<"text_polygon_points_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-points-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.points"})for(const auto* mode:{"plain","cancel","invalid","topology-drop","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,true,false);
            std::cout<<"text_polygon_points_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-points-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.points"})for(const auto* mode:{"plain","cancel","invalid","topology-drop","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,true,false);
            std::cout<<"text_polygon_points_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-points-whip-pending-pointer")){
            for(const auto* field:{"generator.points"}){scalar_pick_pending_pointer("drag",field,false,true,false);scalar_pick_pending_pointer("drag-cancel",field,false,true,false);}
            std::cout<<"text_polygon_points_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-centers-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,false,true);else scalar_pick_pending_pointer("valid",field,false,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Star Center X/Y first-pointer entries satisfy the existing source contract");
            std::cout<<"text_star_centers_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-centers-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_centers_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-centers-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,false,true);
            std::cout<<"text_star_centers_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--star-centers-whip-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"}){scalar_pick_pending_pointer("drag",field,false,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,false,true);}
            std::cout<<"text_star_centers_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-rotation-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.rotation"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,true);else scalar_pick_pending_pointer("valid",field,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Polygon Rotation first-pointer entries satisfy the existing source contract");
            std::cout<<"text_polygon_rotation_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-rotation-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.rotation"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,true);
            std::cout<<"text_polygon_rotation_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-rotation-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.rotation"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,true);
            std::cout<<"text_polygon_rotation_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-rotation-whip-pending-pointer")){
            for(const auto* field:{"generator.rotation"}){scalar_pick_pending_pointer("drag",field,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,true);}
            std::cout<<"text_polygon_rotation_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-centers-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,false,true);else scalar_pick_pending_pointer("valid",field,false,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Polygon Center first-pointer entries satisfy the existing source contract");
            std::cout<<"text_polygon_centers_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-centers-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,false,true);
            std::cout<<"text_polygon_centers_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-centers-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,false,true);
            std::cout<<"text_polygon_centers_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--polygon-centers-whip-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"}){scalar_pick_pending_pointer("drag",field,false,true);scalar_pick_pending_pointer("drag-cancel",field,false,true);}
            std::cout<<"text_polygon_centers_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-centers-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field,true);else scalar_pick_pending_pointer("valid",field,true);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Rectangle Center first-pointer entries satisfy the existing source contract");
            std::cout<<"text_rectangle_centers_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-centers-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field,true);
            std::cout<<"text_rectangle_centers_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-centers-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","entry-type","apply-revision","apply-document","apply-session","apply-source","apply-type","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field,true);
            std::cout<<"text_rectangle_centers_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--rectangle-centers-whip-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"}){scalar_pick_pending_pointer("drag",field,true);scalar_pick_pending_pointer("drag-cancel",field,true);}
            std::cout<<"text_rectangle_centers_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-centers-entry-pending-pointer")){
            bool failed=false;
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* action:{"fx","pick"}){
                try{if(std::string(action)=="fx")scalar_fx_pending_pointer("valid",field);else scalar_pick_pending_pointer("valid",field);}
                catch(const std::exception& error){failed=true;std::cerr<<field<<" / "<<action<<": "<<error.what()<<"\n";}
            }
            check(!failed,"Circle Center first-pointer entries satisfy the existing source contract");
            std::cout<<"text_circle_centers_entry_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-centers-fx-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","expression-scalar","invalid-expression","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture","apply-selection"})scalar_fx_pending_pointer(mode,field);
            std::cout<<"text_circle_centers_fx_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-centers-pick-affected-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"})for(const auto* mode:{"plain","cancel","invalid","entry-revision","entry-document","entry-session","entry-source","apply-revision","apply-document","apply-session","apply-source","apply-gesture","apply-cancelled-gesture"})scalar_pick_pending_pointer(mode,field);
            std::cout<<"text_circle_centers_pick_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--circle-centers-whip-pending-pointer")){
            for(const auto* field:{"generator.center_x","generator.center_y"}){scalar_pick_pending_pointer("drag",field);scalar_pick_pending_pointer("drag-cancel",field);}
            std::cout<<"text_circle_centers_whip_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--path-affected-pending-pointer")){for(const auto* mode:{"update","detach","plain","invalid","invalid-start","invalid-spacing","missing","entry-revision","entry-document","entry-session","entry-gesture","entry-cancelled-gesture"})path_pending_pointer(mode);std::cout<<"text_path_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--path-pending-pointer")){path_pending_pointer("attach");std::cout<<"text_path_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;}
        if(app.arguments().contains("--alignment-source-actions-pending-pointer")){
            for(const auto* action:{"edit","unlink","linked-edit"})alignment_source_action_pending_pointer(action);
            return 0;
        }
        if(app.arguments().contains("--locale-source-actions-pending-pointer")){
            for(const auto* action:{"edit","unlink","linked-edit"})locale_source_action_pending_pointer(action);
            return 0;
        }
        if(app.arguments().contains("--typography-affected-pending-pointer")){
            for(const auto* mode:{"feature-edit","feature-remove","axis-add","axis-edit","axis-remove","apply-gesture","apply-cancelled-gesture","apply-selection"})typography_pending_pointer(mode);
            std::cout<<"text_typography_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--typography-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})typography_pending_pointer(mode);
            std::cout<<"text_typography_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--locale-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})locale_link_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--alignment-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})alignment_link_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--layout-source-actions-pending-pointer")){
            for(const auto* action:{"edit","unlink","linked-edit"})layout_source_action_pending_pointer(action);
            return 0;
        }
        if(app.arguments().contains("--layout-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})layout_link_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--layout-frame-width-pending-pointer")){layout_link_pending_pointer("valid","text.frame_width");return 0;}
        if(app.arguments().contains("--layout-frame-width-affected-pending-pointer")){
            for(const auto* mode:{"cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})layout_link_pending_pointer(mode,"text.frame_width");
            std::cout<<"text_layout_frame_width_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--layout-frame-height-pending-pointer")){layout_link_pending_pointer("valid","text.frame_height");return 0;}
        if(app.arguments().contains("--layout-frame-height-affected-pending-pointer")){
            for(const auto* mode:{"cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})layout_link_pending_pointer(mode,"text.frame_height");
            std::cout<<"text_layout_frame_height_affected_pending_pointer: "<<checks<<" checks passed; Qt Window pointer route\n";return 0;
        }
        if(app.arguments().contains("--direction-source-actions-pending-pointer")){
            for(const auto* action:{"edit","unlink","linked-edit"})direction_source_action_pending_pointer(action);
            return 0;
        }
        if(app.arguments().contains("--direction-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})direction_link_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--italic-source-actions-pending-pointer")){
            family_unlink_pending_pointer(false,true,true);family_unlink_pending_pointer(false,false,true);return 0;
        }
        if(app.arguments().contains("--italic-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})family_drive_pending_pointer(mode,false,true);
            return 0;
        }
        if(app.arguments().contains("--weight-source-actions-pending-pointer")){
            family_unlink_pending_pointer(true,true);family_unlink_pending_pointer(true,false);return 0;
        }
        if(app.arguments().contains("--weight-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})family_drive_pending_pointer(mode,true);
            return 0;
        }
        if(app.arguments().contains("--family-unlink-pending-pointer")){family_unlink_pending_pointer();return 0;}
        if(app.arguments().contains("--family-link-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","entry-revision","entry-document","apply-revision","apply-document","apply-session","plain"})family_drive_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--family-drive-pending-pointer")){family_drive_pending_pointer();return 0;}
        if(app.arguments().contains("--structure-pending-pointer")){structure_pending_pointer();return 0;}
        if(app.arguments().contains("--italic-pending-pointer")){
            for(const auto* mode:{"valid","invalid","revision","document","plain"})italic_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--weight-pending-pointer")){
            for(const auto* mode:{"valid","invalid","revision","document","late-revision","plain"})weight_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--family-pending-pointer")){
            for(const auto* mode:{"cancel","invalid","revision","document","plain"})family_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--content-pending-pointer")){
            for(const auto* mode:{"cancel","apply","invalid","revision","document","plain"})content_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--writing-pending-pointer")){
            for(const auto* mode:{"valid","cancel","invalid","revision","document","plain"})writing_pending_pointer(mode);
            return 0;
        }
        if(app.arguments().contains("--keyboard-focus-help")){keyboard_focus_help();return 0;}
        if(app.arguments().contains("--keyboard-popup-focus-help")){keyboard_popup_focus_help();return 0;}
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
