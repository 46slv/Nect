#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void select_state(QListWidget* list,std::uint64_t id) {
    for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toULongLong()==id) {
        list->setCurrentRow(i);return;
    }
    throw std::runtime_error("History state missing from visible list");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir recovery;Window window(recovery.path());window.show();QApplication::processEvents();
        auto& session=window.host.session;const auto composition=session.document().compositions.front().id;
        Point point;point.id="p";point.x.literal=10;point.y.literal=20;
        session.apply({CreatePath{composition,"","curve","Curve",{{"contour",false,{point}}}}},session.revision());
        const auto original_state=session.history().current_id;const auto original=encode(session.document());
        const Ref ref{"curve","p","x"};
        for(int i=0;i<85;++i)session.apply({Set{ref,100.0+i}},session.revision());
        window.host.edited();const auto final_state=session.history().current_id;const auto final_document=encode(session.document());
        auto* action=window.findChild<QAction*>("show-history");check(action!=nullptr,"History is discoverable through the production View action");
        action->trigger();QApplication::processEvents();
        auto* dialog=window.findChild<QDialog*>("history-dialog");check(dialog&&dialog->isVisible(),"History list opens");
        auto* list=dialog->findChild<QListWidget*>("history-states");auto* restore=dialog->findChild<QPushButton*>("history-restore");
        auto* status=dialog->findChild<QLabel*>("history-status");
        check(list&&restore&&status&&list->count()==87,"History exposes more than the previous 64 operations");
        check(status->text().contains("estimated")&&status->text().contains("current session"),"History declares retention estimate and session scope");
        const auto before=session.revision();select_state(list,original_state);
        check(encode(session.document())==final_document,"Selecting a row alone does not change authored state");
        QTest::mouseClick(restore,Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==before+1&&encode(session.document())==original,"One explicit History action returns through 85 operations atomically");
        check(session.can_redo()&&list->count()==87,"Future states stay available after returning to an earlier state");
        select_state(list,final_state);QTest::mouseClick(restore,Qt::LeftButton);QApplication::processEvents();
        check(encode(session.document())==final_document&&session.revision()==before+2,"Visible History can return to its later state");
        select_state(list,original_state);QTest::mouseClick(restore,Qt::LeftButton);QApplication::processEvents();
        session.apply({Set{ref,777}},session.revision());window.host.edited();QApplication::processEvents();
        check(!session.can_redo()&&list->count()==3&&session.history().current_id>final_state,"A new edit replaces only the redo branch with a fresh stable state ID");
        check(list->currentItem()!=nullptr,"A removed selection resolves to a retained current state");
        window.host.create_document();QApplication::processEvents();
        check(list->count()==1&&session.history().current_id==0,"Replacing the document refreshes the list to the new session only");
        const auto revision=session.revision();QTest::mouseClick(restore,Qt::LeftButton);QApplication::processEvents();
        check(session.revision()==revision&&session.document().objects.empty(),"Returning to the current initial state is a harmless no-op");
        std::cout<<"PASS History UI: long list, explicit restore, redo branch, new session\n";return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
