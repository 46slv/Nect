#pragma once
#include "host.hpp"
#include "canvas.hpp"
#include <QMainWindow>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

namespace nect::desktop {
class Window : public QMainWindow {
public:
    explicit Window(QString recovery_directory);
    ~Window() override;
    Host host;
    Canvas* canvas;
    void refresh();
    void perform(const std::function<void()>& action);
protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    QTreeWidget* tree_;
    QWidget* inspector_;
    QLabel* status_;
    QLabel* breadcrumb_;
    QAction* undo_;
    QAction* redo_;
    bool refreshing_=false;
    std::map<Ref,double> inspector_values_;
    QString tree_signature_;
    std::optional<Ref> whip_target_;
    QString whip_session_;
    QPoint whip_start_;
    bool whip_dragged_=false;
    QWidget* whip_overlay_=nullptr;
    void cancel_whip();
    void rebuild_inspector();
    void add_property(QFormLayout* layout,const Ref& ref,const QString& label);
    void pick_source(Ref target,bool relative=false);
    void save(bool choose);
    void add_curve();
    void group_selection();
};
}
