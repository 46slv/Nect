#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <cmath>

using namespace nect;
using namespace nect::desktop;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
QAction* action(Window& window,const char* name) {
    auto* found=window.findChild<QAction*>(QString::fromLatin1(name));
    check(found!=nullptr,"Definition menu action exists");return found;
}
void accept_text_dialog(QString text) {
    QTimer::singleShot(0,[text=std::move(text)] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            dialog->setTextValue(text);dialog->accept();return;
        }
        throw std::runtime_error("Expected Definition text dialog");
    });
}
void accept_choice_dialog() {
    QTimer::singleShot(0,[] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            if(auto* choices=dialog->findChild<QComboBox*>();choices&&choices->count())choices->setCurrentIndex(0);
            dialog->accept();return;
        }
        throw std::runtime_error("Expected Definition choice dialog");
    });
}
void accept_choice_then_text(QString text) {
    QTimer::singleShot(0,[text=std::move(text)] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
            if(auto* choices=dialog->findChild<QComboBox*>();choices&&choices->count())choices->setCurrentIndex(0);
            dialog->accept();
            QTimer::singleShot(0,[text] {
                for(auto* next:QApplication::topLevelWidgets())if(auto* input=qobject_cast<QInputDialog*>(next)) {
                    input->setTextValue(text);input->accept();return;
                }
                throw std::runtime_error("Expected Definition rename text dialog");
            });
            return;
        }
        throw std::runtime_error("Expected Definition rename choice dialog");
    });
}
void set_override_dialog(double value) {
    QTimer::singleShot(0,[value] {
        for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QDialog*>(widget);
            dialog&&dialog->objectName()=="set-instance-override-dialog") {
            auto* source=dialog->findChild<QComboBox*>("set-instance-override-source");
            auto* property=dialog->findChild<QComboBox*>("set-instance-override-property");
            auto* scalar=dialog->findChild<QDoubleSpinBox*>("set-instance-override-value");
            auto* buttons=dialog->findChild<QDialogButtonBox*>();
            if(!source||!property||!scalar||!buttons)throw std::runtime_error("Incomplete Set Override dialog");
            const auto source_index=source->findData("path-a");
            if(source_index<0)throw std::runtime_error("Stable source Path missing from override picker");
            source->setCurrentIndex(source_index);
            const auto property_index=property->findData("composite.opacity");
            if(property_index<0)throw std::runtime_error("Supported Scalar missing from override picker");
            property->setCurrentIndex(property_index);scalar->setValue(value);
            buttons->button(QDialogButtonBox::Apply)->click();return;
        }
        throw std::runtime_error("Expected Set Override dialog");
    });
}
Object render_rectangle(const Id& id) {
    Object path;path.id=id;path.name=id;path.kind=Kind::path;
    Contour contour;contour.id=id+"-contour";contour.closed=true;
    int point_number=0;
    for(const auto& position:std::vector<Vec2>{{2,2},{18,2},{18,18},{2,18}}) {
        Point point;point.id=id+"-point-"+std::to_string(point_number++);
        point.x.literal=position.x;point.y.literal=position.y;contour.points.push_back(point);
    }
    path.contours.push_back(std::move(contour));
    auto fill=default_operation(id+"-fill","nect.paint.fill");
    fill.parameters.at("r").literal=1;fill.parameters.at("g").literal=0;
    fill.parameters.at("b").literal=0;fill.parameters.at("a").literal=1;
    path.stack.push_back(std::move(fill));path.compositing.opacity.literal=0.8;return path;
}
void canvas_render_contract() {
    auto document=empty_document("canvas-definition-doc","canvas-definition-comp","canvas-definition-board");
    Object outer;outer.id="outer";outer.name="Outer";outer.kind=Kind::group;outer.transform[4].literal=320;outer.transform[5].literal=220;
    Object root;root.id="source-root";root.name="D";root.kind=Kind::group;root.children={"render-path"};
    root.transform[0].literal=2;root.transform[3].literal=2;root.transform[4].literal=400;root.transform[5].literal=300;
    root.transform_parent="outer";root.visible=false;root.compositing.opacity.literal=0.5;
    const auto path=render_rectangle("render-path");
    document.objects.emplace(outer.id,outer);document.objects.emplace(root.id,root);document.objects.emplace(path.id,path);
    document.compositions.front().roots={outer.id,root.id};
    Session session(document);
    session.apply({DefinitionCommand{CreateDefinition{{"canvas-definition","D","source-root"}}},
        DefinitionCommand{CreateInstance{"canvas-definition-comp","","canvas-instance-1","canvas-definition","I1"}},
        Set{{"canvas-instance-1","","transform.tx"},20},Set{{"canvas-instance-1","","transform.ty"},20},
        DefinitionCommand{SetInstanceOverride{"canvas-instance-1",{"render-path","","composite.opacity"},0.2}},
        DefinitionCommand{CreateInstance{"canvas-definition-comp","","canvas-instance-2","canvas-definition","I2"}},
        Set{{"canvas-instance-2","","transform.tx"},80},Set{{"canvas-instance-2","","transform.ty"},20}},0);
    const auto image=Canvas::render_artboard(session.document(),"canvas-definition-comp","canvas-definition-board",1,false);
    const auto first_alpha=image.pixelColor(30,30).alpha(),second_alpha=image.pixelColor(90,30).alpha();
    check(std::abs(first_alpha-26)<=1&&std::abs(second_alpha-102)<=2,
        "Canvas renders both placed proxies with unique geometry, local override and retained source-root opacity");
    check(image.pixelColor(500,400).alpha()==0,
        "Canvas excludes the hidden master's source-root transform and its external Transform Parent");
}
}
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try {
        QTemporaryDir recovery;Window window(recovery.path());window.show();QApplication::processEvents();
        auto& session=window.host.session;const auto composition=session.document().compositions.front().id;
        Point point;point.id="path-point";point.x.literal=70;point.y.literal=80;
        session.apply({CreatePath{composition,"","path-a","Logo Path",{{"path-contour",false,{point}}}},
            GroupContiguous{composition,"",{"path-a"},"source-root","Logo"}},session.revision());
        window.host.edited();window.canvas->set_selection("source-root");
        accept_text_dialog("Logo Definition");action(window,"create-definition-from-selection")->trigger();QApplication::processEvents();
        check(session.document().definitions.size()==1&&session.document().definitions.begin()->second.root=="source-root"&&
            session.document().objects.contains("source-root"),"Desktop creates a named Definition while retaining its source Group");
        const auto definition_id=session.document().definitions.begin()->first;
        accept_choice_then_text("Logo Renamed");action(window,"rename-definition")->trigger();QApplication::processEvents();
        check(session.document().definitions.contains(definition_id)&&session.document().definitions.at(definition_id).name=="Logo Renamed",
            "Desktop renames Definition metadata without changing its stable ID");

        accept_choice_dialog();action(window,"place-definition-instance")->trigger();QApplication::processEvents();
        check(window.canvas->selected_objects().size()==1,"Desktop selects the newly placed Instance");
        const auto instance=window.canvas->selected_objects().front();
        check(session.document().objects.at(instance).kind==Kind::instance&&
            session.document().objects.at(instance).instance->definition==definition_id,
            "Desktop places a stable Instance through the shared Session command");

        set_override_dialog(0.4);action(window,"set-instance-override")->trigger();QApplication::processEvents();
        check(session.document().objects.at(instance).instance->overrides.at({"path-a","","composite.opacity"})==0.4,
            "Desktop sets a scalar override through the source Ref picker");
        accept_choice_dialog();action(window,"reset-instance-override")->trigger();QApplication::processEvents();
        check(session.document().objects.at(instance).instance->overrides.empty(),
            "Desktop resets the selected local override by its stable source Ref");

        session.apply({DefinitionCommand{SetInstanceOverride{instance,{"path-a","","composite.opacity"},0.4}}},session.revision());
        window.host.edited();window.canvas->set_selection(instance);
        action(window,"detach-instance")->trigger();QApplication::processEvents();
        check(session.document().objects.at(instance).kind==Kind::group&&!session.document().objects.at(instance).instance,
            "Desktop detach materializes an independent Group at the placed Instance ID");
        session.undo(session.revision());window.host.edited();
        check(session.document().objects.at(instance).kind==Kind::instance&&
            session.document().objects.at(instance).instance->overrides.size()==1,
            "Desktop detach is one Undoable command restoring the live Definition Instance");
        canvas_render_contract();
        std::cout<<"PASS Desktop Definition and Instance command affordances\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
