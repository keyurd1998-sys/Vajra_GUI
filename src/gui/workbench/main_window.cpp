#include "vajra/gui/workbench/main_window.hpp"
#include "vajra/gui/workbench/dark_theme.hpp"
#include "vajra/gui/canvas/wire_item.hpp"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QPushButton>
#include <QKeySequence>
#include <QActionGroup>
#include <QApplication>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QSvgGenerator>
#include <QFileInfo>
#include <QPageSize>
#include <QPageLayout>
#include <QTimer>
#include <QShowEvent>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

namespace vajra::gui {

MainWindow::MainWindow(SchematicDesign design, QWidget* parent)
    : QMainWindow(parent), design_(std::move(design)) {
    navigator_ = new HierarchyNavigator(design_, this);

    setup_ui();

    // Connect scene signals
    connect(scene_, &QGraphicsScene::selectionChanged, this, &MainWindow::on_selection_changed);
    connect(scene_, &SchematicScene::drill_down_requested, this, &MainWindow::on_drill_down);
    connect(scene_, &SchematicScene::top_box_expand_requested, this, &MainWindow::on_top_box_expand);

    // Connect view signals
    connect(view_, &SchematicView::ascend_requested, this, &MainWindow::on_ascend_hierarchy);
    connect(view_, &SchematicView::reset_to_top_requested, this, &MainWindow::on_reset_to_top);
    connect(view_, &SchematicView::zoom_changed, this, &MainWindow::on_zoom_changed);
    connect(view_, &SchematicView::mouse_moved_scene, this, &MainWindow::on_mouse_moved_scene);
    connect(view_, &SchematicView::drill_down_requested, this, &MainWindow::on_drill_down);
    connect(view_, &SchematicView::tool_mode_changed, this, &MainWindow::on_tool_mode_changed);
    connect(view_, &SchematicView::export_requested, this, [this]() { export_schematic(); });

    // Connect navigator
    connect(navigator_, &HierarchyNavigator::module_changed, this, &MainWindow::on_module_changed);

    // Connect hierarchy tree
    connect(tree_widget_->tree(), &QTreeWidget::itemClicked, this, &MainWindow::on_hierarchy_item_clicked);
    connect(tree_widget_->tree(), &QTreeWidget::itemDoubleClicked, this, &MainWindow::on_hierarchy_item_double_clicked);
    connect(tree_widget_, &HierarchyTree::module_drilldown_requested, this, &MainWindow::display_module);

    // Connect property inspector
    connect(inspector_widget_, &PropertyInspector::net_highlight_requested, this, &MainWindow::on_net_highlight_requested);

    show_top_module_box();
    resize(1440, 900);
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    scene_ = new SchematicScene(this);
    view_ = new SchematicView(scene_, this);
    setCentralWidget(view_);

    setup_menus();
    setup_toolbar();
    setup_docks();
    setup_statusbar();
    apply_dark_theme();
}

void MainWindow::setup_menus() {
    auto* mb = menuBar();

    // File Menu
    auto* file_menu = mb->addMenu("&File");
    act_export_ = file_menu->addAction("&Export Schematic...");
    act_export_->setShortcut(QKeySequence("Ctrl+E"));
    act_export_->setStatusTip("Export current opened schematic in canvas (PNG, PDF, SVG, JPEG, BMP)");
    connect(act_export_, &QAction::triggered, this, [this]() { export_schematic(); });

    file_menu->addSeparator();
    auto* act_hide = file_menu->addAction("&Hide GUI");
    act_hide->setShortcut(QKeySequence("Ctrl+W"));
    act_hide->setStatusTip("Close/hide GUI window while keeping Vajra shell session active");
    connect(act_hide, &QAction::triggered, this, &MainWindow::hide_gui_only);

    auto* act_exit = file_menu->addAction("&Exit Vajra");
    act_exit->setShortcut(QKeySequence("Ctrl+Q"));
    act_exit->setStatusTip("Exit both GUI and active Vajra shell session");
    connect(act_exit, &QAction::triggered, this, &MainWindow::exit_vajra_completely);

    // View Menu
    auto* view_menu = mb->addMenu("&View");

    // Tool Mode selection
    auto* tool_group = new QActionGroup(this);
    tool_group->setExclusive(true);

    act_tool_select_ = view_menu->addAction("&Select Tool");
    act_tool_select_->setCheckable(true);
    act_tool_select_->setChecked(true);
    act_tool_select_->setShortcut(QKeySequence(Qt::Key_S));
    tool_group->addAction(act_tool_select_);
    connect(act_tool_select_, &QAction::triggered, this, [this]() {
        view_->set_tool_mode(CanvasToolMode::SELECT);
    });

    act_tool_hand_ = view_menu->addAction("&Hand (Pan) Tool");
    act_tool_hand_->setCheckable(true);
    act_tool_hand_->setShortcut(QKeySequence(Qt::Key_H));
    tool_group->addAction(act_tool_hand_);
    connect(act_tool_hand_, &QAction::triggered, this, [this]() {
        view_->set_tool_mode(CanvasToolMode::HAND);
    });

    view_menu->addSeparator();

    act_fit_ = view_menu->addAction("&Fit to View");
    act_fit_->setShortcut(QKeySequence(Qt::Key_F));
    connect(act_fit_, &QAction::triggered, view_, [this]() { view_->fit_to_all(); });

    act_zoom_in_ = view_menu->addAction("Zoom &In");
    act_zoom_in_->setShortcuts({QKeySequence::ZoomIn, QKeySequence(Qt::Key_Plus)});
    connect(act_zoom_in_, &QAction::triggered, view_, [this]() { view_->zoom_in(); });

    act_zoom_out_ = view_menu->addAction("Zoom &Out");
    act_zoom_out_->setShortcuts({QKeySequence::ZoomOut, QKeySequence(Qt::Key_Minus)});
    connect(act_zoom_out_, &QAction::triggered, view_, [this]() { view_->zoom_out(); });

    act_reset_zoom_ = view_menu->addAction("&Reset Zoom");
    act_reset_zoom_->setShortcut(QKeySequence("Ctrl+0"));
    connect(act_reset_zoom_, &QAction::triggered, view_, [this]() { view_->reset_zoom(); });

    view_menu->addSeparator();
    auto* act_grid = view_menu->addAction("Toggle &Grid");
    act_grid->setShortcut(QKeySequence(Qt::Key_G));
    connect(act_grid, &QAction::triggered, this, [this]() {
        scene_->set_grid_visible(!scene_->is_grid_visible());
    });

    act_toggle_sidebars_ = view_menu->addAction("Toggle &Sidebars");
    act_toggle_sidebars_->setShortcut(QKeySequence(Qt::Key_B));
    connect(act_toggle_sidebars_, &QAction::triggered, this, &MainWindow::toggle_sidebars);

    // Navigate Menu
    auto* nav_menu = mb->addMenu("&Navigate");
    act_hier_up_ = nav_menu->addAction("Hierarchy &Up");
    act_hier_up_->setShortcuts({QKeySequence(Qt::Key_U), QKeySequence(Qt::Key_Backspace)});
    connect(act_hier_up_, &QAction::triggered, this, &MainWindow::on_ascend_hierarchy);

    act_hier_top_ = nav_menu->addAction("&Top Level");
    act_hier_top_->setShortcut(QKeySequence(Qt::Key_Home));
    connect(act_hier_top_, &QAction::triggered, this, &MainWindow::on_reset_to_top);

    act_box_view_ = nav_menu->addAction("Top &Block Symbol");
    act_box_view_->setShortcut(QKeySequence("Ctrl+T"));
    connect(act_box_view_, &QAction::triggered, this, &MainWindow::show_top_module_box);

    // Help Menu
    auto* help_menu = mb->addMenu("&Help");
    auto* act_shortcuts = help_menu->addAction("&Keyboard & Mouse Shortcuts");
    connect(act_shortcuts, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "Vajra Navigation & Shortcuts",
            "S: Select Tool (Pointer mode)\n"
            "H: Hand / Pan Tool Mode (drag canvas)\n"
            "Spacebar (hold): Temporary Hand / Pan Mode\n"
            "Two-finger Touchpad: Smooth Pan (up/down/left/right)\n"
            "Two-finger Touchscreen: Fluid Pan & Pinch-to-Zoom\n"
            "Arrow Keys: Pan canvas (Shift for 3x speed)\n"
            "Ctrl + Wheel / Pinch: Smooth Zoom\n"
            "F: Fit all objects into canvas\n"
            "+ / -: Zoom In / Zoom Out\n"
            "Ctrl+0: Reset Zoom (100%)\n"
            "Double-click background: Fit all to view\n"
            "Double-click module/block: Drill down into hierarchy\n"
            "U / Backspace: Ascend hierarchy (back to top block)\n"
            "Home: Navigate to top module logic\n"
            "Ctrl+T: View top module block symbol\n"
            "B: Toggle sidebars (maximize canvas)\n"
            "G: Toggle canvas grid dots");
    });

    auto* act_about = help_menu->addAction("&About Vajra");
    connect(act_about, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About Vajra",
            "Vajra (वज्र) EDA Schematic Workbench\n"
            "High-Performance RTLIL & Netlist Visualization Engine\n"
            "Advanced EDA Schematic Viewer and DFT Analysis Suite");
    });
}

void MainWindow::setup_toolbar() {
    auto* tb = addToolBar("Main Toolbar");
    tb->setMovable(false);

    tb->addAction(act_tool_select_);
    tb->addAction(act_tool_hand_);
    tb->addSeparator();

    tb->addAction(act_fit_);
    tb->addAction(act_zoom_in_);
    tb->addAction(act_zoom_out_);
    tb->addSeparator();

    tb->addAction(act_export_);
    tb->addSeparator();

    tb->addAction(act_toggle_sidebars_);
    tb->addSeparator();

    breadcrumb_label_ = new QLabel(this);
    breadcrumb_label_->setStyleSheet("color: #FFFFFF; font-weight: bold; padding-left: 8px;");
    tb->addWidget(breadcrumb_label_);
}

void MainWindow::setup_docks() {
    // Left: Hierarchy Tree Dock
    dock_hierarchy_ = new QDockWidget("Hierarchy Tree", this);
    dock_hierarchy_->setObjectName("HierarchyDock");
    dock_hierarchy_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    tree_widget_ = new HierarchyTree(dock_hierarchy_);
    dock_hierarchy_->setWidget(tree_widget_);
    addDockWidget(Qt::LeftDockWidgetArea, dock_hierarchy_);

    // Right: Property Inspector Dock
    dock_inspector_ = new QDockWidget("Property Inspector", this);
    dock_inspector_->setObjectName("InspectorDock");
    dock_inspector_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspector_widget_ = new PropertyInspector(dock_inspector_);
    dock_inspector_->setWidget(inspector_widget_);
    addDockWidget(Qt::RightDockWidgetArea, dock_inspector_);
}

void MainWindow::setup_statusbar() {
    auto* sb = statusBar();
    status_label_ = new QLabel("Ready", this);
    status_label_->setStyleSheet("color: #FFFFFF; font-weight: bold; margin-right: 12px;");
    sb->addWidget(status_label_);

    module_label_ = new QLabel("Module: -", this);
    module_label_->setStyleSheet("color: #CCCCCC; margin-right: 12px;");
    sb->addWidget(module_label_);

    cells_label_ = new QLabel("Cells: 0", this);
    cells_label_->setStyleSheet("color: #CCCCCC; margin-right: 12px;");
    sb->addWidget(cells_label_);

    nets_label_ = new QLabel("Nets: 0", this);
    nets_label_->setStyleSheet("color: #CCCCCC; margin-right: 12px;");
    sb->addWidget(nets_label_);

    tool_label_ = new QLabel("Tool: Select", this);
    tool_label_->setStyleSheet("color: #FFFFFF; font-weight: bold; margin-right: 12px;");
    sb->addPermanentWidget(tool_label_);

    zoom_label_ = new QLabel("Zoom: 100%", this);
    zoom_label_->setStyleSheet("color: #CCCCCC; margin-right: 12px;");
    sb->addPermanentWidget(zoom_label_);

    pos_label_ = new QLabel("Pos: (0, 0)", this);
    pos_label_->setStyleSheet("color: #CCCCCC;");
    sb->addPermanentWidget(pos_label_);
}

void MainWindow::apply_dark_theme() {
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#121212"));
    pal.setColor(QPalette::WindowText, QColor("#FFFFFF"));
    pal.setColor(QPalette::Base, QColor("#161616"));
    pal.setColor(QPalette::AlternateBase, QColor("#1F1F1F"));
    pal.setColor(QPalette::ToolTipBase, QColor("#1C1C1C"));
    pal.setColor(QPalette::ToolTipText, QColor("#FFFFFF"));
    pal.setColor(QPalette::Text, QColor("#FFFFFF"));
    pal.setColor(QPalette::Button, QColor("#202020"));
    pal.setColor(QPalette::ButtonText, QColor("#FFFFFF"));
    pal.setColor(QPalette::BrightText, QColor("#FFFFFF"));
    pal.setColor(QPalette::Highlight, QColor("#333333"));
    pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    qApp->setPalette(pal);

    setStyleSheet(DARK_THEME_QSS);
}

void MainWindow::display_module(const std::string& module_name) {
    const auto* mod = design_.get_module(module_name);
    if (!mod) return;

    navigator_->set_current_module(module_name);
    scene_->load_module(*mod);
    view_->fit_to_all();
    tree_widget_->populate(design_, module_name);
    inspector_widget_->inspect_module(*mod);

    update_breadcrumb();
    update_status_bar();

    setWindowTitle(QString("Vajra EDA Schematic Workbench - [Module: %1 (Top: %2)]")
        .arg(QString::fromStdString(module_name))
        .arg(QString::fromStdString(design_.top_module)));
}

void MainWindow::show_top_module_box() {
    const auto* top_mod = design_.get_module(design_.top_module);
    if (!top_mod) {
        if (!design_.modules.empty()) {
            top_mod = &design_.modules.begin()->second;
        } else {
            return;
        }
    }

    navigator_->reset_to_top();
    scene_->load_module_box(*top_mod);
    view_->fit_to_all(40.0);
    tree_widget_->populate(design_, top_mod->name);
    inspector_widget_->inspect_module(*top_mod);

    if (breadcrumb_label_) {
        breadcrumb_label_->setText(QString("Breadcrumb: %1 [Top Block Symbol]")
            .arg(QString::fromStdString(top_mod->name)));
    }

    if (module_label_) {
        module_label_->setText(QString("Module: %1 (Block)").arg(QString::fromStdString(top_mod->name)));
    }
    if (cells_label_) {
        cells_label_->setText(QString("Cells: %1").arg(top_mod->cells.size()));
    }
    if (nets_label_) {
        nets_label_->setText(QString("Nets: %1").arg(top_mod->nets.size()));
    }
    if (status_label_) {
        status_label_->setText("Top Block View | Ready");
    }

    setWindowTitle(QString("Vajra EDA Schematic Workbench - [Top Block: %1]")
        .arg(QString::fromStdString(top_mod->name)));
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (first_show_) {
        first_show_ = false;
        QTimer::singleShot(50, this, [this]() {
            if (view_ && scene_ && scene_->is_box_view()) {
                view_->fit_to_all(40.0);
            }
        });
    }
}

void MainWindow::on_top_box_expand() {
    display_module(design_.top_module);
}

void MainWindow::toggle_sidebars() {
    sidebars_collapsed_ = !sidebars_collapsed_;
    dock_hierarchy_->setVisible(!sidebars_collapsed_);
    dock_inspector_->setVisible(!sidebars_collapsed_);
}

void MainWindow::update_breadcrumb() {
    if (breadcrumb_label_ && navigator_) {
        breadcrumb_label_->setText(QString("Breadcrumb: %1")
            .arg(QString::fromStdString(navigator_->get_breadcrumb_string())));
    }
}

void MainWindow::update_status_bar() {
    const auto* mod = navigator_->get_current_module();
    if (!mod) return;

    module_label_->setText(QString("Module: %1").arg(QString::fromStdString(mod->name)));
    cells_label_->setText(QString("Cells: %1").arg(mod->cells.size()));
    nets_label_->setText(QString("Nets: %1").arg(mod->nets.size()));
    if (status_label_) {
        status_label_->setText("Ready");
    }
}

void MainWindow::on_selection_changed() {
    auto items = scene_->selectedItems();
    if (items.empty()) {
        const auto* mod = navigator_->get_current_module();
        if (mod) {
            inspector_widget_->inspect_module(*mod);
        } else {
            inspector_widget_->clear_inspector();
        }
        return;
    }

    auto* item = items.front();
    if (item->type() == TopModuleBoxItem::Type) {
        auto* tb = static_cast<TopModuleBoxItem*>(item);
        inspector_widget_->inspect_module(tb->get_module());
    } else if (item->type() == GateItem::Type) {
        auto* g = static_cast<GateItem*>(item);
        inspector_widget_->inspect_cell(g->get_cell());
        tree_widget_->select_instance(g->get_cell().name);
    } else if (item->type() == ModuleBoxItem::Type) {
        auto* m = static_cast<ModuleBoxItem*>(item);
        inspector_widget_->inspect_cell(m->get_cell());
        tree_widget_->select_instance(m->get_cell().name);
    } else if (item->type() == WireItem::Type) {
        auto* w = static_cast<WireItem*>(item);
        inspector_widget_->inspect_wire(w->get_route(), navigator_->get_current_module());
    }
}

void MainWindow::on_hierarchy_item_clicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QVariant data_var = item->data(0, Qt::UserRole);
    if (!data_var.isValid()) return;

    QVariantList list = data_var.toList();
    if (list.empty()) return;

    QString kind = list[0].toString();
    if (kind == "module") {
        std::string mod_name = list[1].toString().toStdString();
        if (scene_->is_box_view() || navigator_->get_current_module_name() != mod_name) {
            display_module(mod_name);
        } else {
            const auto* mod = design_.get_module(mod_name);
            if (mod) inspector_widget_->inspect_module(*mod);
        }
    } else if (kind == "submodule") {
        std::string target_mod = list[1].toString().toStdString();
        std::string inst_name = list[2].toString().toStdString();
        std::string parent_mod = list.size() > 3 ? list[3].toString().toStdString() : "";

        if (scene_->is_box_view()) {
            display_module(target_mod);
        } else if (parent_mod == navigator_->get_current_module_name()) {
            scene_->select_cell(inst_name);
            auto* m = scene_->get_module_box_item(inst_name);
            if (m) view_->centerOn(m);
            const auto* mod = navigator_->get_current_module();
            if (mod) {
                auto it = mod->cells.find(inst_name);
                if (it != mod->cells.end()) inspector_widget_->inspect_cell(it->second);
            }
        } else {
            display_module(target_mod);
        }
    } else if (kind == "instance") {
        std::string name = list[1].toString().toStdString();
        scene_->select_cell(name);
        auto* g = scene_->get_gate_item(name);
        if (g) view_->centerOn(g);
        const auto* mod = navigator_->get_current_module();
        if (mod) {
            auto it = mod->cells.find(name);
            if (it != mod->cells.end()) inspector_widget_->inspect_cell(it->second);
        }
    } else if (kind == "port") {
        std::string name = list[1].toString().toStdString();
        scene_->select_cell(name);
        auto* g = scene_->get_gate_item(name);
        if (g) view_->centerOn(g);
        const auto* mod = navigator_->get_current_module();
        if (mod) {
            auto it = mod->ports.find(name);
            if (it != mod->ports.end()) inspector_widget_->inspect_port(it->second);
        }
    }
}

void MainWindow::on_hierarchy_item_double_clicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QVariant data_var = item->data(0, Qt::UserRole);
    if (!data_var.isValid()) return;

    QVariantList list = data_var.toList();
    if (list.empty()) return;

    QString kind = list[0].toString();
    if (kind == "module") {
        std::string mod_name = list[1].toString().toStdString();
        display_module(mod_name);
    } else if (kind == "submodule") {
        std::string target = list[1].toString().toStdString();
        display_module(target);
    } else if (kind == "instance") {
        std::string name = list[1].toString().toStdString();
        scene_->select_cell(name);
        auto* g = scene_->get_gate_item(name);
        if (g) view_->centerOn(g);
    }
}

void MainWindow::on_drill_down(const std::string& module_name) {
    if (!design_.get_module(module_name)) return;

    QPointF center = view_->mapToScene(view_->viewport()->rect().center());
    double zoom = view_->get_current_zoom();
    if (navigator_->descend("inst_" + module_name, module_name, center, zoom)) {
        display_module(module_name);
    }
}

void MainWindow::on_ascend_hierarchy() {
    if (navigator_->can_ascend()) {
        QPointF restored_center;
        double restored_zoom = 1.0;
        if (navigator_->ascend(restored_center, restored_zoom)) {
            display_module(navigator_->get_current_module_name());
            view_->centerOn(restored_center);
        }
    } else {
        if (!scene_->is_box_view()) {
            show_top_module_box();
        }
    }
}

void MainWindow::on_reset_to_top() {
    navigator_->reset_to_top();
    display_module(navigator_->get_current_module_name());
}

void MainWindow::on_module_changed(const std::string& /*new_module_name*/) {
    display_module(navigator_->get_current_module_name());
}

void MainWindow::on_net_highlight_requested(const std::string& net_name) {
    scene_->select_net(net_name);
    auto* wire = scene_->get_wire_item(net_name);
    if (wire) {
        view_->centerOn(wire);
    }
}

void MainWindow::on_zoom_changed(double current_zoom) {
    if (zoom_label_) {
        zoom_label_->setText(QString("Zoom: %1%").arg(static_cast<int>(std::round(current_zoom * 100.0))));
    }
}

void MainWindow::on_mouse_moved_scene(const QPointF& scene_pos) {
    if (pos_label_) {
        pos_label_->setText(QString("Pos: (%1, %2)").arg(static_cast<int>(scene_pos.x())).arg(static_cast<int>(scene_pos.y())));
    }
}

void MainWindow::on_tool_mode_changed(CanvasToolMode mode) {
    if (mode == CanvasToolMode::HAND) {
        if (act_tool_hand_ && !act_tool_hand_->isChecked()) act_tool_hand_->setChecked(true);
        if (tool_label_) tool_label_->setText("Tool: Hand (Pan)");
    } else {
        if (act_tool_select_ && !act_tool_select_->isChecked()) act_tool_select_->setChecked(true);
        if (tool_label_) tool_label_->setText("Tool: Select");
    }
}

bool MainWindow::export_schematic(const QString& file_path) {
    if (!scene_) return false;

    QRectF bounds = scene_->itemsBoundingRect();
    if (bounds.isEmpty() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        bounds = scene_->sceneRect();
    }
    if (bounds.isEmpty() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        QMessageBox::warning(this, "Export Schematic", "The canvas has no schematic items to export.");
        return false;
    }

    double pad_x = std::max(60.0, bounds.width() * 0.05);
    double pad_y = std::max(60.0, bounds.height() * 0.05);
    QRectF export_bounds = bounds.adjusted(-pad_x, -pad_y, pad_x, pad_y);

    QString out_path = file_path;
    if (out_path.isEmpty()) {
        std::string cur_mod = navigator_ ? navigator_->get_current_module_name() : "";
        if (cur_mod.empty()) cur_mod = design_.top_module;
        if (cur_mod.empty()) cur_mod = "schematic";

        QString default_filename = QString("%1_schematic.png").arg(QString::fromStdString(cur_mod));
        QString filter = "PNG Image (*.png);;PDF Document (*.pdf);;SVG Vector Graphic (*.svg);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp);;All Files (*.*)";
        QString selected_filter;
        out_path = QFileDialog::getSaveFileName(this, "Export Schematic", default_filename, filter, &selected_filter);
        if (out_path.isEmpty()) {
            return false;
        }
    }

    QFileInfo fi(out_path);
    QString ext = fi.suffix().toLower();
    if (ext.isEmpty()) {
        out_path += ".png";
        ext = "png";
        fi.setFile(out_path);
    }

    bool success = false;
    if (ext == "svg") {
        success = export_to_svg(out_path, export_bounds);
    } else if (ext == "pdf") {
        success = export_to_pdf(out_path, export_bounds);
    } else {
        success = export_to_image(out_path, export_bounds);
    }

    if (success) {
        if (status_label_) {
            status_label_->setText(QString("Exported: %1").arg(fi.fileName()));
        }
    } else {
        QMessageBox::critical(this, "Export Error", QString("Failed to save exported schematic to:\n%1").arg(out_path));
    }

    return success;
}

bool MainWindow::export_to_image(const QString& file_path, const QRectF& bounds) {
    double scale = 2.0;
    double max_dim = std::max(bounds.width(), bounds.height());
    if (max_dim * scale > 8192.0) {
        scale = 8192.0 / max_dim;
    }
    if (scale < 1.0) scale = 1.0;

    int img_w = std::max(100, static_cast<int>(std::ceil(bounds.width() * scale)));
    int img_h = std::max(100, static_cast<int>(std::ceil(bounds.height() * scale)));

    QImage image(img_w, img_h, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#141414"));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    scene_->render(&painter, QRectF(0, 0, img_w, img_h), bounds);
    painter.end();

    return image.save(file_path);
}

bool MainWindow::export_to_svg(const QString& file_path, const QRectF& bounds) {
    QSvgGenerator generator;
    generator.setFileName(file_path);
    generator.setSize(QSize(static_cast<int>(std::ceil(bounds.width())), static_cast<int>(std::ceil(bounds.height()))));
    generator.setViewBox(bounds);
    std::string mod_name = navigator_ ? navigator_->get_current_module_name() : design_.top_module;
    generator.setTitle(QString("Vajra Schematic - %1").arg(QString::fromStdString(mod_name)));
    generator.setDescription("Generated by Vajra EDA Schematic Workbench");

    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(bounds, QColor("#141414"));
    scene_->render(&painter, bounds, bounds);
    painter.end();

    return true;
}

bool MainWindow::export_to_pdf(const QString& file_path, const QRectF& bounds) {
    QPdfWriter pdf_writer(file_path);
    pdf_writer.setPageOrientation(QPageLayout::Landscape);
    pdf_writer.setPageSize(QPageSize(QPageSize::A3));
    std::string mod_name = navigator_ ? navigator_->get_current_module_name() : design_.top_module;
    pdf_writer.setTitle(QString("Vajra Schematic - %1").arg(QString::fromStdString(mod_name)));
    pdf_writer.setCreator("Vajra EDA Schematic Workbench");

    QPainter painter(&pdf_writer);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect page_rect = pdf_writer.pageLayout().paintRectPixels(pdf_writer.resolution());
    painter.fillRect(page_rect, QColor("#141414"));

    double scale_x = static_cast<double>(page_rect.width()) / bounds.width();
    double scale_y = static_cast<double>(page_rect.height()) / bounds.height();
    double scale = std::min(scale_x, scale_y);

    double target_w = bounds.width() * scale;
    double target_h = bounds.height() * scale;
    double offset_x = page_rect.x() + (page_rect.width() - target_w) / 2.0;
    double offset_y = page_rect.y() + (page_rect.height() - target_h) / 2.0;
    QRectF target_rect(offset_x, offset_y, target_w, target_h);

    scene_->render(&painter, target_rect, bounds);
    painter.end();

    return true;
}

void MainWindow::notify_parent_exit() {
    const char* pstr = std::getenv("VAJRA_PARENT_PID");
    if (pstr) {
        try {
            pid_t ppid = static_cast<pid_t>(std::stol(pstr));
            if (ppid > 1 && ppid != getpid()) {
                kill(ppid, SIGTERM);
            }
        } catch (...) {}
    }
}

void MainWindow::hide_gui_only() {
    exit_shell_on_close_ = false;
    bypass_close_dialog_ = true;
    close();
}

void MainWindow::exit_vajra_completely() {
    exit_shell_on_close_ = true;
    notify_parent_exit();
    bypass_close_dialog_ = true;
    close();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (bypass_close_dialog_ || !prompt_on_close_) {
        if (exit_shell_on_close_) {
            notify_parent_exit();
        }
        event->accept();
        qApp->quit();
        return;
    }

    prompt_hide_or_close(event);
    if (event->isAccepted()) {
        qApp->quit();
    }
}

void MainWindow::prompt_hide_or_close(QCloseEvent* event) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Close Vajra Viewer");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("Do you want to hide the GUI or exit Vajra completely?");
    msgBox.setInformativeText(
        "Hide GUI: Closes the GUI interface while keeping the terminal shell session active (reopen anytime with 'show -gui').\n\n"
        "Exit Vajra: Terminates both the GUI interface and the active shell session."
    );
    auto* hideBtn = msgBox.addButton("Hide GUI", QMessageBox::ActionRole);
    auto* exitBtn = msgBox.addButton("Exit Vajra", QMessageBox::DestructiveRole);
    msgBox.addButton("Cancel", QMessageBox::RejectRole);
    msgBox.setDefaultButton(hideBtn);

    msgBox.exec();

    if (msgBox.clickedButton() == hideBtn) {
        exit_shell_on_close_ = false;
        event->accept();
    } else if (msgBox.clickedButton() == exitBtn) {
        exit_shell_on_close_ = true;
        notify_parent_exit();
        event->accept();
    } else {
        event->ignore();
    }
}

} // namespace vajra::gui
