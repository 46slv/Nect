#pragma once
#include "folder_library.hpp"
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
class QDockWidget;
class QDoubleSpinBox;
class QPushButton;
class QStringListModel;
class QScrollArea;
class QToolButton;
class QTabWidget;

namespace nect::desktop {
class ColorTools;
class Window : public QMainWindow {
public:
    explicit Window(QString recovery_directory, std::unique_ptr<FolderLibrary> folder_library = {});
    ~Window() override;
    Host host;
    Canvas* canvas;
    void refresh(bool project_canvas=true);
    void perform(const std::function<void()>& action);
protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    ColorTools* color_tools_;
    std::unique_ptr<FolderLibrary> folder_library_;
    QStringListModel* font_families_=nullptr;
    QPointer<QDialog> history_dialog_;
    QPointer<QDialog> utility_setup_dialog_;
    QListWidget* history_states_=nullptr;
    QLabel* history_status_=nullptr;
    QString history_session_;
    std::uint64_t history_revision_=0;
    void show_history();
    void refresh_history();
    void apply_builtin_effect_favorite(const BuiltinEffectTypeRefV1& effect,
        const QString& expected_session, const Id& expected_target, std::uint64_t expected_revision,
        std::uint64_t expected_generation);
    QTreeWidget* tree_;
    QListWidget* artboards_;
    QDockWidget* effects_dock_=nullptr;
    QLineEdit* effects_search_=nullptr;
    QListWidget* effects_catalog_=nullptr;
    QLabel* effects_target_=nullptr;
    QLabel* effects_status_=nullptr;
    QPushButton* effects_apply_=nullptr;
    QPushButton* effects_favorite_=nullptr;
    QWidget* effects_operations_=nullptr;
    QVBoxLayout* effects_operations_layout_=nullptr;
    QLineEdit* presets_search_=nullptr;
    QListWidget* presets_catalog_=nullptr;
    QLabel* presets_status_=nullptr;
    QPushButton* presets_save_=nullptr;
    QPushButton* presets_publish_=nullptr;
    QPushButton* presets_apply_=nullptr;
    QPushButton* presets_rename_=nullptr;
    QPushButton* presets_update_=nullptr;
    QPushButton* presets_delete_=nullptr;
    QTabWidget* effects_tabs_=nullptr;
    QString effects_session_;
    Id effects_target_id_;
    std::uint64_t effects_revision_=0;
    std::uint64_t effects_generation_=0;
    bool artboard_editing_=false;
    QWidget* inspector_;
    QScrollArea* inspector_scroll_;
    QLabel* status_;
    QLabel* breadcrumb_;
    QScrollArea* utility_scroll_=nullptr;
    QHBoxLayout* utility_layout_=nullptr;
    QToolButton* utility_guides_=nullptr;
    QToolButton* utility_grid_=nullptr;
    QToolButton* utility_snap_=nullptr;
    QDoubleSpinBox* utility_zoom_=nullptr;
    QLabel* utility_artboard_=nullptr;
    QAction* utility_snap_action_=nullptr;
    QAction* undo_;
    QAction* redo_;
    QAction* batch_rename_action_=nullptr;
    QAction* sort_paint_order_action_=nullptr;
    bool refreshing_=false;
    std::optional<std::pair<QString,std::uint64_t>> canvas_notification_;
    bool matrix_expanded_=false;
    std::map<Ref,double> inspector_values_;
    QString tree_signature_;
    QString inspector_context_;
    struct ExpressionDraft {
        QString session,source;
        std::uint64_t revision=0;
        bool replace_binding=false;
    };
    std::map<QByteArray,ExpressionDraft> expression_drafts_;
    std::optional<Ref> whip_target_;
    std::vector<Ref> whip_targets_;
    std::vector<Canvas::Selection> whip_selection_;
    Id whip_composition_,whip_artboard_;
    std::uint64_t whip_revision_=0;
    QString whip_session_;
    QPoint whip_start_;
    bool whip_dragged_=false;
    QWidget* whip_overlay_=nullptr;
    QPointer<QWidget> layout_preview_scope_;
    bool layout_preview_active_=false;
    bool layout_preview_invalid_=false;
    QString layout_preview_session_;
    std::uint64_t layout_preview_revision_=0;
    void cancel_whip();
    void reveal_whip_source();
    bool preview_layout_draft(const std::vector<Command>& commands,QWidget* scope);
    bool commit_layout_draft(const std::vector<Command>& commands,QWidget* scope);
    void cancel_layout_draft(bool refresh_canvas=true);
    bool layout_draft_current() const;
    bool reject_stale_layout_draft();
    void rebuild_inspector(bool use_canvas_values=false);
    void rebuild_effects_panel();
    void sync_tree_selection();
    void add_alignment_controls(QVBoxLayout* layout,const std::vector<Canvas::Selection>& selected);
    void add_multi_properties(QVBoxLayout* layout);
    void add_multi_text_weight(QVBoxLayout* layout,const std::vector<Canvas::Selection>& selected);
    void transform_selection();
    void distribute_selection(const std::string& axis,const std::string& reference="selection",std::optional<double> spacing={});
    void align_selection(const std::string& axis,const std::string& alignment,const std::string& reference);
    std::string alignment_reference_="selection";
    void add_property(QFormLayout* layout,const Ref& ref,const QString& label);
    void add_properties(QFormLayout* layout,const std::vector<Ref>& targets,const QString& label);
    void add_expression_editor(QVBoxLayout* layout,const QByteArray& key,const std::vector<Ref>& targets,const QString& label);
    void pick_source(std::vector<Ref> targets,bool relative=false);
    void save(bool choose);
    void export_png();
    void import_svg();
    double png_scale_=1;
    bool png_white_=false;
    QString png_path_;
    void add_curve();
    void add_primitive(const std::string& type);
    void add_text();
    void import_image(bool linked);
    void show_assets();
    void show_folder_library();
    void add_image_properties(QVBoxLayout*,const Object&);
    void add_text_properties(QVBoxLayout* layout,const Object& object);
    void add_transform_properties(QVBoxLayout* layout,const Object& object);
    void add_compositing_properties(QVBoxLayout* layout,const Object& object);
    std::vector<Id> selected_siblings(Id& parent,std::size_t minimum=2) const;
    void update_batch_rename_action();
    void update_sort_paint_order_action();
    void stack_selection(int direction,bool to_edge);
    void mask_selection(bool top);
    void batch_rename_selection();
    void sort_selection_by_name_paint_order();
    void put_selection_inside();
    void move_selection_out();
    void move_selection_to_next_folder();
    void move_selection_to_previous_folder();
    void move_selection_to_folder();
    void create_folder_from_selection();
    void selection_menu(const QPoint& global);
    void create_definition_from_selection();
    void rename_definition();
    void place_definition_instance();
    void set_instance_override();
    void reset_instance_override();
    void detach_instance();
    void delete_definition();
    void create_collection_from_selection();
    void browse_collections();
    void rename_collection();
    void add_selection_to_collection();
    void remove_selection_from_collection();
    void delete_collection();
    std::optional<Id> choose_collection(const QString& title,const std::vector<Collection>& collections);
    void choose_transform_parent();
    void edit_text_content(const Id& object);
    void convert_to_path();
    void add_operation(const std::string& type,bool radial=false);
    void reveal_operation(const Id& operation);
    void move_operation(const Id& object,const Id& operation,int direction);
    void add_stack(QVBoxLayout* layout,const Object& object);
    void add_gradient(QFormLayout* layout,const Object& object,const ShapeOperation& operation);
    void group_selection();
    void ungroup_selection();
    void create_folder();
    void duplicate_selection();
    void rebuild_artboards();
    void sync_utility_view_state();
    void update_utility_strip();
    void show_layout_setup(QWidget* anchor);
    void rebuild_layout_setup();
    void add_artboard(bool duplicate);
    void move_artboard(int direction);
    void edit_artboard(QVBoxLayout* layout);
};
}
