#include "property_gen.hpp"

#include <array>

namespace property {

namespace {

// Speeds as numerator/denominator, including ones every speed edit must reject.
constexpr std::array<std::array<std::int64_t, 2>, 7> kSpeeds{
    {{1, 2}, {2, 1}, {3, 2}, {1, 1}, {2, 3}, {0, 1}, {-1, 1}}};

void choose_placement(Rng& rng, Op& op, const Extent& e) {
    op.clip = static_cast<int>(rng.between(0, 2)); // the media, sometimes one the track rejects
    op.a = rng.between(0, e.timeline + 30);
    op.b = rng.between(0, 299);
    op.c = rng.between(0, 9) == 0 ? rng.between(0, 400) : rng.between(1, 60);
}

} // namespace

void choose_values(Rng& rng, Op& op, const Extent& e) {
    op.c = rng.between(0, 1);
    if (op.kind == Kind::Append || op.kind == Kind::Insert || op.kind == Kind::Overwrite) {
        choose_placement(rng, op, e);
    } else if (op.kind == Kind::Split || op.kind == Kind::Freeze) {
        op.a = rng.between(0, e.clip);
        op.b = rng.between(0, 20);
    } else if (op.kind == Kind::TrimStart || op.kind == Kind::TrimEnd) {
        op.a = rng.between(-20, 20);
    } else if (op.kind == Kind::Roll || op.kind == Kind::Slip || op.kind == Kind::Slide) {
        op.a = rng.between(-15, 15);
    } else if (op.kind == Kind::Move) {
        op.b = rng.between(0, e.tracks - 1);
        op.a = rng.between(0, e.timeline + 30);
    } else if (op.kind == Kind::RippleMove) {
        op.a = rng.between(0, e.timeline);
    } else if (op.kind == Kind::Speed) {
        const auto& s = kSpeeds.at(static_cast<std::size_t>(rng.between(0, kSpeeds.size() - 1)));
        op.a = s[0];
        op.b = s[1];
    } else if (op.kind == Kind::Ramp) {
        op.a = rng.between(0, 4); // halves: 0 and a sign change are refused
        op.b = rng.between(-1, 4);
    } else if (op.kind == Kind::Connect) {
        op.b = rng.between(0, e.tracks - 1);
        op.c = rng.between(0, 3);
    }
}

} // namespace property
