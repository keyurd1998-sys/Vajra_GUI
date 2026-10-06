#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/canvas/schematic_scene.hpp"
#include "vajra/gui/canvas/schematic_view.hpp"
#include "vajra/gui/canvas/hierarchy_navigator.hpp"
#include "vajra/gui/workbench/hierarchy_tree.hpp"
#include "vajra/gui/workbench/property_inspector.hpp"

#include <QMainWindow>
#include <QDockWidget>
#include <QCloseEvent>
#include <QLabel>
#include <QAction>
#include <string>

namespace vajra::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(SchematicDesign design, QWidget* parent = nullptr);
    ~MainWindow() override;

    void display_module(const std::string& module_name);
    void show_top_module_box();
    void toggle_sidebars();

    SchematicScene* scene() const { return scene_; }
    SchematicView* view() const { return view_; }
    HierarchyNavigator* navigator() const { return navigator_; }
    HierarchyTree* hierarchy_tree() const { return tree_widget_; }
    PropertyInspector* property_inspector() const { return inspector_widget_; }
    QDockWidget* dock_hierarchy() const { return dock_hierarchy_; }
    QDockWidget* dock_inspector() const { return dock_inspector_; }
    bool are_sidebars_collapsed() const { return sidebars_collapsed_; }

    void exit_vajra_completely();
    void hide_gui_only();
    void set_prompt_on_close(bool enable) { prompt_on_close_ = enable; }
    bool prompt_on_close() const { return prompt_on_close_; }

    bool export_schematic(const QString& file_path = "");
    QAction* act_export() const { return act_export_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_selection_changed();
    void on_hierarchy_item_clicked(QTreeWidgetItem* item, int column);
    void on_hierarchy_item_double_clicked(QTreeWidgetItem* item, int column);
    void on_drill_down(const std::string& module_name);
    void on_top_box_expand();
    void on_ascend_hierarchy();
    void on_reset_to_top();
    void on_module_changed(const std::string& new_module_name);
    void on_net_highlight_requested(const std::string& net_name);
    void on_zoom_changed(double current_zoom);
    void on_mouse_moved_scene(const QPointF& scene_pos);
    void on_tool_mode_changed(CanvasToolMode mode);

protected:
    void showEvent(QShowEvent* event) override;

private:
    bool first_show_{true};
    SchematicDesign design_;
    SchematicScene* scene_{nullptr};
    SchematicView* view_{nullptr};
    HierarchyNavigator* navigator_{nullptr};

    QDockWidget* dock_hierarchy_{nullptr};
    QDockWidget* dock_inspector_{nullptr};
    HierarchyTree* tree_widget_{nullptr};
    PropertyInspector* inspector_widget_{nullptr};

    bool sidebars_collapsed_{false};

    // UI elements
    QLabel* breadcrumb_label_{nullptr};
    QLabel* status_label_{nullptr};
    QLabel* module_label_{nullptr};
    QLabel* cells_label_{nullptr};
    QLabel* nets_label_{nullptr};
    QLabel* tool_label_{nullptr};
    QLabel* zoom_label_{nullptr};
    QLabel* pos_label_{nullptr};

    QAction* act_tool_select_{nullptr};
    QAction* act_tool_hand_{nullptr};
    QAction* act_fit_{nullptr};
    QAction* act_zoom_in_{nullptr};
    QAction* act_zoom_out_{nullptr};
    QAction* act_reset_zoom_{nullptr};
    QAction* act_hier_up_{nullptr};
    QAction* act_hier_top_{nullptr};
    QAction* act_box_view_{nullptr};
    QAction* act_toggle_sidebars_{nullptr};
    QAction* act_export_{nullptr};

    void setup_ui();
    void setup_menus();
    void setup_toolbar();
    void setup_docks();
    void setup_statusbar();
    void apply_dark_theme();
    void prompt_hide_or_close(QCloseEvent* event);
    void notify_parent_exit();
    void update_breadcrumb();
    void update_status_bar();

    bool export_to_image(const QString& file_path, const QRectF& bounds);
    bool export_to_svg(const QString& file_path, const QRectF& bounds);
    bool export_to_pdf(const QString& file_path, const QRectF& bounds);

    bool bypass_close_dialog_{false};
    bool exit_shell_on_close_{false};
    bool prompt_on_close_{true};
};

} // namespace vajra::gui
