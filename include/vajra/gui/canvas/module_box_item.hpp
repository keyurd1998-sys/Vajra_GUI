#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/routing/router_models.hpp"

#include <QGraphicsObject>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <unordered_map>
#include <string>
#include <vector>

namespace vajra::gui {

class ModuleBoxItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 2 };

    explicit ModuleBoxItem(const SchematicCell& cell,
                           const std::vector<PinLocation>& pins = {},
                           double node_x = 0.0,
                           double node_y = 0.0,
                           double node_width = 0.0,
                           double node_height = 0.0,
                           QGraphicsItem* parent = nullptr);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    const SchematicCell& get_cell() const { return cell_; }
    QPointF get_pin_local_pos(const std::string& pin_name) const;
    QPointF get_pin_scene_pos(const std::string& pin_name) const;

    double get_width() const { return width_; }
    double get_height() const { return height_; }

signals:
    void drill_down_requested(const std::string& target_module);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    SchematicCell cell_;
    double width_{140.0};
    double height_{90.0};
    double header_height_{24.0};
    double footer_height_{18.0};
    double pin_pitch_{20.0};
    bool is_hovered_{false};

    std::vector<SchematicPin> input_pins_;
    std::vector<SchematicPin> output_pins_;
    std::unordered_map<std::string, QPointF> pin_local_positions_;

    void calculate_layout(const std::vector<PinLocation>& pins,
                          double node_x, double node_y,
                          double node_width, double node_height);
};

} // namespace vajra::gui
