#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
QAction* action(Window& window,const char* name) {
    auto* found=window.findChild<QAction*>(QString::fromLatin1(name));
    check(found!=nullptr,"Collection menu action exists");return found;
}
void text_dialog(QString value) {
    QTimer::singleShot(0,[value=std::move(value)] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            dialog->setTextValue(value);dialog->accept();return;
        }
        throw std::runtime_error("Expected Collection name dialog");
    });
}
void choice_dialog() {
    QTimer::singleShot(0,[] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            if(auto* choices=dialog->findChild<QComboBox*>();choices&&choices->count())choices->setCurrentIndex(0);
            dialog->accept();return;
        }
        throw std::runtime_error("Expected Collection choice dialog");
    });
}
void choice_then_text(QString value) {
    QTimer::singleShot(0,[value=std::move(value)] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            dialog->accept();text_dialog(value);return;
        }
        throw std::runtime_error("Expected Collection rename choice dialog");
    });
}
void browse_dialog() {
    QTimer::singleShot(0,[] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            dialog->accept();
            QTimer::singleShot(0,[] {
                for(auto* top:QApplication::topLevelWidgets())if(auto* members=qobject_cast<QMessageBox*>(top)) {
                    check(members->text().contains("B")&&members->text().contains("A"),
                        "Collection browser lists stable member IDs");
                    members->accept();return;
                }
                throw std::runtime_error("Expected Collection members dialog");
            });
            return;
        }
        throw std::runtime_error("Expected Collection browser picker");
    });
}
Contour rectangle(const Id& id,double x) {
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    for(const auto& xy:std::vector<Vec2>{{x,10},{x+20,10},{x+20,30},{x,30}}) {
        Point point;point.id=id+"-point-"+std::to_string(contour.points.size());
        point.x.literal=xy.x;point.y.literal=xy.y;contour.points.push_back(point);
    }
    return contour;
}
void render_same(const QImage& before,const Window& window,const Id& composition,const Id& artboard) {
    const auto after=Canvas::render_artboard(window.host.session.document(),composition,artboard,1,false);
    check(before==after,"Collection-only Desktop command preserves every Canvas pixel");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir recovery;Window window(recovery.path());window.show();QApplication::processEvents();
        auto& session=window.host.session;const auto composition=session.document().compositions.front().id;
        const auto artboard=session.document().compositions.front().artboards.front().id;
        session.apply({CreatePath{composition,"","A","A",{rectangle("A",10)}},
            CreatePath{composition,"","B","B",{rectangle("B",50)}}},session.revision());
        window.host.edited();window.canvas->set_selections({{"A",{}},{"B",{}}});
        const auto baseline=Canvas::render_artboard(session.document(),composition,artboard,1,false);
        bool painted=false;
        for(int y=0;y<baseline.height()&&!painted;++y)for(int x=0;x<baseline.width();++x)
            if(baseline.pixelColor(x,y).alpha()>0){painted=true;break;}
        check(painted,"Collection pixel oracle starts with visible artwork");
        text_dialog("K");action(window,"create-collection-from-selection")->trigger();QApplication::processEvents();
        check(session.document().collections.size()==1&&
            session.document().collections.front().members==std::vector<Id>({"A","B"}),
            "Desktop creates a non-owning Collection from exact selection");
        render_same(baseline,window,composition,artboard);
        const auto collection_id=session.document().collections.front().id;
        window.canvas->set_selection("A");choice_dialog();action(window,"remove-selection-from-collection")->trigger();
        QApplication::processEvents();
        check(session.document().collections.front().members==std::vector<Id>({"B"}),
            "Desktop removes only selected member through Session");
        render_same(baseline,window,composition,artboard);
        choice_dialog();action(window,"add-selection-to-collection")->trigger();QApplication::processEvents();
        check(session.document().collections.front().members==std::vector<Id>({"B","A"}),
            "Desktop adds selected member in deterministic order");
        render_same(baseline,window,composition,artboard);
        choice_then_text("Renamed K");action(window,"rename-collection")->trigger();QApplication::processEvents();
        check(session.document().collections.front().id==collection_id&&
            session.document().collections.front().name=="Renamed K","Desktop rename preserves Collection ID");
        browse_dialog();action(window,"browse-collections")->trigger();QApplication::processEvents();
        check(window.canvas->selected_objects()==std::vector<Id>({"A"}),
            "Browsing members does not change the structural Canvas selection");
        choice_dialog();action(window,"delete-collection")->trigger();QApplication::processEvents();
        check(session.document().collections.empty()&&session.document().objects.contains("A")&&
            session.document().objects.contains("B"),"Desktop deletes Collection without deleting artwork");
        render_same(baseline,window,composition,artboard);
        std::cout<<"PASS Desktop Collection commands and Canvas pixel equality\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
