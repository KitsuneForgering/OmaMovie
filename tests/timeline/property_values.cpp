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
    } else if (op.kind == Kind::Transition) {
        op.a = rng.between(-1, 40); // a duration of 0 or less is refused
        op.b = rng.between(0, 3);   // 3 names no transition kind
    } else if (op.kind == Kind::Detach) {
        op.b = rng.between(0, e.tracks - 1); // a video lane is refused
    } else if (op.kind == Kind::AddMarker) {
        op.a = rng.between(-2, e.timeline + 30); // before the start is refused
    } else if (op.kind == Kind::RemoveMarker) {
        op.b = rng.between(0, 4); // an index past the last marker is refused
    } else if (op.kind == Kind::AddCaption || op.kind == Kind::SetCaption) {
        op.clip = static_cast<int>(rng.between(0, 4)); // past the last caption is refused
        op.a = rng.between(-2, e.timeline + 30);       // before the start is refused
        op.b = rng.between(-1, 60);                    // empty or negative is refused
        op.c = rng.between(0, 9) == 0 ? 1 : 0;         // text over kMaxCaptionBytes is refused
    } else if (op.kind == Kind::RemoveCaption) {
        op.b = rng.between(0, 4);
    } else if (op.kind == Kind::Video) {
        op.b = rng.between(0, 2);
        op.a = rng.between(-5, 40); // a key out of order or before the media is refused
        op.c = rng.between(0, 5);   // values over 1 (c/4) are refused
    } else if (op.kind == Kind::Connect) {
        op.b = rng.between(0, e.tracks - 1);
        op.c = rng.between(0, 3);
    }
}

} // namespace property
