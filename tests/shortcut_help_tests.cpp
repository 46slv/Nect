#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace nect::desktop;
int main(int argc,char** argv) {
    QApplication app(argc,argv);int checks=0;
    auto require=[&](bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;};
    try {
        QTemporaryDir files;require(files.isValid(),"Temporary directory exists");
        QSettings settings(files.filePath("library.ini"),QSettings::IniFormat);
        Window window(files.path()+"/recovery",std::make_unique<FolderLibrary>(settings));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.show();QApplication::processEvents();
        const auto document=window.host.session.document();const auto history=window.host.session.history();
        auto* open=window.findChild<QAction*>("shortcut-help");require(open,"Help action exists");
        auto* test_action=new QAction("&Live && focused action",&window);
        test_action->setShortcut(QKeySequence("Ctrl+Alt+9"));test_action->setShortcutContext(Qt::WidgetShortcut);
        for(int pass=0;pass<2;++pass) {
            open->trigger();QApplication::processEvents();
            QDialog* dialog=nullptr;
            for(auto* candidate:window.findChildren<QDialog*>("shortcut-help-dialog"))if(candidate->isVisible())dialog=candidate;
            require(dialog,"Help dialog opens repeatedly");
            auto* table=dialog->findChild<QTableWidget*>("shortcut-help-inventory");
            require(table&&table->rowCount()>10,"Help lists actual production assignments");
            require(table->editTriggers()==QAbstractItemView::NoEditTriggers,"Help is read-only");
            bool found=false;
            for(int row=0;row<table->rowCount();++row)if(table->item(row,0)->text()=="Live & focused action") {
                require(table->item(row,1)->text()==test_action->shortcut().toString(QKeySequence::NativeText),"Help uses current key assignment");
                require(table->item(row,2)->text()=="Focused widget","Help states shortcut context");found=true;
            }
            require(found,"Live action is discovered without duplicate help metadata");
            dialog->findChild<QPushButton*>("shortcut-help-close")->click();
            QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            test_action->setShortcut(QKeySequence("Ctrl+Alt+8"));
        }
        require(window.host.session.document()==document&&window.host.session.history()==history,"Help does not author document or history");
        window.host.changed={};window.hide();std::cout<<"PASS "<<checks<<" shortcut Help checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
