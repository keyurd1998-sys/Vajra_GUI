#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/canvas/gate_item.hpp"
#include "vajra/gui/canvas/module_box_item.hpp"
#include "vajra/gui/canvas/top_module_box_item.hpp"

#include <QGraphicsScene>
#include <QColor>
#include <unordered_map>
#include <string>

namespace vajra::gui {

class WireItem;

class SchematicScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit SchematicScene(QObject* parent = nullptr);

    void set_grid_visible(bool visible);
    bool is_grid_visible() const { return grid_visible_; }

    void clear_schematic();
    void load_module(const SchematicModule& module);
    void load_module_box(const SchematicModule& module);

    bool is_box_view() const { return is_box_view_; }
    const SchematicModule& get_current_module() const { return current_module_; }

    void select_cell(const std::string& name);
    void select_net(const std::string& net_name);

    GateItem* get_gate_item(const std::string& name) const;
    ModuleBoxItem* get_module_box_item(const std::string& name) const;
    TopModuleBoxItem* get_top_box_item() const { return top_box_item_; }
    WireItem* get_wire_item(const std::string& net_name) const;

signals:
    void drill_down_requested(const std::string& target_module);
    void top_box_expand_requested();

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    bool grid_visible_{true};
    double grid_size_{20.0};
    QColor background_color_{QColor("#141414")};
    QColor grid_dot_color_{QColor("#2A2A2A")};

    bool is_box_view_{false};
    TopModuleBoxItem* top_box_item_{nullptr};
    SchematicModule current_module_;
    std::unordered_map<std::string, GateItem*> gate_items_;
    std::unordered_map<std::string, ModuleBoxItem*> module_box_items_;
    std::unordered_map<std::string, WireItem*> wire_items_;
};

} // namespace vajra::gui
