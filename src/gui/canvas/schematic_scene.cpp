#include "vajra/gui/canvas/schematic_scene.hpp"
#include "vajra/gui/placement/placement_engine.hpp"
#include "vajra/gui/routing/orthogonal_router.hpp"
#include "vajra/gui/routing/pin_resolver.hpp"
#include "vajra/gui/canvas/wire_item.hpp"

#include <QPainter>
#include <cmath>

namespace vajra::gui {

SchematicScene::SchematicScene(QObject* parent)
    : QGraphicsScene(parent) {
    setBackgroundBrush(background_color_);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
}

void SchematicScene::set_grid_visible(bool visible) {
    if (grid_visible_ != visible) {
        grid_visible_ = visible;
        invalidate(sceneRect(), QGraphicsScene::BackgroundLayer);
    }
}

void SchematicScene::clear_schematic() {
    clear();
    gate_items_.clear();
    module_box_items_.clear();
    wire_items_.clear();
    top_box_item_ = nullptr;
    is_box_view_ = false;
}

void SchematicScene::load_module_box(const SchematicModule& module) {
    clear_schematic();
    current_module_ = module;
    is_box_view_ = true;

    top_box_item_ = new TopModuleBoxItem(module);
    top_box_item_->setPos(0.0, 0.0);
    addItem(top_box_item_);

    connect(top_box_item_, &TopModuleBoxItem::expand_requested,
            this, &SchematicScene::top_box_expand_requested);

    QRectF all_bounds = itemsBoundingRect();
    if (all_bounds.isEmpty() || all_bounds.width() <= 0.0 || all_bounds.height() <= 0.0) {
        all_bounds = top_box_item_->boundingRect();
    }
    double pad = 200.0;
    setSceneRect(all_bounds.adjusted(-pad, -pad, pad, pad));
}

void SchematicScene::load_module(const SchematicModule& module) {
    clear_schematic();
    is_box_view_ = false;
    current_module_ = module;

    if (module.cells.empty() && module.ports.empty()) {
        return;
    }

    // 1. Run Sugiyama 4-stage Placement Pipeline
    auto placement = PlacementEngine::run(module);

    PinResolver pin_resolver(placement.graph, module);

    // 2. Add Placed Nodes (Gates, Module Boxes, Ports)
    for (const auto& [nid, node] : placement.graph.nodes) {
        auto node_pins = pin_resolver.get_node_pins(node.id);
        if (node.kind == "PRIMARY_INPUT") {
            SchematicCell port_cell;
            port_cell.id = node.name;
            port_cell.name = node.name;
            port_cell.cell_type = (node.name.find("clk") != std::string::npos) ? "CLOCK" : "INPUT";
            port_cell.type = NodeType::PRIMARY_INPUT;

            SchematicPin p;
            p.name = node.name;
            p.direction = PinDirection::OUTPUT;
            port_cell.pins.push_back(p);

            auto* item = new GateItem(port_cell, node_pins, node.x, node.y, node.width, node.height);
            item->setPos(node.x, node.y);
            addItem(item);
            gate_items_[node.id] = item;
            gate_items_[node.name] = item;
        } else if (node.kind == "PRIMARY_OUTPUT") {
            SchematicCell port_cell;
            port_cell.id = node.name;
            port_cell.name = node.name;
            port_cell.cell_type = "OUTPUT";
            port_cell.type = NodeType::PRIMARY_OUTPUT;

            SchematicPin p;
            p.name = node.name;
            p.direction = PinDirection::INPUT;
            port_cell.pins.push_back(p);

            auto* item = new GateItem(port_cell, node_pins, node.x, node.y, node.width, node.height);
            item->setPos(node.x, node.y);
            item->setZValue(1.0);
            addItem(item);
            gate_items_[node.id] = item;
            gate_items_[node.name] = item;
        } else if (node.kind == "MODULE") {
            const auto* cell_ptr = module.get_cell(node.id);
            if (cell_ptr) {
                auto* mod_item = new ModuleBoxItem(*cell_ptr, node_pins, node.x, node.y, node.width, node.height);
                mod_item->setPos(node.x, node.y);
                mod_item->setZValue(1.0);
                addItem(mod_item);
                module_box_items_[node.id] = mod_item;
                module_box_items_[node.name] = mod_item;

                connect(mod_item, &ModuleBoxItem::drill_down_requested,
                        this, &SchematicScene::drill_down_requested);
            }
        } else {
            const auto* cell_ptr = module.get_cell(node.id);
            if (cell_ptr) {
                auto* gate_item = new GateItem(*cell_ptr, node_pins, node.x, node.y, node.width, node.height);
                gate_item->setPos(node.x, node.y);
                gate_item->setZValue(1.0);
                addItem(gate_item);
                gate_items_[node.id] = gate_item;
                gate_items_[node.name] = gate_item;
            }
        }
    }

    // 3. Run Manhattan Orthogonal Auto-Router
    auto routes = OrthogonalRouter::route(module, placement);

    // 4. Add WireItems to Scene
    for (auto& route : routes) {
        std::string net_name = route.net_name;
        auto* wire = new WireItem(std::move(route));
        wire->setZValue(0.0);
        addItem(wire);
        wire_items_[net_name] = wire;
    }

    // 5. Update scene bounding rect to encompass all placed items, routed wires, and corridor tracks
    QRectF all_bounds = itemsBoundingRect();
    if (all_bounds.isEmpty() || all_bounds.width() <= 0.0 || all_bounds.height() <= 0.0) {
        all_bounds = QRectF(placement.bbox.x, placement.bbox.y, placement.bbox.width, placement.bbox.height);
    }
    double pad_x = std::max(500.0, all_bounds.width() * 0.15);
    double pad_y = std::max(500.0, all_bounds.height() * 0.15);
    setSceneRect(all_bounds.adjusted(-pad_x, -pad_y, pad_x, pad_y));
}

GateItem* SchematicScene::get_gate_item(const std::string& name) const {
    auto it = gate_items_.find(name);
    return it != gate_items_.end() ? it->second : nullptr;
}

ModuleBoxItem* SchematicScene::get_module_box_item(const std::string& name) const {
    auto it = module_box_items_.find(name);
    return it != module_box_items_.end() ? it->second : nullptr;
}

WireItem* SchematicScene::get_wire_item(const std::string& net_name) const {
    auto it = wire_items_.find(net_name);
    return it != wire_items_.end() ? it->second : nullptr;
}

void SchematicScene::select_cell(const std::string& name) {
    clearSelection();
    auto it_g = gate_items_.find(name);
    if (it_g != gate_items_.end()) {
        it_g->second->setSelected(true);
        return;
    }
    auto it_m = module_box_items_.find(name);
    if (it_m != module_box_items_.end()) {
        it_m->second->setSelected(true);
        return;
    }
}

void SchematicScene::select_net(const std::string& net_name) {
    clearSelection();
    auto it_w = wire_items_.find(net_name);
    if (it_w != wire_items_.end()) {
        it_w->second->setSelected(true);
    }
}

void SchematicScene::drawBackground(QPainter* painter, const QRectF& rect) {
    // Fill background solid color
    painter->fillRect(rect, background_color_);

    if (!grid_visible_) return;

    painter->save();
    painter->setPen(grid_dot_color_);

    double left = std::floor(rect.left() / grid_size_) * grid_size_;
    double top = std::floor(rect.top() / grid_size_) * grid_size_;

    // Draw grid dots
    std::vector<QPointF> points;
    for (double x = left; x <= rect.right(); x += grid_size_) {
        for (double y = top; y <= rect.bottom(); y += grid_size_) {
            points.emplace_back(x, y);
        }
    }

    if (!points.empty()) {
        painter->drawPoints(points.data(), static_cast<int>(points.size()));
    }

    painter->restore();
}

} // namespace vajra::gui
