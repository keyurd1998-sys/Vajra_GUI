#pragma once

#include "vajra/liberty/models.hpp"

namespace vajra::liberty {

// Normalizes all metric values in the library to Vajra internal standard units:
// Time -> picoseconds (ps)
// Capacitance -> femtofarads (fF)
// Voltage -> millivolts (mV)
void normalize_units(Library& lib);

} // namespace vajra::liberty
