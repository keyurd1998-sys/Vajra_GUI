#pragma once

#include "vajra/gui/models.hpp"

#include <string>

namespace Yosys {
namespace RTLIL {
    struct Design;
    struct Module;
    struct Cell;
    struct Wire;
    struct SigSpec;
}
}

namespace vajra::liberty {
class LibraryManager;
}

namespace vajra::gui {

class RTLILAdapter {
public:
    static SchematicDesign extract_design(Yosys::RTLIL::Design* design,
                                         const std::string& top_module_name = "",
                                         const liberty::LibraryManager* lib_mgr = nullptr);
    static SchematicModule extract_module(Yosys::RTLIL::Module* module,
                                         Yosys::RTLIL::Design* design = nullptr,
                                         const liberty::LibraryManager* lib_mgr = nullptr);

private:
    static void extract_ports(Yosys::RTLIL::Module* module, SchematicModule& target);
    static void extract_cells(Yosys::RTLIL::Module* module,
                             SchematicModule& target,
                             Yosys::RTLIL::Design* design,
                             const liberty::LibraryManager* lib_mgr);
    static void extract_assigns(Yosys::RTLIL::Module* module, SchematicModule& target);
    static void reconcile_nets(SchematicModule& target);
    static std::string sigspec_to_net_name(const Yosys::RTLIL::SigSpec& sig);
};

} // namespace vajra::gui
