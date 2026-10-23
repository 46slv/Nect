#include "window.hpp"
#include <QApplication>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidgetItemIterator>
#include <cmath>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void near(double v,double expected,const char* why){check(std::abs(v-expected)<1e-6,why);}
template<class T>T* field(Window& w,const Ref& ref) {
    for(auto* widget:w.findChildren<T*>())if(widget->isVisible()) {
        const auto data=QJsonDocument::fromJson(widget->property("nect-reference").toByteArray()).object();
        if(data["object"].toString().toStdString()==ref.object&&data["point"].toString().toStdString()==ref.point&&data["field"].toString().toStdString()==ref.field)return widget;
    }
    std::string available;
    for(auto* widget:w.findChildren<T*>())if(widget->isVisible())available+=widget->property("nect-reference").toByteArray().toStdString()+" ";
    throw std::runtime_error("Visible property field not found: "+ref.object+"/"+ref.field+"; available "+available);
}
void edit(Window& w,const Ref& ref,const char* value){QApplication::processEvents();auto* f=field<QLineEdit>(w,ref);w.findChild<QScrollArea*>()->ensureWidgetVisible(f);
    f->setFocus();f->selectAll();QTest::keyClicks(f,value);QTest::keyClick(f,Qt::Key_Return);QApplication::processEvents();}
std::vector<Command> fixture() {
    std::vector<Command> commands;
    for(int i=0;i<4;++i){auto source=default_primitive("source-"+std::to_string(i),"nect.shape.rectangle");
        source.parameters.at("center_x").literal=100+100*i;source.parameters.at("center_y").literal=200;
        source.parameters.at("width").literal=50;source.parameters.at("height").literal=60;
        commands.push_back(CreatePrimitive{"comp","","rect-"+std::to_string(i),"Rectangle "+std::to_string(i),source});}
    return commands;
}
void controls() {
    QTemporaryDir temp;Window w(temp.path());auto document=empty_document("doc","comp","board");
    document.compositions.push_back(empty_document("other-doc","other-comp","other-board").compositions.front());w.host.session=Session(document);
    auto& s=w.host.session;auto commands=fixture();
    commands.push_back(CreateText{"comp","","picker-text","Picker text",default_text("picker-text-source","Picker source")});
    commands.push_back(CreatePrimitive{"other-comp","","foreign","Foreign source",default_primitive("foreign-source","nect.shape.circle")});
    s.apply(commands,s.revision());w.host.edited();w.show();QApplication::processEvents();
    const std::vector<Canvas::Selection> targets{{"rect-0",""},{"rect-1",""},{"rect-2",""}};
    w.canvas->set_selections(targets);QApplication::processEvents();const Ref x{"rect-0","","generator.center_x"};
    auto* input=field<QLineEdit>(w,x);check(input->text().isEmpty()&&input->placeholderText()=="Mixed","Mixed values are explicit and not averaged");
    check(w.findChild<QTreeWidget*>()->selectedItems().size()==3,"Canvas selection synchronizes tree");
    const auto original=encode(s.document());auto revision=s.revision();edit(w,x,"400");
    check(s.revision()==revision+1,"Batch absolute is one edit");for(int i=0;i<3;++i)near(evaluate(s.document()).at({"rect-"+std::to_string(i),"","generator.center_x"}),400,"Absolute sets every target");
    check(w.canvas->selections()==targets&&field<QLineEdit>(w,x)->hasFocus(),"Batch field focus and selection survive commit");
    s.undo(s.revision());w.host.edited();check(encode(s.document())==original,"One Undo restores all targets");
    edit(w,x,"+=10");for(int i=0;i<3;++i)near(evaluate(s.document()).at({"rect-"+std::to_string(i),"","generator.center_x"}),110+100*i,"Relative preserves each difference");
    field<QPushButton>(w,x)->click();QApplication::processEvents();QDialog* picker=nullptr;
    for(auto* dialog:w.findChildren<QDialog*>())if(dialog->isVisible())picker=dialog;
    check(picker,"Picker opens for multiple targets");auto* list=picker->findChild<QListWidget*>();int source=-1;bool read_only_text_ref=false,numeric_text_ref=false;
    for(int i=0;i<list->count();++i){auto ref=QJsonDocument::fromJson(list->item(i)->data(Qt::UserRole).toByteArray()).object();
        if(ref["object"]=="foreign"&&ref["field"]=="generator.center_x")source=i;
        if(ref["object"]=="picker-text") {
            const auto field=ref["field"].toString();
            if(field=="text.content"||field=="text.family"||field=="text.locale"||field=="text.layout"||field=="text.direction"||field=="text.alignment")read_only_text_ref=true;
            if(field=="text.font_size")numeric_text_ref=true;
        }}
    check(source>=0,"Searchable source keeps stable reference");list->setCurrentRow(source);QApplication::processEvents();
    check(!read_only_text_ref&&numeric_text_ref,"Numeric source picker omits read-only Text strings/enums and keeps Text Scalars");
    check(w.canvas->selected_object=="foreign"&&w.canvas->active_composition()=="other-comp","Source inspection can change composition and visible selection");
    picker->reject();QApplication::processEvents();check(w.canvas->selections()==targets&&w.canvas->active_artboard()=="board","Cancel restores complete frozen target selection and frame");
    field<QPushButton>(w,x)->click();QApplication::processEvents();picker=nullptr;
    for(auto* dialog:w.findChildren<QDialog*>())if(dialog->isVisible())picker=dialog;
    list=picker->findChild<QListWidget*>();list->setCurrentRow(source);revision=s.revision();
    picker->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();QApplication::processEvents();
    check(s.revision()==revision+1&&w.canvas->selections()==targets,"Picking links all frozen targets and restores selection");
    for(int i=0;i<3;++i)check(property(s.document(),{"rect-"+std::to_string(i),"","generator.center_x"}).binding.has_value(),"Every target retains its link");
    const auto linked=encode(s.document());revision=s.revision();edit(w,x,"500");check(s.revision()==revision&&encode(s.document())==linked,"Typing cannot silently unlink the batch");
    // A real Ctrl-click in the extended tree must not collapse the Canvas set.
    w.canvas->set_selection("rect-0");auto* tree=w.findChild<QTreeWidget*>();QTreeWidgetItem* second=nullptr;
    for(QTreeWidgetItemIterator i(tree);*i;++i)if((*i)->data(0,Qt::UserRole).toString()=="rect-1"&&(*i)->data(0,Qt::UserRole+1).toString().isEmpty())second=*i;
    check(second,"Second tree object exists");tree->scrollToItem(second);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::ControlModifier,tree->visualItemRect(second).center());QApplication::processEvents();
    check(w.canvas->selections().size()==2&&tree->selectedItems().size()==2,"Extended tree and Canvas keep the same two objects");
    w.host.save(temp.path()+"/batch.nect");const auto saved=encode(s.document());w.host.open(temp.path()+"/batch.nect");check(encode(s.document())==saved,"Batch links save and reopen without additional UI state");
}
void duplicate_selection() {
    QTemporaryDir temp;Window w(temp.path());w.host.session=Session(empty_document("doc","comp","board"));
    auto& s=w.host.session;s.apply(fixture(),s.revision());w.host.edited();w.show();QApplication::processEvents();
    w.canvas->set_selections({{"rect-0",""},{"rect-1",""}});const auto before=s.document();const auto revision=s.revision();
    auto* duplicate=w.findChild<QAction*>("duplicate-objects");check(duplicate&&duplicate->shortcut()==QKeySequence("Ctrl+D"),"Discoverable duplicate shortcut");
    w.canvas->setFocus();QTest::keyClick(w.canvas,Qt::Key_D,Qt::ControlModifier);QApplication::processEvents();
    check(s.revision()==revision+1&&s.document().objects.size()==6,"Shortcut duplicates selection in one edit");
    const auto copies=w.canvas->selected_objects();check(copies.size()==2&&copies[0]!="rect-0"&&copies[1]!="rect-1","Copies selected after duplicate");
    check(w.findChild<QTreeWidget*>()->selectedItems().size()==2,"Tree and Canvas select both copies");
    const auto copied=s.document();s.undo(s.revision());w.host.edited();check(s.document()==before,"One Undo removes both copies");
    s.redo(s.revision());w.host.edited();check(s.document()==copied,"Redo restores stable copied state");
    w.canvas->set_selection(copies.front());edit(w,{copies.front(),"","generator.width"},"72");
    check(evaluate(s.document()).at({"rect-0","","generator.width"})==50,"Inspector editing copy leaves original intact");
    check(decode(encode(s.document()))==s.document(),"Edited copied state roundtrips");
}
void canvas_drag() {
    Session s(empty_document("doc","comp","board"));s.apply(fixture(),s.revision());auto board=s.document().compositions.front().artboards.front();board.width=640;board.height=480;
    s.apply({UpdateArtboard{"comp",board}},s.revision());Canvas c(s);c.set_snap_enabled(false);QString error;c.error=[&](QString e){error=e;};c.resize(740,580);c.show();QApplication::processEvents();c.fit_artboard();
    auto click=[&](int x,int y,Qt::KeyboardModifiers m=Qt::NoModifier){QTest::mouseClick(&c,Qt::LeftButton,m,QPoint(x+50,y+50));QApplication::processEvents();};
    auto drag=[&](int x,int y,int dx,int dy,bool cancel=false){QTest::mousePress(&c,Qt::LeftButton,Qt::NoModifier,QPoint(x+50,y+50));QTest::mouseMove(&c,QPoint(x+50+dx,y+50+dy),1);QApplication::processEvents();
        if(cancel)QTest::keyClick(&c,Qt::Key_Escape);QTest::mouseRelease(&c,Qt::LeftButton,Qt::NoModifier,QPoint(x+50+dx,y+50+dy));QApplication::processEvents();};
    click(100,170);click(200,170,Qt::ShiftModifier);check(c.selections().size()==2,"Shift-click adds an object");
    const auto before=encode(s.document());auto revision=s.revision();drag(200,170,25,15);
    check(error.isEmpty()&&s.revision()==revision+1,"Multi-object translation commits once");
    for(int i=0;i<2;++i){near(evaluate(s.document()).at({"rect-"+std::to_string(i),"","transform.tx"}),25,"Selected objects translate once X");near(evaluate(s.document()).at({"rect-"+std::to_string(i),"","transform.ty"}),15,"Selected objects translate once Y");}
    s.undo(s.revision());c.refresh();check(encode(s.document())==before,"One Undo restores complete object drag");
    c.set_selections({{"rect-0","source-0-top-left"},{"rect-1","source-1-top-left"}});
    check(c.selections().size()==2,"Generated point roles are selectable across objects");revision=s.revision();drag(75,170,20,10);
    check(error.isEmpty()&&s.revision()==revision+1,"Multi-point drag commits once");
    for(int i=0;i<2;++i)near(evaluate(s.document()).at({"rect-"+std::to_string(i),"source-"+std::to_string(i)+"-top-left","x"}),95+100*i,"Each point retains its starting difference");
    const auto moved=encode(s.document());revision=s.revision();drag(95,180,20,10,true);check(encode(s.document())==moved&&s.revision()==revision,"Escape cancels every point together");
}
}
int main(int argc,char** argv){qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);try{controls();canvas_drag();duplicate_selection();std::cout<<"Batch UI passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
