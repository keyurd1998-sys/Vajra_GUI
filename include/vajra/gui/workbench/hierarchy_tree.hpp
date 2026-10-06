#pragma once

#include "vajra/gui/models.hpp"

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <string>
#include <unordered_set>

namespace vajra::gui {

class HierarchyTree : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyTree(QWidget* parent = nullptr);

    void populate(const SchematicDesign& design, const std::string& active_module_name = "");
    void set_active_module(const std::string& module_name);
    void select_instance(const std::string& instance_name);

    QTreeWidget* tree() const { return tree_; }
    QLineEdit* search_bar() const { return search_edit_; }

signals:
    void instance_selected(const std::string& instance_name);
    void module_drilldown_requested(const std::string& module_name);
    void port_selected(const std::string& port_name);

private slots:
    void on_filter_changed(const QString& text);
    void on_item_clicked(QTreeWidgetItem* item, int column);
    void on_item_double_clicked(QTreeWidgetItem* item, int column);
    void on_custom_context_menu(const QPoint& pos);

private:
    QLineEdit* search_edit_{nullptr};
    QTreeWidget* tree_{nullptr};
    SchematicDesign design_;
    std::string active_module_name_;

    void setup_ui();
    void build_full_tree();
    void build_submodule_children(QTreeWidgetItem* parent_item, const std::string& module_name, int depth, std::unordered_set<std::string>& active_path);
    bool filter_item(QTreeWidgetItem* item, const QString& query);
};

} // namespace vajra::gui
