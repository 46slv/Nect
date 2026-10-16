#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QTest::qWait(20);}
Document fixture(){
    auto d=empty_document("instance-actions","composition","source-board");
    d.compositions.front().artboards.push_back({"target-board","Target",700,0,640,480});
    Object root;root.id="root";root.name="Shared source";root.kind=Kind::group;root.children={"rectangle","text"};
    Object rectangle;rectangle.id="rectangle";rectangle.name="Rectangle";
    rectangle.source=default_primitive("rectangle-source","nect.shape.rectangle");
    rectangle.stack={default_operation("source-fill","nect.paint.fill")};
    Object text;text.id="text";text.name="Text";text.kind=Kind::text;text.text=default_text("text-source","Retained text");
    text.text->parameters.at("font_size").expression=Expression{"24 + 2",1};
    d.objects.emplace(root.id,root);d.objects.emplace(rectangle.id,rectangle);d.objects.emplace(text.id,text);
    d.compositions.front().roots={root.id};
    Session s(d);s.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        DefinitionCommand{CreateInstance{"composition",{},"first","definition","First"}},
        DefinitionCommand{CreateInstance{"composition",{},"second","definition","Second"}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target-board","template",Id{"content"}}}},0);
    return s.document();
}
struct Snapshot {
    Document document,preview;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture_active;
    explicit Snapshot(const Session& s):document(s.document()),preview(s.preview_document()),native(encode(document)),revision(s.revision()),generation(s.gesture_generation()),history(s.history()),gesture_active(s.gesture_active()){}
    bool unchanged(const Session& s)const{return document==s.document()&&preview==s.preview_document()&&native==encode(s.document())&&revision==s.revision()&&generation==s.gesture_generation()&&history==s.history()&&gesture_active==s.gesture_active();}
};
void select(Window& w,const Id& id){w.canvas->set_selection(id);events();
    auto* dock=w.findChild<QDockWidget*>("properties");check(dock,"Properties dock exists");dock->show();dock->raise();events();}
QPushButton* button(Window& w,const char* name){
    auto* b=w.findChild<QPushButton*>(name);check(b,"Selected Instance exposes contextual Scalar Set/Reset actions");
    for(auto* scroll:w.findChildren<QScrollArea*>())if(scroll->widget()&&scroll->widget()->isAncestorOf(b))scroll->ensureWidgetVisible(b);
    events();check(b->isVisible()&&b->visibleRegion().contains(b->rect()),"Contextual action is fully reachable in real Properties viewport");return b;
}
void click(QPushButton* b){check(b->isEnabled(),"Contextual action enabled");QTest::mouseClick(b,Qt::LeftButton);events();}
void history(Window& w,const QString& label){for(auto* a:w.findChildren<QAction*>())if(a->text()==label){a->trigger();events();return;}throw std::runtime_error("History action absent");}
void set_override(Window& w,const Id& source,const QString& field,const QString& number,bool cancel=false){
    auto* entry=button(w,"instance-scalar-set");
    std::exception_ptr failure;
    QTimer::singleShot(0,[&]{QPointer<QDialog> dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());try{
        check(dialog&&dialog->objectName()=="set-instance-override-dialog","Contextual Set opens existing canonical dialog");
        auto* target=dialog->findChild<QComboBox*>("set-instance-override-source");auto* property=dialog->findChild<QComboBox*>("set-instance-override-property");
        auto* value=dialog->findChild<QDoubleSpinBox*>("set-instance-override-value");check(target&&property&&value,"Existing source/field/value controls available");
        const auto target_index=target->findData(QString::fromStdString(source));check(target_index>=0,"Exact stable source offered");target->setCurrentIndex(target_index);
        const auto field_index=property->findData(field);check(field_index>=0,"Supported canonical field offered");property->setCurrentIndex(field_index);
        QTest::mouseClick(value,Qt::LeftButton);QTest::keyClick(value,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(value,number);
        auto* buttons=dialog->findChild<QDialogButtonBox*>();check(buttons,"Apply/Cancel available");
        QTest::mouseClick(buttons->button(cancel?QDialogButtonBox::Cancel:QDialogButtonBox::Apply),Qt::LeftButton);
    }catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});
    click(entry);if(failure)std::rethrow_exception(failure);events();
}
void reset_override(Window& w,const QString& field){
    auto* entry=button(w,"instance-scalar-reset");
    std::exception_ptr failure;
    QTimer::singleShot(0,[&]{QPointer<QInputDialog> dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());try{
        check(dialog&&dialog->windowTitle()=="Reset Instance override","Contextual Reset opens existing chooser");
        auto* chooser=dialog->findChild<QComboBox*>();check(chooser,"Reset chooser available");int index=-1;
        for(int i=0;i<chooser->count();++i)if(chooser->itemText(i).endsWith(field))index=i;
        check(index>=0,"Exact override field offered");chooser->setCurrentIndex(index);QTest::keyClick(chooser,Qt::Key_Return);
    }catch(...){failure=std::current_exception();if(dialog)dialog->reject();}});
    click(entry);if(failure)std::rethrow_exception(failure);events();
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir scratch;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path());
    app.setOrganizationName("NectTest");app.setApplicationName("InstanceScalarActions");
    try{Window w(scratch.filePath("recovery"));w.host.session=Session(fixture());w.host.session_id="instance-actions-session";w.host.edited();w.resize(1280,900);w.show();w.activateWindow();events();select(w,"first");
        auto& s=w.host.session;const Snapshot original(s);auto* set=button(w,"instance-scalar-set");check(set->isEnabled(),"Fresh selected Instance has Set entry");
        check(!button(w,"instance-scalar-reset")->isEnabled(),"No Scalar override disables Reset");check(original.unchanged(s),"Reaching Properties actions is source/native/history neutral");
        const auto evidence=qEnvironmentVariable("NECT_INSTANCE_ACTION_EVIDENCE");
        if(!evidence.isEmpty())check(w.grab().save(evidence),"Real contextual action Window captured");
        set_override(w,"rectangle","generator.width","173",true);check(original.unchanged(s),"Cancel preserves complete source/native/history");
        set_override(w,"rectangle","generator.width","173");Session expected(original.document);expected.apply({DefinitionCommand{SetInstanceOverride{"first",{"rectangle",{},"generator.width"},173}}},0);
        check(s.document()==expected.document()&&encode(s.document())==encode(expected.document())&&s.revision()==1&&s.history()==expected.history(),"Pointer Set and keyboard value equal exactly one canonical transaction");
        const auto width_document=s.document();history(w,"Undo");check(s.document()==original.document&&encode(s.document())==original.native,"One Undo restores all shared source/Instance/Template/IDs");history(w,"Redo");check(s.document()==width_document,"One Redo restores exact local override");
        select(w,"second");check(!button(w,"instance-scalar-reset")->isEnabled(),"Other Instance remains inherited");select(w,"first");
        Session size_expected(s.document());size_expected.apply({DefinitionCommand{SetInstanceOverride{"first",{"text",{},"text.font_size"},31}}},0);
        const auto size_revision=s.revision();set_override(w,"text","text.font_size","31");
        check(s.document()==size_expected.document()&&s.revision()==size_revision+1,"Driven source font-size override equals one canonical transaction without altering expression");
        auto before_reset=s.document();Session reset_expected(before_reset);reset_expected.apply({DefinitionCommand{ResetInstanceOverride{"first",{"text",{},"text.font_size"}}}},0);
        const auto reset_revision=s.revision();reset_override(w,"text.font_size");check(s.document()==reset_expected.document()&&s.revision()==reset_revision+1,"Reset removes only chosen Scalar override");
        check(s.document().objects.at("text")==original.document.objects.at("text"),"Driven shared Text source retained");history(w,"Undo");check(s.document()==before_reset,"Reset Undo restores both overrides exactly");history(w,"Redo");check(s.document()==reset_expected.document(),"Reset Redo restores exact inheritance");
        reset_override(w,"generator.width");check(s.document()==original.document,"Last Reset returns entire document to retained source");check(!button(w,"instance-scalar-reset")->isEnabled(),"Reset disables when no Scalar overrides remain");
        set_override(w,"rectangle","transform.tx","47");const auto authored=s.document();const auto native=encode(authored);const auto file=scratch.filePath("instance-actions.nect");w.host.save(file);w.host.open(file);events();select(w,"first");
        check(s.document()==authored&&encode(s.document())==native,"Same Window native reopen preserves complete contextual override source");
        Window cold(scratch.filePath("cold"));cold.host.open(file);check(cold.host.session.document()==authored,"Fresh Window native reopen preserves source/identity");
        Session reloaded_expected(authored);reloaded_expected.apply({DefinitionCommand{SetInstanceOverride{"first",{"rectangle",{},"generator.height"},91}}},0);
        set_override(w,"rectangle","generator.height","91");check(s.document()==reloaded_expected.document()&&s.revision()==1,"Fresh post-reload contextual edit binds current Session and exact Instance");
        history(w,"Undo");check(s.document()==authored&&encode(s.document())==native,"Post-reload Undo restores full saved native source");
        select(w,"content");const auto template_before=s.document();Session template_expected(template_before);
        template_expected.apply({DefinitionCommand{SetInstanceOverride{"content",{"rectangle",{},"generator.width"},221}}},0);
        const auto template_revision=s.revision();set_override(w,"rectangle","generator.width","221");
        check(s.document()==template_expected.document()&&s.revision()==template_revision+1,"Template content Instance contextual edit retains all assignment/source/other Instance identities");
        history(w,"Undo");check(s.document()==template_before,"Template content override Undo exact");
        w.canvas->set_selections({{"first",{}},{"second",{}}});events();
        check(!w.findChild<QPushButton*>("instance-scalar-set"),"Multi-selection never exposes a one-Instance mutation entry");
        select(w,"rectangle");check(!w.findChild<QPushButton*>("instance-scalar-set"),"Plain source has no Instance-only action");
        w.host.changed={};cold.host.changed={};w.hide();std::cout<<"PASS "<<checks<<" contextual Instance action checks; physical input NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
