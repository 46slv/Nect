#include "window.hpp"
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
    app.setStyleSheet(
        "QMainWindow,QDialog,QWidget{background:#25292f;color:#e1e5eb;}"
        "QLineEdit,QTreeWidget,QListWidget{background:#1c2026;border:1px solid #393f49;border-radius:3px;padding:4px;}"
        "QTreeWidget::item,QListWidget::item{padding:5px;}"
        "QTreeWidget::item:selected,QListWidget::item:selected{background:#354d5b;}"
        "QPushButton{background:#333943;border:1px solid #454d58;border-radius:3px;padding:4px;}"
        "QPushButton:hover{background:#414a56;}"
        "QGroupBox{border:1px solid #3a414b;border-radius:4px;margin-top:12px;padding-top:10px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;}"
        "QToolBar{spacing:8px;padding:4px;border-bottom:1px solid #3b424a;}"
        "QMenu{border:1px solid #49515c;}QMenu::item:selected{background:#43505f;}"
    );
    QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"automation-endpoint","Local Session API pipe/socket name","name"});
    parser.addOption({"recovery-dir","Recovery directory (defaults to local app data)","path"});
    parser.addOption({"ready-file","Write local API identity after window startup","path"});
    parser.addPositionalArgument("document","Native document to open", "[document]");parser.process(app);
    try {
        auto recovery=parser.value("recovery-dir");
        if(recovery.isEmpty())recovery=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recovery";
        nect::desktop::Window window(recovery);
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
