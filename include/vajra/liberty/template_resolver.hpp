#pragma once

#include "vajra/liberty/models.hpp"

namespace vajra::liberty {

// Resolves NLDM table templates, inheriting axis variables and breakpoints from lu_table_template
void resolve_templates(Library& lib);

} // namespace vajra::liberty
