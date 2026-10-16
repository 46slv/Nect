#pragma once
#include "host.hpp"
#include <QMainWindow>
#include <QPointF>
#include <map>
class QTreeWidget;
class QLineEdit;
class QLabel;
class QToolButton;
class QSplitter;
class QScrollArea;
class QAction;
namespace nect::desktop {
class Canvas;
}
namespace nect::studio {
class NodeView;
class Workspace final : public QMainWindow {
  public:
    explicit Workspace(const QString &recovery_directory, QWidget *parent = nullptr);
    ~Workspace() override;
    desktop::Host host;
    desktop::Canvas *canvas = nullptr;
    void add_rectangle();
    void open_document(const QString &);
    void save_document(const QString &);
    void refresh();
    void select_object(const Id &);
    void set_rectangle_tool(bool);
    void set_nodes_visible(bool);
    bool nodes_visible() const;

  protected:
    bool eventFilter(QObject *, QEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

  private:
    QTreeWidget *objects_ = nullptr;
    QLineEdit *name_ = nullptr;
    QLineEdit *fill_ = nullptr;
    QWidget *inspector_ = nullptr;
    QWidget *object_panel_ = nullptr;
    QWidget *node_panel_ = nullptr;
    QWidget *properties_ = nullptr;
    QLabel *empty_ = nullptr;
    QLabel *document_label_ = nullptr;
    QLabel *zoom_ = nullptr;
    QLabel *persistence_ = nullptr;
    QLabel *selection_ = nullptr;
    QToolButton *swatch_ = nullptr;
    QToolButton *rectangle_ = nullptr;
    QToolButton *select_ = nullptr;
    QToolButton *hand_ = nullptr;
    QToolButton *objects_toggle_ = nullptr;
    QToolButton *inspector_toggle_ = nullptr;
    QToolButton *nodes_toggle_ = nullptr;
    QSplitter *horizontal_ = nullptr;
    QSplitter *vertical_ = nullptr;
    NodeView *nodes_ = nullptr;
    QAction *undo_ = nullptr;
    QAction *redo_ = nullptr;
    std::map<QString, QLineEdit *> fields_;
    Id selection_id_;
    bool refreshing_ = false, rectangle_tool_ = false, creating_ = false;
    QPointF creation_start_;
    Id creation_id_, creation_source_;
    bool confirm_replace();
    void save_dialog(bool save_as);
    void execute(const std::function<void()> &);
    void apply(const std::vector<Command> &);
    void cancel_creation();
    void update_creation(QPointF);
    void update_panels();
    Ref fill_ref() const;
    void commit_fill(const QString &);
    void bind_numeric(QLineEdit *, QString);
};
} // namespace nect::studio
