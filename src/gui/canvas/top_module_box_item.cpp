#include "vajra/gui/canvas/top_module_box_item.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <algorithm>
#include <cctype>

namespace vajra::gui {

namespace {

struct TopBoxPalette {
    static inline const QColor BOX_FILL{"#141923"};
    static inline const QColor BOX_BORDER{"#0284C7"};
    static inline const QColor BOX_BORDER_HOVER{"#FACC15"};
    static inline const QColor BOX_BORDER_SELECTED{"#FFEB3B"};
    static inline const QColor HEADER_FILL{"#1E293B"};
    static inline const QColor HEADER_TEXT{"#FFFFFF"};
    static inline const QColor SUBTITLE_TEXT{"#94A3B8"};
    static inline const QColor BADGE_FILL{"#0369A1"};
    static inline const QColor BADGE_TEXT{"#E0F2FE"};
    static inline const QColor PIN_LINE{"#38BDF8"};
    static inline const QColor PIN_DOT{"#0284C7"};
    static inline const QColor PIN_TEXT{"#E2E8F0"};
    static inline const QColor PIN_BUS_TEXT{"#7DD3FC"};
    static inline const QColor PIN_CLOCK{"#FF9900"};
    static inline const QColor PIN_RESET{"#FF7043"};
    static inline const QColor PROMPT_BG{"#0F172A"};
    static inline const QColor PROMPT_BORDER{"#0284C7"};
    static inline const QColor PROMPT_TEXT{"#38BDF8"};
};

std::string to_lower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

} // anonymous namespace

TopModuleBoxItem::TopModuleBoxItem(const SchematicModule& module, QGraphicsItem* parent)
    : QGraphicsObject(parent), module_(module) {

    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);

    calculate_layout();

    std::string tip = "Module: " + module_.name + "\n" +
                      "Instances: " + std::to_string(module_.cells.size()) + "\n" +
                      "Ports: " + std::to_string(module_.ports.size()) + "\n" +
                      "Nets: " + std::to_string(module_.nets.size());
    setToolTip(QString::fromStdString(tip));
}

void TopModuleBoxItem::calculate_layout() {
    input_ports_.clear();
    output_ports_.clear();
    input_pin_coords_.clear();
    output_pin_coords_.clear();

    for (const auto& [_, port] : module_.ports) {
        if (port.name.find('[') == std::string::npos && module_.ports.find(port.name + "[0]") != module_.ports.end()) {
            continue;
        }
        if (port.direction == PinDirection::INPUT || port.direction == PinDirection::INOUT) {
            input_ports_.push_back(port);
        } else {
            output_ports_.push_back(port);
        }
    }

    // Sort inputs: clocks & resets first, then alphabetical
    std::sort(input_ports_.begin(), input_ports_.end(), [](const SchematicPin& a, const SchematicPin& b) {
        std::string la = to_lower(a.name);
        std::string lb = to_lower(b.name);
        int pri_a = (a.is_clock || la.find("clk") != std::string::npos) ? 0
                  : (a.is_reset || la.find("rst") != std::string::npos || la.find("reset") != std::string::npos) ? 1 : 2;
        int pri_b = (b.is_clock || lb.find("clk") != std::string::npos) ? 0
                  : (b.is_reset || lb.find("rst") != std::string::npos || lb.find("reset") != std::string::npos) ? 1 : 2;
        if (pri_a != pri_b) return pri_a < pri_b;
        return a.name < b.name;
    });

    // Sort outputs: alphabetical
    std::sort(output_ports_.begin(), output_ports_.end(), [](const SchematicPin& a, const SchematicPin& b) {
        return a.name < b.name;
    });

    size_t max_pins = std::max({input_ports_.size(), output_ports_.size(), size_t{1}});
    double needed_height = header_height_ + (max_pins * pin_pitch_) + footer_height_;

    // Make the box a square as requested by user, with enlarged minimum dimensions
    double side = std::max(800.0, needed_height);
    box_width_ = side;
    box_height_ = side;

    double pins_height = box_height_ - header_height_ - footer_height_;

    // Compute input pin coordinates (West Flank)
    double in_pitch = pin_pitch_;
    if (!input_ports_.empty() && input_ports_.size() < 16) {
        in_pitch = std::max(pin_pitch_, std::min(48.0, pins_height / (input_ports_.size() + 1)));
    }
    double in_total_span = input_ports_.empty() ? 0.0 : (input_ports_.size() - 1) * in_pitch;
    double in_start_y = header_height_ + (pins_height - in_total_span) / 2.0;

    for (size_t i = 0; i < input_ports_.size(); ++i) {
        double y = in_start_y + (i * in_pitch);
        QPointF term_pt(-stub_length_, y);
        QPointF box_pt(0.0, y);
        input_pin_coords_[input_ports_[i].name] = {term_pt, box_pt};
    }

    // Compute output pin coordinates (East Flank)
    double out_pitch = pin_pitch_;
    if (!output_ports_.empty() && output_ports_.size() < 16) {
        out_pitch = std::max(pin_pitch_, std::min(48.0, pins_height / (output_ports_.size() + 1)));
    }
    double out_total_span = output_ports_.empty() ? 0.0 : (output_ports_.size() - 1) * out_pitch;
    double out_start_y = header_height_ + (pins_height - out_total_span) / 2.0;

    for (size_t i = 0; i < output_ports_.size(); ++i) {
        double y = out_start_y + (i * out_pitch);
        QPointF box_pt(box_width_, y);
        QPointF term_pt(box_width_ + stub_length_, y);
        output_pin_coords_[output_ports_[i].name] = {term_pt, box_pt};
    }
}

std::string TopModuleBoxItem::format_port_label(const SchematicPin& port) const {
    if (port.name.find('[') != std::string::npos) {
        return port.name;
    }
    if (port.bit_index >= 0) {
        return port.name + "[" + std::to_string(port.bit_index) + "]";
    }
    return port.name;
}

QRectF TopModuleBoxItem::boundingRect() const {
    double margin = 20.0;
    return QRectF(-stub_length_ - margin,
                  -margin,
                  box_width_ + 2.0 * (stub_length_ + margin),
                  box_height_ + 2.0 * margin);
}

void TopModuleBoxItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void TopModuleBoxItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void TopModuleBoxItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        event->accept();
        emit expand_requested();
    } else {
        QGraphicsObject::mouseDoubleClickEvent(event);
    }
}

void TopModuleBoxItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    double lod = option ? option->levelOfDetailFromTransform(painter->worldTransform()) : 1.0;

    // 1. Box Body & Border
    QRectF box_rect(0.0, 0.0, box_width_, box_height_);
    QColor border_color = isSelected() ? TopBoxPalette::BOX_BORDER_SELECTED
                        : is_hovered_ ? TopBoxPalette::BOX_BORDER_HOVER
                        : TopBoxPalette::BOX_BORDER;
    double border_width = (isSelected() || is_hovered_) ? 2.5 : 2.0;

    painter->setPen(QPen(border_color, border_width));
    painter->setBrush(QBrush(TopBoxPalette::BOX_FILL));
    painter->drawRoundedRect(box_rect, 8.0, 8.0);

    // 2. Header Bar
    QPainterPath header_path;
    header_path.moveTo(0.0, 8.0);
    header_path.arcTo(0.0, 0.0, 16.0, 16.0, 180.0, -90.0);
    header_path.lineTo(box_width_ - 8.0, 0.0);
    header_path.arcTo(box_width_ - 16.0, 0.0, 16.0, 16.0, 90.0, -90.0);
    header_path.lineTo(box_width_, header_height_);
    header_path.lineTo(0.0, header_height_);
    header_path.closeSubpath();

    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(TopBoxPalette::HEADER_FILL));
    painter->drawPath(header_path);

    // Header divider line
    painter->setPen(QPen(border_color.darker(130), 1.5));
    painter->drawLine(QPointF(0.0, header_height_), QPointF(box_width_, header_height_));

    // 3. Header Text & Badges
    painter->setPen(TopBoxPalette::HEADER_TEXT);
    QFont font_title("Monospace", 15, QFont::Bold);
    font_title.setStyleHint(QFont::TypeWriter);
    painter->setFont(font_title);
    painter->drawText(QRectF(15.0, 10.0, box_width_ - 30.0, 30.0),
                      Qt::AlignCenter,
                      QString::fromStdString(module_.name));

    QFont font_sub("SansSerif", 9);
    painter->setFont(font_sub);
    painter->setPen(TopBoxPalette::SUBTITLE_TEXT);
    std::string subtitle = "Structural Verilog Design | " +
                           std::to_string(module_.cells.size()) + " Instances | " +
                           std::to_string(module_.ports.size()) + " I/O Ports | " +
                           std::to_string(module_.nets.size()) + " Nets";
    painter->drawText(QRectF(15.0, 42.0, box_width_ - 30.0, 18.0),
                      Qt::AlignCenter,
                      QString::fromStdString(subtitle));

    // Top module badge
    QRectF badge_rect(box_width_ / 2.0 - 75.0, 63.0, 150.0, 16.0);
    painter->setPen(Qt::NoPen);
    painter->setBrush(TopBoxPalette::BADGE_FILL);
    painter->drawRoundedRect(badge_rect, 4.0, 4.0);

    QFont font_badge("Monospace", 8, QFont::Bold);
    painter->setFont(font_badge);
    painter->setPen(TopBoxPalette::BADGE_TEXT);
    painter->drawText(badge_rect, Qt::AlignCenter, "TOP MODULE BLOCK");

    // 4. Input Ports (West Flank)
    QFont font_pin("Monospace", 8);
    font_pin.setStyleHint(QFont::TypeWriter);
    painter->setFont(font_pin);

    for (const auto& port : input_ports_) {
        const auto& [term_pt, box_pt] = input_pin_coords_[port.name];
        double y = term_pt.y();

        bool is_clk = port.is_clock || (to_lower(port.name).find("clk") != std::string::npos);
        bool is_rst = port.is_reset || (to_lower(port.name).find("rst") != std::string::npos);
        QColor wire_col = is_clk ? TopBoxPalette::PIN_CLOCK : is_rst ? TopBoxPalette::PIN_RESET : TopBoxPalette::PIN_LINE;

        // Stub line
        painter->setPen(QPen(wire_col, 1.6));
        painter->drawLine(term_pt, box_pt);

        // Terminal dot
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(wire_col));
        painter->drawEllipse(term_pt, 3.5, 3.5);

        // Entering arrowhead at box edge
        QPainterPath tri;
        tri.moveTo(box_pt.x() - 6.0, y - 3.5);
        tri.lineTo(box_pt.x() - 1.0, y);
        tri.lineTo(box_pt.x() - 6.0, y + 3.5);
        tri.closeSubpath();
        painter->drawPath(tri);

        // Clock internal chevron inside box
        if (is_clk) {
            QPainterPath chevron;
            chevron.moveTo(0.0, y - 6.0);
            chevron.lineTo(8.0, y);
            chevron.lineTo(0.0, y + 6.0);
            painter->setPen(QPen(TopBoxPalette::PIN_CLOCK, 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(chevron);
        }

        // Port label inside box
        if (lod >= 0.25) {
            QColor text_col = is_clk ? TopBoxPalette::PIN_CLOCK
                            : is_rst ? TopBoxPalette::PIN_RESET
                            : (port.name.find('[') != std::string::npos) ? TopBoxPalette::PIN_BUS_TEXT
                            : TopBoxPalette::PIN_TEXT;
            painter->setPen(text_col);
            double label_x = is_clk ? 14.0 : 8.0;
            std::string label_str = format_port_label(port);
            painter->drawText(QRectF(label_x, y - 10.0, box_width_ * 0.45 - label_x, 20.0),
                              Qt::AlignVCenter | Qt::AlignLeft,
                              QString::fromStdString(label_str));
        }
    }

    // 5. Output Ports (East Flank)
    for (const auto& port : output_ports_) {
        const auto& [term_pt, box_pt] = output_pin_coords_[port.name];
        double y = term_pt.y();

        QColor wire_col = TopBoxPalette::PIN_LINE;

        // Stub line
        painter->setPen(QPen(wire_col, 1.6));
        painter->drawLine(box_pt, term_pt);

        // Terminal dot
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(wire_col));
        painter->drawEllipse(term_pt, 3.5, 3.5);

        // Exiting arrowhead at stub terminal
        QPainterPath tri;
        tri.moveTo(term_pt.x() - 6.0, y - 3.5);
        tri.lineTo(term_pt.x() - 1.0, y);
        tri.lineTo(term_pt.x() - 6.0, y + 3.5);
        tri.closeSubpath();
        painter->drawPath(tri);

        // Port label inside box
        if (lod >= 0.25) {
            QColor text_col = (port.name.find('[') != std::string::npos) ? TopBoxPalette::PIN_BUS_TEXT : TopBoxPalette::PIN_TEXT;
            painter->setPen(text_col);
            std::string label_str = format_port_label(port);
            painter->drawText(QRectF(box_width_ * 0.55, y - 10.0, box_width_ * 0.45 - 8.0, 20.0),
                              Qt::AlignVCenter | Qt::AlignRight,
                              QString::fromStdString(label_str));
        }
    }

    // 6. Footer Status Hint
    QFont font_foot("SansSerif", 8);
    painter->setFont(font_foot);
    painter->setPen(TopBoxPalette::SUBTITLE_TEXT);
    painter->drawText(QRectF(15.0, box_height_ - footer_height_, box_width_ - 30.0, footer_height_),
                      Qt::AlignCenter,
                      "Top Module View | Primary I/O Interfaces Only");
}

} // namespace vajra::gui
