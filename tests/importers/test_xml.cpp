// The XML reader behind the MLT/Kdenlive importer (libs/importers/src/xml.hpp): structure,
// entities and the refusals that keep it safe on untrusted files (CLAUDE.md §18).
#include "xml.hpp"

#include <string>

#include "oma_test.hpp"

namespace xml = oma::importers::xml;

namespace {

bool reads_structure_entities_and_cdata() {
    const auto root = xml::parse(R"(<?xml version="1.0" encoding="utf-8"?>
<!-- a comment --><mlt version="7.40.0" title="a &amp; b &#233;">
  <producer id="p0" in="5"><property name="resource">dir/clip &lt;1&gt;.mp4</property></producer>
  <note><![CDATA[1 < 2 & 3]]></note><empty/>
</mlt>)");
    if (!root || root->name != "mlt" || root->children.size() != 3)
        return false;
    const auto& prop = root->children[0].children.at(0);
    return root->attribute("title") == "a & b \xC3\xA9" && root->attribute("missing").empty() &&
           root->children[0].attribute("in") == "5" && prop.attribute("name") == "resource" &&
           prop.text == "dir/clip <1>.mp4" && root->children[1].text == "1 < 2 & 3";
}

bool refused(const std::string& text) {
    return !xml::parse(text);
}

std::string nested(int depth) {
    std::string s;
    for (int i = 0; i < depth; ++i)
        s += "<a>";
    for (int i = 0; i < depth; ++i)
        s += "</a>";
    return s;
}

} // namespace

void run_xml_tests() {
    describe("XML reader", {
        it("reads elements, attributes, entities and CDATA",
           { expect(reads_structure_entities_and_cdata()).toBeTruthy(); });
        it("refuses DTDs, unknown entities, mismatched tags and deep nesting", {
            expect(refused(R"(<!DOCTYPE x [<!ENTITY a "aaaa">]><x>&a;</x>)")).toBeTruthy();
            expect(refused("<x>&unknown;</x>")).toBeTruthy();
            expect(refused("<x><y></x></y>")).toBeTruthy();
            expect(refused("<x a=\"1<2\"/>")).toBeTruthy();
            expect(refused("<x>&#0;</x>")).toBeTruthy();
            expect(refused("<x></x><y/>")).toBeTruthy();
            expect(refused(nested(65))).toBeTruthy();
            expect(refused(nested(64))).toBeFalsy();
        });
    });
}
