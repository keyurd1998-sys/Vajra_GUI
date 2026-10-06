#pragma once

#include "vajra/gui/models.hpp"
#include "vajra/gui/routing/router_models.hpp"

#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <string>
#include <vector>

namespace vajra::gui {

class PropertyInspector : public QWidget {
    Q_OBJECT
public:
    explicit PropertyInspector(QWidget* parent = nullptr);

    void inspect_cell(const SchematicCell& cell);
    void inspect_net(const SchematicNet& net);
    void inspect_wire(const NetRoute& route, const SchematicModule* module = nullptr);
    void inspect_module(const SchematicModule& module);
    void inspect_port(const SchematicPin& port);
    void clear_inspector();

signals:
    void net_highlight_requested(const std::string& net_name);

private slots:
    void on_pin_table_double_clicked(int row, int column);

private:
    QLabel* title_label_{nullptr};
    QTableWidget* attr_table_{nullptr};
    QLabel* pins_label_{nullptr};
    QTableWidget* pin_table_{nullptr};

    void setup_ui();
    void populate_attr_table(const std::vector<std::pair<std::string, std::string>>& attrs);
};

} // namespace vajra::gui
