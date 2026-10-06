#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/symbol_classifier.hpp"
#include "vajra/gui/routing/router_models.hpp"

#include <QGraphicsItem>
#include <QPainterPath>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>

namespace vajra::gui {

class GateItem : public QGraphicsItem {
public:
    enum { Type = UserType + 1 };

    explicit GateItem(const SchematicCell& cell,
                      const std::vector<PinLocation>& pins = {},
                      double node_x = 0.0,
                      double node_y = 0.0,
                      double node_width = 0.0,
                      double node_height = 0.0,
                      QGraphicsItem* parent = nullptr);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    const SchematicCell& get_cell() const { return cell_; }
    const SymbolClassification& get_classification() const { return classification_; }

    QPointF get_pin_local_pos(const std::string& pin_name) const;
    QPointF get_pin_scene_pos(const std::string& pin_name) const;

    double get_width() const { return width_; }
    double get_height() const { return height_; }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    SchematicCell cell_;
    SymbolClassification classification_;
    double width_{70.0};
    double height_{50.0};
    bool is_hovered_{false};

    QPainterPath gate_path_;
    QPainterPath lead_path_;
    std::vector<QPainterPath> bubble_paths_;
    std::unordered_map<std::string, QPointF> pin_local_positions_;
    std::optional<QPointF> clk_pin_pos_;

    void setup_dimensions_and_pins(const std::vector<PinLocation>& pins,
                                   double node_x, double node_y,
                                   double node_width, double node_height);
    void build_vector_paths();
    void render_labels(QPainter* painter, double lod);
    void render_pin_labels(QPainter* painter, double lod);
    void render_sequential_decorations(QPainter* painter, double lod);
};

} // namespace vajra::gui
