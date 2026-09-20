#include "window.hpp"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QStatusBar>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
QByteArray reference(const Ref& r) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(r.object)},
        {"point",QString::fromStdString(r.point)},{"field",QString::fromStdString(r.field)}}).toJson(QJsonDocument::Compact);
}
template<class T> T* field(Window& w,const Ref& ref) {
    const auto data=reference(ref);
    for(auto* widget:w.findChildren<T*>())
        if(widget->isVisible()&&widget->property("nect-reference").toByteArray()==data)return widget;
    throw std::runtime_error("Visible property widget missing");
}
void move(Window& w,QPoint global) {
    QMouseEvent event(QEvent::MouseMove,w.mapFromGlobal(global),global,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&w,&event);QApplication::processEvents();
}
void release(Window& w,QPoint global) {
    QMouseEvent event(QEvent::MouseButtonRelease,w.mapFromGlobal(global),global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&w,&event);QApplication::processEvents();
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir temp;Window w(temp.path());w.show();QApplication::processEvents();
        auto& s=w.host.session;const auto comp=s.document().compositions.front().id;
        Point a,b;a.id="a1";a.x.literal=100;a.y.literal=180;b.id="b1";b.x.literal=200;b.y.literal=250;
        s.apply({CreatePath{comp,"","a","Target",{{"ca",false,{a}}}},CreatePath{comp,"","b","Source",{{"cb",false,{b}}}}},0);
        w.canvas->set_selection("a","a1");w.host.edited();QApplication::processEvents();
        const Ref target{"a","a1","x"},source{"b","b1","y"};
        auto* input=field<QLineEdit>(w,target);input->setFocus();QTest::keyClicks(input,"draft");
        const auto draft=input->text();QTest::qWait(1100);
        check(input->text()==draft&&input->hasFocus(),"Recovery status must not replace focused input drafts");
        check(s.revision()==1,"Incomplete draft is not committed by recovery");
        input->setText("100");input->setModified(false);
        auto* whip=field<QPushButton>(w,target);
        QTest::mousePress(whip,Qt::LeftButton,Qt::NoModifier,whip->rect().center());
        auto* tree=w.findChild<QTreeWidget*>();check(tree!=nullptr,"Object tree exists");
        QTreeWidgetItem* item=nullptr;
        for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->data(0,Qt::UserRole)=="b")item=tree->topLevelItem(i);
        check(item!=nullptr,"Source row exists");
        move(w,tree->viewport()->mapToGlobal(tree->visualItemRect(item).center()));
        check(w.canvas->selected_object=="b","Pick-whip can inspect another object without changing its target");
        auto* source_field=field<QLineEdit>(w,source);
        release(w,source_field->mapToGlobal(source_field->rect().center()));
        if(s.revision()!=2||evaluate(s.document()).at(target)!=250)
            std::cerr<<"revision="<<s.revision()<<" value="<<evaluate(s.document()).at(target)
                <<" status="<<w.statusBar()->currentMessage().toStdString()<<'\n';
        check(s.revision()==2&&evaluate(s.document()).at(target)==250,"Pick-whip commits one shared command");
        check(nect::property(s.document(),target).binding->source==source,"Pick-whip persists stable source ID");
        check(w.canvas->selected_object=="a","Pick-whip returns to target context");
        s.undo(2);w.host.edited();QApplication::processEvents();
        check(evaluate(s.document()).at(target)==100&&!nect::property(s.document(),target).binding,"Pick-whip undo restores target");
        whip=field<QPushButton>(w,target);QTest::mousePress(whip,Qt::LeftButton,Qt::NoModifier,whip->rect().center());
        move(w,tree->viewport()->mapToGlobal(tree->visualItemRect(tree->topLevelItem(1)).center()));
        QTest::keyClick(&w,Qt::Key_Escape);QApplication::processEvents();
        check(s.revision()==3&&!nect::property(s.document(),target).binding&&w.canvas->selected_object=="a",
            "Pick-whip cancellation preserves document and restores context");
        std::cout<<"PASS Inspector recovery draft and cross-object pick-whip/undo/cancel\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
