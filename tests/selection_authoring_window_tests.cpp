#include "window.hpp"
#include "visual_style.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFont>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
template<class T>T* named(Window& window,const char* name){
    events();for(auto* item:window.findChildren<T*>(name))if(item->isVisible())return item;
    if(const auto output=qEnvironmentVariable("NECT_SELECTION_CAPTURE");!output.isEmpty())window.grab().save(output+".failure.png");
    throw std::runtime_error(std::string("Missing visible control: ")+name);
}
void choose(QComboBox* combo,const char* id){const auto index=combo->findData(QString::fromLatin1(id));check(index>=0,"Stable ID is offered");combo->setCurrentIndex(index);events();}
void click(Window& window,const char* name){
    auto* button=named<QPushButton>(window,name);check(button->isEnabled(),"Public action is enabled");
    auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");check(scroll!=nullptr,"Production Inspector scroll exists");
    scroll->horizontalScrollBar()->setValue(0);scroll->ensureWidgetVisible(button);events();
    const auto bounds=QRect(button->mapTo(scroll->viewport(),QPoint(0,0)),button->size());
    if(scroll->horizontalScrollBar()->value()!=0||!scroll->viewport()->rect().contains(bounds)){
        if(const auto output=qEnvironmentVariable("NECT_SELECTION_CAPTURE");!output.isEmpty())window.grab().save(output+".failure.png");
        throw std::runtime_error(std::string("Unreachable batch action: ")+name+" bounds="+std::to_string(bounds.x())+","+std::to_string(bounds.y())+","+std::to_string(bounds.width())+","+std::to_string(bounds.height())+" viewport="+std::to_string(scroll->viewport()->width())+","+std::to_string(scroll->viewport()->height())+" horizontal="+std::to_string(scroll->horizontalScrollBar()->value()));
    }
    check(true,"Complete batch action is reachable without horizontal Inspector movement");
    button->click();events();
}
void select(Window& window){window.canvas->set_selections({{"first",""},{"second",""}});events();}
Document fixture(Document document){
    document.objects.clear();auto& composition=document.compositions.front();composition.roots={"path","first","second"};
    Object path;path.id="path";path.name="Guide path";path.kind=Kind::path;
    Point begin;begin.id="begin";begin.y.literal=200;Point end;end.id="end";end.x.literal=1000;end.y.literal=200;
    path.contours={{"contour",false,{begin,end}}};document.objects.emplace(path.id,path);
    for(const auto* id:{"first","second"}){
        Object text;text.id=id;text.name=id;text.kind=Kind::text;text.text=default_text(std::string(id)+"-text",id);
        text.stack.push_back(default_operation(std::string(id)+"-fill","nect.paint.fill"));
        if(std::string(id)=="second")text.transform[5].literal=80;
        document.objects.emplace(text.id,text);
    }
    PresetDefinition preset;preset.schema_version=2;preset.id="offset-preset";preset.label="Offset 7";
    const auto operation=default_operation("source-offset","nect.shape.offset");
    PresetEntry entry;entry.type=operation.type;entry.version=operation.version;
    for(const auto& [key,value]:operation.parameters)entry.parameters[key]=value.literal;
    entry.parameters["amount"]=7;preset.entries={entry};document.preset_definitions.emplace(preset.id,preset);
    MacroDefinitionRevision graph;graph.graph_version=2;graph.input={"input","local_paths_and_paint"};
    graph.output={"output","local_paths_and_paint"};
    graph.nodes={{operation,"offset-in","offset-out"}};
    graph.edges={{{"","input"},{"source-offset","offset-in"}},{{"source-offset","offset-out"},{"","output"}}};
    graph.output_mapping={"source-offset","offset-out"};
    graph.public_parameters={{"macro.offset.amount","Amount","source-offset","amount","number","du","local_paths_and_paint"}};
    MacroDefinition macro;macro.id="window-macro";macro.label="Offset";macro.latest_revision=1;
    macro.revisions={{1,graph}};document.macro_definitions.emplace(macro.id,macro);
    return document;
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    auto font=app.font();font.setFamily("Yu Gothic UI");font.setPixelSize(13);app.setFont(font);
    try{
        QTemporaryDir scratch;check(scratch.isValid(),"Owned scratch exists");
        QSettings preferences(scratch.filePath("library.ini"),QSettings::IniFormat);
        Window window(scratch.path()+"/recovery",std::make_unique<FolderLibrary>(preferences));
        window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();events();
        auto* properties=window.findChild<QDockWidget*>("properties");check(properties!=nullptr,"Properties dock exists");
        properties->setMinimumWidth(300);properties->setMaximumWidth(300);events();
        window.host.session=Session(fixture(window.host.session.document()));window.host.edited();select(window);
        auto& session=window.host.session;const auto original=session.document();
        check(named<QWidget>(window,"preset-batch-panel")!=nullptr,"Preset batch is connected to production multi-Inspector");
        check(named<QWidget>(window,"text-path-batch-panel")!=nullptr,"Text Path batch is connected to production multi-Inspector");
        choose(named<QComboBox>(window,"preset-batch-catalog"),"offset-preset");
        click(window,"preset-batch-apply");
        check(session.revision()==1,"Preset batch makes one revision");
        const auto& first=session.document().objects.at("first").stack;
        const auto& second=session.document().objects.at("second").stack;
        check(first.size()==2&&second.size()==2,"Both selected Text stacks receive the Preset");
        check(first.back().id!=second.back().id,"Applied operation IDs are distinct");
        check(first.back().parameters.at("amount").literal==7&&second.back().parameters.at("amount").literal==7,"Both targets receive the exact saved value");
        session.undo(session.revision());window.host.edited();check(session.document()==original,"One Undo restores both Text stacks");
        session.redo(session.revision());window.host.edited();check(session.document().objects.at("second").stack.size()==2,"One Redo restores both");
        session.undo(session.revision());window.host.edited();select(window);
        choose(named<QComboBox>(window,"text-path-batch-path"),"path");
        choose(named<QComboBox>(window,"text-path-batch-contour"),"contour");
        named<QLineEdit>(window,"text-path-batch-start")->setText("50");
        named<QLineEdit>(window,"text-path-batch-spacing")->setText("2");events();
        const auto before_attach=session.document();const auto revision=session.revision();
        click(window,"text-path-batch-apply");
        check(session.revision()==revision+1,"Path attachment batch makes one revision");
        for(const auto* id:{"first","second"}){
            const auto& text=*session.document().objects.at(id).text;
            check(text.path_attachment&&text.path_attachment->path=="path"&&text.path_attachment->contour=="contour"&&text.path_attachment->start==50&&text.path_attachment->spacing==2,"Both editable Text objects retain exact attachment identity and values");
            check(text.content==before_attach.objects.at(id).text->content,"Attachment preserves each authored string");
        }
        const auto attached=session.document();check(decode(encode(attached))==attached,"Batch result survives native serialization");
        session.undo(session.revision());window.host.edited();check(session.document()==before_attach,"One Undo restores all attachments");
        session.redo(session.revision());window.host.edited();check(session.document()==attached,"One Redo restores all attachments");select(window);
        click(window,"text-path-batch-detach");check(session.document()==before_attach,"Public Detach preserves other Text source fields");
        session.undo(session.revision());window.host.edited();check(session.document()==attached,"Detach has one exact Undo");select(window);
        choose(named<QComboBox>(window,"macro-batch-catalog"),"window-macro");
        check(named<QComboBox>(window,"macro-batch-revision")->currentData().toULongLong()==1,"Window retains the explicit Macro revision pin");
        const auto macro_revision=session.revision();click(window,"macro-batch-apply");
        check(session.revision()==macro_revision+1,"Macro batch makes one revision");
        for(const auto* id:{"first","second"}){
            const auto& stack=session.document().objects.at(id).stack;
            check(stack.size()==2&&stack.back().macro&&stack.back().macro->definition=="window-macro"&&stack.back().macro->pinned_revision==1,"Both selected Text objects receive the pinned Macro");
        }
        const auto with_macro=session.document();check(decode(encode(with_macro))==with_macro,"Pinned batch Macro survives native serialization");
        session.undo(session.revision());window.host.edited();check(session.document()==attached,"One Undo restores both Macro targets exactly");
        session.redo(session.revision());window.host.edited();check(session.document()==with_macro,"One Redo restores both Macro instances");select(window);
        auto* scroll=window.findChild<QScrollArea*>("inspector-scroll");scroll->verticalScrollBar()->setValue(0);events();
        if(const auto output=qEnvironmentVariable("NECT_SELECTION_DOCUMENT");!output.isEmpty()){
            const auto bytes=encode(session.document());QFile file(output);
            check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&file.write(bytes.data(),static_cast<qint64>(bytes.size()))==static_cast<qint64>(bytes.size()),"Owned candidate document is saved");
        }
        if(const auto output=qEnvironmentVariable("NECT_SELECTION_CAPTURE");!output.isEmpty())check(window.grab().save(output),"Owned offscreen preview is saved");
        std::cout<<"PASS production selection authoring Window ("<<checks<<" checks)\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
