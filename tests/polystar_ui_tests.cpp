#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
QAction* action(Window& window,const char* name) {auto* result=window.findChild<QAction*>(name);check(result,"Find production action");return result;}
QLineEdit* field(Window& window,const Ref& ref) {
    const auto key=QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},{"point",QString::fromStdString(ref.point)},
        {"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);
    for(auto* edit:window.findChildren<QLineEdit*>())if(edit->isVisible()&&edit->property("nect-reference").toByteArray()==key)return edit;
    throw std::runtime_error("Visible generated property field is missing");
}
void edit(Window& window,const Ref& ref,const char* text) {
    auto* input=field(window,ref);window.findChild<QScrollArea*>()->ensureWidgetVisible(input);QApplication::processEvents();
    input->setFocus();input->selectAll();QTest::keyClicks(input,text);QTest::keyClick(input,Qt::Key_Return);QApplication::processEvents();
}
QTreeWidgetItem* row(Window& window,const Id& id) {
    auto* tree=window.findChild<QTreeWidget*>();
    for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->data(0,Qt::UserRole).toString().toStdString()==id)return tree->topLevelItem(i);
    throw std::runtime_error("Generated object tree row is missing");
}
void refresh(Window& window) {window.host.edited();QApplication::processEvents();}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir temp;Window window(temp.path());window.show();QApplication::processEvents();auto& session=window.host.session;
        action(window,"add-star")->trigger();QApplication::processEvents();const auto star=window.canvas->selected_object;
        const auto source=session.document().objects.at(star).source->id;
        const Ref count{star,"","generator.points"},outer{star,source+"-outer-1-5","x"};
        check(session.document().objects.at(star).source->type=="nect.shape.star"&&row(window,star)->childCount()==10,
              "Add Star creates a retained five-point source and ten editable vertices");
        check(window.findChild<QLabel*>("primitive-topology-note")!=nullptr,"Count/correction behavior is visible near the source");
        edit(window,{star,"","generator.inner_radius"},"70");
        check(evaluate(session.document()).at({star,"","generator.inner_radius"})==70,"Inner radius edits the real generator");
        window.canvas->set_selection(star,outer.point);QApplication::processEvents();edit(window,outer,"444");
        const auto corrected=encode(session.document());const auto revision=session.revision();
        edit(window,count,"6");
        check(session.revision()==revision&&encode(session.document())==corrected&&window.statusBar()->currentMessage().contains("UNRESOLVED_POINT_EDIT"),
              "GUI rejects a count change that removes the corrected angular role without partial mutation");
        bool confirmed=false;
        QTimer::singleShot(0,[&]{for(auto* top:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(top)) {
            if(box->windowTitle()=="Reset point edits"){confirmed=box->text().contains("one undoable edit");box->button(QMessageBox::Reset)->click();return;}
        }});
        auto* reset=window.findChild<QPushButton*>("point-edit-reset");check(reset,"Explicit reset is discoverable");reset->click();QApplication::processEvents();
        check(confirmed&&!session.document().objects.at(star).point_edit&&session.revision()==revision+1,"Reviewed reset removes only point overrides in one command");
        edit(window,count,"6");check(row(window,star)->childCount()==12&&window.canvas->selected_point.empty(),"New topology refreshes tree and clears disappeared point selection");
        session.undo(session.revision());refresh(window);session.undo(session.revision());refresh(window);
        check(encode(session.document())==corrected,"Undo count and reset restores the exact original source and correction");
        action(window,"add-polygon")->trigger();QApplication::processEvents();const auto polygon=window.canvas->selected_object;
        check(row(window,polygon)->childCount()==6,"Add Polygon starts with a retained hexagon");
        session.apply({Link{{polygon,"","generator.points"},{count,1,0,"copy_local_value"}}},session.revision());refresh(window);
        check(row(window,polygon)->childCount()==5,"Bound Polygon count updates generated tree topology through the normal property graph");
        session.apply({Set{count,10}},session.revision());refresh(window);
        check(row(window,star)->childCount()==20&&row(window,polygon)->childCount()==10&&evaluate(session.document()).at(outer)==444,
              "Linked count updates Canvas/tree while retaining the same angular correction instead of reindexing it");
        const auto values=evaluate(session.document());const auto geometry=evaluate_shape(session.document(),polygon,values);
        check(geometry.paths.front().contours->front().points.size()==10,"Shared shape evaluator receives resolved dynamic topology");
        check(row(window,star)->child(4)->text(0).contains("72"),"Point labels expose the preserved angular role");
        window.host.save(temp.path()+"/polystar.nect");window.host.open(temp.path()+"/polystar.nect");QApplication::processEvents();
        check(row(window,star)->childCount()==20&&row(window,polygon)->childCount()==10,"Saved bound topology reopens in the production Window");
        std::cout<<"PASS Polygon/Star Add, count/radius controls, explicit correction reset, linked topology and reopen\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
