#include "vajra/gui/workbench/hierarchy_tree.hpp"

#include <QMenu>
#include <QHeaderView>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <vector>
#include <algorithm>
#include <functional>

namespace vajra::gui {

HierarchyTree::HierarchyTree(QWidget* parent)
    : QWidget(parent) {
    setup_ui();
}

void HierarchyTree::setup_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText("Filter hierarchy / instances...");
    search_edit_->setClearButtonEnabled(true);
    layout->addWidget(search_edit_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({"Hierarchy Object", "Type / Info"});
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->setColumnWidth(0, 190);
    tree_->setAlternatingRowColors(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(tree_);

    connect(search_edit_, &QLineEdit::textChanged, this, &HierarchyTree::on_filter_changed);
    connect(tree_, &QTreeWidget::itemClicked, this, &HierarchyTree::on_item_clicked);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &HierarchyTree::on_item_double_clicked);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &HierarchyTree::on_custom_context_menu);
}

void HierarchyTree::populate(const SchematicDesign& design, const std::string& active_module_name) {
    bool must_rebuild = (tree_->topLevelItemCount() == 0 ||
                         design_.modules.size() != design.modules.size() ||
                         design_.top_module != design.top_module);
    design_ = design;
    if (must_rebuild) {
        build_full_tree();
    }
    set_active_module(active_module_name.empty() ? design_.top_module : active_module_name);
}

void HierarchyTree::build_full_tree() {
    tree_->clear();

    std::string top_name = design_.top_module;
    if (top_name.empty() && !design_.modules.empty()) {
        top_name = design_.modules.begin()->first;
    }

    const auto* top_mod = design_.get_module(top_name);
    if (!top_mod) {
        return;
    }

    auto* top_item = new QTreeWidgetItem(tree_);
    top_item->setText(0, QString::fromStdString(top_mod->name));
    top_item->setText(1, QString::number(top_mod->cells.size()) + " cells");
    QFont bold_font;
    bold_font.setBold(true);
    top_item->setFont(0, bold_font);
    top_item->setForeground(0, QBrush(QColor("#FFFFFF")));
    top_item->setForeground(1, QBrush(QColor("#CCCCCC")));
    top_item->setData(0, Qt::UserRole, QVariantList{"module", QString::fromStdString(top_mod->name)});

    std::unordered_set<std::string> active_path;
    active_path.insert(top_name);
    build_submodule_children(top_item, top_name, 0, active_path);

    top_item->setExpanded(true);
    if (top_item->childCount() > 0) {
        top_item->child(0)->setExpanded(true);
    }
}

void HierarchyTree::build_submodule_children(QTreeWidgetItem* parent_item, const std::string& module_name, int depth, std::unordered_set<std::string>& active_path) {
    const auto* mod = design_.get_module(module_name);
    if (!mod) return;

    // 1. Submodules (Hierarchical Blocks)
    std::vector<const SchematicCell*> submodules;
    for (const auto& [cid, cell] : mod->cells) {
        if (cell.is_submodule || cell.type == NodeType::MODULE) {
            submodules.push_back(&cell);
        }
    }
    std::sort(submodules.begin(), submodules.end(), [](const auto* a, const auto* b) {
        return a->name < b->name;
    });

    if (!submodules.empty()) {
        auto* submods_root = new QTreeWidgetItem(parent_item);
        submods_root->setText(0, "Submodules");
        submods_root->setText(1, QString::number(submodules.size()) + " blocks");
        submods_root->setForeground(0, QBrush(QColor("#FFFFFF")));
        submods_root->setForeground(1, QBrush(QColor("#A0AEC0")));
        QFont cat_font;
        cat_font.setBold(true);
        submods_root->setFont(0, cat_font);

        for (const auto* sub : submodules) {
            auto* child = new QTreeWidgetItem(submods_root);
            child->setText(0, QString::fromStdString(sub->name));
            std::string target = sub->submodule_target.empty() ? sub->cell_type : sub->submodule_target;
            std::string type_label = "[" + target + "]";
            child->setText(1, QString::fromStdString(type_label));
            child->setForeground(0, QBrush(QColor("#FFFFFF")));
            child->setForeground(1, QBrush(QColor("#CCCCCC")));
            child->setData(0, Qt::UserRole, QVariantList{"submodule", QString::fromStdString(target), QString::fromStdString(sub->name), QString::fromStdString(module_name)});

            // Recursively build submodule subtree
            if (depth < 8 && active_path.find(target) == active_path.end()) {
                active_path.insert(target);
                build_submodule_children(child, target, depth + 1, active_path);
                active_path.erase(target);
            }
        }
    }

    // 2. Sequential Elements (DFFs, Latches)
    std::vector<const SchematicCell*> seq_cells;
    for (const auto& [cid, cell] : mod->cells) {
        if (cell.type == NodeType::DFF || cell.type == NodeType::LATCH) {
            seq_cells.push_back(&cell);
        }
    }
    std::sort(seq_cells.begin(), seq_cells.end(), [](const auto* a, const auto* b) {
        return a->name < b->name;
    });

    if (!seq_cells.empty()) {
        auto* seq_root = new QTreeWidgetItem(parent_item);
        seq_root->setText(0, "Sequential Elements");
        seq_root->setText(1, QString::number(seq_cells.size()) + " flops/latches");
        seq_root->setForeground(0, QBrush(QColor("#FFFFFF")));
        seq_root->setForeground(1, QBrush(QColor("#A0AEC0")));
        QFont cat_font;
        cat_font.setBold(true);
        seq_root->setFont(0, cat_font);

        size_t count = 0;
        for (const auto* c : seq_cells) {
            if (count++ >= 200) {
                auto* more_item = new QTreeWidgetItem(seq_root);
                more_item->setText(0, QString("... and %1 more").arg(seq_cells.size() - 200));
                more_item->setForeground(0, QBrush(QColor("#888888")));
                break;
            }
            auto* child = new QTreeWidgetItem(seq_root);
            child->setText(0, QString::fromStdString(c->name));
            child->setText(1, QString::fromStdString(c->cell_type));
            child->setForeground(0, QBrush(QColor("#FFFFFF")));
            child->setForeground(1, QBrush(QColor("#CCCCCC")));
            child->setData(0, Qt::UserRole, QVariantList{"instance", QString::fromStdString(c->name), QString::fromStdString(module_name)});
        }
    }

    // 3. Combinational Logic
    std::vector<const SchematicCell*> comb_cells;
    for (const auto& [cid, cell] : mod->cells) {
        if (!cell.is_submodule && cell.type != NodeType::MODULE &&
            cell.type != NodeType::DFF && cell.type != NodeType::LATCH &&
            cell.type != NodeType::PRIMARY_INPUT && cell.type != NodeType::PRIMARY_OUTPUT) {
            comb_cells.push_back(&cell);
        }
    }
    std::sort(comb_cells.begin(), comb_cells.end(), [](const auto* a, const auto* b) {
        return a->name < b->name;
    });

    if (!comb_cells.empty()) {
        auto* comb_root = new QTreeWidgetItem(parent_item);
        comb_root->setText(0, "Combinational Logic");
        comb_root->setText(1, QString::number(comb_cells.size()) + " gates");
        comb_root->setForeground(0, QBrush(QColor("#FFFFFF")));
        comb_root->setForeground(1, QBrush(QColor("#A0AEC0")));
        QFont cat_font;
        cat_font.setBold(true);
        comb_root->setFont(0, cat_font);

        size_t count = 0;
        for (const auto* c : comb_cells) {
            if (count++ >= 200) {
                auto* more_item = new QTreeWidgetItem(comb_root);
                more_item->setText(0, QString("... and %1 more").arg(comb_cells.size() - 200));
                more_item->setForeground(0, QBrush(QColor("#888888")));
                break;
            }
            auto* child = new QTreeWidgetItem(comb_root);
            child->setText(0, QString::fromStdString(c->name));
            child->setText(1, QString::fromStdString(c->cell_type));
            child->setForeground(0, QBrush(QColor("#FFFFFF")));
            child->setForeground(1, QBrush(QColor("#CCCCCC")));
            child->setData(0, Qt::UserRole, QVariantList{"instance", QString::fromStdString(c->name), QString::fromStdString(module_name)});
        }
    }

    // 4. Primary Ports
    if (!mod->ports.empty()) {
        auto* ports_root = new QTreeWidgetItem(parent_item);
        ports_root->setText(0, "Primary Ports");
        ports_root->setText(1, QString::number(mod->ports.size()) + " ports");
        ports_root->setForeground(0, QBrush(QColor("#FFFFFF")));
        ports_root->setForeground(1, QBrush(QColor("#A0AEC0")));
        QFont cat_font;
        cat_font.setBold(true);
        ports_root->setFont(0, cat_font);

        std::vector<const SchematicPin*> ports;
        for (const auto& [pname, pin] : mod->ports) {
            ports.push_back(&pin);
        }
        std::sort(ports.begin(), ports.end(), [](const auto* a, const auto* b) {
            return a->name < b->name;
        });

        for (const auto* p : ports) {
            auto* child = new QTreeWidgetItem(ports_root);
            child->setText(0, QString::fromStdString(p->name));
            std::string dir_str = (p->direction == PinDirection::INPUT) ? "INPUT" :
                                  ((p->direction == PinDirection::OUTPUT) ? "OUTPUT" : "INOUT");
            if (p->is_clock) dir_str += " (Clock)";
            if (p->is_reset) dir_str += " (Reset)";
            child->setText(1, QString::fromStdString(dir_str));
            child->setForeground(0, QBrush(QColor("#FFFFFF")));
            child->setForeground(1, QBrush(QColor("#CCCCCC")));
            child->setData(0, Qt::UserRole, QVariantList{"port", QString::fromStdString(p->name), QString::fromStdString(module_name)});
        }
    }
}

void HierarchyTree::set_active_module(const std::string& module_name) {
    active_module_name_ = module_name.empty() ? design_.top_module : module_name;
    if (tree_->topLevelItemCount() == 0) return;

    QTreeWidgetItem* target_item = nullptr;

    std::function<bool(QTreeWidgetItem*)> search_module = [&](QTreeWidgetItem* item) -> bool {
        QVariant data_var = item->data(0, Qt::UserRole);
        if (data_var.isValid()) {
            QVariantList list = data_var.toList();
            if (!list.empty()) {
                QString kind = list[0].toString();
                if (kind == "module" && list[1].toString().toStdString() == active_module_name_) {
                    target_item = item;
                    return true;
                }
                if (kind == "submodule" && list[1].toString().toStdString() == active_module_name_) {
                    target_item = item;
                    return true;
                }
            }
        }
        for (int i = 0; i < item->childCount(); ++i) {
            if (search_module(item->child(i))) return true;
        }
        return false;
    };

    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        if (search_module(tree_->topLevelItem(i))) break;
    }

    if (target_item) {
        tree_->setCurrentItem(target_item);
        QTreeWidgetItem* p = target_item->parent();
        while (p) {
            p->setExpanded(true);
            p = p->parent();
        }
        tree_->scrollToItem(target_item);
    }
}

void HierarchyTree::select_instance(const std::string& instance_name) {
    if (instance_name.empty()) return;
    QString target = QString::fromStdString(instance_name);

    std::function<bool(QTreeWidgetItem*)> search_item = [&](QTreeWidgetItem* item) -> bool {
        if (item->text(0) == target) {
            tree_->setCurrentItem(item);
            QTreeWidgetItem* p = item->parent();
            while (p) {
                p->setExpanded(true);
                p = p->parent();
            }
            tree_->scrollToItem(item);
            return true;
        }
        for (int i = 0; i < item->childCount(); ++i) {
            if (search_item(item->child(i))) {
                return true;
            }
        }
        return false;
    };

    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        if (search_item(tree_->topLevelItem(i))) break;
    }
}

void HierarchyTree::on_filter_changed(const QString& text) {
    QString q = text.trimmed().toLower();
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        filter_item(tree_->topLevelItem(i), q);
    }
}

bool HierarchyTree::filter_item(QTreeWidgetItem* item, const QString& query) {
    bool match = item->text(0).toLower().contains(query) || item->text(1).toLower().contains(query);
    bool child_match = false;
    for (int i = 0; i < item->childCount(); ++i) {
        if (filter_item(item->child(i), query)) {
            child_match = true;
        }
    }
    bool visible = match || child_match || query.isEmpty();
    item->setHidden(!visible);
    if (child_match && !query.isEmpty()) {
        item->setExpanded(true);
    }
    return visible;
}

void HierarchyTree::on_item_clicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QVariant data_var = item->data(0, Qt::UserRole);
    if (!data_var.isValid()) return;

    QVariantList list = data_var.toList();
    if (list.empty()) return;

    QString kind = list[0].toString();
    if (kind == "module") {
        std::string mod_name = list[1].toString().toStdString();
        if (mod_name != active_module_name_) {
            emit module_drilldown_requested(mod_name);
        }
    } else if (kind == "submodule") {
        std::string target_module = list[1].toString().toStdString();
        std::string inst_name = list[2].toString().toStdString();
        std::string parent_mod = list.size() > 3 ? list[3].toString().toStdString() : "";
        if (parent_mod == active_module_name_) {
            emit instance_selected(inst_name);
        } else {
            emit module_drilldown_requested(target_module);
        }
    } else if (kind == "instance") {
        emit instance_selected(list[1].toString().toStdString());
    } else if (kind == "port") {
        emit port_selected(list[1].toString().toStdString());
    }
}

void HierarchyTree::on_item_double_clicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QVariant data_var = item->data(0, Qt::UserRole);
    if (!data_var.isValid()) return;

    QVariantList list = data_var.toList();
    if (list.empty()) return;

    QString kind = list[0].toString();
    if (kind == "module") {
        std::string mod_name = list[1].toString().toStdString();
        emit module_drilldown_requested(mod_name);
    } else if (kind == "submodule") {
        std::string target_module = list[1].toString().toStdString();
        emit module_drilldown_requested(target_module);
    } else if (kind == "instance") {
        emit instance_selected(list[1].toString().toStdString());
    }
}

void HierarchyTree::on_custom_context_menu(const QPoint& pos) {
    QTreeWidgetItem* item = tree_->itemAt(pos);
    if (!item) return;

    QVariant data_var = item->data(0, Qt::UserRole);
    if (!data_var.isValid()) return;

    QVariantList list = data_var.toList();
    if (list.empty()) return;

    QString kind = list[0].toString();
    QMenu menu(this);

    if (kind == "module") {
        QString mod_name = list[1].toString();
        auto* drill_act = menu.addAction(QString("View Module '%1'").arg(mod_name));
        connect(drill_act, &QAction::triggered, this, [this, mod_name]() {
            emit module_drilldown_requested(mod_name.toStdString());
        });
    } else if (kind == "submodule") {
        QString mod_type = list[1].toString();
        QString inst_name = list[2].toString();

        auto* drill_act = menu.addAction(QString("Open Module '%1'").arg(mod_type));
        connect(drill_act, &QAction::triggered, this, [this, mod_type]() {
            emit module_drilldown_requested(mod_type.toStdString());
        });

        menu.addSeparator();
        auto* sel_act = menu.addAction(QString("Select Instance '%1'").arg(inst_name));
        connect(sel_act, &QAction::triggered, this, [this, inst_name]() {
            emit instance_selected(inst_name.toStdString());
        });
    } else if (kind == "instance") {
        QString inst_name = list[1].toString();
        auto* sel_act = menu.addAction(QString("Select '%1'").arg(inst_name));
        connect(sel_act, &QAction::triggered, this, [this, inst_name]() {
            emit instance_selected(inst_name.toStdString());
        });
    } else if (kind == "port") {
        QString port_name = list[1].toString();
        auto* sel_act = menu.addAction(QString("Select Port '%1'").arg(port_name));
        connect(sel_act, &QAction::triggered, this, [this, port_name]() {
            emit port_selected(port_name.toStdString());
        });
    }

    if (!menu.actions().isEmpty()) {
        menu.exec(tree_->viewport()->mapToGlobal(pos));
    }
}

} // namespace vajra::gui
