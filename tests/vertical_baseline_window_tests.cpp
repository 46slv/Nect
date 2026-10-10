#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QApplication::processEvents();}
Document fixture(Document document){
    document.objects.clear();document.compositions.front().roots={"first","second"};
    for(const auto* id:{"first","second"}){
        const bool first=std::string(id)=="first";Object text;text.id=id;text.name=id;text.kind=Kind::text;
        text.text=default_text(std::string(id)+"-source",first?"日ABC本":"縦XYZ書き");
        text.text->direction="vertical";text.text->parameters.at("font_size").literal=first?24:42;
        text.text->parameters.at("origin_x").literal=first?12:45;text.transform[4].literal=first?80:210;
        text.transform[5].literal=first?0:90;
        text.stack={default_operation(std::string(id)+"-fill","nect.paint.fill")};
        document.objects.emplace(text.id,text);
    }return document;
}
double baseline(const Document& document,const Id& id){
    const auto values=evaluate(document);const auto layout=evaluate_text_projection(document,id,values);
    check(!layout.column_baselines_x.empty(),"Measured first-column baseline exists");
    const auto world=evaluate_transforms(document,values).at(id).world;
    return world[0]*layout.column_baselines_x.front()+world[4];
}
void click(Window& window){
    events();QTest::qWait(20);
    auto* button=window.findChild<QPushButton*>("quick-align-x-baseline");
    check(button&&button->isEnabled()&&button->isVisible(),"Vertical baseline public action is present");
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");check(scroll!=nullptr,"Properties scroll exists");
    // The production stylesheet queues wrapped-label layout after the first
    // event pass. Settle it before scrolling the entire action into view.
    QTest::qWait(20);
    scroll->horizontalScrollBar()->setValue(0);scroll->ensureWidgetVisible(button);QTest::qWait(20);
    const auto bounds=QRect(button->mapTo(scroll->viewport(),QPoint{}),button->size());
    if(scroll->horizontalScrollBar()->value()!=0||!scroll->viewport()->rect().contains(bounds)) {
        if(const auto output=qEnvironmentVariable("NECT_VERTICAL_BASELINE_CAPTURE");!output.isEmpty())window.grab().save(output);
        throw std::runtime_error("Vertical action width: button="+std::to_string(bounds.x())+","+std::to_string(bounds.y())+","+
            std::to_string(bounds.width())+","+std::to_string(bounds.height())+" viewport="+
            std::to_string(scroll->viewport()->width())+","+std::to_string(scroll->viewport()->height())+
            " content="+std::to_string(scroll->widget()->width())+" min="+
            std::to_string(scroll->widget()->minimumSizeHint().width()));
    }
    check(scroll->horizontalScrollBar()->value()==0&&scroll->viewport()->rect().contains(bounds),"Vertical action fits 300px Inspector without horizontal scrolling");
    button->click();events();
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    try{
        QTemporaryDir scratch;check(scratch.isValid(),"Owned workspace exists");
        QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
        Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();
        auto* properties=window.findChild<QDockWidget*>("properties");check(properties!=nullptr,"Production Properties dock exists");
        properties->setMinimumWidth(300);properties->setMaximumWidth(300);properties->raise();events();
        auto& session=window.host.session;session=Session(fixture(session.document()));window.host.edited();
        window.canvas->set_selections({{"first",""},{"second",""}});events();
        const auto original=session.document();const auto old_history=session.history();const auto first_x=baseline(original,"first");
        check(std::abs(first_x-baseline(original,"second"))>1,"Fixture starts with distinct measured baselines");
        click(window);const auto aligned=session.document();
        check(session.revision()==1&&session.history().states.size()==old_history.states.size()+1,"Window alignment is one command/Undo");
        check(std::abs(baseline(aligned,"first")-baseline(aligned,"second"))<1e-7&&std::abs(baseline(aligned,"first")-first_x)<1e-7,"Measured vertical first-column baselines align to stationary source");
        check(aligned.objects.at("first")==original.objects.at("first")&&aligned.objects.at("second").text==original.objects.at("second").text,"Source stays byte-exact and target retains Japanese/mixed text/font/layout");
        session.undo(session.revision());window.host.edited();check(session.document()==original,"One Undo restores exact authored document");
        session.redo(session.revision());window.host.edited();check(session.document()==aligned,"One Redo restores exact alignment");
        const auto path=scratch.filePath("vertical.nect.json");window.host.save(path);
        Host reopened(scratch.filePath("reopened"));reopened.open(path);check(reopened.session.document()==aligned,"Native Host reopen retains aligned source");
        if(const auto output=qEnvironmentVariable("NECT_VERTICAL_BASELINE_DOCUMENT");!output.isEmpty()) {
            QFile file(output);const auto bytes=encode(aligned);
            check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(bytes.data(),static_cast<qint64>(bytes.size()))==static_cast<qint64>(bytes.size()),"Owned aligned candidate saved");
        }
        auto mixed=original;mixed.objects.at("second").text->direction="horizontal";session=Session(mixed);window.host.edited();
        window.canvas->set_selections({{"first",""},{"second",""}});events();
        const auto mixed_history=session.history();click(window);
        check(session.document()==mixed&&session.revision()==0&&session.history()==mixed_history,"Later incompatible target rejects entire UI alignment atomically");
        auto* reference=window.findChild<QComboBox*>("alignment-target");
        check(reference!=nullptr,"Alignment reference selector exists");
        const auto unsupported=reference->findData(QStringLiteral("artboard:")+QString::fromStdString(mixed.compositions.front().artboards.front().id));
        check(unsupported>=0,"Exact Artboard reference exists");reference->setCurrentIndex(unsupported);events();
        check(!window.findChild<QPushButton*>("quick-align-x-baseline")->isEnabled()&&!window.findChild<QPushButton*>("quick-align-y-baseline")->isEnabled(),"Both baseline actions disable unsupported reference types");
        std::cout<<"vertical_baseline_window_tests: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
