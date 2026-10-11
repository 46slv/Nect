#pragma once

#include "window.hpp"
#include "nect/io.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace template_reset_incoming {
using namespace nect;
using namespace nect::desktop;
inline int checks=0;
inline void check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
    ++checks;
}
inline void events(){QApplication::processEvents();}
inline Document fixture() {
    auto d=empty_document("reset-incoming-document","reset-composition","reset-target");
    auto& c=d.compositions.front();
    auto& target=c.artboards.front();
    target.name="Same frame";target.x=1400;target.y=100;target.width=640;target.height=480;
    target.layout=ArtboardLayout{Margin{5,6,7,8},std::nullopt};
    target.local_guides={{"local-guide","Local","y",15.125,true}};
    ArtboardTemplateAssignment assignment;
    assignment.template_id="reset-template";assignment.grid_id="target-grid";
    assignment.content_instance="reset-content";assignment.width_override=700;
    assignment.height_override=490;assignment.margin_overridden=true;
    assignment.guide_position_overrides={{"source-guide",45.125}};
    assignment.guide_enabled_overrides={{"source-guide",false}};
    target.template_assignment=assignment;
    Artboard source{"reset-source","Same frame",0,0,1200,800};
    source.layout=ArtboardLayout{Margin{10,20,30,40},Grid{"source-grid",{30,20,100,80},1,1,0,0}};
    source.local_guides={{"source-guide","Source","x",20.25,true}};
    c.artboards.push_back(source);
    c.artboards.push_back({"reset-decoy","Same frame",3000,0,2100,1700});
    c.templates={{"reset-template","Shared","reset-source",Id{"reset-definition"}}};
    Object root;root.id="reset-root";root.name="Source root";root.kind=Kind::group;
    root.visible=false;root.children={"reset-logo","reset-caption"};
    Object logo;logo.id="reset-logo";logo.name="Logo";
    logo.source=default_primitive("reset-rectangle","nect.shape.rectangle");
    logo.source->parameters.at("width").literal=60;
    logo.source->parameters.at("height").literal=40;
    logo.transform[4].literal=10;logo.transform[5].literal=6;
    auto fill=default_operation("reset-fill","nect.paint.fill");
    fill.parameters.at("r").literal=0.9;fill.parameters.at("g").literal=0.2;
    fill.parameters.at("b").literal=0.1;fill.parameters.at("a").literal=1;
    logo.stack={fill};
    Object caption;caption.id="reset-caption";caption.name="Caption";caption.kind=Kind::text;
    caption.text=default_text("reset-text","Retained full source");
    Object content;content.id="reset-content";content.name="Shared";content.kind=Kind::instance;
    content.instance=DefinitionInstance{"reset-definition",{{Ref{"reset-logo","","transform.tx"},90}}};
    content.transform[4].literal=1400;content.transform[5].literal=100;
    d.objects.emplace(root.id,root);d.objects.emplace(logo.id,logo);
    d.objects.emplace(caption.id,caption);d.objects.emplace(content.id,content);
    d.definitions.emplace("reset-definition",Definition{"reset-definition","Shared","reset-root"});
    c.roots={root.id,content.id};
    Composition unrelated;unrelated.id="unrelated-composition";unrelated.name="Independent";
    unrelated.artboards={{"unrelated-artboard","Independent",0,0,320,240}};
    d.compositions.push_back(unrelated);
    return d;
}
inline Document incoming_document() {
    auto d=fixture();d.compositions.front().artboards[1].width=1300;return d;
}
inline void native_valid(const Document& d) {
    check(decode(encode(d))==d,"Independent complete Template/Composition/Artboard/content/Guide native oracle validates before Window");
    Session accepted(d);check(accepted.document()==d,"Independent complete fixture accepted by Session before Window");
}
inline void state(const Session& actual,const Session& expected,const Document& document) {
    check(actual.document()==document&&encode(actual.document())==encode(document),"Entire authored Document equals independent oracle");
    check(actual.history()==expected.history()&&actual.revision()==expected.revision(),"Entire History including labels/estimated bytes and revision equals independent canonical oracle");
    check(actual.can_undo()==expected.can_undo()&&actual.can_redo()==expected.can_redo(),"Complete Undo/Redo availability retained");
    check(actual.gesture_active()==expected.gesture_active()&&actual.gesture_generation()==expected.gesture_generation(),"Entire gesture state retained");
}
inline QByteArray bytes(const QString& path) {
    QFile file(path);check(file.open(QIODevice::ReadOnly),"Read exact owned bytes");
    auto value=file.readAll();check(file.error()==QFileDevice::NoError,"Owned read complete");return value;
}
inline int cold(const QString& path,bool replacement=false) {
    check(QApplication::platformName()=="windows","Cold native process uses Windows platform");
    auto expected=incoming_document();
    if(replacement)expected.compositions.front().artboards[1].width=1400;
    native_valid(expected);
    QTemporaryDir scratch;check(scratch.isValid(),"Cold Host owns recovery directory");
    const auto original=bytes(path);
    Host host(scratch.path());host.open(path);
    const Session fresh(expected);state(host.session,fresh,expected);
    check(original==QByteArray::fromStdString(encode(expected)),"Cold native bytes match entire independent incoming oracle");
    host.flush();check(bytes(path)==original,"Cold Host preserves original incoming native bytes");
    std::cout<<"PASS cold Template Reset incoming native: "<<checks<<" checks\n";return 0;
}
inline int run(bool replacement=false) {
    check(QApplication::platformName()=="windows","Actual native Windows Window platform");
    const auto seed=fixture();auto before=seed, incoming=incoming_document();
    Session initial(seed);
    if(replacement) {
        before.objects.at("reset-caption").name="Original prior edit";
        initial.apply({Rename{"reset-caption","Original prior edit"}},0);
        incoming.compositions.front().artboards[1].width=1400;
    }
    check(initial.document()==before,"Independent original whole Document and prior History are established before Window");
    auto reset=incoming;reset.compositions.front().artboards.front().template_assignment->width_override.reset();
    native_valid(before);native_valid(incoming);native_valid(reset);
    const auto replacement_document=incoming_document();
    if(replacement)native_valid(replacement_document);
    Session replacement_base(replacement_document),reference(replacement?replacement_document:before);
    reference.apply({UpdateArtboard{"reset-composition",incoming.compositions.front().artboards[1]}},0);
    state(reference,reference,incoming);
    check(reference.revision()==1,"Independent incoming source edit advances canonical revision before Window");
    auto source_frame=QJsonDocument::fromJson(QByteArray::fromStdString(encode(incoming))).object()
        .value("compositions").toArray().at(0).toObject().value("artboards").toArray().at(1).toObject();
    // update_artboard is the existing frame/layout API; it preserves authored Guides.
    // Guide mutations have separate typed commands and are not fields in this request.
    source_frame.remove("local_guides");
    const QJsonObject command{{"type","update_artboard"},{"composition","reset-composition"},{"artboard",source_frame}};
    const QJsonObject core_request{{"op","apply"},{"expected_revision",0},{"commands",QJsonArray{command}}};
    auto api_reference=replacement?replacement_base:initial;
    const auto api_result=QJsonDocument::fromJson(QByteArray::fromStdString(nect::request(api_reference,
        QJsonDocument(core_request).toJson(QJsonDocument::Compact).toStdString()))).object();
    check(api_result.value("ok").toBool(),"Independent exact incoming API request validates before Window");
    state(api_reference,reference,incoming);
    auto stale=reference;
    const Command reset_command=ArtboardTemplateCommand{ResetArtboardTemplateOverride{"reset-composition","reset-target","frame.width"}};
    if(replacement) {
        check(initial.revision()==reference.revision(),"Independent replacement incoming revision equals original captured revision before Window");
        stale.apply({reset_command},initial.revision());
        check(stale.document()==reset,"Independent numeric revision alone would accept Reset in replacement Session before Window");
    }else {
        bool refused=false;
        try{stale.apply({reset_command},0);}
        catch(const Error& error){check(error.code=="REVISION_CONFLICT","Independent canonical stale Reset code derived before Window");refused=true;}
        check(refused,"Canonical stale Reset refuses before Window");state(stale,reference,incoming);
    }
    auto undone=reference;undone.undo(undone.revision());
    const auto& undo_document=replacement?replacement_document:before;
    check(undone.document()==undo_document,"Independent incoming Undo returns entire prior Document before Window");
    auto redone=undone;redone.redo(redone.revision());
    check(redone.document()==incoming,"Independent incoming Redo returns entire source edit before Window");
    const auto effective=evaluate_artboard(incoming.compositions.front(),"reset-target");
    check(effective.width==700&&effective.height==490,"Independent incoming target still has both own overrides");
    check(evaluate_artboard(reset.compositions.front(),"reset-target").width==(replacement?1400:1300),"Independent Reset result would inherit changed source width");
    const auto expected_error=replacement?QString("SESSION_CONFLICT: Template command belongs to another document session"):
        QString("REVISION_CONFLICT: Template command captured revision 0 but the current revision is 1; discard it and refresh the captured Artboard");
    // Window::perform renders code + what(); Reset's accepted dialog closes before revalidation.
    QTemporaryDir scratch;check(scratch.isValid(),"Owned recovery/settings/native scratch");
    QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
    settings.setFallbacksEnabled(false);settings.setValue("owned/sentinel","reset-incoming");settings.sync();
    const auto settings_before=bytes(settings.fileName());
    const auto source_path=scratch.filePath("original.nect");
    {QFile file(source_path);check(file.open(QIODevice::WriteOnly),"Owned original snapshot opens");
        const auto encoded=QByteArray::fromStdString(encode(before));check(file.write(encoded)==encoded.size(),"Entire owned initial snapshot writes");}
    const auto source_before=bytes(source_path);
    const auto replacement_path=scratch.filePath("replacement.nect");
    if(replacement) {
        QFile file(replacement_path);check(file.open(QIODevice::WriteOnly),"Owned replacement native snapshot opens");
        const auto encoded=QByteArray::fromStdString(encode(replacement_document));
        check(file.write(encoded)==encoded.size(),"Whole independent replacement native snapshot writes before Window");
    }
    Window window(scratch.filePath("recovery"),std::make_unique<FolderLibrary>(settings),&settings);
    window.resize(1400,900);window.host.session=initial;window.host.session_id="template-reset-incoming-session";
    window.refresh();window.show();window.activateWindow();events();
    auto* list=window.findChild<QListWidget*>("artboards");check(list,"Public Artboard list exists");
    bool selected=false;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()=="reset-target"){
        list->setCurrentItem(list->item(i));selected=true;break;}
    check(selected,"Exact stable target selected through public Artboard list");events();
    auto* edit=window.findChild<QPushButton*>("artboard-edit");check(edit&&edit->isVisible()&&edit->isEnabled(),"Public Edit captured Artboard available");
    QTest::mouseClick(edit,Qt::LeftButton);events();
    auto* dock=window.findChild<QDockWidget*>("properties");check(dock,"Production Properties dock exists");dock->show();dock->raise();events();
    QPointer<QPushButton> action=window.findChild<QPushButton*>("artboard-template-reset-override");
    check(action&&action->isVisible()&&action->isEnabled(),"Existing Reset override action is reachable");
    if(auto* scroll=window.findChild<QScrollArea*>("inspector-scroll"))scroll->ensureWidgetVisible(action);
    events();state(window.host.session,initial,before);
    const auto session_id=window.host.session_id;
    auto live_session_id=session_id;
    const auto composition=window.canvas->active_composition(),artboard=window.canvas->active_artboard();
    const auto selection=window.canvas->selections();
    std::exception_ptr failure;QPointer<QDialog> modal;bool entered=false;int pressed=0,released=0,clicked=0;
    QObject::connect(action,&QPushButton::pressed,&window,[&]{++pressed;});
    QObject::connect(action,&QPushButton::released,&window,[&]{++released;});
    QObject::connect(action,&QPushButton::clicked,&window,[&]{++clicked;});
    QTimer::singleShot(0,&window,[&]{
        modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        try {
            check(modal&&modal->objectName()=="reset-artboard-template-override-dialog"&&modal->parentWidget()==&window,
                "Actual existing production Reset opens its owned modal");entered=true;
            auto* field=modal->findChild<QComboBox*>("template-reset-field");check(field,"Existing Reset field selector present");
            const auto index=field->findData("frame.width");check(index>=0,"Exact original own width override is offered");
            field->setCurrentIndex(index);state(window.host.session,initial,before);
            check(field->currentData().toString()=="frame.width","Choosing width reset remains authored-state neutral");
            if(replacement) {
                window.host.open(replacement_path);events();live_session_id=window.host.session_id;
                check(live_session_id!=session_id,"Actual Host.open creates a different owning Session, retaining Document and target IDs");
                state(window.host.session,replacement_base,replacement_document);
                check(modal&&modal->isVisible()&&QApplication::activeModalWidget()==modal,"Actual native replacement keeps original owned Reset modal open");
            }
            // Use the exact frame request independently validated before Window.
            const QJsonObject envelope{{"op","core"},{"session_id",live_session_id},{"document_id","reset-incoming-document"},{"request",core_request}};
            const auto response=QJsonDocument::fromJson(window.host.dispatch(QJsonDocument(envelope).toJson(QJsonDocument::Compact))).object();
            if(!response.value("ok").toBool()||response.value("revision").toInteger()!=1)
                std::cerr<<"Incoming Host API response: "<<QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString()<<'\n';
            check(response.value("ok").toBool()&&response.value("revision").toInteger()==1,"Canonical Host source-width edit succeeds while original Reset modal is open");
            state(window.host.session,reference,incoming);
            if(replacement)check(window.host.session.revision()==initial.revision(),"Actual replacement incoming revision matches original modal's numeric revision");
            check(modal&&modal->isVisible()&&QApplication::activeModalWidget()==modal,"Incoming leaves original Reset modal open until confirmation");
            check(window.host.session_id==live_session_id&&window.canvas->active_composition()==composition&&window.canvas->active_artboard()==artboard&&window.canvas->selections()==selection,
                "Incoming retains exact Host/Canvas target and selection identities");
            auto* buttons=modal->findChild<QDialogButtonBox*>();check(buttons&&buttons->button(QDialogButtonBox::Apply),"Existing Reset selected field Apply exists");
            QTest::mouseClick(buttons->button(QDialogButtonBox::Apply),Qt::LeftButton);
        }catch(...){failure=std::current_exception();if(modal)modal->reject();}
    });
    QTest::mouseClick(action,Qt::LeftButton);events();if(failure)std::rethrow_exception(failure);
    check(entered&&pressed==1&&released==1&&clicked==1,"Native QTest Reset pointer press/release invokes existing handler exactly once");
    check(!modal&&!QApplication::activeModalWidget(),"Accepted existing Reset dialog closes before stale error is displayed");
    check(window.statusBar()->currentMessage()==expected_error,"Exact existing Window conflict code/message matches captured context owner");
    state(window.host.session,reference,incoming);
    check(window.host.session_id==live_session_id&&window.canvas->active_composition()==composition&&window.canvas->active_artboard()==artboard&&window.canvas->selections()==selection,
        "Stale Reset retains exact Host/Canvas target and selection identities");
    const auto undo=[&](const QString& text){for(auto* a:window.findChildren<QAction*>())if(a->text()==text){a->trigger();events();return;}throw std::runtime_error("Missing existing History action");};
    undo("Undo");state(window.host.session,undone,undo_document);
    undo("Redo");state(window.host.session,redone,incoming);
    const auto path=scratch.filePath("incoming.nect");window.host.save(path);
    check(bytes(path)==QByteArray::fromStdString(encode(incoming)),"Actual Host Save contains entire independent incoming Document");
    QProcess process;process.start(QCoreApplication::applicationFilePath(),{replacement?"--template-reset-session-cold":"--template-reset-incoming-cold",path});
    check(process.waitForStarted(10000),"Start fresh native Host process for incoming cold reopen");
    const bool finished=process.waitForFinished(15000);if(!finished){process.kill();process.waitForFinished(5000);}
    const auto output=process.readAllStandardOutput(),errors=process.readAllStandardError();
    std::cout<<output.toStdString();std::cerr<<errors.toStdString();
    check(finished&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0&&output.contains("PASS cold Template Reset incoming native:"),
        "Fresh Windows Host cold native readback passes its complete independent oracle");
    check(bytes(source_path)==source_before,"Original source native snapshot remains byte-identical");
    if(replacement)check(bytes(replacement_path)==QByteArray::fromStdString(encode(replacement_document)),"Actual Host.open original replacement file remains byte-identical through refusal/Undo/Redo/Save");
    settings.sync();check(bytes(settings.fileName())==settings_before,"Whole owned settings remain byte-identical");
    window.close();window.host.flush();
    std::cout<<"PASS native Windows Template Reset after "<<(replacement?"replacement Session":"incoming source")<<": "<<checks<<" checks\n";return 0;
}
}
