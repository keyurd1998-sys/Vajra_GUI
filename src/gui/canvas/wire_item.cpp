#include "vajra/gui/canvas/wire_item.hpp"

#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneHoverEvent>

namespace vajra::gui {

namespace {

struct WirePalette {
    static inline const QColor WIRE_NORMAL{"#5599DD"};
    static inline const QColor WIRE_CLOCK{"#FF9900"};
    static inline const QColor WIRE_RESET{"#FF7043"};
    static inline const QColor WIRE_HOVER{"#00FFCC"};
    static inline const QColor WIRE_SELECTED{"#FFEB3B"};
};

} // anonymous namespace

WireItem::WireItem(NetRoute route, QGraphicsItem* parent)
    : QGraphicsItem(parent), route_(std::move(route)) {

    setFlags(ItemIsSelectable);
    setAcceptHoverEvents(true);

    build_paths();

    std::string tip = "Net: " + route_.net_name;
    if (route_.is_clock) tip += " (Clock)";
    if (route_.is_reset) tip += " (Reset)";
    if (route_.is_high_fanout) tip += " [High Fanout]";
    setToolTip(QString::fromStdString(tip));
}

void WireItem::build_paths() {
    wire_path_.clear();
    dot_paths_.clear();

    for (const auto& seg : route_.segments) {
        wire_path_.moveTo(seg.p1.x, seg.p1.y);
        wire_path_.lineTo(seg.p2.x, seg.p2.y);
    }

    for (const auto& dot : route_.solder_dots) {
        QPainterPath dp;
        dp.addEllipse(QPointF(dot.pos.x, dot.pos.y), dot.radius, dot.radius);
        dot_paths_.push_back(dp);
    }

    bounding_rect_ = wire_path_.boundingRect();

    for (const auto& stub : route_.hfn_stubs) {
        Point tag_pos = stub.is_driver ? stub.end : stub.start;
        double tag_w = std::max(28.0, static_cast<double>(stub.label.size()) * 6.5 + 8.0);
        double tag_h = 13.0;
        QRectF tag_rect;
        if (stub.arrow_direction == "RIGHT") {
            tag_rect = QRectF(tag_pos.x - tag_w, tag_pos.y - tag_h / 2.0, tag_w, tag_h);
        } else if (stub.arrow_direction == "LEFT") {
            tag_rect = QRectF(tag_pos.x, tag_pos.y - tag_h / 2.0, tag_w, tag_h);
        } else if (stub.arrow_direction == "UP") {
            tag_rect = QRectF(tag_pos.x - tag_w / 2.0, tag_pos.y, tag_w, tag_h);
        } else {
            tag_rect = QRectF(tag_pos.x - tag_w / 2.0, tag_pos.y - tag_h, tag_w, tag_h);
        }
        bounding_rect_ = bounding_rect_.united(tag_rect);
    }

    bounding_rect_ = bounding_rect_.adjusted(-8.0, -8.0, 8.0, 8.0);
}

QRectF WireItem::boundingRect() const {
    return bounding_rect_;
}

QPainterPath WireItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    QPainterPath p = stroker.createStroke(wire_path_);

    for (const auto& stub : route_.hfn_stubs) {
        Point tag_pos = stub.is_driver ? stub.end : stub.start;
        double tag_w = std::max(28.0, static_cast<double>(stub.label.size()) * 6.5 + 8.0);
        double tag_h = 13.0;
        QRectF tag_rect;
        if (stub.arrow_direction == "RIGHT") {
            tag_rect = QRectF(tag_pos.x - tag_w, tag_pos.y - tag_h / 2.0, tag_w, tag_h);
        } else if (stub.arrow_direction == "LEFT") {
            tag_rect = QRectF(tag_pos.x, tag_pos.y - tag_h / 2.0, tag_w, tag_h);
        } else if (stub.arrow_direction == "UP") {
            tag_rect = QRectF(tag_pos.x - tag_w / 2.0, tag_pos.y, tag_w, tag_h);
        } else {
            tag_rect = QRectF(tag_pos.x - tag_w / 2.0, tag_pos.y - tag_h, tag_w, tag_h);
        }
        p.addRect(tag_rect);
    }

    return p;
}

void WireItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = true;
    update();
    QGraphicsItem::hoverEnterEvent(event);
}

void WireItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = false;
    update();
    QGraphicsItem::hoverLeaveEvent(event);
}

void WireItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor color;
    double width = 1.5;

    if (isSelected()) {
        color = WirePalette::WIRE_SELECTED;
        width = 2.5;
    } else if (is_hovered_) {
        color = WirePalette::WIRE_HOVER;
        width = 2.2;
    } else if (route_.is_clock) {
        color = WirePalette::WIRE_CLOCK;
        width = 1.6;
    } else if (route_.is_reset) {
        color = WirePalette::WIRE_RESET;
        width = 1.6;
    } else {
        color = WirePalette::WIRE_NORMAL;
        width = 1.5;
    }

    QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(wire_path_);

    // Draw solder dots
    if (!dot_paths_.empty()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(color));
        for (const auto& dp : dot_paths_) {
            painter->drawPath(dp);
        }
    }

    // Draw HFN tag badges and labels
    double lod = option ? option->levelOfDetailFromTransform(painter->worldTransform()) : 1.0;
    if (lod >= 0.2 && !route_.hfn_stubs.empty()) {
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        for (const auto& stub : route_.hfn_stubs) {
            Point tag_pos = stub.is_driver ? stub.end : stub.start;
            double tag_w = std::max(28.0, static_cast<double>(stub.label.size()) * 6.5 + 8.0);
            double tag_h = 13.0;
            QRectF tag_rect;
            if (stub.arrow_direction == "RIGHT") {
                tag_rect = QRectF(tag_pos.x - tag_w, tag_pos.y - tag_h / 2.0, tag_w, tag_h);
            } else if (stub.arrow_direction == "LEFT") {
                tag_rect = QRectF(tag_pos.x, tag_pos.y - tag_h / 2.0, tag_w, tag_h);
            } else if (stub.arrow_direction == "UP") {
                tag_rect = QRectF(tag_pos.x - tag_w / 2.0, tag_pos.y, tag_w, tag_h);
            } else {
                tag_rect = QRectF(tag_pos.x - tag_w / 2.0, tag_pos.y - tag_h, tag_w, tag_h);
            }

            bool is_lit = isSelected() || is_hovered_;
            QColor bg_color = is_lit ? QColor("#1e293b") : QColor("#0f172a");
            QColor border_color = is_lit ? color : QColor("#475569");
            QColor text_color = is_lit ? color : QColor("#94a3b8");

            painter->setPen(QPen(border_color, is_lit ? 1.5 : 1.0));
            painter->setBrush(QBrush(bg_color));
            painter->drawRoundedRect(tag_rect, 2.0, 2.0);

            painter->setPen(text_color);
            QFont tag_font("Monospace", 6, is_lit ? QFont::Bold : QFont::Normal);
            painter->setFont(tag_font);
            painter->drawText(tag_rect, Qt::AlignCenter, QString::fromStdString(stub.label));
        }
    }
}

} // namespace vajra::gui
