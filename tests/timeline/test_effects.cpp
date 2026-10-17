#include "oma/timeline/effects.hpp"

#include <string>
#include <vector>

#include "oma_test.hpp"

using namespace oma::timeline;

namespace {

const EffectDefinition& def(std::string_view id) {
    return *find_effect_definition(id);
}

std::vector<std::string> ids(const std::vector<Effect>& effects) {
    std::vector<std::string> out;
    for (const Effect& e : effects) {
        out.push_back(e.definition);
    }
    return out;
}

std::vector<std::string> list3(std::string a, std::string b, std::string c) {
    return {std::move(a), std::move(b), std::move(c)};
}

Effect named(std::string definition) {
    return {.definition = std::move(definition), .enabled = true, .params = {}};
}

Effect unknown() {
    return named("org.example.glow");
}

} // namespace

void run_effect_tests() {
    describe("Video effects (ADR-0016)", {
        it("name every look kind once", {
            int looks = 0;
            for (const EffectDefinition& d : effect_definitions()) {
                looks += d.stage == EffectStage::Look ? 1 : 0;
                expect(find_effect_definition(d.id) == &d).toBeTruthy();
                expect(validate_effects(std::vector<Effect>{make_effect(d)}).has_value())
                    .toBeTruthy();
            }
            expect(looks).toBe(static_cast<int>(FilterKind::Vignette));
            expect(find_effect_definition("oma.look.nope") == nullptr).toBeTruthy();
        });

        it("read missing parameters as their defaults", {
            Effect sepia = named("oma.look.sepia");
            expect(effect_param(sepia, "amount")).toBe(1.0);
            sepia.params.resize(1);
            sepia.params[0].name = "amount";
            sepia.params[0].value = 0.25;
            expect(effect_param(sepia, "amount")).toBe(0.25);
            expect(effect_param(unknown(), "amount")).toBe(0.0);
        });

        it("add an effect after the others of its stage", {
            std::vector<Effect> stack = with_effect({}, def("oma.look.sepia"));
            stack = with_effect(stack, def("oma.detail")); // an earlier stage goes first
            stack = with_effect(stack, def("oma.look.cool"));
            expect(ids(stack) == list3("oma.detail", "oma.look.sepia", "oma.look.cool"))
                .toBeTruthy();
            expect(with_effect(stack, def("oma.look.cool")).size()).toBe(3U); // once per clip
        });

        it("move an effect only within its stage", {
            std::vector<Effect> stack = with_effect({}, def("oma.detail"));
            stack = with_effect(stack, def("oma.look.sepia"));
            stack = with_effect(stack, def("oma.look.cool"));
            stack = with_effect_moved(stack, "oma.look.cool", -5);
            expect(ids(stack) == list3("oma.detail", "oma.look.cool", "oma.look.sepia"))
                .toBeTruthy();
            stack = with_effect_moved(stack, "oma.detail", 1);
            expect(ids(stack) == list3("oma.detail", "oma.look.cool", "oma.look.sepia"))
                .toBeTruthy();
            stack.push_back(unknown());
            stack = with_effect_moved(stack, "oma.look.sepia", 1); // not past an unknown effect
            expect(stack[2].definition).toBe(std::string("oma.look.sepia"));
            stack = with_effect_moved(stack, "org.example.glow", -1);
            expect(stack[3].definition).toBe(std::string("org.example.glow"));
        });

        it("bound counts and names", {
            std::vector<Effect> many;
            for (std::size_t i = 0; i <= kMaxEffects; ++i) {
                many.push_back(named("x." + std::to_string(i)));
            }
            expect(validate_effects(many).has_value()).toBeFalsy();
            many.pop_back();
            expect(validate_effects(many).has_value()).toBeTruthy();
            many[0].definition = std::string(kMaxEffectNameBytes + 1, 'x');
            expect(validate_effects(many).has_value()).toBeFalsy();
            many[0].definition.clear();
            expect(validate_effects(many).has_value()).toBeFalsy();
        });
    });
}
