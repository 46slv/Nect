#include "window.hpp"
#include "visual_style.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <iostream>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    app.setApplicationName("Nect");app.setOrganizationName("Nect");
    app.setStyle("Fusion");
    app.setStyleSheet(nect::desktop::application_style_sheet());
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"automation-endpoint","Local Session API pipe/socket name","name"});
    parser.addOption({"recovery-dir","Recovery directory (defaults to local app data)","path"});
    parser.addOption({"ready-file","Write local API identity after window startup","path"});
    parser.addPositionalArgument("document","Native document to open", "[document]");parser.process(app);
    try {
        auto recovery=parser.value("recovery-dir");
        if(recovery.isEmpty())recovery=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recovery";
        QSettings workspace_preferences(QSettings::NativeFormat,QSettings::UserScope,"Nect","Nect");
        nect::desktop::Window window(recovery,{},&workspace_preferences);
        if(!parser.positionalArguments().isEmpty())window.host.open(parser.positionalArguments().front());
        const auto endpoint=parser.value("automation-endpoint");
        if(!endpoint.isEmpty())window.host.listen(endpoint);
        window.show();
        QTimer::singleShot(0,&window,[&]{
            window.canvas->fit_artboard();
            const auto path=parser.value("ready-file");
            if(!path.isEmpty()) {
                auto hello=QJsonDocument::fromJson(window.host.dispatch("{\"op\":\"hello\"}")).object();
                hello["endpoint"]=window.host.endpoint();
                QFile file(path);
                if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(hello).toJson());
            }
        });
        return app.exec();
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
