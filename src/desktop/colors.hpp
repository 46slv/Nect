#pragma once
#include "nect/core.hpp"
#include <QObject>
#include <QPointer>
#include <QString>
#include <array>
#include <optional>
#include <vector>

class QDialog;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace nect::desktop {
class Window;

// Session UI state belongs here; named colors and all links belong to Session.
class ColorTools final : public QObject {
public:
    explicit ColorTools(Window& window);
    QPushButton* menu_button(const Ref& ref,QWidget* parent=nullptr);
    void show_manager();
    void refresh();
private:
    struct UsedColor {ColorValue value;std::vector<Ref> refs;};
    Window& window_;
    std::vector<ColorValue> history_;
    std::vector<UsedColor> used_colors_;
    QPointer<QDialog> manager_;
    QListWidget* named_=nullptr;
    QListWidget* used_=nullptr;
    QListWidget* usages_=nullptr;
    QListWidget* copied_=nullptr;
    QLabel* editor_status_=nullptr;
    QLineEdit* name_=nullptr;
    QLineEdit* hex_=nullptr;
    std::array<QLineEdit*,4> channels_{};
    QPushButton* apply_=nullptr;
    QPushButton* discard_=nullptr;
    QWidget* editor_=nullptr;
    QVBoxLayout* named_actions_=nullptr;
    QVBoxLayout* usage_actions_=nullptr;
    QString manager_session_,editor_session_;
    std::optional<NamedColor> editor_base_;
    ColorValue editor_value_;
    std::optional<Ref> usage_selection_;
    bool refreshing_=false;

    void check_target(const Ref& ref,const QString& session) const;
    void populate_menu(QMenu* menu,const Ref& ref,const QString& session);
    void copy(const Ref& ref,const QString& session,bool reference);
    void copy_value(const ColorValue& value);
    void remember(ColorValue value);
    void pick_named(const Ref& target,const QString& session);
    void create_named(const ColorValue& value);
    bool editor_dirty() const;
    void load_editor(const Id& id,const std::map<Ref,double>& values);
    void clear_editor();
    void apply_editor();
    void select_used(int row);
    void select_usage(int row);
    void refresh_history();
};
} // namespace nect::desktop
