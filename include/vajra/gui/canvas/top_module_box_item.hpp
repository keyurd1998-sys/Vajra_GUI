#pragma once

#include "vajra/gui/models.hpp"

#include <QGraphicsObject>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <unordered_map>
#include <string>
#include <vector>

namespace vajra::gui {

class TopModuleBoxItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 4 };

    explicit TopModuleBoxItem(const SchematicModule& module, QGraphicsItem* parent = nullptr);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    const SchematicModule& get_module() const { return module_; }
    double get_width() const { return box_width_; }
    double get_height() const { return box_height_; }

signals:
    void expand_requested();

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    SchematicModule module_;
    double box_width_{500.0};
    double box_height_{500.0};
    double header_height_{85.0};
    double footer_height_{45.0};
    double pin_pitch_{32.0};
    double stub_length_{50.0};
    bool is_hovered_{false};

    std::vector<SchematicPin> input_ports_;
    std::vector<SchematicPin> output_ports_;
    // port_name -> (terminal_point, box_edge_point)
    std::unordered_map<std::string, std::pair<QPointF, QPointF>> input_pin_coords_;
    std::unordered_map<std::string, std::pair<QPointF, QPointF>> output_pin_coords_;

    void calculate_layout();
    std::string format_port_label(const SchematicPin& port) const;
};

} // namespace vajra::gui
