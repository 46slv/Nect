#include "theme.hpp"
#include "workspace.hpp"
#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Nect Studio");
    app.setOrganizationName("Nect");
    nect::studio::apply_theme(app);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"open", "Open an existing native document", "path"});
    parser.addOption({"recovery-dir", "Use an owned recovery directory", "path"});
    parser.addOption({"capture",
                      "Render the widget tree to a PNG, then exit (not a desktop screenshot)",
                      "path"});
    parser.addOption({"width", "Logical client width", "DIP", "1440"});
    parser.addOption({"height", "Logical client height", "DIP", "840"});
    parser.addOption({"nodes", "Show the selected object's pipeline"});
    parser.addOption({"select-first", "Select the first opened object for inspection"});
    parser.process(app);
    try {
        auto recovery = parser.value("recovery-dir");
        if (recovery.isEmpty())
            recovery = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                       "/recovery";
        nect::studio::Workspace w(recovery);
        w.resize(parser.value("width").toInt(), parser.value("height").toInt());
        if (parser.isSet("open"))
            w.open_document(parser.value("open"));
        w.show();
        if (parser.isSet("nodes") || parser.isSet("select-first")) {
            if (!w.host.session.document().objects.empty())
                w.select_object(w.host.session.document().objects.begin()->first);
            if (parser.isSet("nodes"))
                w.set_nodes_visible(true);
        }
        if (parser.isSet("capture"))
            QTimer::singleShot(300, &w, [&] {
                const auto pixels = w.grab();
                const bool ok = pixels.save(parser.value("capture"));
                QJsonArray menu_items;
                for (const auto *a : w.menuBar()->actions()) {
                    const auto r = w.menuBar()->actionGeometry(const_cast<QAction *>(a));
                    menu_items.append(QJsonObject{{"text", a->text()},
                                                  {"visible", a->isVisible()},
                                                  {"x", r.x()},
                                                  {"y", r.y()},
                                                  {"width", r.width()},
                                                  {"height", r.height()}});
                }
                QFile metadata(parser.value("capture") + ".json");
                const bool written = metadata.open(QIODevice::WriteOnly);
                if (written)
                    metadata.write(
                        QJsonDocument(
                            QJsonObject{
                                {"capture_type", "Qt widget render; no native desktop screenshot"},
                                {"logical_width", w.width()},
                                {"logical_height", w.height()},
                                {"pixel_width", pixels.width()},
                                {"pixel_height", pixels.height()},
                                {"device_pixel_ratio", w.devicePixelRatioF()},
                                {"platform", QApplication::platformName()},
                                {"menu_visible", w.menuBar()->isVisible()},
                                {"menu_width", w.menuBar()->width()},
                                {"menu_height", w.menuBar()->height()},
                                {"menu_items", menu_items}})
                            .toJson());
                app.exit(ok && written ? 0 : 2);
            });
        return app.exec();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Nect: %s\n", e.what());
        return 1;
    }
}
