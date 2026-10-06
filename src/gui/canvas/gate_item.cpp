#include "vajra/gui/canvas/gate_item.hpp"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneHoverEvent>
#include <cmath>

namespace vajra::gui {

namespace {

struct Palette {
    static inline const QColor BG_DARK{"#18191F"};
    static inline const QColor GATE_BODY{"#22252E"};
    static inline const QColor GATE_BORDER{"#4FC3F7"};
    static inline const QColor GATE_BORDER_SELECTED{"#FFEB3B"};
    static inline const QColor GATE_BORDER_HOVER{"#00E676"};
    static inline const QColor GATE_FILL{"#282C37"};
    static inline const QColor DFF_FILL{"#1E2836"};
    static inline const QColor SDFF_FILL{"#182B3A"};
    static inline const QColor LATCH_FILL{"#2A261D"};
    static inline const QColor PORT_IN_FILL{"#1B382B"};
    static inline const QColor PORT_IN_BORDER{"#00E676"};
    static inline const QColor PORT_OUT_FILL{"#3E2723"};
    static inline const QColor PORT_OUT_BORDER{"#FF7043"};
    static inline const QColor TEXT_PRIMARY{"#E0E0E0"};
    static inline const QColor TEXT_MUTED{"#90A4AE"};
    static inline const QColor PIN_COLOR{"#81D4FA"};
    static inline const QColor BUBBLE_FILL{"#18191F"};
    static inline const QColor CLOCK_TRIANGLE{"#FFB74D"};
    static inline const QColor BADGE_DFF{"#00E676"};
    static inline const QColor BADGE_SDFF{"#00E5FF"};
    static inline const QColor BADGE_LATCH{"#FFB300"};
    static inline const QColor PIN_LABEL_TEXT{"#B0BEC5"};
};

} // anonymous namespace

GateItem::GateItem(const SchematicCell& cell,
                   const std::vector<PinLocation>& pins,
                   double node_x,
                   double node_y,
                   double node_width,
                   double node_height,
                   QGraphicsItem* parent)
    : QGraphicsItem(parent), cell_(cell) {

    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);

    if (cell_.type == NodeType::PRIMARY_INPUT) {
        classification_.gate_type = GateType::PRIMARY_INPUT;
        classification_.description = "Primary Input Port";
    } else if (cell_.type == NodeType::PRIMARY_OUTPUT) {
        classification_.gate_type = GateType::PRIMARY_OUTPUT;
        classification_.description = "Primary Output Port";
    } else {
        auto it_type = cell_.attributes.find("symbol_gate_type");
        if (it_type != cell_.attributes.end()) {
            classification_.gate_type = SymbolClassifier::string_to_gate_type(it_type->second);
            auto it_desc = cell_.attributes.find("symbol_description");
            if (it_desc != cell_.attributes.end()) classification_.description = it_desc->second;
            if (cell_.attributes.find("is_inverting") != cell_.attributes.end()) classification_.is_inverting = true;
            if (cell_.attributes.find("is_scan") != cell_.attributes.end()) classification_.is_scan = (cell_.attributes.at("is_scan") == "true");
            if (cell_.attributes.find("is_latch") != cell_.attributes.end()) classification_.is_latch = (cell_.attributes.at("is_latch") == "true");
            if (cell_.attributes.find("is_falling_edge") != cell_.attributes.end()) classification_.is_falling_edge = (cell_.attributes.at("is_falling_edge") == "true");
            if (cell_.attributes.find("has_reset") != cell_.attributes.end()) classification_.has_reset = (cell_.attributes.at("has_reset") == "true");
            if (cell_.attributes.find("has_preset") != cell_.attributes.end()) classification_.has_preset = (cell_.attributes.at("has_preset") == "true");
            if (cell_.attributes.find("is_reset_active_low") != cell_.attributes.end()) classification_.is_reset_active_low = (cell_.attributes.at("is_reset_active_low") == "true");
            if (cell_.attributes.find("is_preset_active_low") != cell_.attributes.end()) classification_.is_preset_active_low = (cell_.attributes.at("is_preset_active_low") == "true");
            if (cell_.attributes.find("reset_pin") != cell_.attributes.end()) classification_.reset_pin = cell_.attributes.at("reset_pin");
            if (cell_.attributes.find("preset_pin") != cell_.attributes.end()) classification_.preset_pin = cell_.attributes.at("preset_pin");
            if (cell_.attributes.find("scan_in_pin") != cell_.attributes.end()) classification_.scan_in_pin = cell_.attributes.at("scan_in_pin");
            if (cell_.attributes.find("scan_enable_pin") != cell_.attributes.end()) classification_.scan_enable_pin = cell_.attributes.at("scan_enable_pin");
            if (cell_.attributes.find("qn_pin") != cell_.attributes.end()) classification_.qn_pin = cell_.attributes.at("qn_pin");

            for (const auto& pin : cell_.pins) {
                if (pin.is_inverted) {
                    if (std::find(classification_.bubble_pins.begin(), classification_.bubble_pins.end(), pin.name) == classification_.bubble_pins.end()) {
                        classification_.bubble_pins.push_back(pin.name);
                    }
                }
                if (pin.is_clock) {
                    classification_.has_clock = true;
                    classification_.clock_pin = pin.name;
                }
                if (pin.is_reset) {
                    classification_.has_reset = true;
                    classification_.reset_pin = pin.name;
                }
            }
        } else {
            classification_ = SymbolClassifier::classify(cell_.cell_type);
        }
    }

    setup_dimensions_and_pins(pins, node_x, node_y, node_width, node_height);
    build_vector_paths();

    std::string tip = "Instance: " + cell_.name + "\n" +
                      "Type: " + cell_.cell_type + "\n" +
                      "Symbol: " + SymbolClassifier::gate_type_to_string(classification_.gate_type);
    setToolTip(QString::fromStdString(tip));
}

void GateItem::setup_dimensions_and_pins(const std::vector<PinLocation>& pins,
                                         double node_x, double node_y,
                                         double node_width, double node_height) {
    if (node_width > 0.0 && node_height > 0.0) {
        width_ = node_width;
        height_ = node_height;
    } else {
        bool is_bus = (cell_.name.find('[') != std::string::npos);
        switch (classification_.gate_type) {
            case GateType::NOT:
            case GateType::BUF:
                width_ = 60.0;
                height_ = 40.0;
                break;
            case GateType::DFF:
            case GateType::SDFF:
            case GateType::LATCH:
                width_ = (classification_.gate_type == GateType::SDFF) ? 110.0 : 100.0;
                height_ = 70.0;
                break;
            case GateType::PRIMARY_INPUT:
            case GateType::PRIMARY_OUTPUT:
                width_ = is_bus ? 110.0 : 75.0;
                height_ = 28.0;
                break;
            default:
                width_ = 80.0;
                height_ = 50.0;
                break;
        }
    }

    pin_local_positions_.clear();
    clk_pin_pos_.reset();

    // If external pins from PinResolver are provided, use their exact coordinates!
    if (!pins.empty()) {
        for (const auto& p : pins) {
            QPointF local_pos(p.x - node_x, p.y - node_y);
            pin_local_positions_[p.pin_name] = local_pos;
            if (p.role == "CLOCK" || p.pin_name == "CLK" || p.pin_name == "C" || p.pin_name == "clk") {
                clk_pin_pos_ = local_pos;
            }
        }
        return;
    }

    // Fallback: Compute local positions matching PinResolver
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> south_pins;
    std::string clk_pin;
    std::string rst_pin;

    for (const auto& pin : cell_.pins) {
        std::string lp = pin.name;
        std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
        if (pin.is_clock || lp == "c" || lp == "clk" || lp == "cp") {
            clk_pin = pin.name;
        } else if (pin.is_reset || lp.find("rst") != std::string::npos || lp.find("clr") != std::string::npos) {
            rst_pin = pin.name;
        } else if (classification_.gate_type == GateType::MUX && (lp == "s" || lp == "sel")) {
            south_pins.push_back(pin.name);
        } else if (pin.direction == PinDirection::INPUT) {
            inputs.push_back(pin.name);
        } else if (pin.direction == PinDirection::OUTPUT) {
            outputs.push_back(pin.name);
        }
    }

    if (classification_.gate_type == GateType::PRIMARY_INPUT) {
        pin_local_positions_[cell_.name] = QPointF(width_, height_ / 2.0);
        pin_local_positions_["OUT"] = QPointF(width_, height_ / 2.0);
        pin_local_positions_["Y"] = QPointF(width_, height_ / 2.0);
        if (!outputs.empty()) {
            pin_local_positions_[outputs[0]] = QPointF(width_, height_ / 2.0);
        }
        return;
    }

    if (classification_.gate_type == GateType::PRIMARY_OUTPUT) {
        pin_local_positions_[cell_.name] = QPointF(0.0, height_ / 2.0);
        pin_local_positions_["IN"] = QPointF(0.0, height_ / 2.0);
        pin_local_positions_["A"] = QPointF(0.0, height_ / 2.0);
        if (!inputs.empty()) {
            pin_local_positions_[inputs[0]] = QPointF(0.0, height_ / 2.0);
        }
        return;
    }

    if (classification_.gate_type == GateType::DFF ||
        classification_.gate_type == GateType::SDFF ||
        classification_.gate_type == GateType::LATCH) {
        if (!clk_pin.empty()) {
            double clk_x = rst_pin.empty() ? (width_ / 2.0) : 28.0;
            pin_local_positions_[clk_pin] = QPointF(clk_x, height_);
            clk_pin_pos_ = QPointF(clk_x, height_);
        }
        if (!rst_pin.empty()) {
            double rst_x = clk_pin.empty() ? (width_ / 2.0) : (width_ - 28.0);
            pin_local_positions_[rst_pin] = QPointF(rst_x, height_);
        }
        if (!classification_.preset_pin.empty()) {
            pin_local_positions_[classification_.preset_pin] = QPointF(width_ / 2.0, 0.0);
        }
    } else {
        if (!clk_pin.empty()) south_pins.push_back(clk_pin);
        if (!rst_pin.empty()) south_pins.push_back(rst_pin);
        for (size_t i = 0; i < south_pins.size(); ++i) {
            double x = (width_ * (i + 1.0)) / (south_pins.size() + 1.0);
            pin_local_positions_[south_pins[i]] = QPointF(x, height_);
        }
    }

    // Inputs on left flank
    size_t in_count = inputs.size();
    for (size_t i = 0; i < in_count; ++i) {
        double y = (height_ * (i + 1.0)) / (in_count + 1.0);
        pin_local_positions_[inputs[i]] = QPointF(0.0, y);
    }

    // Outputs on right flank
    size_t out_count = outputs.size();
    for (size_t i = 0; i < out_count; ++i) {
        double y = (height_ * (i + 1.0)) / (out_count + 1.0);
        pin_local_positions_[outputs[i]] = QPointF(width_, y);
    }
}

void GateItem::build_vector_paths() {
    gate_path_.clear();
    lead_path_.clear();
    bubble_paths_.clear();

    const double w = width_;
    const double h = height_;
    const double r_bubble = 3.5;

    switch (classification_.gate_type) {
        case GateType::NOT: {
            double tip_x = w - r_bubble * 2.0;
            gate_path_.moveTo(0, 0);
            gate_path_.lineTo(tip_x, h / 2.0);
            gate_path_.lineTo(0, h);
            gate_path_.closeSubpath();

            QPainterPath bp;
            bp.addEllipse(QPointF(w - r_bubble, h / 2.0), r_bubble, r_bubble);
            bubble_paths_.push_back(bp);
            break;
        }
        case GateType::BUF: {
            gate_path_.moveTo(0, 0);
            gate_path_.lineTo(w, h / 2.0);
            gate_path_.lineTo(0, h);
            gate_path_.closeSubpath();
            break;
        }
        case GateType::AND:
        case GateType::NAND: {
            double front_x = (classification_.gate_type == GateType::NAND) ? (w - r_bubble * 2.0) : w;
            double mid_x = front_x * 0.5;

            gate_path_.moveTo(0, 0);
            gate_path_.lineTo(mid_x, 0);
            gate_path_.arcTo(mid_x - front_x * 0.5, 0, front_x, h, 90.0, -180.0);
            gate_path_.lineTo(0, h);
            gate_path_.closeSubpath();

            if (classification_.gate_type == GateType::NAND) {
                QPainterPath bp;
                bp.addEllipse(QPointF(w - r_bubble, h / 2.0), r_bubble, r_bubble);
                bubble_paths_.push_back(bp);
            }
            break;
        }
        case GateType::OR:
        case GateType::NOR: {
            double tip_x = (classification_.gate_type == GateType::NOR) ? (w - r_bubble * 2.0) : w;

            gate_path_.moveTo(0, 0);
            gate_path_.quadTo(w * 0.22, h / 2.0, 0, h);
            gate_path_.quadTo(w * 0.6, h * 0.95, tip_x, h / 2.0);
            gate_path_.quadTo(w * 0.6, h * 0.05, 0, 0);
            gate_path_.closeSubpath();

            if (classification_.gate_type == GateType::NOR) {
                QPainterPath bp;
                bp.addEllipse(QPointF(w - r_bubble, h / 2.0), r_bubble, r_bubble);
                bubble_paths_.push_back(bp);
            }

            // Lead lines connecting West input pins to the curved back
            for (const auto& [name, pos] : pin_local_positions_) {
                if (pos.x() <= 1.0) {
                    double t = std::max(0.0, std::min(1.0, pos.y() / h));
                    double curve_x = 2.0 * (1.0 - t) * t * (w * 0.22);
                    if (curve_x > 1.0) {
                        lead_path_.moveTo(pos.x(), pos.y());
                        lead_path_.lineTo(curve_x, pos.y());
                    }
                }
            }
            break;
        }
        case GateType::XOR:
        case GateType::XNOR: {
            double tip_x = (classification_.gate_type == GateType::XNOR) ? (w - r_bubble * 2.0) : w;

            // Main OR body
            gate_path_.moveTo(w * 0.1, 0);
            gate_path_.quadTo(w * 0.32, h / 2.0, w * 0.1, h);
            gate_path_.quadTo(w * 0.65, h * 0.95, tip_x, h / 2.0);
            gate_path_.quadTo(w * 0.65, h * 0.05, w * 0.1, 0);
            gate_path_.closeSubpath();

            // Offset detached rear arc
            gate_path_.moveTo(0, 0);
            gate_path_.quadTo(w * 0.22, h / 2.0, 0, h);

            if (classification_.gate_type == GateType::XNOR) {
                QPainterPath bp;
                bp.addEllipse(QPointF(w - r_bubble, h / 2.0), r_bubble, r_bubble);
                bubble_paths_.push_back(bp);
            }

            // Lead lines connecting West input pins through detached arc to main body
            for (const auto& [name, pos] : pin_local_positions_) {
                if (pos.x() <= 1.0) {
                    double t = std::max(0.0, std::min(1.0, pos.y() / h));
                    double body_x = w * 0.1 + 2.0 * (1.0 - t) * t * (w * 0.22);
                    if (body_x > 1.0) {
                        lead_path_.moveTo(pos.x(), pos.y());
                        lead_path_.lineTo(body_x, pos.y());
                    }
                }
            }
            break;
        }
        case GateType::MUX: {
            gate_path_.moveTo(0, 0);
            gate_path_.lineTo(w, h * 0.2);
            gate_path_.lineTo(w, h * 0.8);
            gate_path_.lineTo(0, h);
            gate_path_.closeSubpath();

            // Lead lines connecting South select pins to the slanted bottom
            for (const auto& [name, pos] : pin_local_positions_) {
                if (std::abs(pos.y() - h) <= 1.5) {
                    double body_y = h * (1.0 - 0.2 * std::max(0.0, std::min(1.0, pos.x() / w)));
                    if (h - body_y > 1.0) {
                        lead_path_.moveTo(pos.x(), body_y);
                        lead_path_.lineTo(pos.x(), pos.y());
                    }
                }
            }
            break;
        }
        case GateType::PRIMARY_INPUT: {
            // [ > ] badge pointing into circuit
            gate_path_.moveTo(0, 0);
            gate_path_.lineTo(w - 10.0, 0);
            gate_path_.lineTo(w, h / 2.0);
            gate_path_.lineTo(w - 10.0, h);
            gate_path_.lineTo(0, h);
            gate_path_.closeSubpath();
            break;
        }
        case GateType::PRIMARY_OUTPUT: {
            // [ > ] badge pointing away from circuit
            gate_path_.moveTo(0, h / 2.0);
            gate_path_.lineTo(10.0, 0);
            gate_path_.lineTo(w, 0);
            gate_path_.lineTo(w, h);
            gate_path_.lineTo(10.0, h);
            gate_path_.closeSubpath();
            break;
        }
        case GateType::DFF:
        case GateType::SDFF:
        case GateType::LATCH:
        default: {
            gate_path_.addRoundedRect(QRectF(0, 0, w, h), 4.0, 4.0);
            if (classification_.gate_type == GateType::SDFF) {
                // Add vertical partition line for Scan Multiplexer
                lead_path_.moveTo(26.0, 3.0);
                lead_path_.lineTo(26.0, h - 3.0);
            }
            break;
        }
    }

    // Pin-level inversion bubbles for active-low / inverted terminals (placed outside of cell outline)
    for (const auto& bp : classification_.bubble_pins) {
        auto it = pin_local_positions_.find(bp);
        if (it == pin_local_positions_.end()) {
            std::string lower_bp = bp;
            std::transform(lower_bp.begin(), lower_bp.end(), lower_bp.begin(), ::tolower);
            for (auto pit = pin_local_positions_.begin(); pit != pin_local_positions_.end(); ++pit) {
                std::string lp = pit->first;
                std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
                if (lp == lower_bp) {
                    it = pit;
                    break;
                }
            }
        }
        if (it == pin_local_positions_.end()) continue;
        QPointF pos = it->second;
        QPainterPath bpath;
        if (pos.y() >= h - 2.0) {
            // South pin (e.g. active-low reset or falling-edge clock) -> exterior below bottom edge
            bpath.addEllipse(QPointF(pos.x(), h + r_bubble), r_bubble, r_bubble);
        } else if (pos.y() <= 2.0) {
            // North pin (e.g. active-low preset) -> exterior above top edge
            bpath.addEllipse(QPointF(pos.x(), -r_bubble), r_bubble, r_bubble);
        } else if (pos.x() >= w - 2.0) {
            // East pin (e.g. QN) -> exterior to the right of right edge
            if (classification_.gate_type != GateType::NOT &&
                classification_.gate_type != GateType::NAND &&
                classification_.gate_type != GateType::NOR &&
                classification_.gate_type != GateType::XNOR) {
                bpath.addEllipse(QPointF(w + r_bubble, pos.y()), r_bubble, r_bubble);
            }
        } else if (pos.x() <= 2.0) {
            // West pin -> exterior to the left of left edge
            bpath.addEllipse(QPointF(-r_bubble, pos.y()), r_bubble, r_bubble);
        }
        if (!bpath.isEmpty()) {
            bubble_paths_.push_back(bpath);
        }
    }
}

QRectF GateItem::boundingRect() const {
    return QRectF(-14.0, -14.0, width_ + 28.0, height_ + 28.0);
}

QPainterPath GateItem::shape() const {
    QPainterPath p;
    p.addRect(-7.0, -7.0, width_ + 14.0, height_ + 14.0);
    return p;
}

QPointF GateItem::get_pin_local_pos(const std::string& pin_name) const {
    auto it = pin_local_positions_.find(pin_name);
    if (it != pin_local_positions_.end()) {
        return it->second;
    }
    // Fallback: default to center of left flank
    return QPointF(0.0, height_ / 2.0);
}

QPointF GateItem::get_pin_scene_pos(const std::string& pin_name) const {
    return mapToScene(get_pin_local_pos(pin_name));
}

void GateItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = true;
    update();
    QGraphicsItem::hoverEnterEvent(event);
}

void GateItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    is_hovered_ = false;
    update();
    QGraphicsItem::hoverLeaveEvent(event);
}

void GateItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    double lod = option->levelOfDetailFromTransform(painter->worldTransform());

    // Outline pen
    QPen pen;
    if (isSelected()) {
        pen = QPen(Palette::GATE_BORDER_SELECTED, 2.4);
    } else if (is_hovered_) {
        pen = QPen(Palette::GATE_BORDER_HOVER, 2.0);
    } else if (classification_.gate_type == GateType::PRIMARY_INPUT) {
        pen = QPen(Palette::PORT_IN_BORDER, 1.4);
    } else if (classification_.gate_type == GateType::PRIMARY_OUTPUT) {
        pen = QPen(Palette::PORT_OUT_BORDER, 1.4);
    } else {
        pen = QPen(Palette::GATE_BORDER, 1.4);
    }
    painter->setPen(pen);

    // Fast rendering for zoomed far out
    if (lod < 0.15) {
        painter->setBrush(QBrush(Palette::GATE_FILL));
        painter->drawRect(QRectF(0, 0, width_, height_));
        return;
    }

    // Body fill brush
    QBrush brush;
    if (classification_.gate_type == GateType::PRIMARY_INPUT) {
        brush = QBrush(Palette::PORT_IN_FILL);
    } else if (classification_.gate_type == GateType::PRIMARY_OUTPUT) {
        brush = QBrush(Palette::PORT_OUT_FILL);
    } else if (classification_.gate_type == GateType::SDFF) {
        brush = QBrush(Palette::SDFF_FILL);
    } else if (classification_.gate_type == GateType::LATCH) {
        brush = QBrush(Palette::LATCH_FILL);
    } else if (classification_.gate_type == GateType::DFF) {
        brush = QBrush(Palette::DFF_FILL);
    } else {
        brush = QBrush(Palette::GATE_FILL);
    }
    painter->setBrush(brush);

    // Draw main vector gate shape
    painter->drawPath(gate_path_);

    // Draw pin lead lines for curved/slanted symbols (OR, NOR, XOR, XNOR, MUX, SDFF partition)
    if (!lead_path_.isEmpty()) {
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(lead_path_);
    }

    // Draw inverting bubbles
    if (!bubble_paths_.empty()) {
        painter->setBrush(QBrush(Palette::BUBBLE_FILL));
        for (const auto& bp : bubble_paths_) {
            painter->drawPath(bp);
        }
    }

    // Draw internal clock triangle on South/bottom edge pointing up (DFF and SDFF only; Latches are level-sensitive)
    if ((classification_.gate_type == GateType::DFF || classification_.gate_type == GateType::SDFF) && lod >= 0.25) {
        double local_clk_x = 28.0;
        if (clk_pin_pos_.has_value()) {
            local_clk_x = clk_pin_pos_->x();
        }
        double base_y = height_;
        double peak_y = height_ - 8.0;
        QPainterPath chevron;
        chevron.moveTo(local_clk_x - 5.0, base_y);
        chevron.lineTo(local_clk_x, peak_y);
        chevron.lineTo(local_clk_x + 5.0, base_y);

        painter->setPen(QPen(Palette::CLOCK_TRIANGLE, 1.6));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(chevron);
    }

    // Draw pin connection terminal dots (suppressed on pins with inversion bubbles)
    if (lod >= 0.3) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(Palette::PIN_COLOR));
        for (const auto& [name, pos] : pin_local_positions_) {
            bool is_bubble = std::any_of(classification_.bubble_pins.begin(),
                                         classification_.bubble_pins.end(),
                                         [&](const std::string& bp) {
                                             if (name == bp) return true;
                                             std::string s1 = name, s2 = bp;
                                             std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
                                             std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
                                             return s1 == s2;
                                         });
            if (!is_bubble) {
                painter->drawEllipse(pos, 2.5, 2.5);
            }
        }
    }

    // Render sequential badges ([DFF], [SDFF], [LATCH])
    if (lod >= 0.2) {
        render_sequential_decorations(painter, lod);
    }

    // Render internal pin labels (D, Q, QN, CLK, RST, PRE, SI, SE)
    if (lod >= 0.25) {
        render_pin_labels(painter, lod);
    }

    // Render instance / port text labels
    if (lod >= 0.25) {
        render_labels(painter, lod);
    }
}

void GateItem::render_sequential_decorations(QPainter* painter, double lod) {
    if (classification_.gate_type != GateType::DFF &&
        classification_.gate_type != GateType::SDFF &&
        classification_.gate_type != GateType::LATCH) {
        return;
    }

    QFont badge_font("Monospace", 6, QFont::Bold);
    badge_font.setStyleHint(QFont::TypeWriter);
    painter->setFont(badge_font);

    QString badge_text;
    QColor badge_color;
    if (classification_.gate_type == GateType::SDFF) {
        badge_text = "[SDFF]";
        badge_color = Palette::BADGE_SDFF;
    } else if (classification_.gate_type == GateType::LATCH) {
        badge_text = "[LATCH]";
        badge_color = Palette::BADGE_LATCH;
    } else {
        badge_text = "[DFF]";
        badge_color = Palette::BADGE_DFF;
    }

    painter->setPen(QPen(badge_color));
    QRectF badge_rect(4.0, 3.0, width_ - 8.0, 11.0);
    painter->drawText(badge_rect, Qt::AlignTop | Qt::AlignHCenter, badge_text);

    // For SDFF: Draw Scan MUX annotation in left MUX partition
    if (classification_.gate_type == GateType::SDFF && lod >= 0.3) {
        QFont mux_font("Monospace", 5, QFont::Normal);
        painter->setFont(mux_font);
        painter->setPen(QPen(Palette::TEXT_MUTED));
        QRectF mux_rect(3.0, 5.0, 22.0, 10.0);
        painter->drawText(mux_rect, Qt::AlignCenter, "MUX");
    }
}

void GateItem::render_pin_labels(QPainter* painter, double /*lod*/) {
    if (classification_.gate_type != GateType::DFF &&
        classification_.gate_type != GateType::SDFF &&
        classification_.gate_type != GateType::LATCH) {
        return;
    }

    QFont pfont("Monospace", 6, QFont::Bold);
    pfont.setStyleHint(QFont::TypeWriter);
    painter->setFont(pfont);
    painter->setPen(QPen(Palette::PIN_LABEL_TEXT));

    for (const auto& [pname, pos] : pin_local_positions_) {
        std::string lp = pname;
        std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);

        // West Inputs
        if (pos.x() <= 2.0) {
            std::string label = pname;
            if (lp == "d" || lp == "data" || lp == "din") label = "D";
            else if (lp == "si" || lp == "scd" || lp == "ti") label = "SI";
            else if (lp == "se" || lp == "sce" || lp == "te") label = "SE";
            QRectF r(6.0, pos.y() - 6.0, 20.0, 12.0);
            painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(label));
        }
        // East Outputs
        else if (pos.x() >= width_ - 2.0) {
            std::string label = pname;
            if (lp == "q" || lp == "out") label = "Q";
            else if (lp == "qn" || lp == "q_n" || lp == "qb") label = "QN";
            QRectF r(width_ - 26.0, pos.y() - 6.0, 20.0, 12.0);
            painter->drawText(r, Qt::AlignVCenter | Qt::AlignRight, QString::fromStdString(label));
        }
        // South Pins (Reset, Clock/Enable)
        else if (pos.y() >= height_ - 2.0) {
            if (pname == classification_.reset_pin || lp.find("rst") != std::string::npos || lp.find("clr") != std::string::npos) {
                std::string label = classification_.is_reset_active_low ? "RN" : "RST";
                QRectF r(pos.x() - 14.0, height_ - 20.0, 28.0, 12.0);
                painter->drawText(r, Qt::AlignCenter, QString::fromStdString(label));
            } else if (classification_.gate_type == GateType::LATCH &&
                       (pname == classification_.clock_pin || lp == "e" || lp == "en" || lp == "gate" || lp == "g")) {
                QRectF r(pos.x() - 10.0, height_ - 18.0, 20.0, 12.0);
                painter->drawText(r, Qt::AlignCenter, "G");
            }
        }
        // North Pins (Preset)
        else if (pos.y() <= 2.0) {
            if (pname == classification_.preset_pin || lp.find("pre") != std::string::npos || lp.find("set") != std::string::npos) {
                std::string label = classification_.is_preset_active_low ? "SN" : "SET";
                QRectF r(pos.x() - 14.0, 10.0, 28.0, 12.0);
                painter->drawText(r, Qt::AlignCenter, QString::fromStdString(label));
            }
        }
    }
}

void GateItem::render_labels(QPainter* painter, double lod) {
    QFont font("Monospace", 8, QFont::Bold);
    font.setStyleHint(QFont::TypeWriter);
    painter->setFont(font);
    painter->setPen(QPen(Palette::TEXT_PRIMARY));

    std::string display_name = cell_.name;
    if (display_name.empty()) {
        display_name = cell_.cell_type;
    }
    if (display_name.size() > 14) {
        display_name = display_name.substr(0, 12) + "..";
    }

    bool is_seq = (classification_.gate_type == GateType::DFF ||
                   classification_.gate_type == GateType::SDFF ||
                   classification_.gate_type == GateType::LATCH);

    double left_margin = (classification_.gate_type == GateType::SDFF) ? 28.0 : (is_seq ? 22.0 : 2.0);
    double right_margin = is_seq ? 22.0 : 2.0;
    QRectF label_rect(left_margin, is_seq ? 15.0 : 2.0, width_ - left_margin - right_margin, is_seq ? (height_ - 30.0) : (height_ - 4.0));
    painter->drawText(label_rect, Qt::AlignCenter, QString::fromStdString(display_name));

    // Show cell type below name if high zoom
    if (lod >= 0.55 && classification_.gate_type != GateType::PRIMARY_INPUT &&
        classification_.gate_type != GateType::PRIMARY_OUTPUT && !cell_.cell_type.empty()) {
        QFont subfont("Monospace", 6);
        painter->setFont(subfont);
        painter->setPen(QPen(Palette::TEXT_MUTED));

        std::string subtext = cell_.cell_type;
        if (subtext.size() > 16) {
            subtext = subtext.substr(0, 14) + "..";
        }
        QRectF subrect(left_margin, height_ - 15.0, width_ - left_margin - right_margin, 12.0);
        painter->drawText(subrect, Qt::AlignCenter, QString::fromStdString(subtext));
    }
}

} // namespace vajra::gui
