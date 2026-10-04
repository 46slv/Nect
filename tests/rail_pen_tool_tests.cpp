#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
auto snapshot(Session& s){return std::tuple{s.document(),encode(s.document()),s.revision(),s.history()};}
void show(Window& w){w.setAttribute(Qt::WA_DontShowOnScreen);w.resize(1100,750);w.show();w.activateWindow();events();}
auto active_board(Window& w){
    const auto& compositions=w.host.session.document().compositions;
    const auto comp=std::find_if(compositions.begin(),compositions.end(),[&](const auto& c){return c.id==w.canvas->active_composition();});
    check(comp!=compositions.end(),"Active drawing Composition exists");
    return evaluate_artboard(*comp,w.canvas->active_artboard());
}
QPoint screen(Window& w,double x,double y){
    const auto b=active_board(w);
    return {qRound(w.canvas->width()/2.0+(x-b.x-b.width/2)*w.canvas->zoom()),
        qRound(w.canvas->height()/2.0+(y-b.y-b.height/2)*w.canvas->zoom())};
}
QPointF authored(Window& w,QPoint p,const QTransform& parent){
    const auto b=active_board(w);
    const QPointF world{b.x+b.width/2+(p.x()-w.canvas->width()/2.0)/w.canvas->zoom(),
        b.y+b.height/2+(p.y()-w.canvas->height()/2.0)/w.canvas->zoom()};
    return parent.inverted().map(world);
}
void activate(Window& w){
    auto* pen=w.findChild<QToolButton*>("tool-pen");check(pen&&pen->isVisible()&&pen->isEnabled(),"Real Pen Rail button reachable");
    const auto before=snapshot(w.host.session);QTest::mouseClick(pen,Qt::LeftButton);events();
    check(w.canvas->draw_mode()&&pen->isChecked()&&snapshot(w.host.session)==before,"Pen activation does not author artwork/history");
}
// Independent complete-Document oracle for each existing canonical creation command.
Id point(Window& w,Session& oracle,QPoint p,const Id& path={},const Id& parent={},const QTransform& transform={},const Id& composition="composition"){
    const auto revision=w.host.session.revision();const auto history=w.host.session.history().states.size();
    QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,p);events();
    auto& actual=w.host.session;const auto id=w.canvas->selected_object;
    check(!id.empty()&&actual.document().objects.contains(id),"Pointer authored a selected Path");
    if(path.empty()) {
        std::cout<<"New path target="<<id<<" already_exists="<<oracle.document().objects.contains(id)<<'\n';
        check(!oracle.document().objects.contains(id),"New placement cannot append to a Path owned by an expired drawing context");
    }
    const auto& object=actual.document().objects.at(id);const auto& contour=object.contours.front();
    Point expected;expected.id=w.canvas->selected_point;const auto local=authored(w,p,transform);
    const auto& inserted=contour.points.back();
    std::cout.precision(17);std::cout<<"Point actual="<<inserted.x.literal<<','<<inserted.y.literal
        <<" oracle="<<local.x()<<','<<local.y()<<'\n';
    check(std::abs(inserted.x.literal-local.x())<1e-9&&std::abs(inserted.y.literal-local.y())<1e-9,
        "Independent screen/world/parent inverse gives the authored point coordinates");
    // Equivalent inverse arithmetic can differ by a final floating-point bit.
    // Only the independently checked coordinates enter the complete source oracle.
    expected.x.literal=inserted.x.literal;expected.y.literal=inserted.y.literal;
    if(path.empty())oracle.apply({CreatePath{composition,parent,id,"Path",{{contour.id,false,{expected}}}}},oracle.revision());
    else {check(id==path,"Next point targets the current Path only");oracle.apply({AddPoint{path,contour.id,expected}},oracle.revision());}
    std::cout<<"Complete source="<<(actual.document()==oracle.document())<<" revision delta="<<(actual.revision()-revision)
        <<" history delta="<<(actual.history().states.size()-history)<<'\n';
    check(actual.document()==oracle.document()&&actual.revision()==revision+1&&actual.history().states.size()==history+1,
        "Actual pointer authoring matches complete canonical Document/revision/history oracle");
    return id;
}
void finish_open(Window& w,int key=Qt::Key_Return){
    auto& s=w.host.session;const auto before=snapshot(s);QTest::keyClick(w.canvas,static_cast<Qt::Key>(key));events();
    const auto* pen=w.findChild<QToolButton*>("tool-pen");
    std::cout<<"Finish open: pen="<<w.canvas->draw_mode()<<" checked="<<pen->isChecked()<<" document_unchanged="<<(snapshot(s)==before)<<'\n';
    check(w.canvas->draw_mode()&&pen->isChecked(),"Enter finishes only the current Path and retains active Pen");
    check(snapshot(s)==before,"Finishing an open Path is transient and does not add a revision/history edit");
}
void choose_board(Window& w,int index,const Id& composition,const Id& board){
    auto* list=w.findChild<QListWidget*>("artboards");check(list&&list->isVisible()&&index<list->count(),"Real Artboards navigation is reachable");
    const auto before=snapshot(w.host.session);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(index)).center());events();
    std::cout<<"Artboard switch: comp="<<w.canvas->active_composition()<<" board="<<w.canvas->active_artboard()<<" pen="<<w.canvas->draw_mode()<<'\n';
    check(w.canvas->active_composition()==composition&&w.canvas->active_artboard()==board,"Real Artboards click changes the intended active plane");
    check(w.canvas->draw_mode()&&w.findChild<QToolButton*>("tool-pen")->isChecked(),"Artboard navigation retains active Pen");
    check(snapshot(w.host.session)==before,"Artboard navigation does not author or add history");
}
void pen_shortcut_reactivation(Window& w,const Document& initial){
    for(const auto key:{Qt::Key_P,Qt::Key_G}) {
        w.canvas->set_draw_mode(false);w.host.session=Session(initial);w.host.edited();events();w.canvas->fit_artboard();events();
        Session oracle(initial);activate(w);
        const auto path=point(w,oracle,screen(w,90,120));point(w,oracle,screen(w,190,140),path);
        QApplication::setActiveWindow(&w);w.canvas->setFocus();events();check(w.canvas->hasFocus(),"Shortcut reaches the actual focused Canvas");
        const auto before=snapshot(w.host.session);QTest::keyClick(w.canvas,key);events();
        check(w.canvas->draw_mode()&&w.findChild<QToolButton*>("tool-pen")->isChecked()&&snapshot(w.host.session)==before,"P/G reactivation retains Pen without authoring");
        point(w,oracle,screen(w,290,150),path);
        check(w.host.session.document().objects.size()==1,"Reactivating Pen cannot silently start another Path");
        activate(w);point(w,oracle,screen(w,390,170),path);
        const auto authored=w.host.session.document();w.host.session.undo(w.host.session.revision());w.host.edited();events();
        check(w.host.session.document().objects.at(path).contours.front().points.size()==3,"One Undo removes only the last continued point");
        w.host.session.redo(w.host.session.revision());w.host.edited();events();check(w.host.session.document()==authored,"Redo restores the complete continued Path");
        finish_open(w);const auto next=point(w,oracle,screen(w,450,250));check(next!=path,"Enter still finishes the current Path before another placement");
        QTest::keyClick(w.canvas,Qt::Key_Escape);events();check(!w.canvas->draw_mode(),"Escape still exits Pen explicitly");
    }
}
void pen_session_expiry(Window& w,const Document& initial,QTemporaryDir& scratch){
    Session oracle(initial);activate(w);const auto pending=point(w,oracle,screen(w,90,120));point(w,oracle,screen(w,190,140),pending);
    const auto source=w.host.session.document();const auto file=scratch.filePath("pending-pen.nect.json");w.host.save(file);events();
    const auto saved=snapshot(w.host.session);const auto previous_session=w.host.session_id;
    w.host.open(file);events();w.canvas->fit_artboard();events();
    check(w.host.session_id!=previous_session&&w.host.session.document()==source,"Native reopen starts a distinct Session with the same Document and Path IDs");
    check(w.canvas->draw_mode()&&w.findChild<QToolButton*>("tool-pen")->isChecked(),"Native reopen preserves active workspace Pen");
    check(std::get<0>(saved)==w.host.session.document(),"Reopen does not author a source edit");
    Session reopened(source);const auto next=point(w,reopened,screen(w,290,150));
    check(next!=pending&&w.host.session.document().objects.at(pending)==source.objects.at(pending),"New Session expires only the pending target and preserves the original Path");
    point(w,reopened,screen(w,390,170),next);const auto final=w.host.session.document();
    w.host.session.undo(w.host.session.revision());w.host.edited();events();
    check(w.host.session.document().objects.at(next).contours.front().points.size()==1&&w.host.session.document().objects.at(pending)==source.objects.at(pending),"Undo in the new Session removes only its new point");
    w.host.session.redo(w.host.session.revision());w.host.edited();events();check(w.host.session.document()==final,"Redo in the new Session restores complete authored state");
    w.host.save(file);events();QSettings cold_preferences(scratch.filePath("cold-pending.ini"),QSettings::IniFormat);
    Window cold(scratch.filePath("cold-pending"),std::make_unique<FolderLibrary>(cold_preferences),&cold_preferences);show(cold);cold.host.open(file);events();
    check(cold.host.session.document()==final&&!cold.canvas->draw_mode(),"New Window native reopen preserves both Paths and defaults to Selection");
}
void pen_menu_completion(Window& w,const Document& initial,QTemporaryDir& scratch){
    Session oracle(initial);activate(w);
    const auto pending=point(w,oracle,screen(w,90,120));point(w,oracle,screen(w,190,140),pending);
    point(w,oracle,screen(w,160,230),pending);
    const auto before_close=w.host.session.document();const auto contour=before_close.objects.at(pending).contours.front().id;
    QMenu* edit=nullptr;QAction* toggle=nullptr;
    for(auto* action:w.menuBar()->actions())if(action->text()=="&Edit")edit=action->menu();
    check(edit,"Actual Edit menu exists");
    for(auto* action:edit->actions())if(action->text()=="Close / open contour")toggle=action;
    check(toggle&&toggle->isEnabled(),"Actual close/open command is enabled for the pending Pen contour");
    QApplication::setActiveWindow(&w);w.canvas->setFocus();events();
    QTest::mouseClick(w.menuBar(),Qt::LeftButton,Qt::NoModifier,w.menuBar()->actionGeometry(edit->menuAction()).center());events();
    check(edit->isVisible(),"Pointer opens the real Edit menu");
    QTest::mouseClick(edit,Qt::LeftButton,Qt::NoModifier,edit->actionGeometry(toggle).center());events();
    oracle.apply({CloseContour{pending,contour,true}},oracle.revision());
    check(snapshot(w.host.session)==snapshot(oracle),"Menu closure is one exact canonical CloseContour with complete source/revision/history");
    check(w.canvas->draw_mode()&&w.findChild<QToolButton*>("tool-pen")->isChecked(),"Menu closure retains the active Pen tool");
    const auto closed=w.host.session.document();
    std::cout<<"Menu closed="<<closed.objects.at(pending).contours.front().closed<<" next placement must create a distinct Path\n";
    const auto next=point(w,oracle,screen(w,330,240));
    check(next!=pending&&w.host.session.document().objects.at(pending)==closed.objects.at(pending),
        "Next pointer placement preserves the finished closed Path and creates a distinct Path");
    const auto with_next=w.host.session.document();
    w.host.session.undo(w.host.session.revision());w.host.edited();events();
    check(w.host.session.document()==closed,"One Undo removes only the subsequent Path");
    w.host.session.redo(w.host.session.revision());w.host.edited();events();
    check(w.host.session.document()==with_next,"One Redo restores both complete authored Paths");
    w.host.session.undo(w.host.session.revision());w.host.edited();events();
    w.host.session.undo(w.host.session.revision());w.host.edited();events();
    check(w.host.session.document()==before_close&&w.canvas->draw_mode(),"Undoing closure restores source without resurrecting the expired pending draw target");
    auto branch_oracle=w.host.session;const auto target=screen(w,390,280);const auto local=authored(w,target,{});
    QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,target);events();
    const auto after_undo=w.canvas->selected_object;const auto& new_source=w.host.session.document().objects.at(after_undo);
    check(!branch_oracle.document().objects.contains(after_undo),"Rebranch placement creates a distinct stable Object ID");
    const auto& new_contour=new_source.contours.front();Point expected;expected.id=w.canvas->selected_point;
    const auto& inserted=new_contour.points.front();
    check(std::abs(inserted.x.literal-local.x())<1e-9&&std::abs(inserted.y.literal-local.y())<1e-9,
        "Independent world/local coordinates also hold after Undo rebranch");
    expected.x.literal=inserted.x.literal;expected.y.literal=inserted.y.literal;
    branch_oracle.apply({CreatePath{"composition",{},after_undo,"Path",{{new_contour.id,false,{expected}}}}},branch_oracle.revision());
    check(snapshot(w.host.session)==snapshot(branch_oracle)&&!w.host.session.can_redo(),
        "New placement after Undo matches full canonical source/revision/history and prunes only the old redo branch");
    check(after_undo!=pending&&w.host.session.document().objects.at(pending)==before_close.objects.at(pending),
        "Placement after closure Undo starts another distinct Path without retargeting restored source");
    const auto final=w.host.session.document();const auto file=scratch.filePath("pen-menu-completion.nect.json");w.host.save(file);w.host.open(file);events();
    check(w.host.session.document()==final&&w.canvas->draw_mode(),"Same Window native reopen preserves full source and Pen");
    QSettings cold_preferences(scratch.filePath("menu-cold.ini"),QSettings::IniFormat);
    Window cold(scratch.filePath("menu-cold"),std::make_unique<FolderLibrary>(cold_preferences),&cold_preferences);show(cold);cold.host.open(file);events();
    check(cold.host.session.document()==final&&!cold.canvas->draw_mode(),"New Window native reopen preserves complete editable source with default Selection");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        QTemporaryDir scratch;check(scratch.isValid(),"Owned preferences/recovery scratch");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        preferences.setValue("unrelated","preserved");preferences.setValue("workspace/tools/textCreationDirection","vertical");
        Window w(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);show(w);
        auto initial=empty_document("pen-document","composition","board");
        initial.compositions.front().artboards.front().width=640;initial.compositions.front().artboards.front().height=480;
        w.host.session=Session(initial);w.host.edited();events();w.canvas->fit_artboard();w.canvas->set_snap_enabled(false);events();
        auto& s=w.host.session;
        if(app.arguments().contains("--menu-completion-only")){
            pen_menu_completion(w,initial,scratch);std::cout<<"pen_menu_completion: "<<checks<<" checks passed\n";return 0;
        }
        if(app.arguments().contains("--shortcut-reactivation-only")||app.arguments().contains("--session-expiry-only")){
            if(app.arguments().contains("--session-expiry-only"))pen_session_expiry(w,initial,scratch);
            else pen_shortcut_reactivation(w,initial);
            check(preferences.value("unrelated").toString()=="preserved"&&preferences.value("workspace/tools/textCreationDirection").toString()=="vertical","Pen continuation preserves workspace preferences");
            std::cout<<"pen_context_contract: "<<checks<<" checks passed\n";return 0;
        }
        Session oracle(initial);activate(w);
        const auto first=point(w,oracle,screen(w,90,120));point(w,oracle,screen(w,190,140),first);
        const auto first_source=s.document().objects.at(first);finish_open(w);finish_open(w,Qt::Key_Enter);
        const auto second_start=screen(w,290,150);const auto second=point(w,oracle,second_start);
        check(second!=first&&s.document().objects.at(first)==first_source,"Next click creates an independent Path and preserves the completed open source");
        point(w,oracle,screen(w,390,170),second);point(w,oracle,screen(w,340,250),second);
        const auto before_close=s.document();const auto contour=s.document().objects.at(second).contours.front().id;
        const auto before_revision=s.revision();const auto before_history=s.history().states.size();
        QTest::mouseClick(w.canvas,Qt::LeftButton,Qt::NoModifier,second_start);events();
        oracle.apply({CloseContour{second,contour,true}},oracle.revision());
        std::cout<<"Close Path: pen="<<w.canvas->draw_mode()<<" closed="<<s.document().objects.at(second).contours.front().closed<<'\n';
        check(w.canvas->draw_mode()&&w.findChild<QToolButton*>("tool-pen")->isChecked(),"Clicking first anchor closes the contour and retains Pen");
        check(s.document()==oracle.document()&&s.revision()==before_revision+1&&s.history().states.size()==before_history+1,
            "Close is exactly one canonical command with complete source and history preserved");
        const auto closed=s.document();s.undo(s.revision());w.host.edited();events();check(s.document()==before_close,"One Undo restores complete pre-close source");
        s.redo(s.revision());w.host.edited();events();check(s.document()==closed,"One Redo restores complete closed source");
        // Undo/Redo advance revision independently; the helper checks each actual command delta.
        Session next(closed);const auto third=point(w,next,screen(w,480,300));
        check(third!=first&&third!=second&&s.document().objects.at(second)==closed.objects.at(second),
            "After close/Undo/Redo, next click starts a new Path without reopening the closed source");
        finish_open(w);const auto escaped=snapshot(s);QTest::keyClick(w.canvas,Qt::Key_Escape);events();
        check(!w.canvas->draw_mode()&&w.findChild<QToolButton*>("tool-selection")->isChecked()&&snapshot(s)==escaped,
            "Escape exits Pen without rewriting completed artwork/history");

        // Navigation finishes only transient drawing ownership, never appending to the old plane.
        auto boards=initial;auto second_board=boards.compositions.front().artboards.front();
        second_board.id="second-board";second_board.name="Second";second_board.x=800;
        boards.compositions.front().artboards.push_back(second_board);
        auto other=empty_document("unused","other-composition","other-board").compositions.front();
        other.artboards.front().x=1600;other.artboards.front().width=640;other.artboards.front().height=480;
        boards.compositions.push_back(other);w.host.session=Session(boards);w.host.edited();events();w.canvas->fit_artboard();events();
        Session board_oracle(boards);activate(w);const auto pending=point(w,board_oracle,screen(w,100,100));
        const auto pending_source=s.document().objects.at(pending);choose_board(w,1,"composition","second-board");
        const auto on_second=point(w,board_oracle,screen(w,900,120));
        check(on_second!=pending&&s.document().objects.at(pending)==pending_source,"New Artboard starts a new Path and preserves the pending source");
        const auto previous=s.document();choose_board(w,2,"other-composition","other-board");
        const auto on_other=point(w,board_oracle,screen(w,1700,150),{},{},{},"other-composition");
        check(s.document().compositions.front()==previous.compositions.front()&&
            s.document().compositions.back().roots==std::vector<Id>{on_other},"Composition navigation places only in the new plane and retains all old roots/source");
        finish_open(w);QTest::keyClick(w.canvas,Qt::Key_Escape);events();

        // Both finished paths and subsequent placement remain in the current transformed Group.
        auto grouped=initial;Object group;group.id="group";group.name="Drawing group";group.kind=Kind::group;
        group.transform={Scalar{0,{}},Scalar{1,{}},Scalar{-1,{}},Scalar{0,{}},Scalar{520,{}},Scalar{70,{}}};
        grouped.objects.emplace(group.id,group);grouped.compositions.front().roots={group.id};
        Point seed_a;seed_a.id="seed-a";seed_a.x.literal=30;seed_a.y.literal=40;
        Point seed_b=seed_a;seed_b.id="seed-b";seed_b.x.literal=50;
        Session seeded(grouped);seeded.apply({CreatePath{"composition","group","seed","Existing artwork",{{"seed-contour",false,{seed_a,seed_b}}}}},seeded.revision());
        grouped=seeded.document();w.host.session=Session(grouped);w.host.edited();events();w.canvas->set_selection("seed");w.canvas->fit_artboard();events();
        check(w.canvas->drill_scope()=="group","Existing child selection supplies the exact drawing scope");
        const QTransform parent(0,1,-1,0,520,70);Session group_oracle(grouped);activate(w);
        const auto child=point(w,group_oracle,screen(w,450,180),{},"group",parent);point(w,group_oracle,screen(w,430,220),child,"group",parent);
        finish_open(w);const auto sibling=point(w,group_oracle,screen(w,380,180),{},"group",parent);
        check(sibling!=child&&s.document().objects.at("group").children==std::vector<Id>{"seed",child,sibling},
            "Next Path remains a sibling in the exact transformed Group scope");
        finish_open(w);const auto native=encode(s.document());const auto file=scratch.filePath("pen.nect.json");
        w.host.save(file);w.host.open(file);events();check(encode(s.document())==native&&w.canvas->draw_mode(),"Same Window reopen retains editable source and active workspace Pen");
        Window cold(scratch.filePath("cold"),std::make_unique<FolderLibrary>(preferences),&preferences);show(cold);cold.host.open(file);events();
        check(encode(cold.host.session.document())==native&&!cold.canvas->draw_mode(),"Fresh Window reopens complete native source with default Selection");
        check(preferences.value("unrelated").toString()=="preserved"&&preferences.value("workspace/tools/textCreationDirection").toString()=="vertical",
            "Pen workflow preserves unrelated preferences and Text variant");
        std::cout<<"rail_pen_tool_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"After "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
