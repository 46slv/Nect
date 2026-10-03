#include "macro_boolean_window_smoke.hpp"
#include <QDockWidget>
#include <QScrollBar>
#include <iostream>
using namespace macro_boolean_window_smoke;
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        QTemporaryDir files;require(files.isValid(),"Temporary directory exists");
        QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
        Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();load(window);
        auto* dock=window.findChild<QDockWidget*>("properties");
        auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");
        require(dock&&scroll,"Production Properties panel exists");
        dock->setFixedWidth(300);events();QTest::qWait(20);events();
        const Snapshot before(window.host.session);
        for(const auto* parameter:{amount_id,copies_id}) {
            auto* field=numeric(window,parameter);reveal(window,field);
            const auto point=field->mapTo(scroll->viewport(),QPoint(0,0));
            require(point.x()>=0&&point.x()+field->width()<=scroll->viewport()->width(),"Macro number fits narrow Inspector");
        }
        int resets=0;
        for(auto* reset:window.findChildren<QPushButton*>("macro-reset-amount-instance"))if(reset->isVisible()) {
            reveal(window,reset);const auto point=reset->mapTo(scroll->viewport(),QPoint(0,0));
            require(point.x()>=0&&point.x()+reset->width()<=scroll->viewport()->width(),"Macro Reset is not horizontally clipped");
            require(reset->width()>=reset->minimumSizeHint().width(),"Macro Reset retains full button width");++resets;
        }
        require(resets==2,"Both numeric Macro resets checked");
        require(before.unchanged(window.host.session),"Layout and reveal do not mutate document or history");
        auto* bar=scroll->verticalScrollBar();require(bar->maximum()>100,"Fixture has meaningful Inspector overflow");
        bar->setValue(bar->maximum()/2);const int saved=bar->value();
        window.refresh(false);window.refresh(false);events();QTest::qWait(20);events();
        require(bar->value()==saved,"Repeated same-context refresh preserves scroll");
        bar->setValue(bar->maximum());window.canvas->set_selection("other");window.host.edited();
        events();QTest::qWait(20);events();
        require(bar->value()==0,"Selection plus immediate host refresh preserves pending context reset");
        bar->setValue(bar->maximum());auto* add=window.findChild<QAction*>("add-text");require(add,"Add Text action exists");
        add->trigger();events();QTest::qWait(20);events();
        auto* edit=visible<QPushButton>(window,"edit-text-content");require(edit,"New text has an editing entry");
        const QRect edit_rect(edit->mapTo(scroll->viewport(),QPoint(0,0)),edit->size());
        require(scroll->viewport()->rect().contains(edit_rect),"New text editing entry is reachable immediately after deep scroll");
        window.host.changed={};window.hide();
        std::cout<<"PASS "<<checks<<" Inspector viewport checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
