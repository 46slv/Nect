#include "window.hpp"
#include "nect/io.hpp"
#include <QApplication>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QMenu>
#include <iostream>
#include <stdexcept>
#include <tuple>
using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
constexpr auto key="workspace/tools/textCreationDirection";
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
void events(){QApplication::processEvents();}
auto snapshot(Session& session){return std::tuple{session.document(),encode(session.document()),session.revision(),session.history()};}
void show(Window& window){window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(1100,750);window.show();events();}
void choose(Window& window,bool vertical){
    window.findChild<QToolButton*>("tool-text")->menu()->actions().at(vertical?1:0)->trigger();events();
}
void click(Window& window,const char* name){QTest::mouseClick(window.findChild<QToolButton*>(name),Qt::LeftButton);events();}
void label(Window& window,bool vertical){
    check(window.findChild<QToolButton*>("tool-text")->accessibleName().startsWith(vertical?"Vertical Text":"Horizontal Text"),
        "Window restores the creation variant from its owned store");
}
void child(const QString& stage,const QString& root){
    QSettings settings(root+"/settings.ini",QSettings::IniFormat);
    Window window(root+"/recovery-"+stage,std::make_unique<FolderLibrary>(settings),&settings);show(window);
    check(window.findChild<QToolButton*>("tool-selection")->isChecked()&&!window.canvas->text_mode(),
        "Reopen restores the variant but active tool remains per Window");
    auto& session=window.host.session;
    if(stage=="write"){
        auto document=session.document();Object existing;existing.id="existing-text";
        existing.name="Existing Japanese Text";existing.kind=Kind::text;
        existing.text=default_text("existing-text-source","日本語（ABC123）");
        existing.text->family="Yu Gothic";existing.text->weight=600;existing.text->italic=true;
        document.compositions.front().roots.push_back(existing.id);document.objects.emplace(existing.id,existing);
        session=Session(document);window.host.edited();window.canvas->set_selection(existing.id);events();
        label(window,false);const auto before=snapshot(session);choose(window,true);
        check(snapshot(session)==before,"Choosing Vertical is document/native/revision/history neutral");
        window.host.save(root+"/document.nect");
    }else{
        const bool vertical=stage=="read-vertical";label(window,vertical);
        window.host.open(root+"/document.nect");events();label(window,vertical);
        check(session.document().objects.at("existing-text").text->direction=="horizontal"&&
            session.document().objects.at("existing-text").text->family=="Yu Gothic"&&
            session.document().objects.at("existing-text").text->weight==600,
            "Preference reopen does not convert or restyle existing Japanese Text");
        window.canvas->set_selection("existing-text");events();
        const auto before=snapshot(session);
        for(const auto* tool:{"tool-text","tool-pen","tool-selection","tool-text"})click(window,tool);
        check(snapshot(session)==before,"Document open and ordinary tool switches retain exact authored/native/history state");
        const auto original=session.document();const auto revision=session.revision();const auto history=session.history();
        QTest::mouseClick(window.canvas,Qt::LeftButton,Qt::NoModifier,window.canvas->rect().center());events();
        const auto created=session.document();
        check(created.objects.size()==original.objects.size()+1&&session.revision()==revision+1&&
            session.history().states.size()==history.states.size()+1,"Reopened ordinary Text click creates through one Session command");
        check(created.objects.at(window.canvas->selected_object).text->direction==(vertical?"vertical":"horizontal"),
            "Actual Canvas creation uses persisted direction");
        session.undo(session.revision());check(session.document()==original,"One Undo restores complete pre-creation document");
        session.redo(session.revision());check(session.document()==created,"One Redo restores exact created Text");
        window.host.open(root+"/document.nect");events();label(window,vertical);
        window.canvas->set_selection("existing-text");events();
        if(vertical){const auto before_horizontal=snapshot(session);choose(window,false);
            check(snapshot(session)==before_horizontal,"Choosing Horizontal after reload never converts authored Text");}
    }
    settings.sync();check(settings.value("unrelated/value").toByteArray()==QByteArray("keep\0bytes",10),
        "Saving creation preference preserves unrelated binary keys");
}
void run_child(const QString& stage,const QString& root){
    QProcess process;process.start(QCoreApplication::applicationFilePath(),{"--child",stage,root});
    check(process.waitForStarted(5000),"Owned separate test process starts");
    check(process.waitForFinished(25000),"Owned separate test process exits");
    const auto output=process.readAllStandardOutput()+process.readAllStandardError();std::cout<<output.constData();
    check(process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0,"Separate preference/Window reopen passes");
}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");
    try{
        const auto args=app.arguments();
        if(args.size()==4&&args[1]=="--child"){child(args[2],args[3]);std::cout<<args[2].toStdString()<<": "<<checks<<" checks passed\n";return 0;}
        QTemporaryDir scratch;check(scratch.isValid(),"Preferences and recovery use owned scratch only");
        {
            QSettings settings(scratch.filePath("settings.ini"),QSettings::IniFormat);
            settings.setValue("unrelated/value",QByteArray("keep\0bytes",10));settings.sync();
            check(settings.status()==QSettings::NoError,"Owned unrelated preference is seeded");
        }
        run_child("write",scratch.path());run_child("read-vertical",scratch.path());run_child("read-horizontal",scratch.path());
        for(const auto& invalid:{QVariant{},QVariant("sideways"),QVariant(17)}){
            QSettings settings(scratch.filePath("fallback.ini"),QSettings::IniFormat);
            if(invalid.isValid())settings.setValue(key,invalid);else settings.remove(key);settings.sync();
            Window window(scratch.filePath("fallback-recovery"),std::make_unique<FolderLibrary>(settings),&settings);show(window);
            label(window,false);const auto before=snapshot(window.host.session);click(window,"tool-text");
            check(snapshot(window.host.session)==before,"Absent/invalid preference falls back without authored/history mutation");
            check(settings.value(key)==invalid,"Fallback does not rewrite the invalid or absent preference");
        }
        {
            QSettings library(scratch.filePath("library.ini"),QSettings::IniFormat);
            Window window(scratch.filePath("no-store-recovery"),std::make_unique<FolderLibrary>(library));show(window);
            const auto before=snapshot(window.host.session);choose(window,true);label(window,true);
            check(snapshot(window.host.session)==before&&window.statusBar()->currentMessage().contains("could not be saved"),
                "Unavailable store retains transient tool choice and explicitly reports failure without authored mutation");
        }
        const auto readonly=scratch.filePath("readonly.ini");
        {QSettings settings(readonly,QSettings::IniFormat);settings.setValue(key,"horizontal");settings.sync();}
        QFile file(readonly);check(file.open(QIODevice::ReadOnly),"Read-only fixture can be inspected");const auto original=file.readAll();file.close();
        check(file.setPermissions(QFile::ReadOwner|QFile::ReadGroup|QFile::ReadOther),"Owned fixture is made read-only");
        {
            QSettings settings(readonly,QSettings::IniFormat),library(scratch.filePath("readonly-library.ini"),QSettings::IniFormat);
            check(!settings.isWritable(),"Backing preference file is actually read-only");
            Window window(scratch.filePath("readonly-recovery"),std::make_unique<FolderLibrary>(library),&settings);show(window);
            const auto before=snapshot(window.host.session);choose(window,true);label(window,true);
            check(snapshot(window.host.session)==before&&window.statusBar()->currentMessage().contains("could not be saved"),
                "Read-only preference reports failure without converting Text or committing history");
        }
        check(file.open(QIODevice::ReadOnly)&&file.readAll()==original,"Failed storage leaves preference file byte-exact");file.close();
        file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
        std::cout<<"workspace_text_variant_tests: "<<checks<<" parent checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
