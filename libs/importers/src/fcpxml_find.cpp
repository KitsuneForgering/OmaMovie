#include "fcpxml_detail.hpp"

namespace oma::importers::fcpxml {

void projects(const xml::Element& e, std::vector<const xml::Element*>& out) {
    for (const xml::Element& child : e.children) {
        if (child.name == "project")
            out.push_back(&child);
        else if (child.name == "library" || child.name == "event")
            projects(child, out);
    }
}

const xml::Element* child_named(const xml::Element& e, std::string_view name) {
    for (const xml::Element& child : e.children)
        if (child.name == name)
            return &child;
    return nullptr;
}

} // namespace oma::importers::fcpxml
