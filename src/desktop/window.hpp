#pragma once
#include "host.hpp"
#include "canvas.hpp"
#include <QMainWindow>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QListWidget>
#include <QPointer>

class QDialog;
class QStringListModel;

namespace nect::desktop {
class ColorTools;
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
    ColorTools* color_tools_;
    QStringListModel* font_families_=nullptr;
    QPointer<QDialog> history_dialog_;
    QListWidget* history_states_=nullptr;
    QLabel* history_status_=nullptr;
    QString history_session_;
    std::uint64_t history_revision_=0;
    void show_history();
    void refresh_history();
    QTreeWidget* tree_;
    QListWidget* artboards_;
    bool artboard_editing_=false;
    QWidget* inspector_;
    QScrollArea* inspector_scroll_;
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
    void reveal_whip_source();
    void rebuild_inspector();
    void add_property(QFormLayout* layout,const Ref& ref,const QString& label);
    void pick_source(Ref target,bool relative=false);
    void save(bool choose);
    void add_curve();
    void add_primitive(const std::string& type);
    void add_text();
    void add_text_properties(QVBoxLayout* layout,const Object& object);
    void edit_text_content(const Id& object);
    void convert_to_path();
    void add_operation(const std::string& type,bool radial=false);
    void move_operation(const Id& object,const Id& operation,int direction);
    void add_stack(QVBoxLayout* layout,const Object& object);
    void add_gradient(QFormLayout* layout,const Object& object,const ShapeOperation& operation);
    void group_selection();
    void rebuild_artboards();
    void add_artboard(bool duplicate);
    void move_artboard(int direction);
    void edit_artboard(QVBoxLayout* layout);
};
}
