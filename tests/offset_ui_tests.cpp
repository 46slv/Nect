#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
QScrollArea* inspector_scroll(Window& window) {
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
    check(scroll,"Production Inspector scroll exists");return scroll;
}
template<class T>T* widget(Window& window,const QString& name) {
    QApplication::processEvents();
    for(auto* item:window.findChildren<T*>())if(item->isVisible()&&item->objectName()==name)return item;
    throw std::runtime_error("Missing visible control: "+name.toStdString());
}
void trigger(Window& window,const char* name){auto* a=window.findChild<QAction*>(name);check(a,"Production action exists");a->trigger();QApplication::processEvents();}
void edit(Window& window,const Ref& ref,const char* text) {
    const auto key=QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},{"point",QString::fromStdString(ref.point)},
        {"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);
    QLineEdit* input=nullptr;
    for(auto* candidate:window.findChildren<QLineEdit*>())if(candidate->isVisible()&&candidate->property("nect-reference").toByteArray()==key){input=candidate;break;}
    check(input,"Offset uses normal property controls");inspector_scroll(window)->ensureWidgetVisible(input);
    input->setFocus();input->selectAll();QTest::keyClicks(input,text);QTest::keyClick(input,Qt::Key_Return);QApplication::processEvents();
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir temp;Window window(temp.path());window.show();QApplication::processEvents();auto& s=window.host.session;
        trigger(window,"add-rectangle");const auto id=window.canvas->selected_object;
        const auto original=encode(s.document());const auto source=s.document().objects.at(id).source;
        trigger(window,"add-offset");const auto offset=s.document().objects.at(id).stack.back().id;
        const auto operation=QString::fromStdString(offset);const auto amount=operation_ref(id,offset,"amount");
        check(s.document().objects.at(id).stack.back().type=="nect.shape.offset","Add menu creates retained Offset");
        auto* scroll=inspector_scroll(window);scroll->verticalScrollBar()->setValue(0);
        const auto navigation_revision=s.revision();widget<QPushButton>(window,"stack-jump")->menu()->actions().back()->trigger();QApplication::processEvents();
        auto* group=widget<QGroupBox>(window,"stack-operation-"+operation);
        check(scroll->viewport()->rect().contains(group->mapTo(scroll->viewport(),QPoint(10,20)))&&s.revision()==navigation_revision,
              "Stack navigation reveals Offset without changing authored state");
        edit(window,amount,"-12");check(evaluate(s.document()).at(amount)==-12,"Signed amount uses Session");
        widget<QComboBox>(window,"operation-line-join-"+operation)->setCurrentIndex(1);QApplication::processEvents();
        check(s.document().objects.at(id).stack.back().line_join=="round","Round join options are authored");
        widget<QComboBox>(window,"operation-fill-rule-"+operation)->setCurrentIndex(1);QApplication::processEvents();
        check(s.document().objects.at(id).stack.back().line_join=="round"&&s.document().objects.at(id).stack.back().fill_rule=="evenodd","Changing rule preserves join");
        edit(window,amount,"=10 + 5");check(evaluate(s.document()).at(amount)==15,"Offset accepts bounded expressions");
        widget<QCheckBox>(window,"operation-enabled-"+operation)->setChecked(false);QApplication::processEvents();
        check(!s.document().objects.at(id).stack.back().enabled&&s.document().objects.at(id).source==source,"Bypass retains source and options");
        const auto current=encode(s.document());check(encode(decode(current))==current,"UI-authored Offset survives exact native roundtrip");
        for(int i=0;i<6;++i){s.undo(s.revision());window.host.edited();QApplication::processEvents();}
        check(encode(s.document())==original,"Each UI edit undoes once back to original primitive");
        trigger(window,"add-curve");const auto before=encode(s.document());const auto revision=s.revision();
        trigger(window,"add-offset");
        check(s.revision()==revision&&encode(s.document())==before&&window.statusBar()->currentMessage().contains("OFFSET"),"Open path rejection is visible and atomic");
        std::cout<<"Offset UI controls, expressions, bypass, Undo and errors passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
