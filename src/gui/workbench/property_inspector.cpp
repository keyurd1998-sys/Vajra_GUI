#include "vajra/gui/workbench/property_inspector.hpp"

#include <QHeaderView>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <algorithm>

namespace vajra::gui {

PropertyInspector::PropertyInspector(QWidget* parent)
    : QWidget(parent) {
    setup_ui();
}

void PropertyInspector::setup_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    title_label_ = new QLabel("No item selected", this);
    QFont title_font;
    title_font.setBold(true);
    title_font.setPointSize(11);
    title_label_->setFont(title_font);
    title_label_->setStyleSheet("color: #FFFFFF; font-weight: bold; padding: 4px 2px;");
    layout->addWidget(title_label_);

    attr_table_ = new QTableWidget(this);
    attr_table_->setColumnCount(2);
    attr_table_->setHorizontalHeaderLabels({"Property", "Value"});
    attr_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    attr_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    attr_table_->verticalHeader()->setVisible(false);
    attr_table_->setAlternatingRowColors(true);
    attr_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(attr_table_);

    pins_label_ = new QLabel("Pin Connections", this);
    QFont pins_font;
    pins_font.setBold(true);
    pins_label_->setFont(pins_font);
    pins_label_->setStyleSheet("color: #E2E8F0; font-weight: bold; margin-top: 4px;");
    layout->addWidget(pins_label_);

    pin_table_ = new QTableWidget(this);
    pin_table_->setColumnCount(4);
    pin_table_->setHorizontalHeaderLabels({"Pin", "Dir", "Connected Net", "Role / Info"});
    pin_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    pin_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    pin_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    pin_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    pin_table_->verticalHeader()->setVisible(false);
    pin_table_->setAlternatingRowColors(true);
    pin_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(pin_table_, &QTableWidget::cellDoubleClicked, this, &PropertyInspector::on_pin_table_double_clicked);
    layout->addWidget(pin_table_);

    clear_inspector();
}

void PropertyInspector::populate_attr_table(const std::vector<std::pair<std::string, std::string>>& attrs) {
    attr_table_->setRowCount(static_cast<int>(attrs.size()));
    for (int row = 0; row < static_cast<int>(attrs.size()); ++row) {
        auto* p_item = new QTableWidgetItem(QString::fromStdString(attrs[row].first));
        p_item->setForeground(QBrush(QColor("#CCCCCC")));
        auto* v_item = new QTableWidgetItem(QString::fromStdString(attrs[row].second));
        v_item->setForeground(QBrush(QColor("#FFFFFF")));
        attr_table_->setItem(row, 0, p_item);
        attr_table_->setItem(row, 1, v_item);
    }
}

void PropertyInspector::inspect_cell(const SchematicCell& cell) {
    std::string prefix = cell.is_submodule ? "Submodule: " : "Instance: ";
    if (cell.type == NodeType::PRIMARY_INPUT || cell.type == NodeType::PRIMARY_OUTPUT) {
        prefix = "Port: ";
    }
    title_label_->setText(QString::fromStdString(prefix + cell.name));

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.emplace_back("Instance Name", cell.name);
    attrs.emplace_back("Cell Type", cell.cell_type);

    std::string cat = "Combinational Logic";
    if (cell.is_submodule || cell.type == NodeType::MODULE) {
        cat = "Hierarchical Submodule";
    } else if (cell.type == NodeType::DFF) {
        cat = "Sequential DFF";
    } else if (cell.type == NodeType::LATCH) {
        cat = "Sequential Latch";
    } else if (cell.type == NodeType::PRIMARY_INPUT) {
        cat = "Primary Input";
    } else if (cell.type == NodeType::PRIMARY_OUTPUT) {
        cat = "Primary Output";
    }
    attrs.emplace_back("Category", cat);

    if (cell.is_submodule && !cell.submodule_target.empty()) {
        attrs.emplace_back("Target Module", cell.submodule_target);
    }

    for (const auto& [k, v] : cell.attributes) {
        attrs.emplace_back(k, v);
    }

    size_t in_count = 0;
    size_t out_count = 0;
    for (const auto& pin : cell.pins) {
        if (pin.direction == PinDirection::INPUT) in_count++;
        else out_count++;
    }
    attrs.emplace_back("Fanin Pins", std::to_string(in_count));
    attrs.emplace_back("Fanout Pins", std::to_string(out_count));

    populate_attr_table(attrs);

    // Pin connections table
    pins_label_->setText(QString("Pin Connections (%1)").arg(cell.pins.size()));
    pins_label_->setVisible(true);
    pin_table_->setVisible(true);
    pin_table_->setRowCount(static_cast<int>(cell.pins.size()));

    for (int row = 0; row < static_cast<int>(cell.pins.size()); ++row) {
        const auto& pin = cell.pins[row];
        auto* p_item = new QTableWidgetItem(QString::fromStdString(pin.name));
        p_item->setForeground(QBrush(QColor("#CCCCCC")));
        std::string dir_str = (pin.direction == PinDirection::INPUT) ? "INPUT" :
                              ((pin.direction == PinDirection::OUTPUT) ? "OUTPUT" : "INOUT");
        auto* d_item = new QTableWidgetItem(QString::fromStdString(dir_str));
        d_item->setForeground(QBrush(QColor("#CCCCCC")));

        std::string net_name = pin.net_name.empty() ? "<unconnected>" : pin.net_name;
        auto* n_item = new QTableWidgetItem(QString::fromStdString(net_name));
        n_item->setForeground(QBrush(pin.net_name.empty() ? QColor("#888888") : QColor("#FFFFFF")));

        std::string role_str = "-";
        if (pin.is_clock) role_str = "Clock";
        else if (pin.is_reset) role_str = "Reset";
        else if (pin.is_inverted) role_str = "Inverted";
        auto* r_item = new QTableWidgetItem(QString::fromStdString(role_str));
        r_item->setTextAlignment(Qt::AlignCenter);
        r_item->setForeground(QBrush(QColor("#CCCCCC")));

        pin_table_->setItem(row, 0, p_item);
        pin_table_->setItem(row, 1, d_item);
        pin_table_->setItem(row, 2, n_item);
        pin_table_->setItem(row, 3, r_item);
    }
}

void PropertyInspector::inspect_net(const SchematicNet& net) {
    title_label_->setText(QString::fromStdString("Net: " + net.name));

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.emplace_back("Net Name", net.name);
    attrs.emplace_back("Bus Width", std::to_string(net.width));
    std::string role = "Data";
    if (net.is_clock) role = "Clock";
    else if (net.is_reset) role = "Reset";
    else if (net.is_bus) role = "Bus";
    attrs.emplace_back("Role", role);
    attrs.emplace_back("Driver Pin", net.driver_pin.empty() ? "<none>" : net.driver_pin);
    attrs.emplace_back("Fanout (Loads)", std::to_string(net.sink_pins.size()));

    populate_attr_table(attrs);

    size_t total_pins = (net.driver_pin.empty() ? 0 : 1) + net.sink_pins.size();
    pins_label_->setText(QString("Connected Pins (%1)").arg(total_pins));
    pins_label_->setVisible(true);
    pin_table_->setVisible(true);
    pin_table_->setRowCount(static_cast<int>(total_pins));

    int row = 0;
    if (!net.driver_pin.empty()) {
        auto* drv_pin = new QTableWidgetItem(QString::fromStdString(net.driver_pin));
        drv_pin->setForeground(QBrush(QColor("#CCCCCC")));
        pin_table_->setItem(row, 0, drv_pin);
        auto* drv_role = new QTableWidgetItem("DRIVER");
        drv_role->setForeground(QBrush(QColor("#CCCCCC")));
        pin_table_->setItem(row, 1, drv_role);
        auto* n_item = new QTableWidgetItem(QString::fromStdString(net.name));
        n_item->setForeground(QBrush(QColor("#FFFFFF")));
        pin_table_->setItem(row, 2, n_item);
        auto* src_item = new QTableWidgetItem("Source");
        src_item->setForeground(QBrush(QColor("#CCCCCC")));
        pin_table_->setItem(row, 3, src_item);
        row++;
    }

    for (const auto& sink : net.sink_pins) {
        auto* sink_pin = new QTableWidgetItem(QString::fromStdString(sink));
        sink_pin->setForeground(QBrush(QColor("#CCCCCC")));
        pin_table_->setItem(row, 0, sink_pin);
        auto* load_role = new QTableWidgetItem("LOAD");
        load_role->setForeground(QBrush(QColor("#CCCCCC")));
        pin_table_->setItem(row, 1, load_role);
        auto* n_item = new QTableWidgetItem(QString::fromStdString(net.name));
        n_item->setForeground(QBrush(QColor("#FFFFFF")));
        pin_table_->setItem(row, 2, n_item);
        auto* sink_item = new QTableWidgetItem("Sink");
        sink_item->setForeground(QBrush(QColor("#CCCCCC")));
        pin_table_->setItem(row, 3, sink_item);
        row++;
    }
}

void PropertyInspector::inspect_wire(const NetRoute& route, const SchematicModule* module) {
    if (module) {
        auto it = module->nets.find(route.net_name);
        if (it != module->nets.end()) {
            inspect_net(it->second);
            return;
        }
    }

    title_label_->setText(QString::fromStdString("Net: " + route.net_name));

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.emplace_back("Net Name", route.net_name);
    attrs.emplace_back("Segments", std::to_string(route.segments.size()));
    attrs.emplace_back("Solder Dots", std::to_string(route.solder_dots.size()));
    attrs.emplace_back("Clock", route.is_clock ? "Yes" : "No");
    attrs.emplace_back("Reset", route.is_reset ? "Yes" : "No");
    attrs.emplace_back("High Fanout", route.is_high_fanout ? "Yes" : "No");

    populate_attr_table(attrs);

    pins_label_->setVisible(false);
    pin_table_->setVisible(false);
}

void PropertyInspector::inspect_module(const SchematicModule& module) {
    title_label_->setText(QString::fromStdString("Module: " + module.name));

    size_t dff_count = 0;
    size_t sub_count = 0;
    for (const auto& [cid, cell] : module.cells) {
        if (cell.type == NodeType::DFF || cell.type == NodeType::LATCH) dff_count++;
        if (cell.is_submodule || cell.type == NodeType::MODULE) sub_count++;
    }

    size_t in_ports = 0;
    size_t out_ports = 0;
    for (const auto& [pname, pin] : module.ports) {
        if (pin.direction == PinDirection::INPUT) in_ports++;
        else out_ports++;
    }

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.emplace_back("Module Name", module.name);
    attrs.emplace_back("Total Cells", std::to_string(module.cells.size()));
    attrs.emplace_back("Total Nets", std::to_string(module.nets.size()));
    attrs.emplace_back("Submodules", std::to_string(sub_count));
    attrs.emplace_back("Sequential Flops", std::to_string(dff_count));
    attrs.emplace_back("Primary Inputs", std::to_string(in_ports));
    attrs.emplace_back("Primary Outputs", std::to_string(out_ports));

    populate_attr_table(attrs);

    pins_label_->setVisible(false);
    pin_table_->setVisible(false);
}

void PropertyInspector::inspect_port(const SchematicPin& port) {
    title_label_->setText(QString::fromStdString("Port: " + port.name));

    std::vector<std::pair<std::string, std::string>> attrs;
    attrs.emplace_back("Port Name", port.name);
    std::string dir_str = (port.direction == PinDirection::INPUT) ? "Input" :
                          ((port.direction == PinDirection::OUTPUT) ? "Output" : "Inout");
    attrs.emplace_back("Direction", dir_str);
    attrs.emplace_back("Connected Net", port.net_name.empty() ? "<unconnected>" : port.net_name);
    attrs.emplace_back("Is Clock", port.is_clock ? "Yes" : "No");
    attrs.emplace_back("Is Reset", port.is_reset ? "Yes" : "No");

    populate_attr_table(attrs);

    pins_label_->setVisible(false);
    pin_table_->setVisible(false);
}

void PropertyInspector::clear_inspector() {
    title_label_->setText("No item selected");
    attr_table_->setRowCount(0);
    pins_label_->setVisible(false);
    pin_table_->setRowCount(0);
    pin_table_->setVisible(false);
}

void PropertyInspector::on_pin_table_double_clicked(int row, int column) {
    if (column == 2) {
        auto* item = pin_table_->item(row, column);
        if (item) {
            std::string net_name = item->text().trimmed().toStdString();
            if (!net_name.empty() && net_name != "<unconnected>") {
                emit net_highlight_requested(net_name);
            }
        }
    }
}

} // namespace vajra::gui
