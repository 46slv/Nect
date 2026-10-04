#include "viewport_layout.hpp"
#include "visual_style.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect::desktop;
int main(int argc,char** argv) {
    QApplication app(argc,argv);int checks=0;
    auto check=[&](bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;};
    try {
        auto* view=new QLabel("Canvas");ViewportLayout layout(view);layout.resize(1000,650);
        auto* input=new QLineEdit("exact draft",layout.utility_contents());
        auto* toggle=new QPushButton("Toggle",layout.utility_contents());toggle->setCheckable(true);toggle->setChecked(true);
        layout.utility_layout()->addWidget(input);layout.utility_layout()->addWidget(toggle);
        layout.show();QApplication::processEvents();
        const auto* contents=layout.utility_contents();const auto* scroll=layout.utility_scroll();
        check(layout.utility_placement()==UtilityPlacement::top,"Default placement stays top");
        check(scroll->geometry().bottom()<view->geometry().top(),"Utility precedes Canvas");
        input->setText("uncommitted draft");input->setModified(true);input->setFocus();
        for(int i=0;i<4;++i) {
            const auto placement=i%2?UtilityPlacement::top:UtilityPlacement::bottom;
            layout.set_utility_placement(placement);QApplication::processEvents();
            check(layout.utility_contents()==contents&&layout.utility_scroll()==scroll,"Relocation preserves Utility identity");
            check(input->text()=="uncommitted draft"&&input->isModified(),"Relocation preserves draft without a commit");
            check(toggle->isChecked(),"Relocation preserves view toggle state");
            check(placement==UtilityPlacement::top?scroll->geometry().bottom()<view->geometry().top():
                view->geometry().bottom()<scroll->geometry().top(),"Relocation changes only layout order");
        }
        auto* wide=new QWidget(layout.utility_contents());wide->setMinimumWidth(1400);layout.utility_layout()->addWidget(wide);
        QApplication::processEvents();QTest::qWait(20);QApplication::processEvents();
        check(scroll->horizontalScrollBar()->maximum()>0,"Narrow Utility retains horizontal reachability");
        check(scroll->height()==VisualMetrics::utility_strip_max_height,"Overflow reserves scrollbar height");
        const QRect screen(0,0,1000,650);const QSize popup(300,200);
        check(anchored_popup_position({100,20,30,30},popup,screen)==QPoint(100,52),"Top anchor opens below");
        check(anchored_popup_position({100,610,30,30},popup,screen)==QPoint(100,408),"Bottom anchor flips above");
        check(anchored_popup_position({100,300,30,30},popup,screen,true)==QPoint(100,98),"Bottom placement prefers above");
        const auto clamped=anchored_popup_position({990,640,10,10},popup,screen);
        check(screen.contains(QRect(clamped,popup)),"Popup stays inside available screen");
        const QRect left_screen(-1920,0,1920,1080);
        check(left_screen.contains(QRect(anchored_popup_position({-10,1000,10,20},popup,left_screen),popup)),"Negative-origin monitor is respected");
        const auto oversized=anchored_popup_position({40,40,30,30},{1200,800},screen);
        check(oversized==screen.topLeft(),"Oversized popup safely clamps to available origin");
        QMainWindow shell;auto* left=new QDockWidget("Structure",&shell);
        configure_fixed_panel(left,Qt::LeftDockWidgetArea);left->setWidget(new QLabel("Objects"));
        shell.addDockWidget(Qt::LeftDockWidgetArea,left);shell.setCentralWidget(new QLabel("Canvas"));shell.show();
        QApplication::processEvents();
        check(left->features()==QDockWidget::DockWidgetClosable,"Fixed panels stay closable without move/float affordances");
        check(left->allowedAreas()==Qt::LeftDockWidgetArea,"Fixed panel region is explicit");
        left->toggleViewAction()->trigger();QApplication::processEvents();
        check(!left->isVisible(),"Fixed panel can be hidden");
        left->toggleViewAction()->trigger();QApplication::processEvents();
        check(left->isVisible()&&!left->isFloating(),"Fixed panel reopens in its region");
        std::cout<<"PASS "<<checks<<" viewport composition checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
