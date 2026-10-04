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
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
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
struct ContextSnapshot {
    Document document;std::string native;std::uint64_t revision,generation;HistoryInfo history;bool gesture;
    explicit ContextSnapshot(const Session& s):document(s.document()),native(encode(document)),revision(s.revision()),
        generation(s.gesture_generation()),history(s.history()),gesture(s.gesture_active()){}
    bool unchanged(const Session& s)const{return document==s.document()&&document==s.preview_document()&&
        native==encode(s.document())&&revision==s.revision()&&history==s.history()&&
        generation==s.gesture_generation()&&gesture==s.gesture_active();}
};
void text_weight_context(){
    QTemporaryDir scratch;check(scratch.isValid(),"Weight probe owns disposable scratch");
    QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();events();
    auto& session=window.host.session;auto document=fixture(session.document());
    document.objects.at("first").text->weight=400;
    session=Session(document);window.host.edited();window.canvas->set_selection("first");events();
    QApplication::setActiveWindow(&window);events();
    const auto draft=[&](const char* value){
        auto* weight=named<QSpinBox>(window,"text-weight");
        auto* scroll=named<QScrollArea>(window,"inspector-scroll");scroll->ensureWidgetVisible(weight);events();
        weight->setFocus();events();weight->selectAll();QTest::keyClicks(weight,value);events();
        check(weight->hasFocus()&&weight->text()==value,"Real focused Weight receives unfinished keyboard draft");
        return weight;
    };
    const ContextSnapshot initial(session);auto* old=draft("700");
    check(initial.unchanged(session),"Typing Weight leaves complete authored source/history unchanged");
    auto external=*session.document().objects.at("first").text;external.weight=500;
    session.apply({UpdateText{"first",external}},session.revision());
    const ContextSnapshot incoming(session);const auto session_id=window.host.session_id;
    check(old->hasFocus()&&old->text()=="700"&&session.document().objects.at("first").text->weight==500,
        "External canonical update precedes Inspector refresh with old draft still focused");
    window.host.edited();events();
    std::cout<<"external Weight refresh weight="<<session.document().objects.at("first").text->weight
        <<" revision="<<session.revision()<<" expected="<<incoming.revision<<'\n';
    check(incoming.unchanged(session)&&window.host.session_id==session_id,
        "Inspector refresh preserves complete external source without stale Weight commit or history");
    check(named<QSpinBox>(window,"text-weight")->value()==500,"Replacement Inspector shows canonical external Weight");
    auto* normal=draft("600");check(incoming.unchanged(session),"Normal Weight remains draft until Return");
    Session oracle(incoming.document);auto expected=external;expected.weight=600;
    oracle.apply({UpdateText{"first",expected}},oracle.revision());
    QTest::keyClick(normal,Qt::Key_Return);events();
    check(session.document()==oracle.document()&&session.preview_document()==oracle.document()&&
        session.revision()==incoming.revision+1&&session.history().states.size()==incoming.history.states.size()+1,
        "Return commits one complete canonical UpdateText transaction");
    const ContextSnapshot committed(session);auto* unchanged=draft("600");
    QTest::keyClick(unchanged,Qt::Key_Return);events();
    check(committed.unchanged(session),"Unchanged Weight Return adds no history or revision");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==incoming.document&&encode(session.document())==incoming.native,
        "Undo normal edit restores complete externally updated source");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==initial.document&&encode(session.document())==initial.native,
        "Undo external edit restores complete original source");
    session.redo(session.revision());session.redo(session.revision());window.host.edited();events();
    check(session.document()==committed.document&&encode(session.document())==committed.native,
        "Redo restores both complete canonical transactions");
    const auto saved=scratch.filePath("weight.nect.json");window.host.save(saved);events();
    const ContextSnapshot before_open(session);window.host.open(saved);events();
    check(session.document()==before_open.document&&session.preview_document()==before_open.document&&
        encode(session.document())==before_open.native&&session.revision()==0&&session.history().states.size()==1,
        "Same Window native reopen preserves complete Weight and unrelated source");
}
void object_name_context(){
    QTemporaryDir scratch;check(scratch.isValid(),"Name probe owns disposable scratch");
    QSettings preferences(scratch.filePath("settings.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences),&preferences);
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();events();
    auto& session=window.host.session;session=Session(fixture(session.document()));window.host.edited();events();
    window.canvas->set_selection("first");events();
    const auto name_editor=[&]()->QLineEdit*{
        for(auto* field:window.findChildren<QLineEdit*>())
            if(field->isVisible()&&field->accessibleName()=="Object name")return field;
        throw std::runtime_error("Missing visible Object name field");
    };
    auto replacement=session.document();replacement.objects.at("first").name="Loaded name";
    const auto incoming=scratch.filePath("incoming.nect.json");
    QFile file(incoming);check(file.open(QIODevice::WriteOnly),"Owned replacement native opens");
    const auto bytes=QByteArray::fromStdString(encode(replacement));
    check(file.write(bytes)==bytes.size(),"Complete replacement native saved");file.close();
    QApplication::setActiveWindow(&window);events();
    auto* name=name_editor();name->setFocus();events();name->selectAll();
    QTest::keyClicks(name,"Outgoing name draft");events();
    std::cout<<"name draft focus="<<name->hasFocus()<<" modified="<<name->isModified()<<'\n';
    check(name->hasFocus()&&name->isModified(),"Actual focused name field has an uncommitted draft");
    const auto old_identity=window.host.session_id;
    check(session.document().objects.at("first").name!="Outgoing name draft","Typing has not renamed source");
    window.host.open(incoming);events();
    std::cout<<"native-open name="<<session.document().objects.at("first").name
        <<" revision="<<session.revision()<<'\n';
    check(window.host.session_id!=old_identity,"Native open establishes another Session");
    check(session.document()==replacement&&session.revision()==0&&session.history().states.size()==1,
        "Native open never applies an outgoing Object-name draft to the incoming same-ID object");
    check(encode(session.document())==bytes.toStdString(),"Loaded complete native source stays exact");
    const ContextSnapshot loaded(session);const auto selection=window.canvas->selections();
    const auto type_name=[&](const char* value){
        auto* input=name_editor();input->setFocus();events();input->selectAll();QTest::keyClicks(input,value);events();
        check(input->hasFocus()&&input->isModified(),"Name draft receives actual focused key input");return input;
    };
    auto* current=type_name("Current name");
    check(loaded.unchanged(session),"Normal name draft is view-only until Return");
    Session oracle(loaded.document);oracle.apply({Rename{"first","Current name"}},oracle.revision());
    QTest::keyClick(current,Qt::Key_Return);events();
    check(session.document()==oracle.document()&&session.revision()==loaded.revision+1&&
        session.history().states.size()==loaded.history.states.size()+1&&window.canvas->selections()==selection,
        "Actual Return commits one canonical Rename without changing stable selection or other source");
    const auto renamed=session.document();session.undo(session.revision());window.host.edited();events();
    check(session.document()==loaded.document,"Name Undo restores the complete loaded source");
    session.redo(session.revision());window.host.edited();events();
    check(session.document()==renamed,"Name Redo restores the complete renamed source");
    const ContextSnapshot unchanged_name(session);current=type_name("Current name");
    QTest::keyClick(current,Qt::Key_Return);events();
    check(unchanged_name.unchanged(session),"Reentering an unchanged name creates no revision or Undo entry");
    current=type_name("Stale local name");const auto before_external=session.document();
    Session external_oracle(before_external);external_oracle.apply({Rename{"first","External name"}},external_oracle.revision());
    session.apply({Rename{"first","External name"}},session.revision());const ContextSnapshot external(session);
    window.host.edited();events();
    check(external.unchanged(session)&&session.document()==external_oracle.document(),
        "An external canonical edit and Inspector rebuild never commits the stale focused name draft");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==before_external,"One Undo removes only the external rename after stale-draft refusal");
    session.redo(session.revision());window.host.edited();events();
    check(session.document()==external_oracle.document(),"External rename Redo remains exact");
    const auto saved=session.document();const auto saved_file=scratch.filePath("name-result.nect.json");
    window.host.save(saved_file);window.host.open(saved_file);events();
    check(session.document()==saved&&session.revision()==0&&session.history().states.size()==1,
        "Same Window native save and reopen retains the exact committed name source");
    check(file.open(QIODevice::ReadOnly)&&file.readAll()==bytes,"Original incoming native bytes remain unchanged");file.close();
}
void mixed_text_context(){
    QTemporaryDir scratch;check(scratch.isValid(),"Context probe owns disposable scratch");
    QSettings preferences(scratch.filePath("library.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();events();
    auto* properties=named<QDockWidget>(window,"properties");
    properties->setMinimumWidth(300);properties->setMaximumWidth(300);events();
    auto document=fixture(window.host.session.document());
    Session setup(document);setup.apply({CreatePrimitive{document.compositions.front().id,{},"rectangle","Retained Rectangle",
        default_primitive("retained-rectangle","nect.shape.rectangle")}},setup.revision());
    auto& session=window.host.session;session=Session(setup.document());window.host.edited();events();
    const ContextSnapshot initial(session);
    const auto alignment_visible=[&]{const auto panels=window.findChildren<QWidget*>("text-alignment-batch-panel");
        return std::any_of(panels.begin(),panels.end(),[](auto* panel){return panel->isVisible();});};
    const auto mixed=std::vector<Canvas::Selection>{{"rectangle",{}},{"path",{}},{"first",{}}};
    window.canvas->set_selections(mixed);events();
    const auto selected=window.canvas->selections();
    check(!alignment_visible(),"Mixed Rectangle/Path/Text context omits inapplicable Text alignment panel");
    check(initial.unchanged(session)&&window.canvas->selections()==selected,
        "Mixed-context presentation preserves complete source/native/history/gesture and exact selection");
    auto* transform=named<QPushButton>(window,"selection-transform-open");
    check(transform->isEnabled()&&named<QPushButton>(window,"quick-align-x-min")->isEnabled(),
        "Mixed context retains usable geometric Align and Transform commands");
    auto* scroll=named<QScrollArea>(window,"inspector-scroll");scroll->ensureWidgetVisible(transform);events();
    check(transform->visibleRegion().contains(transform->rect()),"Geometric Transform remains reachable in narrow mixed context");
    if(const auto output=qEnvironmentVariable("NECT_SELECTION_CAPTURE");!output.isEmpty())
        check(window.grab().save(output),"Repaired mixed-context Window preview saved");
    window.canvas->set_selections({{"rectangle",{}},{"path",{}}});events();
    check(!alignment_visible()&&initial.unchanged(session),"Non-Text context has no Text alignment authority or authored mutation");
    select(window);
    check(alignment_visible()&&named<QComboBox>(window,"text-alignment-batch-editor")->isEnabled(),
        "All-Text context retains its real alignment editor");
    choose(named<QComboBox>(window,"text-alignment-batch-editor"),"center");
    check(initial.unchanged(session),"All-Text alignment choice is draft only");
    Session oracle(initial.document);std::vector<Command> commands;
    for(const auto* id:{"first","second"}){auto text=*initial.document.objects.at(id).text;text.alignment="center";
        commands.push_back(UpdateText{id,std::move(text)});}
    oracle.apply(commands,oracle.revision());
    auto* apply=named<QPushButton>(window,"text-alignment-batch-apply");scroll->ensureWidgetVisible(apply);events();
    check(apply->isEnabled()&&apply->visibleRegion().contains(apply->rect()),"All-Text Apply remains enabled and fully reachable");
    QTest::mouseClick(apply,Qt::LeftButton);events();
    check(session.document()==oracle.document()&&session.revision()==initial.revision+1&&
        session.history().states.size()==initial.history.states.size()+1,
        "Actual all-Text pointer Apply matches one complete canonical batch, preserving non-Text source and IDs");
    const auto committed=encode(session.document());
    check(decode(committed)==oracle.document(),"All-Text batch retains complete native authorship");
    session.undo(session.revision());window.host.edited();events();
    check(session.document()==initial.document&&encode(session.document())==initial.native&&!session.can_undo(),
        "One Undo restores both complete Text sources and unrelated retained Rectangle/Path");
    session.redo(session.revision());window.host.edited();events();
    check(session.document()==oracle.document()&&encode(session.document())==committed,
        "One Redo restores the entire alignment batch exactly");
    const ContextSnapshot after(session);window.canvas->set_selections(mixed);events();
    check(!alignment_visible()&&after.unchanged(session),"Returning to mixed context removes Text alignment without editing its authored result");
    auto driven=session.document();driven.objects.at("first").text->alignment_driver=TextAlignmentDriver{{"second",{},"text.alignment"}};
    session=Session(driven);window.host.edited();select(window);const ContextSnapshot linked(session);
    auto* refused=named<QPushButton>(window,"text-alignment-batch-apply");
    check(alignment_visible()&&!refused->isEnabled()&&
        named<QLabel>(window,"text-alignment-batch-status")->text().startsWith("DRIVEN_PROPERTY"),
        "Compatible all-Text context still explains a genuinely driven-property refusal");
    refused->click();check(linked.unchanged(session),"Driven all-Text refusal preserves source/native/history atomically");
}
void key_spacing_retention(){
    QTemporaryDir scratch;check(scratch.isValid(),"Key spacing probe owns disposable scratch");
    QSettings preferences(scratch.filePath("library.ini"),QSettings::IniFormat);
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(preferences));
    window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1000,650);window.show();events();
    auto document=fixture(window.host.session.document());
    auto& points=document.objects.at("path").contours.front().points;
    points.front().x.literal=200;points.back().x.literal=240;points.back().y.literal=220;
    document.objects.at("first").transform[4].literal=320;document.objects.at("first").transform[5].literal=320;
    auto rectangle=default_primitive("spacing-key-source","nect.shape.rectangle");
    rectangle.parameters.at("center_x").literal=100;rectangle.parameters.at("center_y").literal=100;
    rectangle.parameters.at("width").literal=40;rectangle.parameters.at("height").literal=30;
    Session setup(document);setup.apply({CreatePrimitive{document.compositions.front().id,{},"rectangle","Fixed key",rectangle}},setup.revision());
    auto& session=window.host.session;session=Session(setup.document());window.host.edited();
    window.canvas->set_selections({{"rectangle",{}},{"path",{}},{"first",{}}});events();
    const auto targets=window.canvas->selected_objects();const ContextSnapshot initial(session);
    choose(named<QComboBox>(window,"alignment-target"),"key_object:rectangle");
    auto* spacing=named<QLineEdit>(window,"distribution-spacing");
    auto* scroll=named<QScrollArea>(window,"inspector-scroll");scroll->ensureWidgetVisible(spacing);events();
    check(spacing->isEnabled()&&spacing->visibleRegion().contains(spacing->rect()),"Explicit key spacing input is fully reachable");
    spacing->setFocus();QTest::keyClicks(spacing,"12.500");events();
    check(spacing->text()=="12.500"&&initial.unchanged(session),"Exact spacing keyboard draft and key choice do not author Document or History");
    const auto apply_axis=[&](const char* axis){
        const ContextSnapshot before(session);Session oracle(before.document);
        oracle.apply({DistributeObjects{targets,axis,"key_object:rectangle",12.5}},oracle.revision());
        auto* button=named<QPushButton>(window,std::string("quick-distribute-").append(axis).c_str());
        scroll->ensureWidgetVisible(button);events();
        check(button->isEnabled()&&button->visibleRegion().contains(button->rect()),"Actual Distribute pointer command is ready and reachable");
        QTest::mouseClick(button,Qt::LeftButton);events();
        check(session.document()==oracle.document()&&session.revision()==before.revision+1&&
            session.history().states.size()==before.history.states.size()+1,
            "Each axis applies one complete canonical Distribute command with exact gap");
        check(session.document().objects.at("rectangle")==initial.document.objects.at("rectangle")&&
            session.document().objects.at("second")==initial.document.objects.at("second"),
            "Chosen retained key and complete unrelated Text remain fixed");
        check(named<QComboBox>(window,"alignment-target")->currentData().toString()=="key_object:rectangle"&&
            named<QLineEdit>(window,"distribution-spacing")->text()=="12.500",
            "Inspector rebuild after Distribute retains explicit key and exact spacing draft for the next axis");
    };
    apply_axis("x");const auto horizontal=session.document();apply_axis("y");const auto both=session.document();
    check(decode(encode(both))==both,"Two-axis layout retains complete native authored source");
    session.undo(session.revision());window.host.edited();events();check(session.document()==horizontal,"One Undo restores only the second-axis command");
    session.undo(session.revision());window.host.edited();events();check(session.document()==initial.document,"Second Undo restores complete original mixed source");
    session.redo(session.revision());session.redo(session.revision());window.host.edited();events();
    check(session.document()==both&&named<QLineEdit>(window,"distribution-spacing")->text()=="12.500",
        "Redo restores both commands while transient gap stays exact");
    const ContextSnapshot committed(session);
    spacing=named<QLineEdit>(window,"distribution-spacing");spacing->setText("-1");window.host.edited();events();
    check(named<QLineEdit>(window,"distribution-spacing")->text()=="-1"&&
        !named<QPushButton>(window,"quick-distribute-x")->isEnabled()&&
        !named<QPushButton>(window,"quick-distribute-y")->isEnabled()&&committed.unchanged(session),
        "Invalid gap draft survives refresh with both commands disabled and no authored mutation");
    named<QLineEdit>(window,"distribution-spacing")->clear();window.host.edited();events();
    check(named<QLineEdit>(window,"distribution-spacing")->text().isEmpty()&&
        !named<QPushButton>(window,"quick-distribute-x")->isEnabled()&&committed.unchanged(session),
        "Clearing spacing remains explicit and never substitutes a default gap");
    Window reopened(scratch.filePath("other-recovery"),std::make_unique<FolderLibrary>(preferences));
    reopened.setAttribute(Qt::WA_DontShowOnScreen);reopened.resize(1000,650);reopened.show();
    reopened.host.session=Session(decode(encode(both)));reopened.host.edited();
    reopened.canvas->set_selections({{"rectangle",{}},{"path",{}},{"first",{}}});events();
    check(named<QLineEdit>(reopened,"distribution-spacing")->text().isEmpty()&&reopened.host.session.document()==both,
        "New Window starts without spacing draft; native carries only canonical authored layout");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");app.setStyleSheet(application_style_sheet());
    auto font=app.font();font.setFamily("Yu Gothic UI");font.setPixelSize(13);app.setFont(font);
    try{
        if(app.arguments().contains("--text-weight-context")){
            text_weight_context();std::cout<<"PASS production Text Weight context ("<<checks<<" checks; physical OS input NOT_RUN)\n";return 0;
        }
        if(app.arguments().contains("--object-name-context")){
            object_name_context();std::cout<<"PASS Object name context ("<<checks<<" checks; physical OS input NOT_RUN)\n";return 0;
        }
        if(app.arguments().contains("--mixed-text-context")){
            mixed_text_context();std::cout<<"PASS mixed Text contextual ownership ("<<checks<<" checks; physical OS input NOT_RUN)\n";return 0;
        }
        if(app.arguments().contains("--key-spacing-retention")){
            key_spacing_retention();std::cout<<"PASS key spacing retention ("<<checks<<" checks; physical OS input NOT_RUN)\n";return 0;
        }
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
