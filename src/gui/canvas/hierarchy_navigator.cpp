#include "vajra/gui/canvas/hierarchy_navigator.hpp"

#include <sstream>

namespace vajra::gui {

HierarchyNavigator::HierarchyNavigator(SchematicDesign design, QObject* parent)
    : QObject(parent), design_(std::move(design)) {
    reset_to_top();
}

void HierarchyNavigator::set_design(SchematicDesign design) {
    design_ = std::move(design);
    reset_to_top();
}

void HierarchyNavigator::reset_to_top() {
    history_.clear();
    std::string top = design_.top_module;
    if (top.empty() && !design_.modules.empty()) {
        top = design_.modules.begin()->first;
    }

    if (!top.empty()) {
        HierarchyLevel root;
        root.module_name = top;
        root.instance_name = top;
        root.viewport_center = QPointF(0.0, 0.0);
        root.zoom_level = 1.0;
        history_.push_back(root);
    }
}

void HierarchyNavigator::set_current_module(const std::string& module_name) {
    if (module_name.empty()) return;
    std::string top = design_.top_module.empty() && !design_.modules.empty() ?
                      design_.modules.begin()->first : design_.top_module;

    if (module_name == top) {
        reset_to_top();
        return;
    }

    if (!history_.empty() && history_.back().module_name == module_name) {
        return;
    }

    for (auto it = history_.begin(); it != history_.end(); ++it) {
        if (it->module_name == module_name) {
            history_.erase(it + 1, history_.end());
            emit module_changed(module_name);
            return;
        }
    }

    if (history_.empty()) {
        reset_to_top();
    }
    HierarchyLevel lvl;
    lvl.module_name = module_name;
    lvl.instance_name = module_name;
    lvl.viewport_center = QPointF(0.0, 0.0);
    lvl.zoom_level = 1.0;
    history_.push_back(lvl);
    emit module_changed(module_name);
}

const SchematicModule* HierarchyNavigator::get_current_module() const {
    if (history_.empty()) {
        return nullptr;
    }
    const auto& current_name = history_.back().module_name;
    auto it = design_.modules.find(current_name);
    if (it != design_.modules.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::string HierarchyNavigator::get_current_module_name() const {
    if (history_.empty()) {
        return "";
    }
    return history_.back().module_name;
}

bool HierarchyNavigator::descend(const std::string& instance_name, const std::string& target_module_name,
                                 QPointF cur_center, double cur_zoom) {
    auto it = design_.modules.find(target_module_name);
    if (it == design_.modules.end()) {
        return false;
    }

    if (!history_.empty()) {
        history_.back().viewport_center = cur_center;
        history_.back().zoom_level = cur_zoom;
    }

    HierarchyLevel next_lvl;
    next_lvl.module_name = target_module_name;
    next_lvl.instance_name = instance_name;
    next_lvl.viewport_center = QPointF(0.0, 0.0);
    next_lvl.zoom_level = 1.0;
    history_.push_back(next_lvl);

    emit module_changed(target_module_name);
    return true;
}

bool HierarchyNavigator::ascend(QPointF& restored_center, double& restored_zoom) {
    if (history_.size() <= 1) {
        return false;
    }

    history_.pop_back();
    const auto& parent_lvl = history_.back();
    restored_center = parent_lvl.viewport_center;
    restored_zoom = parent_lvl.zoom_level;

    emit module_changed(parent_lvl.module_name);
    return true;
}

std::string HierarchyNavigator::get_breadcrumb_string() const {
    if (history_.empty()) {
        return "None";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < history_.size(); ++i) {
        if (i > 0) {
            oss << " > ";
        }
        if (i == 0) {
            oss << history_[i].module_name;
        } else {
            oss << history_[i].instance_name << " (" << history_[i].module_name << ")";
        }
    }
    return oss.str();
}

} // namespace vajra::gui
