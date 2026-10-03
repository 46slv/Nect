#pragma once
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <memory>
#include <stdexcept>

// Bounded production Window qualification through Qt/API, without OS input.
namespace instance_text_window_smoke {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void require(bool value,const char* reason){
    if(!value)throw std::runtime_error(reason);++checks;
}
inline void events(){
    QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
}
template<class T>T* named(QObject& parent,const char* name){
    auto* control=parent.findChild<T*>(QString::fromLatin1(name));
    require(control!=nullptr,"Production Text control exists");return control;
}
inline QByteArray bytes(const QString& path){
    QFile file(path);require(file.open(QIODevice::ReadOnly),"Native file is readable");return file.readAll();
}
inline Document fixture(){
    auto document=empty_document("window-text-doc","composition","source-board");
    document.compositions.front().artboards.front().width=640;
    document.compositions.front().artboards.front().height=480;
    document.compositions.front().artboards.push_back({"target-board","Target",700,0,640,480});
    Object root;root.id="source-root";root.kind=Kind::group;root.children={"driver","title"};
    Object driver;driver.id="driver";driver.name="Content source";driver.kind=Kind::text;
    driver.text=default_text("driver-text","Source\r\nSecond line");driver.visible=false;
    Object title;title.id="title";title.name="Title";title.kind=Kind::text;
    title.text=default_text("title-text","Stored literal");
    title.text->content_driver=TextContentDriver{{"driver","","text.content"}};
    title.stack.push_back(default_operation("title-fill","nect.paint.fill"));
    document.objects.emplace(root.id,root);document.objects.emplace(driver.id,driver);document.objects.emplace(title.id,title);
    document.compositions.front().roots={root.id};Session session(document);
    session.apply({DefinitionCommand{CreateDefinition{{"definition","Shared",root.id}}},
        ArtboardTemplateCommand{CreateArtboardTemplate{"composition",{"template","Shared","source-board",Id{"definition"}}}},
        ArtboardTemplateCommand{AssignArtboardTemplate{"composition","target-board","template",Id{"content-instance"}}},
        DefinitionCommand{CreateInstance{"composition","","plain-instance","definition","Plain Instance"}},
        Set{{"plain-instance","","transform.ty"},140}},0);
    return session.document();
}
inline void click(Window& window,const char* name){
    auto* button=named<QPushButton>(window,name);require(button->isEnabled(),"Production Text action is enabled");
    button->click();events();
}
inline void save_as(Window& window,const QString& path){
    QAction* action=nullptr;
    for(auto* candidate:window.findChildren<QAction*>())if(candidate->text().startsWith("Save As")){action=candidate;break;}
    require(action!=nullptr,"Production Save As action exists");
    bool seen=false,timed_out=false;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&window,[&]{
        if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){
            seen=true;dialog->selectFile(path);static_cast<QDialog*>(dialog)->accept();
        }});
    QObject::connect(&deadline,&QTimer::timeout,&window,[&]{timed_out=true;
        if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
    poll.start();deadline.start(3000);action->trigger();poll.stop();deadline.stop();events();window.host.flush();
    require(seen&&!timed_out&&window.host.file_path==path,"Production Save As selects the owned fresh destination");
}
inline QJsonObject geometry(QWidget* panel){
    auto* editor=panel->findChild<QPlainTextEdit*>("instance-text-content-editor");
    auto* apply=panel->findChild<QPushButton*>("instance-text-content-apply");
    auto* inspector=panel->parentWidget();
    return {{"panel_height",panel->height()},{"panel_minimum_hint",panel->minimumSizeHint().height()},
        {"editor_height",editor->height()},{"editor_minimum_hint",editor->minimumSizeHint().height()},
        {"apply_height",apply->height()},{"apply_minimum_hint",apply->minimumSizeHint().height()},
        {"inspector_minimum_height",inspector->minimumHeight()},
        {"inspector_layout_minimum_height",inspector->layout()->minimumSize().height()}};
}
inline void run(const QString& directory){
    require(QFileInfo(directory).isAbsolute()&&QDir().mkpath(directory),"Owned absolute smoke output exists");
    const auto destination=QDir(directory).filePath("Nect Native085 Window Text.nect");
    require(!QFileInfo::exists(destination),"Smoke native destination is fresh");
    QTemporaryDir scratch;require(scratch.isValid(),"Isolated Window recovery exists");
    QSettings settings(scratch.path()+"/library.ini",QSettings::IniFormat);
    Window window(scratch.path()+"/recovery",std::make_unique<FolderLibrary>(settings));window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1400,900);window.show();events();
    window.host.session=Session(fixture());window.host.session_id+="-text-smoke";window.host.edited();events();
    window.canvas->set_active_artboard("composition","source-board",false);
    window.canvas->set_selection("plain-instance");events();
    auto* panel=named<QWidget>(window,"instance-text-content-controls");
    require(panel->isVisible()&&panel->property("nect-instance-id").toString()=="plain-instance","Ordinary Instance exposes production Text panel");
    auto* picker=named<QComboBox>(window,"instance-text-content-source");picker->setCurrentIndex(picker->findData("title"));events();
    require(picker->currentData().toString()=="title","Production picker selects stable descendant Text ID");
    const auto before=window.host.session.document();const auto revision=window.host.session.revision();
    QPointer<QWidget> retired=panel;named<QPlainTextEdit>(window,"instance-text-content-editor")->setPlainText("Local Window Text\nSecond line");
    click(window,"instance-text-content-apply");
    auto expected=before;expected.objects.at("plain-instance").instance->text_content_overrides["title"]="Local Window Text\nSecond line";
    require(!retired&&window.host.session.document()==expected&&window.host.session.revision()==revision+1,"Apply rebuilds Inspector and edits only the selected occurrence in one step");
    require(named<QComboBox>(window,"instance-text-content-source")->currentData().toString()=="title","Rebuilt production Inspector retains source ID");
    require(named<QLabel>(window,"instance-text-content-local-state")->text()=="Local: Override","Rebuilt production Inspector shows local Text");
    const auto instance_geometry=geometry(named<QWidget>(window,"instance-text-content-controls"));
    require(window.grab().save(QDir(directory).filePath("instance-text-panel.png")),"Production Instance screenshot saved");
    click(window,"instance-text-content-reset");
    require(window.host.session.document()==before,"Production Use Source resets selected local Text exactly");
    QAction* undo=nullptr;for(auto* action:window.findChildren<QAction*>())if(action->text()=="Undo"){undo=action;break;}
    require(undo&&undo->isEnabled(),"Production Undo action is enabled");undo->trigger();events();
    require(window.host.session.document()==expected,"One production Undo restores the exact local Text override");
    auto* boards=named<QListWidget>(window,"artboards");bool selected=false;
    for(int row=0;row<boards->count();++row){auto* item=boards->item(row);
        if(item->data(Qt::UserRole+1).toString()=="target-board"){boards->setCurrentItem(item);selected=true;break;}}
    events();require(selected&&window.canvas->active_artboard()=="target-board","Production navigation selects Template frame");
    panel=named<QWidget>(window,"instance-text-content-controls");
    require(panel->isVisible()&&panel->property("nect-instance-id").toString()=="content-instance","Template frame exposes production content Text panel");
    require(window.host.session.document().objects.at("content-instance").instance->text_content_overrides.empty(),"Template content remains independent and inherited");
    const auto template_geometry=geometry(panel);
    require(window.grab().save(QDir(directory).filePath("template-text-panel.png")),"Production Template screenshot saved");
    const auto authored=window.host.session.document();const auto encoded=encode(authored);save_as(window,destination);
    require(bytes(destination)==QByteArray::fromStdString(encoded),"Production Save As writes exact authored native085 bytes");
    require(QJsonDocument::fromJson(bytes(destination)).object().value("version").toString()=="0.85","Production native writer emits version 0.85");
    QSettings cold_settings(scratch.path()+"/cold-library.ini",QSettings::IniFormat);
    Window cold(scratch.path()+"/cold-recovery",std::make_unique<FolderLibrary>(cold_settings));cold.setAttribute(Qt::WA_DontShowOnScreen);cold.show();cold.host.open(destination);events();
    require(cold.host.session.document()==authored&&encode(cold.host.session.document())==encoded,"Fresh production Window reopens exact Text overrides and source drivers");
    QJsonObject receipt{{"status","PASS"},{"checks",checks},{"kind","production Window Qt/API"},{"physical_os_input",false},{"qt_platform",QApplication::platformName()},{"saved_path",destination},{"native_version","0.85"}};
    receipt["instance_geometry"]=instance_geometry;receipt["template_geometry"]=template_geometry;
    const bool clipped=instance_geometry.value("panel_height").toInt()<instance_geometry.value("panel_minimum_hint").toInt()||
        template_geometry.value("panel_height").toInt()<template_geometry.value("panel_minimum_hint").toInt();
    receipt["visual_layout"]=clipped?"CLIPPED_ROOT_WINDOW_RESIDUAL":"PASS";
    QFile output(QDir(directory).filePath("window-smoke.json"));require(output.open(QIODevice::WriteOnly),"Window receipt is writable");output.write(QJsonDocument(receipt).toJson());
    std::cout<<QJsonDocument(receipt).toJson(QJsonDocument::Compact).toStdString()<<'\n';
    window.host.changed={};cold.host.changed={};window.hide();cold.hide();
}
}
