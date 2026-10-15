// The XML reader at the very end of its input: texts that stop mid-attribute, read from buffers
// of exactly their size so ASan sees any read past the end.
#include "xml.hpp"

#include <cstring>
#include <memory>
#include <string_view>

#include "oma_test.hpp"

namespace xml = oma::importers::xml;

namespace {

// In an exactly sized buffer: a std::string would hide a read past the end behind its terminator.
// Found by fuzz_fcpxml.
bool refuses_end_after_equals(const char* text) {
    const std::size_t n = std::strlen(text);
    const auto exact = std::make_unique<char[]>(n);
    std::memcpy(exact.get(), text, n);
    return !xml::parse(std::string_view(exact.get(), n));
}

} // namespace

void run_xml_bounds_tests() {
    describe("XML reader bounds", {
        it("refuses a text that ends after an attribute's equals sign", {
            expect(refuses_end_after_equals("<x a=")).toBeTruthy();
            expect(refuses_end_after_equals("<x a= ")).toBeTruthy();
            expect(refuses_end_after_equals("<x a")).toBeTruthy();
        });
    });
}
