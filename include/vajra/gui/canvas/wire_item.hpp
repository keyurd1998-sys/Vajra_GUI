#pragma once

#include "vajra/gui/routing/router_models.hpp"

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QColor>

namespace vajra::gui {

class WireItem : public QGraphicsItem {
public:
    enum { Type = UserType + 3 };

    explicit WireItem(NetRoute route, QGraphicsItem* parent = nullptr);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    const NetRoute& get_route() const { return route_; }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    NetRoute route_;
    bool is_hovered_{false};
    QRectF bounding_rect_;
    QPainterPath wire_path_;
    std::vector<QPainterPath> dot_paths_;

    void build_paths();
};

} // namespace vajra::gui
