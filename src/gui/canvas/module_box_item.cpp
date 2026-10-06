#include "vajra/gui/canvas/module_box_item.hpp"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <algorithm>

namespace vajra::gui {

namespace {

struct ModPalette {
    static inline const QColor BOX_FILL{"#101927"};
    static inline const QColor BOX_BORDER{"#0284C7"};
    static inline const QColor BOX_BORDER_HOVER{"#38BDF8"};
    static inline const QColor BOX_BORDER_SELECTED{"#FFEB3B"};
    static inline const QColor HEADER_FILL{"#0C4A6E"};
    static inline const QColor HEADER_TEXT{"#38BDF8"};
    static inline const QColor PIN_TEXT{"#E2E8F0"};
    static inline const QColor PIN_DOT{"#38BDF8"};
    static inline const QColor FOOTER_TEXT{"#64748B"};
};

} // anonymous namespace

ModuleBoxItem::ModuleBoxItem(const SchematicCell& cell,
                             const std::vector<PinLocation>& pins,
                             double node_x,
                             double node_y,
                             double node_width,
                             double node_height,
                             QGraphicsItem* parent)
    : QGraphicsObject(parent), cell_(cell) {

    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);

    calculate_layout(pins, node_x, node_y, node_width, node_height);

    std::string tip = "Submodule: " + cell_.cell_type + "\n" +
                      "Instance: " + cell_.name + "\n" +
                      "Inputs: " + std::to_string(input_pins_.size()) + "\n" +
                      "Outputs: " + std::to_string(output_pins_.size()) + "\n" +
                      "Double-click to descend into submodule logic";
    setToolTip(QString::fromStdString(tip));
}

void ModuleBoxItem::calculate_layout(const std::vector<PinLocation>& pins,
                                     double node_x, double node_y,
                                     double node_width, double node_height) {
    input_pins_.clear();
    output_pins_.clear();
    pin_local_positions_.clear();

    for (const auto& pin : cell_.pins) {
        if (pin.direction == PinDirection::INPUT) {
            input_pins_.push_back(pin);
        } else {
            output_pins_.push_back(pin);
        }
    }

    if (node_width > 0.0 && node_height > 0.0) {
        width_ = node_width;
        height_ = node_height;
    } else {
        size_t max_pins = std::max({input_pins_.size(), output_pins_.size(), size_t{1}});
        height_ = header_height_ + footer_height_ + (max_pins * pin_pitch_);
        if (height_ < 80.0) {
            height_ = 80.0;
        }

        // Determine width based on instance and type name
        size_t max_name_len = cell_.name.size() + cell_.cell_type.size() + 8;
        width_ = std::max(160.0, static_cast<double>(max_name_len) * 7.5);
    }

    if (!pins.empty()) {
        for (const auto& p : pins) {
            pin_local_positions_[p.pin_name] = QPointF(p.x - node_x, p.y - node_y);
        }
        return;
    }

    // Fallback: Calculate pin positions
    double in_start_y = header_height_ + (height_ - header_height_ - footer_height_ - input_pins_.size() * pin_pitch_) / 2.0 + pin_pitch_ / 2.0;
    for (size_t i = 0; i < input_pins_.size(); ++i) {
        double y = in_start_y + (i * pin_pitch_);
        pin_local_positions_[input_pins_[i].name] = QPointF(0.0, y);
    }

    double out_start_y = header_height_ + (height_ - header_height_ - footer_height_ - output_pins_.size() * pin_pitch_) / 2.0 + pin_pitch_ / 2.0;
    for (size_t i = 0; i < output_pins_.size(); ++i) {
        double y = out_start_y + (i * pin_pitch_);
        pin_local_positions_[output_pins_[i].name] = QPointF(width_, y);
    }
}

QRectF ModuleBoxItem::boundingRect() const {
    return QRectF(-10.0, -10.0, width_ + 20.0, height_ + 20.0);
}

QPointF ModuleBoxItem::get_pin_local_pos(const std::string& pin_name) const {
    auto it = pin_local_positions_.find(pin_name);
    if (it != pin_local_positions_.end()) {
        return it->second;
    }
    return QPointF(0.0, height_ / 2.0);
}

QPointF ModuleBoxItem::get_pin_scene_pos(const std::string& pin_name) const {
    return mapToScene(get_pin_local_pos(pin_name));
}

void ModuleBoxItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void ModuleBoxItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void ModuleBoxItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    emit drill_down_requested(cell_.cell_type);
    event->accept();
}

void ModuleBoxItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    double lod = option->levelOfDetailFromTransform(painter->worldTransform());

    // Box outline pen
    QPen pen;
    if (isSelected()) {
        pen = QPen(ModPalette::BOX_BORDER_SELECTED, 2.4);
    } else if (is_hovered_) {
        pen = QPen(ModPalette::BOX_BORDER_HOVER, 2.0);
    } else {
        pen = QPen(ModPalette::BOX_BORDER, 1.8);
    }

    // Main box body
    painter->setPen(pen);
    painter->setBrush(QBrush(ModPalette::BOX_FILL));
    painter->drawRoundedRect(QRectF(0, 0, width_, height_), 6.0, 6.0);

    // Header bar
    QRectF header_rect(0, 0, width_, header_height_);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(ModPalette::HEADER_FILL));
    painter->drawRoundedRect(header_rect, 6.0, 6.0);
    painter->fillRect(QRectF(0, header_height_ - 4.0, width_, 4.0), ModPalette::HEADER_FILL);

    // Header text
    painter->setPen(QPen(ModPalette::HEADER_TEXT));
    QFont hdr_font("Monospace", 8, QFont::Bold);
    hdr_font.setStyleHint(QFont::TypeWriter);
    painter->setFont(hdr_font);

    std::string title = "[+] " + cell_.name + " (" + cell_.cell_type + ")";
    painter->drawText(QRectF(6.0, 2.0, width_ - 12.0, header_height_ - 2.0),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QString::fromStdString(title));

    if (lod >= 0.3) {
        // Pins and Pin Labels
        QFont pin_font("Monospace", 7);
        pin_font.setStyleHint(QFont::TypeWriter);
        painter->setFont(pin_font);

        // Input pins on left
        for (const auto& pin : input_pins_) {
            auto pos = pin_local_positions_[pin.name];
            // Dot on boundary
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(ModPalette::PIN_DOT));
            painter->drawEllipse(pos, 2.5, 2.5);

            // Label
            painter->setPen(QPen(ModPalette::PIN_TEXT));
            painter->drawText(QRectF(pos.x() + 5.0, pos.y() - 8.0, width_ * 0.45, 16.0),
                              Qt::AlignVCenter | Qt::AlignLeft,
                              QString::fromStdString(pin.name));
        }

        // Output pins on right
        for (const auto& pin : output_pins_) {
            auto pos = pin_local_positions_[pin.name];
            // Dot on boundary
            painter->setPen(Qt::NoPen);
            painter->setBrush(QBrush(ModPalette::PIN_DOT));
            painter->drawEllipse(pos, 2.5, 2.5);

            // Label
            painter->setPen(QPen(ModPalette::PIN_TEXT));
            painter->drawText(QRectF(pos.x() - width_ * 0.45 - 5.0, pos.y() - 8.0, width_ * 0.45, 16.0),
                              Qt::AlignVCenter | Qt::AlignRight,
                              QString::fromStdString(pin.name));
        }

        // Footer hint
        QFont foot_font("Monospace", 6);
        foot_font.setStyleHint(QFont::TypeWriter);
        painter->setFont(foot_font);
        painter->setPen(QPen(ModPalette::FOOTER_TEXT));
        painter->drawText(QRectF(4.0, height_ - footer_height_, width_ - 8.0, footer_height_),
                          Qt::AlignCenter,
                          "Double-click: drill into logic");
    }
}

} // namespace vajra::gui
