#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <iostream>

using namespace nect;
using namespace nect::desktop;

namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void pump() {QApplication::processEvents();}
QByteArray ref_json(const Ref& ref) {
    return QJsonDocument(QJsonObject{{"object",QString::fromStdString(ref.object)},
        {"point",QString::fromStdString(ref.point)},{"field",QString::fromStdString(ref.field)}}).toJson(QJsonDocument::Compact);
}
QAction* action(Window& window,const char* name) {
    auto* result=window.findChild<QAction*>(QString::fromLatin1(name));check(result!=nullptr,"Visible acceptance action missing");return result;
}
template<class T>T* visible(Window& window,const QString& name) {
    for(auto* widget:window.findChildren<T*>(name))if(widget->isVisible())return widget;
    throw std::runtime_error(("Visible acceptance widget missing: "+name).toStdString());
}
QLineEdit* property_field(Window& window,const Ref& ref) {
    const auto expected=ref_json(ref);
    for(auto* field:window.findChildren<QLineEdit*>())if(field->isVisible()&&field->property("nect-reference").toByteArray()==expected)return field;
    throw std::runtime_error("Visible acceptance property field missing");
}
void select(Window& window,const Id& object) {window.canvas->set_selection(object);window.host.edited();pump();}
void choose(Window& window,const Id& operation,const char* value) {
    auto* combo=visible<QComboBox>(window,"stroke-line-cap-"+QString::fromStdString(operation));
    const auto index=combo->findData(QString::fromLatin1(value));check(index>=0,"Visible acceptance cap option missing");combo->setCurrentIndex(index);pump();
}
void history(Window& window,const char* label) {
    for(auto* candidate:window.findChildren<QAction*>())if(candidate->text()==QString::fromLatin1(label)) {check(candidate->isEnabled(),"Visible acceptance history action disabled");candidate->trigger();pump();return;}
    throw std::runtime_error("Visible acceptance history action missing");
}
void edit_number(Window& window,const Ref& ref,const char* text) {
    auto* input=property_field(window,ref);input->setFocus();input->selectAll();QTest::keyClicks(input,text);QTest::keyClick(input,Qt::Key_Return);pump();
}
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        check(QGuiApplication::platformName()=="windows","Visible acceptance requires the Windows Qt platform");
        check(argc>=2,"Visible acceptance requires a task-owned evidence directory");
        const QString evidence=QDir::cleanPath(QDir::fromNativeSeparators(QString::fromLocal8Bit(argv[1])));check(QDir().mkpath(evidence),"Create task-owned visible evidence directory");
        const QString scratch=evidence+"/scratch";check(QDir().mkpath(scratch),"Create task-owned visible scratch");
        Window window(scratch+"/recovery");window.setWindowTitle("Nect Stroke CP2 r4 T7 acceptance");window.resize(1440,900);window.show();window.raise();window.activateWindow();pump();
        check(window.isVisible()&&window.canvas->isVisible(),"Production Window/Canvas is visible");
        const auto composition=window.host.session.document().compositions.front().id;const auto artboard=window.host.session.document().compositions.front().artboards.front().id;
        Point a,b;a.id="t7-a";a.x.literal=100;a.y.literal=100;b.id="t7-b";b.x.literal=200;b.y.literal=100;
        window.host.session.apply({CreatePath{composition,"","t7-line","T7 line",{{"t7-contour",false,{a,b}}}}},0);window.host.edited();select(window,"t7-line");
        action(window,"add-stroke")->trigger();pump();const auto stroke=window.host.session.document().objects.at("t7-line").stack.back().id;
        edit_number(window,operation_ref("t7-line",stroke,"width"),"20");choose(window,stroke,"round");
        const auto styled=window.host.session.document();const auto source=styled.objects.at("t7-line").contours;check(styled.objects.at("t7-line").stack.back().version==2&&styled.objects.at("t7-line").stack.back().line_cap=="round","GUI Stroke style commits in the visible Window");
        history(window,"Undo");check(window.host.session.document().objects.at("t7-line").stack.back().version==1,"Visible GUI Undo restores v1");history(window,"Redo");check(window.host.session.document()==styled,"Visible GUI Redo restores the exact style");
        check(window.host.session.document().objects.at("t7-line").contours==source,"Undo/Redo never rewrites source geometry");
        const QString native=scratch+"/stroke.nect";window.host.save(native);window.host.recover();const QString recovery=window.host.persistence().value("recovery_file").toString();check(QFile::exists(native)&&QFile::exists(recovery),"Native and recovery artifacts exist");
        Window reopened(scratch+"/reopened");reopened.show();pump();const auto old_session=window.host.session_id;reopened.host.open(native);pump();check(reopened.isVisible()&&reopened.host.session.document()==styled&&reopened.host.session_id!=old_session,"Fresh visible Window native reopen preserves style and identity");
        Window recovered(scratch+"/recovered");recovered.show();pump();recovered.host.open_recovery(recovery);pump();check(recovered.isVisible()&&recovered.host.session.document()==styled&&recovered.host.file_path.isEmpty(),"Fresh visible Window recovery reopen preserves style and detaches source");
        select(recovered,"t7-line");const QString png=evidence+"/stroke-visible.png";recovered.host.export_png(png,composition,artboard,1,false,recovered.host.session.revision());
        const QImage image(png);check(!image.isNull()&&image.pixelColor(95,100).alpha()>0,"Round-cap PNG pixel is present");check(image.pixelColor(91,91).alpha()==0,"Round-cap PNG corner pixel excludes square geometry");
        const auto svg=export_svg(recovered.host.session.document(),composition,artboard);const QString svg_path=evidence+"/stroke-visible.svg";QFile svg_file(svg_path);check(svg_file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Write visible SVG artifact");svg_file.write(QByteArray::fromStdString(svg));svg_file.close();
        check(svg.find("stroke-linecap=\"round\"")!=std::string::npos&&svg.find("selection")==std::string::npos&&svg.find("overlay")==std::string::npos,"SVG export contains style but no selection overlay");
        const QString screenshot=evidence+"/visible-window.png";check(window.grab().save(screenshot),"Capture visible production Window evidence");
        const QJsonObject receipt_json{{"status","PASS"},{"platform",QGuiApplication::platformName()},{"window_visible",window.isVisible()},{"canvas_visible",window.canvas->isVisible()},
            {"scratch_directory",scratch},{"native",native},{"recovery",recovery},{"png",png},{"svg",svg_path},{"screenshot",screenshot},
            {"stroke_operation",QString::fromStdString(stroke)},{"cap","round"},{"join","miter"},{"version",2},{"miter_limit",4},
            {"undo_redo",true},{"native_reopen",true},{"recovery_reopen",true},{"source_contours_unchanged",true},
            {"png_round_alpha_95_100",image.pixelColor(95,100).alpha()},{"png_round_alpha_91_91",image.pixelColor(91,91).alpha()},
            {"selection_overlay_excluded",true},{"svg_style_exported",true}};
        const QString receipt_path=evidence+"/stroke-visible-t7.json";QFile receipt_file(receipt_path);check(receipt_file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Write visible T7 receipt");receipt_file.write(QJsonDocument(receipt_json).toJson(QJsonDocument::Indented));receipt_file.close();
        window.close();reopened.close();recovered.close();pump();
        std::cout<<receipt_path.toStdString()<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
