#pragma once

#include "vajra/gui/models.hpp"

#include <QObject>
#include <QPointF>
#include <string>
#include <vector>

namespace vajra::gui {

struct HierarchyLevel {
    std::string module_name;
    std::string instance_name;
    QPointF viewport_center{0.0, 0.0};
    double zoom_level{1.0};
};

class HierarchyNavigator : public QObject {
    Q_OBJECT
public:
    explicit HierarchyNavigator(SchematicDesign design, QObject* parent = nullptr);

    const SchematicDesign& get_design() const { return design_; }
    void set_design(SchematicDesign design);

    const SchematicModule* get_current_module() const;
    const std::vector<HierarchyLevel>& get_history() const { return history_; }

    bool can_ascend() const { return history_.size() > 1; }
    bool descend(const std::string& instance_name, const std::string& target_module_name,
                 QPointF cur_center, double cur_zoom);
    bool ascend(QPointF& restored_center, double& restored_zoom);
    void reset_to_top();
    void set_current_module(const std::string& module_name);

    std::string get_current_module_name() const;
    std::string get_breadcrumb_string() const;

signals:
    void module_changed(const std::string& new_module_name);

private:
    SchematicDesign design_;
    std::vector<HierarchyLevel> history_;
};

} // namespace vajra::gui
