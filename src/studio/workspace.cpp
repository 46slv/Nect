#include "workspace.hpp"
#include "canvas.hpp"
#include "node_view.hpp"
#include "theme.hpp"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

namespace nect::studio {
namespace {
QWidget *panel() {
    auto *w = new QWidget;
    w->setObjectName("panel");
    return w;
}
QHBoxLayout *strip(QWidget *w) {
    w->setObjectName("strip");
    w->setFixedHeight(32);
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(4, 0, 4, 0);
    l->setSpacing(4);
    return l;
}
QToolButton *button(QString title, QString glyph = {}, QString id = {}) {
    auto *b = new QToolButton;
    b->setText(title);
    b->setToolTip(title);
    b->setAccessibleName(title);
    b->setIconSize({16, 16});
    if (!glyph.isEmpty()) {
        b->setIcon(icon(glyph));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    b->setObjectName(id);
    b->setFixedHeight(28);
    return b;
}
QLabel *section(QString text, QVBoxLayout *layout) {
    auto *l = new QLabel(text);
    l->setObjectName("section");
    layout->addWidget(l);
    return l;
}
QString qs(const std::string &s) {
    return QString::fromStdString(s);
}
void remember_draft(QLineEdit *e, const desktop::Host &host, const Id &target) {
    e->setProperty("draft-session", host.session_id);
    e->setProperty("draft-revision", QVariant::fromValue<qulonglong>(host.session.revision()));
    e->setProperty("draft-target", qs(target));
}
void check_draft(QLineEdit *e, const desktop::Host &host, const Id &target) {
    if (e->property("draft-session").toString() != host.session_id ||
        e->property("draft-revision").toULongLong() != host.session.revision() ||
        e->property("draft-target").toString() != qs(target))
        throw Error("STALE_INPUT", "The selection or document changed. Enter the value again.");
}
} // namespace
Workspace::Workspace(const QString &recovery, QWidget *parent)
    : QMainWindow(parent), host(recovery) {
    setObjectName("nect-studio");
    setWindowTitle("Nect — Untitled");
    resize(1440, 900);
    setMinimumSize(560, 440);
    auto *root = new QWidget;
    root->setObjectName("workspace");
    auto *root_layout = new QVBoxLayout(root);
    root_layout->setSizeConstraint(QLayout::SetNoConstraint);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);
    setCentralWidget(root);
    menuBar()->setFixedHeight(24);
    menuBar()->setNativeMenuBar(false);
    auto action = [this](QMenu *menu, QString title, QKeySequence key, std::function<void()> fn) {
        auto *a = menu->addAction(title);
        a->setShortcut(key);
        connect(a, &QAction::triggered, this, [this, fn] { execute(fn); });
        return a;
    };
    auto *file = menuBar()->addMenu("File");
    action(file, "New", QKeySequence::New, [this] {
        if (confirm_replace()) {
            cancel_creation();
            canvas->cancel_interaction();
            host.create_document();
            canvas->set_selection({});
            canvas->fit_artboard();
        }
    });
    action(file, "Open…", QKeySequence::Open, [this] {
        if (!confirm_replace())
            return;
        const auto path = QFileDialog::getOpenFileName(this, "Open Nect document", {},
                                                       "Nect document (*.nect);;All files (*)");
        if (!path.isEmpty())
            open_document(path);
    });
    file->addSeparator();
    action(file, "Save", QKeySequence::Save, [this] { save_dialog(false); });
    action(file, "Save As…", QKeySequence::SaveAs, [this] { save_dialog(true); });
    file->addSeparator();
    action(file, "Close", QKeySequence::Close, [this] { close(); });
    auto *edit = menuBar()->addMenu("Edit");
    undo_ = action(edit, "Undo", QKeySequence::Undo, [this] {
        cancel_creation();
        canvas->cancel_interaction();
        host.session.undo(host.session.revision());
        host.edited();
    });
    undo_->setIcon(icon("undo"));
    redo_ = action(edit, "Redo", QKeySequence::Redo, [this] {
        cancel_creation();
        canvas->cancel_interaction();
        host.session.redo(host.session.revision());
        host.edited();
    });
    redo_->setIcon(icon("redo"));
    edit->addSeparator();
    action(edit, "Delete selection", QKeySequence::Delete, [this] {
        const auto ids = canvas->selected_objects();
        if (!ids.empty())
            apply({DeleteObjects{ids}});
    });
    auto *view = menuBar()->addMenu("View");
    action(view, "Fit artboard", QKeySequence("Ctrl+0"), [this] { canvas->fit_artboard(); });
    auto *top = new QWidget;
    auto *top_layout = strip(top);
    top_layout->setSizeConstraint(QLayout::SetNoConstraint);
    top->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    root_layout->addWidget(top);
    auto *brand = new QLabel("Nect");
    brand->setObjectName("brand");
    top_layout->addWidget(brand);
    select_ = button("Select", "select", "tool-select");
    select_->setCheckable(true);
    select_->setChecked(true);
    top_layout->addWidget(select_);
    rectangle_ = button("Rectangle", "rectangle", "tool-rectangle");
    rectangle_->setCheckable(true);
    rectangle_->setToolTip("Drag a rectangle on the canvas");
    top_layout->addWidget(rectangle_);
    hand_ = button("Pan", "hand", "tool-pan");
    hand_->setCheckable(true);
    top_layout->addWidget(hand_);
    auto *more = button("Add", {}, "command-add");
    auto *add_menu = new QMenu(more);
    action(add_menu, "Rectangle at center", {}, [this] { add_rectangle(); });
    more->setMenu(add_menu);
    more->setPopupMode(QToolButton::InstantPopup);
    top_layout->addWidget(more);
    auto *add = menuBar()->addMenu("Add");
    action(add, "Rectangle at center", {}, [this] { add_rectangle(); });
    top_layout->addSpacing(12);
    auto *undo_button = button("Undo");
    undo_button->setDefaultAction(undo_);
    undo_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    undo_button->setAccessibleName("Undo");
    top_layout->addWidget(undo_button);
    auto *redo_button = button("Redo");
    redo_button->setDefaultAction(redo_);
    redo_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    redo_button->setAccessibleName("Redo");
    top_layout->addWidget(redo_button);
    top_layout->addStretch();
    objects_toggle_ = button("Objects");
    objects_toggle_->setCheckable(true);
    objects_toggle_->setChecked(true);
    top_layout->addWidget(objects_toggle_);
    inspector_toggle_ = button("Inspector");
    inspector_toggle_->setCheckable(true);
    inspector_toggle_->setChecked(true);
    top_layout->addWidget(inspector_toggle_);
    horizontal_ = new QSplitter;
    horizontal_->setHandleWidth(5);
    horizontal_->setChildrenCollapsible(false);
    root_layout->addWidget(horizontal_, 1);
    object_panel_ = panel();
    object_panel_->setMinimumWidth(170);
    auto *left = new QVBoxLayout(object_panel_);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(0);
    auto *object_header = new QWidget;
    auto *oh = strip(object_header);
    oh->addWidget(new QLabel("  Objects"));
    oh->addStretch();
    left->addWidget(object_header);
    objects_ = new QTreeWidget;
    objects_->setObjectName("objects");
    objects_->setHeaderHidden(true);
    objects_->setIndentation(14);
    objects_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    left->addWidget(objects_, 1);
    auto *left_hint = new QLabel("  Select objects on canvas or here");
    left_hint->setObjectName("muted");
    left_hint->setFixedHeight(32);
    left->addWidget(left_hint);
    horizontal_->addWidget(object_panel_);
    auto *center = panel();
    center->setMinimumWidth(220);
    auto *cl = new QVBoxLayout(center);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    auto *canvas_header = new QWidget;
    auto *ch = strip(canvas_header);
    document_label_ = new QLabel;
    document_label_->setObjectName("document");
    document_label_->setMinimumWidth(75);
    ch->addWidget(document_label_, 1);
    nodes_toggle_ = button("Nodes", "node", "toggle-nodes");
    nodes_toggle_->setCheckable(true);
    ch->addWidget(nodes_toggle_);
    zoom_ = new QLabel("100%");
    zoom_->setMinimumWidth(46);
    zoom_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ch->addWidget(zoom_);
    auto *fit = button("Fit", "fit", "view-fit");
    ch->addWidget(fit);
    cl->addWidget(canvas_header);
    vertical_ = new QSplitter(Qt::Vertical);
    vertical_->setHandleWidth(5);
    vertical_->setChildrenCollapsible(false);
    cl->addWidget(vertical_, 1);
    canvas = new desktop::Canvas(host.session);
    canvas->setObjectName("canvas");
    canvas->set_view_colors(QColor("#191b1e"), QColor("#78a8ff"));
    canvas->installEventFilter(this);
    canvas->set_session_identity_provider([this] { return host.session_id; });
    vertical_->addWidget(canvas);
    node_panel_ = panel();
    auto *nl = new QVBoxLayout(node_panel_);
    nl->setContentsMargins(0, 0, 0, 0);
    nl->setSpacing(0);
    auto *nhw = new QWidget;
    auto *nh = strip(nhw);
    nh->addWidget(new QLabel("  Object pipeline"));
    auto *note = new QLabel("Topology view");
    note->setObjectName("muted");
    nh->addStretch();
    nh->addWidget(note);
    nl->addWidget(nhw);
    nodes_ = new NodeView(host.session);
    auto *node_scroll = new QScrollArea;
    node_scroll->setWidgetResizable(true);
    node_scroll->setWidget(nodes_);
    nl->addWidget(node_scroll, 1);
    vertical_->addWidget(node_panel_);
    node_panel_->hide();
    horizontal_->addWidget(center);
    inspector_ = panel();
    inspector_->setMinimumWidth(260);
    auto *il = new QVBoxLayout(inspector_);
    il->setContentsMargins(0, 0, 0, 0);
    il->setSpacing(0);
    auto *ihw = new QWidget;
    auto *ih = strip(ihw);
    ih->addWidget(new QLabel("  Inspector"));
    ih->addStretch();
    il->addWidget(ihw);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = panel();
    auto *content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 12);
    content_layout->setSpacing(0);
    empty_ = new QLabel("Select an object to edit\nits properties.");
    empty_->setObjectName("muted");
    empty_->setContentsMargins(12, 16, 12, 16);
    content_layout->addWidget(empty_);
    properties_ = panel();
    auto *pl = new QVBoxLayout(properties_);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    section("Object", pl);
    name_ = new QLineEdit;
    name_->setObjectName("object-name");
    name_->setAccessibleName("Object name");
    auto *name_row = new QHBoxLayout;
    name_row->setContentsMargins(12, 0, 12, 4);
    name_row->addWidget(name_);
    pl->addLayout(name_row);
    section("Transform", pl);
    auto *grid = new QGridLayout;
    grid->setContentsMargins(12, 0, 12, 8);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    const std::vector<std::pair<QString, QString>> numeric{{"X", "transform.tx"},
                                                           {"Y", "transform.ty"},
                                                           {"W", "generator.width"},
                                                           {"H", "generator.height"}};
    for (int i = 0; i < 4; ++i) {
        auto *cell = new QWidget;
        auto *row = new QHBoxLayout(cell);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto *label = new QLabel(numeric[i].first);
        label->setFixedWidth(14);
        row->addWidget(label);
        auto *input = new QLineEdit;
        input->setAlignment(Qt::AlignRight);
        input->setMinimumWidth(45);
        input->setObjectName("value-" + numeric[i].first.toLower());
        input->setAccessibleName(numeric[i].first == "X"   ? "Translation X"
                                 : numeric[i].first == "Y" ? "Translation Y"
                                 : numeric[i].first == "W" ? "Width"
                                                           : "Height");
        input->installEventFilter(this);
        row->addWidget(input, 1);
        grid->addWidget(cell, i / 2, i % 2);
        fields_[numeric[i].second] = input;
        bind_numeric(input, numeric[i].second);
    }
    pl->addLayout(grid);
    section("Appearance", pl);
    auto *fill_row = new QHBoxLayout;
    fill_row->setContentsMargins(12, 0, 12, 8);
    fill_row->setSpacing(8);
    fill_row->addWidget(new QLabel("Fill"));
    swatch_ = button("", {}, "fill-swatch");
    swatch_->setFixedWidth(28);
    swatch_->setAccessibleName("Current fill color");
    fill_row->addWidget(swatch_);
    fill_ = new QLineEdit;
    fill_->setObjectName("fill-value");
    fill_->setAccessibleName("Fill hexadecimal color");
    fill_->installEventFilter(this);
    fill_row->addWidget(fill_, 1);
    pl->addLayout(fill_row);
    auto *colors = new QHBoxLayout;
    colors->setContentsMargins(12, 0, 12, 8);
    colors->setSpacing(6);
    for (const auto *color :
         {"#e3e6ea", "#28303a", "#7098b6", "#7fa28d", "#c99469", "#b7797b", "#9c8bb4"}) {
        auto *chip = button("", {}, "color-chip");
        chip->setFixedSize(26, 24);
        chip->setToolTip(color);
        chip->setAccessibleName(QString("Set fill ") + color);
        chip->setStyleSheet(
            QString("QToolButton{background:%1;border:1px solid "
                    "#555b63;border-radius:3px;}QToolButton:hover{border:2px solid #e4e7eb;}")
                .arg(color));
        connect(chip, &QToolButton::clicked, this,
                [this, color] { execute([&] { commit_fill(color); }); });
        colors->addWidget(chip);
    }
    colors->addStretch();
    pl->addLayout(colors);
    content_layout->addWidget(properties_);
    content_layout->addStretch();
    scroll->setWidget(content);
    il->addWidget(scroll, 1);
    horizontal_->addWidget(inspector_);
    horizontal_->setStretchFactor(1, 1);
    horizontal_->setSizes({208, 942, 280});
    statusBar()->setFixedHeight(22);
    statusBar()->setSizeGripEnabled(false);
    selection_ = new QLabel;
    statusBar()->addWidget(selection_, 1);
    persistence_ = new QLabel;
    persistence_->setObjectName("persistence");
    statusBar()->addPermanentWidget(persistence_);
    connect(select_, &QToolButton::clicked, this, [this] {
        set_rectangle_tool(false);
        canvas->set_hand_mode(false);
        hand_->setChecked(false);
    });
    connect(rectangle_, &QToolButton::clicked, this,
            [this](bool checked) { set_rectangle_tool(checked); });
    connect(hand_, &QToolButton::clicked, this, [this](bool checked) {
        set_rectangle_tool(false);
        canvas->set_hand_mode(checked);
        select_->setChecked(!checked);
        hand_->setChecked(checked);
    });
    connect(fit, &QToolButton::clicked, canvas, &desktop::Canvas::fit_artboard);
    connect(nodes_toggle_, &QToolButton::toggled, this, &Workspace::set_nodes_visible);
    connect(objects_toggle_, &QToolButton::toggled, this, [this](bool on) {
        if (width() < 800 && on)
            inspector_toggle_->setChecked(false);
        update_panels();
    });
    connect(inspector_toggle_, &QToolButton::toggled, this, [this](bool on) {
        if (width() < 800 && on)
            objects_toggle_->setChecked(false);
        update_panels();
    });
    connect(objects_, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (refreshing_)
            return;
        std::vector<desktop::Canvas::Selection> ids;
        for (auto *row : objects_->selectedItems()) {
            const auto id = row->data(0, Qt::UserRole).toString();
            if (!id.isEmpty())
                ids.push_back({id.toStdString(), {}});
        }
        canvas->set_selections(std::move(ids));
    });
    name_->installEventFilter(this);
    connect(name_, &QLineEdit::textEdited, this,
            [this] { remember_draft(name_, host, selection_id_); });
    connect(name_, &QLineEdit::editingFinished, this, [this] {
        if (!name_->isModified())
            return;
        execute([this] {
            check_draft(name_, host, selection_id_);
            apply({Rename{selection_id_, name_->text().toStdString()}});
        });
    });
    connect(fill_, &QLineEdit::textEdited, this,
            [this] { remember_draft(fill_, host, selection_id_); });
    connect(swatch_, &QToolButton::clicked, this, [this] {
        fill_->setFocus();
        fill_->selectAll();
    });
    connect(fill_, &QLineEdit::editingFinished, this, [this] {
        if (!fill_->isModified())
            return;
        execute([this] {
            check_draft(fill_, host, selection_id_);
            commit_fill(fill_->text());
        });
    });
    canvas->selection_changed = [this] { refresh(); };
    canvas->document_changed = [this] { host.edited(); };
    canvas->error = [this](QString e) { statusBar()->showMessage(e, 9000); };
    canvas->zoom_changed = [this](double z) {
        zoom_->setText(QString::number(z * 100, 'f', 0) + "%");
    };
    host.changed = [this] { refresh(); };
    host.status_changed = [this] {
        persistence_->setText(host.save_status);
        document_label_->setText(
            (host.dirty() ? "●  " : "") +
            (host.file_path.isEmpty() ? "Untitled" : QFileInfo(host.file_path).fileName()));
    };
    nodes_->activated = [this](QString role) {
        if (role == "fill")
            fill_->setFocus();
        else if (role == "shape")
            fields_.at("generator.width")->setFocus();
        else if (role == "transform")
            fields_.at("transform.tx")->setFocus();
    };
    refresh();
    QTimer::singleShot(0, this, [this] {
        canvas->fit_artboard();
        update_panels();
    });
}
Workspace::~Workspace() {
    cancel_creation();
    canvas->cancel_interaction();
    host.changed = {};
    host.status_changed = {};
    canvas->selection_changed = {};
    canvas->document_changed = {};
    delete takeCentralWidget();
}
void Workspace::execute(const std::function<void()> &fn) {
    try {
        fn();
        statusBar()->clearMessage();
    } catch (const std::exception &e) {
        statusBar()->showMessage(QString::fromUtf8(e.what()), 10000);
        refresh();
    }
}
void Workspace::apply(const std::vector<Command> &commands) {
    cancel_creation();
    canvas->cancel_interaction();
    host.session.apply(commands, host.session.revision());
    host.edited();
}
void Workspace::add_rectangle() {
    cancel_creation();
    canvas->cancel_interaction();
    const auto &comp = *std::find_if(
        host.session.document().compositions.begin(), host.session.document().compositions.end(),
        [this](const auto &c) { return c.id == canvas->active_composition(); });
    const auto board = evaluate_artboard(comp, canvas->active_artboard());
    const auto id = desktop::new_id();
    auto source = default_primitive(desktop::new_id(), "nect.shape.rectangle");
    source.parameters.at("center_x").literal = board.x + board.width / 2;
    source.parameters.at("center_y").literal = board.y + board.height / 2;
    auto fill = default_operation(id + "-fill", "nect.paint.fill");
    fill.parameters.at("r").literal = 0.61;
    fill.parameters.at("g").literal = 0.66;
    fill.parameters.at("b").literal = 0.71;
    host.session.apply(
        {CreatePrimitive{comp.id,
                         {},
                         id,
                         "Rectangle " + std::to_string(host.session.document().objects.size() + 1),
                         source},
         AddOperation{id, fill, 0}, Set{{id, {}, "stroke.width"}, 0}},
        host.session.revision());
    canvas->set_selection(id);
    host.edited();
    canvas->setFocus();
}
void Workspace::select_object(const Id &id) {
    canvas->set_selection(id);
    refresh();
}
void Workspace::save_document(const QString &path) {
    cancel_creation();
    canvas->cancel_interaction();
    host.save(path);
    statusBar()->clearMessage();
    refresh();
}
void Workspace::open_document(const QString &path) {
    cancel_creation();
    canvas->cancel_interaction();
    host.open(path);
    canvas->set_selection({});
    canvas->fit_artboard();
    refresh();
}
bool Workspace::confirm_replace() {
    if (!host.dirty() || host.session.document().objects.empty())
        return true;
    const auto choice = QMessageBox::question(
        this, "Unsaved document", "Save this document before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel)
        return false;
    if (choice == QMessageBox::Discard)
        return true;
    save_dialog(false);
    return !host.dirty();
}
void Workspace::save_dialog(bool save_as) {
    auto path = host.file_path;
    if (save_as || path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Save Nect document",
                                            path.isEmpty() ? "Untitled.nect" : path,
                                            "Nect document (*.nect)");
    if (!path.isEmpty())
        save_document(path);
}
void Workspace::bind_numeric(QLineEdit *input, QString field) {
    connect(input, &QLineEdit::textEdited, this,
            [this, input] { remember_draft(input, host, selection_id_); });
    connect(input, &QLineEdit::editingFinished, this, [this, input, field] {
        if (!input->isModified())
            return;
        execute([this, input, field] {
            check_draft(input, host, selection_id_);
            bool ok = false;
            const auto value = input->text().toDouble(&ok);
            if (!ok || !std::isfinite(value))
                throw Error("INVALID_VALUE", "Enter a finite number");
            apply({Set{{selection_id_, {}, field.toStdString()}, value}});
        });
    });
}
Ref Workspace::fill_ref() const {
    const auto found = host.session.document().objects.find(selection_id_);
    if (found != host.session.document().objects.end())
        for (const auto &e : found->second.stack)
            if (const auto *op = &e; op->type == "nect.paint.fill" && !op->gradient)
                return {selection_id_, {}, "op." + op->id + ".color"};
    return {};
}
void Workspace::commit_fill(const QString &text) {
    const QColor c(text);
    if (!c.isValid())
        throw Error("INVALID_COLOR", "Enter a color such as #7098B6");
    const auto ref = fill_ref();
    if (ref.object.empty())
        throw Error("NO_SOLID_FILL", "This object has no solid Fill");
    apply({SetColor{
        ref,
        ColorValue{"srgb", "srgb", "straight", {c.redF(), c.greenF(), c.blueF(), c.alphaF()}}}});
}
void Workspace::refresh() {
    if (refreshing_)
        return;
    refreshing_ = true;
    canvas->refresh();
    const auto &d = host.session.document();
    const auto selected = canvas->selected_objects();
    selection_id_ = selected.size() == 1 ? selected.front() : Id{};
    {
        const auto scroll = objects_->verticalScrollBar()->value();
        QSignalBlocker block(objects_);
        objects_->clear();
        std::function<void(QTreeWidgetItem *, const Id &)> add = [&](QTreeWidgetItem *parent,
                                                                     const Id &id) {
            const auto &o = d.objects.at(id);
            auto *item = new QTreeWidgetItem(parent, QStringList{qs(o.name)});
            item->setData(0, Qt::UserRole, qs(id));
            item->setIcon(0, icon(o.source ? "rectangle" : "node"));
            item->setSelected(std::find(selected.begin(), selected.end(), id) != selected.end());
            for (const auto &child : o.children)
                add(item, child);
            item->setExpanded(true);
        };
        for (const auto &c : d.compositions) {
            auto *item = new QTreeWidgetItem(
                objects_, QStringList{qs(c.name.empty() ? "Composition" : c.name)});
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            for (const auto &id : c.roots)
                add(item, id);
            item->setExpanded(true);
        }
        objects_->verticalScrollBar()->setValue(scroll);
    }
    const auto found = d.objects.find(selection_id_);
    const bool one = found != d.objects.end();
    properties_->setVisible(one);
    empty_->setVisible(!one);
    empty_->setText(selected.size() > 1 ? QString::number(selected.size()) +
                                              " objects selected\nSelect one to edit properties."
                                        : "Select an object to edit\nits properties.");
    if (one) {
        name_->setText(qs(found->second.name));
        name_->setModified(false);
        const auto &values = canvas->evaluated_values();
        for (const auto &[field, input] : fields_) {
            const Ref ref{selection_id_, {}, field.toStdString()};
            const auto value = values.find(ref);
            input->setEnabled(value != values.end());
            input->setText(value == values.end() ? QString()
                                                 : QString::number(value->second, 'f', 2));
            input->setModified(false);
            if (value != values.end()) {
                try {
                    const auto &scalar = nect::property(d, ref);
                    input->setReadOnly(scalar.binding.has_value() || scalar.expression.has_value());
                    input->setToolTip(input->isReadOnly()
                                          ? "Driven property — source preserved"
                                          : "Enter a value; Enter confirms, Escape cancels");
                } catch (...) {
                    input->setReadOnly(true);
                }
            }
        }
        const auto ref = fill_ref();
        fill_->setEnabled(!ref.object.empty());
        swatch_->setEnabled(!ref.object.empty());
        if (!ref.object.empty()) {
            const auto value = color_value(d, ref, values);
            const auto c =
                QColor::fromRgbF(value.rgba[0], value.rgba[1], value.rgba[2], value.rgba[3]);
            fill_->setText(c.name(c.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb).toUpper());
            swatch_->setStyleSheet(
                QString("QToolButton{background:%1;border:1px solid #737982;border-radius:4px;}")
                    .arg(c.name()));
        } else
            fill_->setText("No solid fill");
        fill_->setModified(false);
    }
    nodes_->set_object(selection_id_);
    undo_->setEnabled(host.session.can_undo());
    redo_->setEnabled(host.session.can_redo());
    selection_->setText(selected.empty()
                            ? "Rectangle: drag to create   |   Wheel: zoom   |   Middle drag: pan"
                            : QString::number(selected.size()) +
                                  " selected   |   Drag to move   |   Arrow keys: nudge");
    const auto title =
        host.file_path.isEmpty() ? QString("Untitled") : QFileInfo(host.file_path).fileName();
    document_label_->setText((host.dirty() ? "●  " : "") + title);
    setWindowTitle("Nect — " + title + (host.dirty() ? " *" : ""));
    persistence_->setText(host.save_status);
    refreshing_ = false;
}
void Workspace::set_rectangle_tool(bool enabled) {
    cancel_creation();
    canvas->cancel_interaction();
    rectangle_tool_ = enabled;
    rectangle_->setChecked(enabled);
    select_->setChecked(!enabled);
    hand_->setChecked(false);
    canvas->set_hand_mode(false);
    canvas->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}
void Workspace::cancel_creation() {
    if (creating_) {
        host.session.cancel_gesture();
        creating_ = false;
        canvas->releaseMouse();
        canvas->refresh();
    }
}
void Workspace::update_creation(QPointF point) {
    const QRectF area = QRectF(creation_start_, point).normalized();
    if (area.width() < 0.1 || area.height() < 0.1)
        return;
    auto source = default_primitive(creation_source_, "nect.shape.rectangle");
    source.parameters.at("center_x").literal = area.center().x();
    source.parameters.at("center_y").literal = area.center().y();
    source.parameters.at("width").literal = area.width();
    source.parameters.at("height").literal = area.height();
    auto fill = default_operation(creation_id_ + "-fill", "nect.paint.fill");
    fill.parameters.at("r").literal = 0.61;
    fill.parameters.at("g").literal = 0.66;
    fill.parameters.at("b").literal = 0.71;
    host.session.update_gesture(
        {CreatePrimitive{canvas->active_composition(),
                         {},
                         creation_id_,
                         "Rectangle " + std::to_string(host.session.document().objects.size() + 1),
                         source},
         AddOperation{creation_id_, fill, 0}, Set{{creation_id_, {}, "stroke.width"}, 0}});
    canvas->refresh();
}
bool Workspace::eventFilter(QObject *target, QEvent *event) {
    if (auto *input = qobject_cast<QLineEdit *>(target);
        input && input->objectName().startsWith("value-") &&
        (event->type() == QEvent::FocusIn ||
         (event->type() == QEvent::FocusOut && !input->isModified()))) {
        for (const auto &[field, control] : fields_)
            if (control == input && !selection_id_.empty()) {
                const auto &values = canvas->evaluated_values();
                const auto it = values.find({selection_id_, {}, field.toStdString()});
                if (it != values.end()) {
                    input->setText(QString::number(it->second,
                                                   event->type() == QEvent::FocusIn ? 'g' : 'f',
                                                   event->type() == QEvent::FocusIn ? 17 : 2));
                    input->setModified(false);
                }
            }
    }
    if (auto *field = qobject_cast<QLineEdit *>(target);
        field && event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        field->setModified(false);
        refresh();
        field->clearFocus();
        return true;
    }
    if (target != canvas)
        return QMainWindow::eventFilter(target, event);
    if (event->type() == QEvent::FocusOut && creating_) {
        cancel_creation();
        return false;
    }
    if (event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        cancel_creation();
        set_rectangle_tool(false);
        return true;
    }
    if (!rectangle_tool_)
        return QMainWindow::eventFilter(target, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *e = static_cast<QMouseEvent *>(event);
        if (e->button() != Qt::LeftButton)
            return false;
        execute([&] {
            canvas->cancel_interaction();
            creation_start_ = canvas->document_position(e->position());
            creation_id_ = desktop::new_id();
            creation_source_ = desktop::new_id();
            host.session.begin_gesture(host.session.revision());
            creating_ = true;
            canvas->setFocus();
            canvas->grabMouse();
        });
        return true;
    }
    if (event->type() == QEvent::MouseMove && creating_) {
        auto *e = static_cast<QMouseEvent *>(event);
        execute([&] { update_creation(canvas->document_position(e->position())); });
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && creating_) {
        auto *e = static_cast<QMouseEvent *>(event);
        if (e->button() != Qt::LeftButton)
            return false;
        execute([&] {
            const auto point = canvas->document_position(e->position());
            const auto area = QRectF(creation_start_, point).normalized();
            if (area.width() < 2 || area.height() < 2) {
                cancel_creation();
                return;
            }
            update_creation(point);
            host.session.commit_gesture();
            creating_ = false;
            canvas->releaseMouse();
            canvas->set_selection(creation_id_);
            host.edited();
            set_rectangle_tool(false);
        });
        return true;
    }
    return QMainWindow::eventFilter(target, event);
}
void Workspace::set_nodes_visible(bool visible) {
    node_panel_->setVisible(visible);
    if (nodes_toggle_->isChecked() != visible)
        nodes_toggle_->setChecked(visible);
    if (visible)
        vertical_->setSizes({600, 210});
}
bool Workspace::nodes_visible() const {
    return !node_panel_->isHidden();
}
void Workspace::update_panels() {
    object_panel_->setVisible(objects_toggle_->isChecked());
    inspector_->setVisible(inspector_toggle_->isChecked());
}
void Workspace::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!objects_toggle_)
        return;
    if (width() < 1200)
        objects_toggle_->setChecked(false);
    if (width() < 800)
        objects_toggle_->setChecked(false);
    hand_->setVisible(width() >= 800);
    if (auto *add = findChild<QToolButton *>("command-add"))
        add->setVisible(width() >= 900);
    update_panels();
}
void Workspace::closeEvent(QCloseEvent *e) {
    try {
        cancel_creation();
        canvas->cancel_interaction();
        if (!confirm_replace()) {
            e->ignore();
            return;
        }
        host.flush();
        e->accept();
    } catch (const std::exception &error) {
        statusBar()->showMessage(QString::fromUtf8(error.what()), 10000);
        e->ignore();
    }
}
} // namespace nect::studio
