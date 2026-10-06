#include "vajra/liberty/template_resolver.hpp"

namespace vajra::liberty {

namespace {

void resolve_table(LUT2D& lut, const Library& lib) {
    if (lut.template_name.empty()) return;

    const TableTemplate* tmpl = lib.find_template(lut.template_name);
    if (!tmpl) return;

    if (lut.variable_1.empty()) lut.variable_1 = tmpl->variable_1;
    if (lut.variable_2.empty()) lut.variable_2 = tmpl->variable_2;
    if (lut.index_1.empty()) lut.index_1 = tmpl->index_1;
    if (lut.index_2.empty()) lut.index_2 = tmpl->index_2;
}

} // namespace

void resolve_templates(Library& lib) {
    for (auto& [cell_name, cell] : lib.cells) {
        for (auto& [pin_name, pin] : cell.pins) {
            for (auto& arc : pin.timing_arcs) {
                if (arc.cell_rise) resolve_table(*arc.cell_rise, lib);
                if (arc.cell_fall) resolve_table(*arc.cell_fall, lib);
                if (arc.rise_transition) resolve_table(*arc.rise_transition, lib);
                if (arc.fall_transition) resolve_table(*arc.fall_transition, lib);
                if (arc.rise_constraint) resolve_table(*arc.rise_constraint, lib);
                if (arc.fall_constraint) resolve_table(*arc.fall_constraint, lib);
            }
        }
    }
}

} // namespace vajra::liberty
