#include "oma/project/document.hpp"

#include "oma/timeline/effects.hpp"

#include <simdjson.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <format>
#include <fstream>
#include <span>
#include <system_error>

namespace oma::project {

namespace {

namespace tl = oma::timeline;

Error invalid(std::string message, std::string context = {}) {
    return {ErrorCode::InvalidData, Category::Project, std::move(message), std::move(context)};
}

Error io_error(std::string message, const std::filesystem::path& path, int err) {
    return {ErrorCode::IoError, Category::Project,
            std::format("{}: {}", std::move(message),
                        std::error_code(err, std::generic_category()).message()),
            path.string()};
}

// Bounds for untrusted files (CLAUDE.md §18). The file size bounds every allocation; the counts
// fail early with a clear message.
constexpr std::size_t kMaxFileBytes = std::size_t{64} << 20;
constexpr std::size_t kMaxDepth = 32;
constexpr std::size_t kMaxItems = 100000; // media, tracks, clips per track, markers, LUTs

// Enumerations by name, in enumerator order: the file stays readable and does not depend on
// the numeric values.
constexpr std::array kTrackKinds{"video", "audio", "caption"};
constexpr std::array kFits{"fit", "fill", "stretch", "native"};
constexpr std::array kBlends{"normal", "add", "multiply", "screen"};
constexpr std::array kFilters{"none", "black_and_white", "sepia", "vintage", "cool",
                              "warm", "vignette"};
constexpr std::array kTransitions{"dissolve", "dip_to_black", "wipe"};
constexpr std::array kTitlePlacements{"lower_third", "center", "top"};
constexpr std::array kInterpolations{"hold", "linear", "ease"};
constexpr std::array kSegmentKinds{"linear", "freeze", "ramp"};

template <typename E, std::size_t N>
std::string_view name_of(const std::array<const char*, N>& names, E value) {
    return names.at(static_cast<std::size_t>(value));
}

// ------------------------------------------------------------------------------------ writing

// A small pretty-printing JSON writer: one value per line, so a project diffs well.
class Writer {
public:
    void begin_object() { open('{'); }
    void end_object() { close('}'); }
    void begin_array() { open('['); }
    void end_array() { close(']'); }

    void key(std::string_view k) {
        separate();
        string(k);
        out_ += ": ";
        after_key_ = true;
    }

    void value(std::string_view s) {
        separate();
        string(s);
    }
    void value(const char* s) { value(std::string_view(s)); }
    void value(bool b) {
        separate();
        out_ += b ? "true" : "false";
    }
    template <typename T>
        requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
    void value(T v) {
        separate();
        std::array<char, 64> buf{};
        // Shortest representation that reads back to the same value.
        const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v);
        out_.append(buf.data(), ec == std::errc{} ? end : buf.data());
    }
    void raw(std::string_view json) {
        separate();
        out_ += json;
    }

    template <typename T>
    void field(std::string_view k, const T& v) {
        key(k);
        value(v);
    }

    [[nodiscard]] std::string take() {
        out_ += '\n';
        return std::move(out_);
    }

private:
    void open(char c) {
        separate();
        out_ += c;
        ++depth_;
        first_ = true;
    }
    void close(char c) {
        --depth_;
        if (!first_) {
            newline();
        }
        out_ += c;
        first_ = false;
    }
    void separate() {
        if (after_key_) {
            after_key_ = false;
            return;
        }
        if (depth_ == 0) {
            return;
        }
        if (!first_) {
            out_ += ',';
        }
        newline();
        first_ = false;
    }
    void newline() {
        out_ += '\n';
        out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
    }
    void string(std::string_view s) {
        out_ += '"';
        for (const char c : s) {
            switch (c) {
            case '"':
                out_ += "\\\"";
                break;
            case '\\':
                out_ += "\\\\";
                break;
            case '\n':
                out_ += "\\n";
                break;
            case '\t':
                out_ += "\\t";
                break;
            case '\r':
                out_ += "\\r";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out_ += std::format("\\u{:04x}", static_cast<unsigned>(c));
                } else {
                    out_ += c;
                }
            }
        }
        out_ += '"';
    }

    std::string out_;
    int depth_ = 0;
    bool first_ = true;
    bool after_key_ = false;
};

std::string rational(Rational r) {
    return std::format("{}/{}", r.num(), r.den());
}

// Times are exact: an integer count of a rational timebase (CLAUDE.md §6).
void time(Writer& w, std::string_view k, const RationalTime& t) {
    w.key(k);
    w.begin_object();
    w.field("value", t.value());
    w.field("timebase", rational(t.timebase()));
    w.end_object();
}

void transform(Writer& w, const tl::Transform& t) {
    w.begin_object();
    w.field("offset_x", t.offset_x);
    w.field("offset_y", t.offset_y);
    w.field("scale_x", t.scale_x);
    w.field("scale_y", t.scale_y);
    w.field("rotation", t.rotation);
    w.end_object();
}

void curve(Writer& w, std::string_view k, const std::vector<tl::CurvePoint>& points) {
    w.key(k);
    w.begin_array();
    for (const tl::CurvePoint& p : points) {
        w.begin_array();
        w.value(p.x);
        w.value(p.y);
        w.end_array();
    }
    w.end_array();
}

void triple(Writer& w, std::string_view k, const std::array<double, 3>& v) {
    w.key(k);
    w.begin_array();
    for (const double x : v) {
        w.value(x);
    }
    w.end_array();
}

// Keyed numbers (format 6): [{at, value, interpolation}].
void scalar_keys(Writer& w, std::string_view name, const std::vector<tl::ScalarKey>& keys) {
    w.key(name);
    w.begin_array();
    for (const tl::ScalarKey& k : keys) {
        w.begin_object();
        time(w, "at", k.at);
        w.field("value", k.value);
        w.field("interpolation", name_of(kInterpolations, k.interpolation));
        w.end_object();
    }
    w.end_array();
}

void video(Writer& w, const tl::VideoProperties& v) {
    w.key("video");
    w.begin_object();
    w.field("fit", name_of(kFits, v.fit));
    w.key("crop");
    w.begin_object();
    w.field("left", v.crop.left);
    w.field("top", v.crop.top);
    w.field("right", v.crop.right);
    w.field("bottom", v.crop.bottom);
    w.end_object();
    w.key("transform");
    transform(w, v.transform);
    w.key("transform_keys");
    w.begin_array();
    for (const tl::TransformKey& k : v.transform_keys) {
        w.begin_object();
        time(w, "at", k.at);
        w.key("value");
        transform(w, k.value);
        w.field("interpolation", name_of(kInterpolations, k.interpolation));
        w.end_object();
    }
    w.end_array();
    w.field("opacity", v.opacity);
    scalar_keys(w, "opacity_keys", v.opacity_keys);
    w.field("blend", name_of(kBlends, v.blend));
    w.key("color");
    w.begin_object();
    w.field("exposure", v.color.exposure);
    w.field("contrast", v.color.contrast);
    w.field("saturation", v.color.saturation);
    w.field("temperature", v.color.temperature);
    w.end_object();
    w.key("effects"); // format 4 (ADR-0016)
    w.begin_array();
    for (const tl::Effect& e : v.effects) {
        w.begin_object();
        w.field("definition", e.definition);
        w.field("enabled", e.enabled);
        w.key("params");
        w.begin_object();
        for (const tl::EffectParam& p : e.params) {
            w.field(p.name, p.value);
        }
        w.end_object();
        w.end_object();
    }
    w.end_array();
    w.key("grade");
    w.begin_object();
    triple(w, "slope", v.grade.cdl.slope);
    triple(w, "offset", v.grade.cdl.offset);
    triple(w, "power", v.grade.cdl.power);
    w.field("saturation", v.grade.cdl.saturation);
    curve(w, "master", v.grade.curves.master);
    curve(w, "red", v.grade.curves.red);
    curve(w, "green", v.grade.curves.green);
    curve(w, "blue", v.grade.curves.blue);
    w.field("lut", v.grade.lut.value());
    w.field("lut_amount", v.grade.lut_amount);
    w.end_object();
    w.end_object();
}

void audio(Writer& w, const tl::AudioProperties& a) {
    w.key("audio");
    w.begin_object();
    w.field("gain", a.gain);
    scalar_keys(w, "gain_keys", a.gain_keys);
    w.field("muted", a.muted);
    time(w, "fade_in", a.fade_in);
    time(w, "fade_out", a.fade_out);
    w.key("eq");
    w.begin_object();
    w.field("low_db", a.eq.low_db);
    w.field("mid_db", a.eq.mid_db);
    w.field("high_db", a.eq.high_db);
    w.end_object();
    w.key("noise");
    w.begin_object();
    w.field("amount", a.noise.amount);
    w.field("floor_db", a.noise.floor_db);
    w.end_object();
    w.end_object();
}

void clip(Writer& w, const tl::Clip& c) {
    w.begin_object();
    w.field("id", c.id.value());
    w.field("media", c.media.value());
    time(w, "start", c.start);
    time(w, "duration", c.duration);
    time(w, "source_in", c.source_in);
    if (c.time_map.is_constant()) {
        w.field("speed", rational(c.time_map.speed()));
    } else {
        w.key("time_map");
        w.begin_array();
        for (const tl::TimeSegment& s : c.time_map.segments()) {
            w.begin_object();
            w.field("kind", name_of(kSegmentKinds, s.kind));
            w.field("length", s.length);
            if (s.kind == tl::TimeSegment::Kind::Linear) {
                w.field("speed", rational(s.from));
            } else if (s.kind == tl::TimeSegment::Kind::Ramp) {
                w.field("from", rational(s.from));
                w.field("to", rational(s.to));
            }
            w.end_object();
        }
        w.end_array();
    }
    video(w, c.video);
    audio(w, c.audio);
    w.field("audio_detached", c.audio_detached);
    if (c.anchor) {
        w.key("anchor");
        w.begin_object();
        w.field("primary", c.anchor->primary.value());
        time(w, "source", c.anchor->source);
        w.end_object();
    }
    if (c.title) {
        // Format 3 (ADR-0015).
        const tl::Title& t = *c.title;
        w.key("title");
        w.begin_object();
        w.field("text", t.text);
        w.field("font", t.font);
        w.field("size", t.size);
        w.key("color");
        w.begin_object();
        w.field("r", static_cast<double>(t.color[0]));
        w.field("g", static_cast<double>(t.color[1]));
        w.field("b", static_cast<double>(t.color[2]));
        w.field("a", static_cast<double>(t.color[3]));
        w.end_object();
        w.field("placement", name_of(kTitlePlacements, t.placement));
        w.end_object();
    }
    if (c.transition_in) {
        w.key("transition_in");
        w.begin_object();
        w.field("kind", name_of(kTransitions, c.transition_in->kind));
        time(w, "duration", c.transition_in->duration);
        w.end_object();
    }
    w.end_object();
}

std::string relative_to(const std::filesystem::path& path, const std::filesystem::path& dir) {
    return dir.empty() ? path.string() : path.lexically_relative(dir).string();
}

// ------------------------------------------------------------------------------------ reading

using Object = simdjson::dom::object;
using Element = simdjson::dom::element;

std::string where(std::string_view k, std::string_view context) {
    return context.empty() ? std::string(k) : std::format("{}.{}", context, k);
}

Result<Element> member(const Object& o, std::string_view k, std::string_view context) {
    Element e;
    if (o.at_key(k).get(e) != simdjson::SUCCESS) {
        return std::unexpected(invalid("missing field", where(k, context)));
    }
    return e;
}

template <typename T>
Result<T> get(const Object& o, std::string_view k, std::string_view context) {
    auto e = member(o, k, context);
    if (!e) {
        return std::unexpected(e.error());
    }
    T v{};
    if constexpr (std::is_same_v<T, double>) {
        // Integers written for whole doubles read back as doubles too.
        if (e->get_double().get(v) != simdjson::SUCCESS) {
            return std::unexpected(invalid("not a number", where(k, context)));
        }
    } else if (e->get(v) != simdjson::SUCCESS) {
        return std::unexpected(invalid("wrong type", where(k, context)));
    }
    return v;
}

// An optional field: its default when absent (older or partial files), an error when wrong.
template <typename T>
Result<T> get_or(const Object& o, std::string_view k, T fallback, std::string_view context) {
    Element e;
    if (o.at_key(k).get(e) != simdjson::SUCCESS) {
        return fallback;
    }
    return get<T>(o, k, context);
}

Result<float> get_float(const Object& o, std::string_view k, float fallback,
                        std::string_view context) {
    auto v = get_or<double>(o, k, static_cast<double>(fallback), context);
    if (!v) {
        return std::unexpected(v.error());
    }
    return static_cast<float>(*v);
}

template <typename E, std::size_t N>
Result<E> enum_of(const std::array<const char*, N>& names, std::string_view s,
                  std::string_view context) {
    const auto it = std::ranges::find(names, s);
    if (it == names.end()) {
        return std::unexpected(invalid(std::format("unknown value '{}'", s), std::string(context)));
    }
    return static_cast<E>(it - names.begin());
}

template <typename E, std::size_t N>
Result<E> get_enum(const Object& o, std::string_view k, const std::array<const char*, N>& names,
                   E fallback, std::string_view context) {
    Element e;
    if (o.at_key(k).get(e) != simdjson::SUCCESS) {
        return fallback;
    }
    std::string_view s;
    if (e.get(s) != simdjson::SUCCESS) {
        return std::unexpected(invalid("not a string", where(k, context)));
    }
    return enum_of<E>(names, s, where(k, context));
}

Result<Object> get_object(const Object& o, std::string_view k, std::string_view context) {
    return get<Object>(o, k, context);
}

// The elements of an array field (none when an optional one is absent). A default
// simdjson array cannot be iterated, hence the vector of element handles.
Result<std::vector<Element>> get_array(const Object& o, std::string_view k,
                                       std::string_view context, bool optional = false) {
    Element e;
    if (o.at_key(k).get(e) != simdjson::SUCCESS && optional) {
        return std::vector<Element>{};
    }
    auto a = get<simdjson::dom::array>(o, k, context);
    if (!a) {
        return std::unexpected(a.error());
    }
    if (a->size() > kMaxItems) {
        return std::unexpected(invalid("too many items", where(k, context)));
    }
    return std::vector<Element>(a->begin(), a->end());
}

Result<Rational> parse_rational(std::string_view s, std::string_view context) {
    const auto slash = s.find('/');
    std::int64_t num = 0;
    std::int64_t den = 0;
    const auto parse = [](std::string_view part, std::int64_t& out) {
        const auto [end, ec] = std::from_chars(part.data(), part.data() + part.size(), out);
        return ec == std::errc{} && end == part.data() + part.size();
    };
    if (slash == std::string_view::npos || !parse(s.substr(0, slash), num) ||
        !parse(s.substr(slash + 1), den)) {
        return std::unexpected(invalid("not a rational 'num/den'", std::string(context)));
    }
    auto r = Rational::make(num, den);
    if (!r) {
        return std::unexpected(invalid(r.error().message(), std::string(context)));
    }
    return r;
}

Result<Rational> get_rational(const Object& o, std::string_view k, std::string_view context) {
    auto s = get<std::string_view>(o, k, context);
    if (!s) {
        return std::unexpected(s.error());
    }
    return parse_rational(*s, where(k, context));
}

Result<RationalTime> get_time(const Object& o, std::string_view k, std::string_view context) {
    Element e;
    if (o.at_key(k).get(e) != simdjson::SUCCESS) {
        return RationalTime{};
    }
    const std::string at = where(k, context);
    auto t = get_object(o, k, context);
    if (!t) {
        return std::unexpected(t.error());
    }
    auto value = get<std::int64_t>(*t, "value", at);
    auto timebase = get_rational(*t, "timebase", at);
    if (!value || !timebase) {
        return std::unexpected(!value ? value.error() : timebase.error());
    }
    auto r = RationalTime::make(*value, *timebase);
    if (!r) {
        return std::unexpected(invalid(r.error().message(), at));
    }
    return r;
}

// Collects the first error of a sequence of reads, so each field reads in one line.
class Reader {
public:
    template <typename T>
    T operator()(Result<T> r, T fallback = T{}) {
        if (!r) {
            if (!error_) {
                error_ = r.error();
            }
            return fallback;
        }
        return std::move(*r);
    }
    void fail(const Error& e) {
        if (!error_) {
            error_ = e;
        }
    }
    [[nodiscard]] bool failed() const noexcept { return error_.has_value(); }
    [[nodiscard]] Error error() const {
        return error_.value_or(Error(ErrorCode::Internal, Category::Project, "no error recorded"));
    }

private:
    std::optional<Error> error_;
};

Result<tl::Transform> read_transform(const Object& o, std::string_view context) {
    Reader r;
    tl::Transform t{.offset_x = r(get_or<double>(o, "offset_x", 0.0, context)),
                    .offset_y = r(get_or<double>(o, "offset_y", 0.0, context)),
                    .scale_x = r(get_or<double>(o, "scale_x", 1.0, context)),
                    .scale_y = r(get_or<double>(o, "scale_y", 1.0, context)),
                    .rotation = r(get_or<double>(o, "rotation", 0.0, context))};
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    return t;
}

Result<std::vector<tl::CurvePoint>> read_curve(const Object& o, std::string_view k,
                                               std::string_view context) {
    auto points = get_array(o, k, context, true);
    if (!points) {
        return std::unexpected(points.error());
    }
    std::vector<tl::CurvePoint> out;
    for (const Element p : *points) {
        simdjson::dom::array xy;
        double x = 0;
        double y = 0;
        if (p.get(xy) != simdjson::SUCCESS || xy.size() != 2 ||
            xy.at(0).get_double().get(x) != simdjson::SUCCESS ||
            xy.at(1).get_double().get(y) != simdjson::SUCCESS) {
            return std::unexpected(invalid("a curve point is [x, y]", where(k, context)));
        }
        out.push_back({.x = x, .y = y});
        if (out.size() > tl::kMaxCurvePoints) {
            return std::unexpected(invalid("too many curve points", where(k, context)));
        }
    }
    return out;
}

Result<std::array<double, 3>> read_triple(const Object& o, std::string_view k,
                                          std::array<double, 3> fallback,
                                          std::string_view context) {
    Element e;
    if (o.at_key(k).get(e) != simdjson::SUCCESS) {
        return fallback;
    }
    simdjson::dom::array a;
    std::array<double, 3> out{};
    if (e.get(a) != simdjson::SUCCESS || a.size() != 3) {
        return std::unexpected(invalid("expected three numbers", where(k, context)));
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (a.at(i).get_double().get(out[i]) != simdjson::SUCCESS) {
            return std::unexpected(invalid("expected three numbers", where(k, context)));
        }
    }
    return out;
}

// Format 6; Timeline::restore checks ranges and order. The count is bounded before copying.
Result<std::vector<tl::ScalarKey>> read_scalar_keys(const Object& o, std::string_view name,
                                                    std::string_view at) {
    Reader r;
    std::vector<tl::ScalarKey> keys;
    for (const Element e : r(get_array(o, name, at, true))) {
        const std::string ka = where(name, at);
        Object ko;
        if (e.get(ko) != simdjson::SUCCESS || keys.size() >= tl::kMaxKeys) {
            return std::unexpected(invalid("invalid or too many keys", ka));
        }
        keys.push_back({.at = r(get_time(ko, "at", ka)),
                        .value = r(get<double>(ko, "value", ka)),
                        .interpolation = r(get_enum(ko, "interpolation", kInterpolations,
                                                    tl::Interpolation::Linear, ka))});
    }
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    return keys;
}

Result<tl::VideoProperties> read_video(const Object& clip, std::string_view context) {
    tl::VideoProperties v;
    Element e;
    if (clip.at_key("video").get(e) != simdjson::SUCCESS) {
        return v;
    }
    const std::string at = where("video", context);
    Object o;
    if (e.get(o) != simdjson::SUCCESS) {
        return std::unexpected(invalid("not an object", at));
    }
    Reader r;
    v.fit = r(get_enum(o, "fit", kFits, tl::Fit::Fit, at));
    if (Object c; o.at_key("crop").get(c) == simdjson::SUCCESS) {
        const std::string ca = where("crop", at);
        v.crop = {.left = r(get_or<double>(c, "left", 0.0, ca)),
                  .top = r(get_or<double>(c, "top", 0.0, ca)),
                  .right = r(get_or<double>(c, "right", 0.0, ca)),
                  .bottom = r(get_or<double>(c, "bottom", 0.0, ca))};
    }
    if (Object t; o.at_key("transform").get(t) == simdjson::SUCCESS) {
        v.transform = r(read_transform(t, where("transform", at)));
    }
    auto keys = r(get_array(o, "transform_keys", at, true));
    for (const Element k : keys) {
        const std::string ka = where("transform_keys", at);
        Object ko;
        if (k.get(ko) != simdjson::SUCCESS || v.transform_keys.size() >= tl::kMaxKeys) {
            return std::unexpected(invalid("invalid or too many keys", ka));
        }
        Object value;
        if (ko.at_key("value").get(value) != simdjson::SUCCESS) {
            return std::unexpected(invalid("missing field", where("value", ka)));
        }
        v.transform_keys.push_back(
            {.at = r(get_time(ko, "at", ka)),
             .value = r(read_transform(value, where("value", ka))),
             .interpolation =
                 r(get_enum(ko, "interpolation", kInterpolations, tl::Interpolation::Linear, ka))});
    }
    v.opacity = r(get_float(o, "opacity", 1.0F, at));
    v.opacity_keys = r(read_scalar_keys(o, "opacity_keys", at));
    v.blend = r(get_enum(o, "blend", kBlends, tl::BlendMode::Normal, at));
    if (Object c; o.at_key("color").get(c) == simdjson::SUCCESS) {
        const std::string ca = where("color", at);
        v.color = {.exposure = r(get_or<double>(c, "exposure", 0.0, ca)),
                   .contrast = r(get_or<double>(c, "contrast", 0.0, ca)),
                   .saturation = r(get_or<double>(c, "saturation", 0.0, ca)),
                   .temperature = r(get_or<double>(c, "temperature", 0.0, ca))};
    }
    for (const Element item : r(get_array(o, "effects", at, true))) {
        // Format 4 (ADR-0016); Timeline::restore checks definitions, names and ranges. Names are
        // bounded here, before they are copied.
        const std::string ea = where("effects", at);
        Object eo;
        Object params;
        std::string_view definition;
        if (item.get(eo) != simdjson::SUCCESS || v.effects.size() >= tl::kMaxEffects ||
            eo.at_key("definition").get(definition) != simdjson::SUCCESS ||
            definition.size() > tl::kMaxEffectNameBytes) {
            return std::unexpected(invalid("invalid effect or too many effects", ea));
        }
        tl::Effect effect{.definition = std::string(definition),
                          .enabled = r(get_or<bool>(eo, "enabled", true, ea)),
                          .params = {}};
        if (eo.at_key("params").get(params) == simdjson::SUCCESS) {
            for (const auto [name, value] : params) {
                double number = 0.0;
                if (effect.params.size() >= tl::kMaxEffectParams ||
                    name.size() > tl::kMaxEffectNameBytes ||
                    value.get(number) != simdjson::SUCCESS) {
                    return std::unexpected(invalid("invalid effect parameter", ea));
                }
                effect.params.push_back({.name = std::string(name), .value = number});
            }
        }
        v.effects.push_back(std::move(effect));
    }
    // Migration 3 -> 4 (ADR-0016): the single filter and the sharpness become effects with the
    // same parameters, so the picture does not change. Version 4 never writes these fields.
    if (Object f; o.at_key("filter").get(f) == simdjson::SUCCESS) {
        const std::string fa = where("filter", at);
        const auto kind = r(get_enum(f, "kind", kFilters, tl::FilterKind::None, fa));
        const double amount = r(get_or<double>(f, "amount", 1.0, fa));
        for (const tl::EffectDefinition& d : tl::effect_definitions()) {
            if (kind != tl::FilterKind::None && d.look == kind) {
                v.effects.push_back({.definition = std::string(d.id),
                                     .enabled = true,
                                     .params = {{.name = "amount", .value = amount}}});
            }
        }
    }
    if (const double sharpness = r(get_or<double>(o, "sharpness", 0.0, at)); sharpness != 0.0) {
        v.effects.insert(v.effects.begin(), {.definition = "oma.detail",
                                             .enabled = true,
                                             .params = {{.name = "amount", .value = sharpness}}});
    }
    if (Object g; o.at_key("grade").get(g) == simdjson::SUCCESS) {
        const std::string ga = where("grade", at);
        v.grade.cdl.slope = r(read_triple(g, "slope", {1.0, 1.0, 1.0}, ga));
        v.grade.cdl.offset = r(read_triple(g, "offset", {0.0, 0.0, 0.0}, ga));
        v.grade.cdl.power = r(read_triple(g, "power", {1.0, 1.0, 1.0}, ga));
        v.grade.cdl.saturation = r(get_or<double>(g, "saturation", 1.0, ga));
        v.grade.curves = {.master = r(read_curve(g, "master", ga)),
                          .red = r(read_curve(g, "red", ga)),
                          .green = r(read_curve(g, "green", ga)),
                          .blue = r(read_curve(g, "blue", ga))};
        v.grade.lut = tl::LutId(r(get_or<std::uint64_t>(g, "lut", 0, ga)));
        v.grade.lut_amount = r(get_or<double>(g, "lut_amount", 1.0, ga));
    }
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    return v;
}

Result<tl::AudioProperties> read_audio(const Object& clip, std::string_view context) {
    tl::AudioProperties a;
    Element e;
    if (clip.at_key("audio").get(e) != simdjson::SUCCESS) {
        return a;
    }
    const std::string at = where("audio", context);
    Object o;
    if (e.get(o) != simdjson::SUCCESS) {
        return std::unexpected(invalid("not an object", at));
    }
    Reader r;
    a.gain = r(get_float(o, "gain", 1.0F, at));
    a.gain_keys = r(read_scalar_keys(o, "gain_keys", at));
    a.muted = r(get_or<bool>(o, "muted", false, at));
    a.fade_in = r(get_time(o, "fade_in", at));
    a.fade_out = r(get_time(o, "fade_out", at));
    if (Object eq; o.at_key("eq").get(eq) == simdjson::SUCCESS) {
        const std::string ea = where("eq", at);
        a.eq = {.low_db = r(get_float(eq, "low_db", 0.0F, ea)),
                .mid_db = r(get_float(eq, "mid_db", 0.0F, ea)),
                .high_db = r(get_float(eq, "high_db", 0.0F, ea))};
    }
    if (Object n; o.at_key("noise").get(n) == simdjson::SUCCESS) {
        const std::string na = where("noise", at);
        a.noise = {.amount = r(get_float(n, "amount", 0.0F, na)),
                   .floor_db = r(get_float(n, "floor_db", -50.0F, na))};
    }
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    return a;
}

Result<tl::TimeMap> read_time_map(const Object& o, std::string_view at) {
    const std::string ta = where("time_map", at);
    Reader r;
    std::vector<tl::TimeSegment> segments;
    for (const Element e : r(get_array(o, "time_map", at))) {
        Object so;
        if (e.get(so) != simdjson::SUCCESS || segments.size() >= tl::kMaxTimeSegments) {
            return std::unexpected(invalid("invalid or too many time segments", ta));
        }
        tl::TimeSegment s{
            .kind = r(get_enum(so, "kind", kSegmentKinds, tl::TimeSegment::Kind::Linear, ta)),
            .length = r(get<std::int64_t>(so, "length", ta)),
            .from = Rational::literal(0, 1),
            .to = Rational::literal(0, 1)};
        if (s.kind == tl::TimeSegment::Kind::Linear) {
            s.from = s.to = r(get_rational(so, "speed", ta));
        } else if (s.kind == tl::TimeSegment::Kind::Ramp) {
            s.from = r(get_rational(so, "from", ta));
            s.to = r(get_rational(so, "to", ta));
        }
        segments.push_back(s);
    }
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    auto map = tl::TimeMap::segmented(std::move(segments));
    if (!map) {
        return std::unexpected(invalid(map.error().message(), ta));
    }
    return map;
}

Result<tl::Clip> read_clip(Element e, std::string_view context) {
    Object o;
    if (e.get(o) != simdjson::SUCCESS) {
        return std::unexpected(invalid("a clip is an object", std::string(context)));
    }
    Reader r;
    tl::Clip c;
    c.id = tl::ClipId(r(get<std::uint64_t>(o, "id", context)));
    const std::string at = std::format("{} {}", context, c.id.value());
    c.media = tl::MediaId(r(get<std::uint64_t>(o, "media", at)));
    c.start = r(get_time(o, "start", at));
    c.duration = r(get_time(o, "duration", at));
    c.source_in = r(get_time(o, "source_in", at));
    if (Element segments; o.at_key("time_map").get(segments) == simdjson::SUCCESS) {
        // Format 2 (ADR-0013): a segmented map instead of a constant speed.
        auto read = read_time_map(o, at);
        if (!read) {
            return std::unexpected(read.error());
        }
        c.time_map = *read;
    } else if (Element speed; o.at_key("speed").get(speed) == simdjson::SUCCESS) {
        // Not through Reader: GCC 16 -O2 reports Result<TimeMap>'s moved value as maybe
        // uninitialized (a false positive) inside its template.
        auto map = tl::TimeMap::constant(r(get_rational(o, "speed", at), Rational::literal(1, 1)));
        if (map) {
            c.time_map = *map;
        } else {
            r.fail(map.error());
        }
    }
    c.video = r(read_video(o, at));
    c.audio = r(read_audio(o, at));
    c.audio_detached = r(get_or<bool>(o, "audio_detached", false, at));
    if (Object a; o.at_key("anchor").get(a) == simdjson::SUCCESS) {
        // Format 2 (ADR-0014); Timeline::restore checks the connection.
        const std::string aa = where("anchor", at);
        c.anchor = tl::Anchor{.primary = tl::ClipId(r(get<std::uint64_t>(a, "primary", aa))),
                              .source = r(get_time(a, "source", aa))};
    }
    if (Object t; o.at_key("title").get(t) == simdjson::SUCCESS) {
        // Format 3 (ADR-0015); Timeline::restore checks sizes and ranges. The text is bounded
        // here too, before it is copied.
        const std::string ta = where("title", at);
        tl::Title title;
        const std::string_view text = r(get<std::string_view>(t, "text", ta));
        const std::string_view font =
            r(get_or<std::string_view>(t, "font", std::string_view{}, ta));
        if (text.size() > tl::kMaxTitleBytes || font.size() > 256) {
            return std::unexpected(invalid("title text or font too long", ta));
        }
        title.text = std::string(text);
        title.font = std::string(font);
        title.size = r(get_or<double>(t, "size", 0.08, ta));
        if (Object rgba; t.at_key("color").get(rgba) == simdjson::SUCCESS) {
            const std::string ca = where("color", ta);
            title.color = {r(get_float(rgba, "r", 1.0F, ca)), r(get_float(rgba, "g", 1.0F, ca)),
                           r(get_float(rgba, "b", 1.0F, ca)), r(get_float(rgba, "a", 1.0F, ca))};
        }
        title.placement =
            r(get_enum(t, "placement", kTitlePlacements, tl::TitlePlacement::LowerThird, ta));
        c.title = std::move(title);
    }
    if (Object t; o.at_key("transition_in").get(t) == simdjson::SUCCESS) {
        const std::string ta = where("transition_in", at);
        c.transition_in = tl::Transition{
            .kind = r(get_enum(t, "kind", kTransitions, tl::TransitionKind::Dissolve, ta)),
            .duration = r(get_time(t, "duration", ta))};
    }
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    return c;
}

constexpr std::array kKnownFields{"format", "format_version", "canvas",
                                  "media",  "luts",           "sequence"};

} // namespace

// ------------------------------------------------------------------------------------- public

Result<Fingerprint> fingerprint_file(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::unexpected(io_error("cannot read the file size", path, ec.value()));
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(io_error("cannot open", path, errno));
    }
    constexpr std::uint64_t kSpan = std::uint64_t{64} * 1024;
    std::uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a 64
    std::vector<char> buf(kSpan);
    const auto mix = [&](std::uint64_t offset) {
        in.seekg(static_cast<std::streamoff>(offset));
        in.read(buf.data(), static_cast<std::streamsize>(std::min(kSpan, size - offset)));
        for (const char c : std::span(buf.data(), static_cast<std::size_t>(in.gcount()))) {
            hash = (hash ^ static_cast<unsigned char>(c)) * 0x100000001b3ULL;
        }
    };
    mix(0);
    if (size > kSpan) {
        in.clear();
        mix(std::max(kSpan, size - kSpan));
    }
    return Fingerprint{.size = size, .hash = hash};
}

std::optional<std::filesystem::path> find_relocated(const std::filesystem::path& original,
                                                    const Fingerprint& fingerprint,
                                                    std::span<const std::filesystem::path> roots) {
    const auto name = original.filename();
    if (name.empty()) {
        return std::nullopt;
    }
    for (const std::filesystem::path& root : roots) {
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(
            root, std::filesystem::directory_options::skip_permission_denied, ec);
        std::size_t seen = 0;
        for (; !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (++seen > kRelinkEntries) {
                break; // a huge tree: give up on this root rather than stall
            }
            if (it.depth() >= kRelinkDepth) {
                it.disable_recursion_pending();
            }
            std::error_code fe;
            if (it->path().filename() != name || !it->is_regular_file(fe) ||
                it->file_size(fe) != fingerprint.size) {
                continue;
            }
            if (auto print = fingerprint_file(it->path()); print && *print == fingerprint) {
                return it->path();
            }
        }
    }
    return std::nullopt;
}

Result<std::string> to_json(const Document& doc, const std::filesystem::path& project_dir) {
    Writer w;
    w.begin_object();
    w.field("format", "omamovie-project");
    w.field("format_version", kFormatVersion);
    w.key("canvas");
    w.begin_object();
    w.field("width", doc.canvas_width);
    w.field("height", doc.canvas_height);
    w.end_object();
    w.key("media");
    w.begin_array();
    for (const MediaRef& m : doc.media) {
        w.begin_object();
        w.field("id", m.info.id.value());
        w.field("name", m.name);
        w.field("path", relative_to(m.path, project_dir));
        w.field("absolute_path", m.path.string());
        w.field("size", m.fingerprint.size);
        w.field("hash", std::format("{:016x}", m.fingerprint.hash));
        time(w, "start", m.info.start);
        time(w, "duration", m.info.duration);
        w.field("has_video", m.info.has_video);
        w.field("has_audio", m.info.has_audio);
        w.field("still", m.info.still);
        w.field("audio_only", m.audio_only);
        w.end_object();
    }
    w.end_array();
    w.key("luts");
    w.begin_array();
    for (const LutRef& l : doc.luts) {
        w.begin_object();
        w.field("id", l.info.id.value());
        w.field("name", l.info.name);
        w.field("path", relative_to(l.path, project_dir));
        w.field("absolute_path", l.path.string());
        w.end_object();
    }
    w.end_array();
    if (doc.timeline) {
        const tl::Timeline& t = *doc.timeline;
        w.key("sequence");
        w.begin_object();
        w.field("frame_rate", rational(t.frame_rate().fps()));
        w.field("timebase", rational(t.timebase()));
        w.field("storyline", doc.storyline.value());
        // Which library media and LUTs the sequence has registered.
        w.key("media");
        w.begin_array();
        for (const tl::MediaInfo& m : t.media()) {
            w.value(m.id.value());
        }
        w.end_array();
        w.key("luts");
        w.begin_array();
        for (const tl::LutInfo& l : t.luts()) {
            w.value(l.id.value());
        }
        w.end_array();
        w.key("tracks");
        w.begin_array();
        for (const tl::Track& track : t.tracks()) {
            w.begin_object();
            w.field("id", track.id.value());
            w.field("kind", name_of(kTrackKinds, track.kind));
            w.field("name", track.name);
            w.field("muted", track.muted);
            w.field("hidden", track.hidden);
            w.key("clips");
            w.begin_array();
            for (const tl::Clip& c : track.clips) {
                clip(w, c);
            }
            w.end_array();
            w.end_object();
        }
        w.end_array();
        w.key("markers");
        w.begin_array();
        for (const tl::Marker& m : t.markers()) {
            w.begin_object();
            w.field("id", m.id.value());
            time(w, "time", m.time);
            w.field("name", m.name);
            w.end_object();
        }
        w.end_array();
        w.key("captions"); // format 5 (ADR-0017)
        w.begin_array();
        for (const tl::Caption& c : t.captions()) {
            w.begin_object();
            w.field("id", c.id.value());
            time(w, "start", c.start);
            time(w, "duration", c.duration);
            w.field("text", c.text);
            w.end_object();
        }
        w.end_array();
        w.end_object();
    }
    for (const auto& [k, raw] : doc.unknown) {
        w.key(k);
        w.raw(raw);
    }
    w.end_object();
    return w.take();
}

Result<Document> from_json(std::string_view json, const std::filesystem::path& project_dir) {
    if (json.size() > kMaxFileBytes) {
        return std::unexpected(invalid("project file too large"));
    }
    if (json.empty()) {
        return std::unexpected(invalid("the project file is empty"));
    }
    simdjson::dom::parser parser(kMaxFileBytes);
    if (parser.allocate(json.size(), kMaxDepth) != simdjson::SUCCESS) {
        return std::unexpected(invalid("cannot allocate the parser"));
    }
    const simdjson::padded_string padded(json);
    Element root;
    if (const auto err = parser.parse(padded).get(root); err != simdjson::SUCCESS) {
        return std::unexpected(
            invalid(std::format("not valid JSON: {}", simdjson::error_message(err))));
    }
    Object o;
    if (root.get(o) != simdjson::SUCCESS) {
        return std::unexpected(invalid("the project is not a JSON object"));
    }
    Reader r;
    if (r(get<std::string_view>(o, "format", "")) != "omamovie-project") {
        return std::unexpected(r.failed() ? r.error() : invalid("not an OmaMovie project"));
    }
    const auto version = r(get<std::int64_t>(o, "format_version", ""));
    if (r.failed()) {
        return std::unexpected(r.error());
    }
    if (version > kFormatVersion) {
        return std::unexpected(Error(ErrorCode::Unsupported, Category::Project,
                                     "made by a newer version of OmaMovie; update to open it",
                                     std::format("format_version {} (this version reads up to {})",
                                                 version, kFormatVersion)));
    }
    if (version < 1) {
        return std::unexpected(invalid("invalid format_version"));
    }
    // Migrations vN -> vN+1 (ADR-0007). 1 -> 2 changes nothing: version 2 only adds the optional
    // `time_map` of a clip (ADR-0013), so a version 1 document already reads as version 2.
    // 2 -> 3 changes nothing either: version 3 only adds a clip's optional `title` (ADR-0015).
    // 3 -> 4 turns a clip's `filter` and `sharpness` into `effects` (ADR-0016, read_video).
    // 4 -> 5 changes nothing: version 5 only adds the sequence's optional `captions` (ADR-0017).
    // 5 -> 6 changes nothing either: version 6 only adds optional `opacity_keys`/`gain_keys`.

    Document doc;
    const auto resolve = [&](std::string_view relative, std::string_view absolute) {
        std::filesystem::path abs(absolute);
        std::error_code ec;
        if (!project_dir.empty() && !std::filesystem::exists(abs, ec)) {
            auto rel = (project_dir / relative).lexically_normal();
            if (std::filesystem::exists(rel, ec)) {
                return rel; // the project folder moved with its media
            }
        }
        return abs;
    };
    if (Object canvas; o.at_key("canvas").get(canvas) == simdjson::SUCCESS) {
        doc.canvas_width = static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
            r(get_or<std::uint64_t>(canvas, "width", 1920, "canvas")), 16, 16384));
        doc.canvas_height = static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
            r(get_or<std::uint64_t>(canvas, "height", 1080, "canvas")), 16, 16384));
    }
    for (const Element e : r(get_array(o, "media", "", true))) {
        Object m;
        if (e.get(m) != simdjson::SUCCESS) {
            return std::unexpected(invalid("a media item is an object", "media"));
        }
        MediaRef ref;
        ref.info.id = tl::MediaId(r(get<std::uint64_t>(m, "id", "media")));
        const std::string at = std::format("media {}", ref.info.id.value());
        ref.name = std::string(r(get<std::string_view>(m, "name", at)));
        ref.path = resolve(r(get<std::string_view>(m, "path", at)),
                           r(get<std::string_view>(m, "absolute_path", at)));
        ref.fingerprint.size = r(get_or<std::uint64_t>(m, "size", 0, at));
        const std::string_view hash = r(get_or<std::string_view>(m, "hash", "0", at));
        std::from_chars(hash.data(), hash.data() + hash.size(), ref.fingerprint.hash, 16);
        ref.info.start = r(get_time(m, "start", at));
        ref.info.duration = r(get_time(m, "duration", at));
        ref.info.has_video = r(get_or<bool>(m, "has_video", false, at));
        ref.info.has_audio = r(get_or<bool>(m, "has_audio", false, at));
        ref.info.still = r(get_or<bool>(m, "still", false, at));
        ref.audio_only = r(get_or<bool>(m, "audio_only", false, at));
        doc.media.push_back(std::move(ref));
    }
    for (const Element e : r(get_array(o, "luts", "", true))) {
        Object l;
        if (e.get(l) != simdjson::SUCCESS) {
            return std::unexpected(invalid("a LUT is an object", "luts"));
        }
        LutRef ref;
        ref.info.id = tl::LutId(r(get<std::uint64_t>(l, "id", "luts")));
        ref.info.name = std::string(r(get<std::string_view>(l, "name", "luts")));
        ref.path = resolve(r(get<std::string_view>(l, "path", "luts")),
                           r(get<std::string_view>(l, "absolute_path", "luts")));
        doc.luts.push_back(std::move(ref));
    }
    if (r.failed()) {
        return std::unexpected(r.error());
    }

    if (Object seq; o.at_key("sequence").get(seq) == simdjson::SUCCESS) {
        const auto rate = FrameRate::make(
            r(get_rational(seq, "frame_rate", "sequence"), Rational::literal(30, 1)));
        const Rational timebase =
            r(get_rational(seq, "timebase", "sequence"), Rational::literal(1, 30));
        doc.storyline = tl::TrackId(r(get_or<std::uint64_t>(seq, "storyline", 0, "sequence")));
        std::vector<tl::MediaInfo> media;
        for (const Element e : r(get_array(seq, "media", "sequence", true))) {
            std::uint64_t id = 0;
            const auto it = e.get(id) == simdjson::SUCCESS
                                ? std::ranges::find(doc.media, tl::MediaId(id),
                                                    [](const MediaRef& m) { return m.info.id; })
                                : doc.media.end();
            if (it == doc.media.end()) {
                return std::unexpected(
                    invalid("the sequence references unknown media", "sequence.media"));
            }
            media.push_back(it->info);
        }
        std::vector<tl::LutInfo> luts;
        for (const Element e : r(get_array(seq, "luts", "sequence", true))) {
            std::uint64_t id = 0;
            const auto it = e.get(id) == simdjson::SUCCESS
                                ? std::ranges::find(doc.luts, tl::LutId(id),
                                                    [](const LutRef& l) { return l.info.id; })
                                : doc.luts.end();
            if (it == doc.luts.end()) {
                return std::unexpected(
                    invalid("the sequence references an unknown LUT", "sequence.luts"));
            }
            luts.push_back(it->info);
        }
        std::vector<tl::Track> tracks;
        for (const Element e : r(get_array(seq, "tracks", "sequence"))) {
            Object t;
            if (e.get(t) != simdjson::SUCCESS) {
                return std::unexpected(invalid("a track is an object", "sequence.tracks"));
            }
            tl::Track track;
            track.id = tl::TrackId(r(get<std::uint64_t>(t, "id", "track")));
            const std::string at = std::format("track {}", track.id.value());
            track.kind = r(get_enum(t, "kind", kTrackKinds, tl::TrackKind::Video, at));
            track.name = std::string(r(get_or<std::string_view>(t, "name", "", at)));
            track.muted = r(get_or<bool>(t, "muted", false, at));
            track.hidden = r(get_or<bool>(t, "hidden", false, at));
            for (const Element c : r(get_array(t, "clips", at, true))) {
                track.clips.push_back(r(read_clip(c, "clip")));
                if (r.failed()) {
                    return std::unexpected(r.error());
                }
            }
            tracks.push_back(std::move(track));
        }
        std::vector<tl::Marker> markers;
        for (const Element e : r(get_array(seq, "markers", "sequence", true))) {
            Object m;
            if (e.get(m) != simdjson::SUCCESS) {
                return std::unexpected(invalid("a marker is an object", "sequence.markers"));
            }
            markers.push_back(
                {.id = tl::MarkerId(r(get<std::uint64_t>(m, "id", "marker"))),
                 .time = r(get_time(m, "time", "marker")),
                 .name = std::string(r(get_or<std::string_view>(m, "name", "", "marker")))});
        }
        // Format 5 (ADR-0017); Timeline::restore checks timing, order and overlaps. The count and
        // each text are bounded here, before anything is copied.
        std::vector<tl::Caption> captions;
        for (const Element e : r(get_array(seq, "captions", "sequence", true))) {
            Object c;
            std::string_view text;
            if (e.get(c) != simdjson::SUCCESS || captions.size() >= tl::kMaxCaptions ||
                c.at_key("text").get(text) != simdjson::SUCCESS ||
                text.size() > tl::kMaxCaptionBytes) {
                return std::unexpected(
                    invalid("invalid or too many captions", "sequence.captions"));
            }
            captions.push_back({.id = tl::CaptionId(r(get<std::uint64_t>(c, "id", "caption"))),
                                .start = r(get_time(c, "start", "caption")),
                                .duration = r(get_time(c, "duration", "caption")),
                                .text = std::string(text)});
        }
        if (r.failed()) {
            return std::unexpected(r.error());
        }
        if (!rate) {
            return std::unexpected(invalid(rate.error().message(), "sequence.frame_rate"));
        }
        auto timeline =
            tl::Timeline::restore(*rate, timebase, std::move(tracks), std::move(markers),
                                  std::move(media), std::move(luts), std::move(captions));
        if (!timeline) {
            return std::unexpected(Error(ErrorCode::InvalidData, Category::Project,
                                         timeline.error().message(), timeline.error().context()));
        }
        if (doc.storyline.valid() && timeline->find_track(doc.storyline) == nullptr) {
            return std::unexpected(invalid("the storyline is not a track", "sequence.storyline"));
        }
        doc.timeline = std::move(*timeline);
    }

    for (const auto field : o) {
        if (std::ranges::find(kKnownFields, field.key) == kKnownFields.end()) {
            doc.unknown.emplace_back(std::string(field.key), simdjson::minify(field.value));
        }
    }
    return doc;
}

Result<void> save(const Document& doc, const std::filesystem::path& file) {
    const std::filesystem::path dir = file.parent_path().empty() ? "." : file.parent_path();
    auto text = to_json(doc, std::filesystem::absolute(dir));
    if (!text) {
        return std::unexpected(text.error());
    }
    // Same directory, so the rename is atomic on the same filesystem.
    std::string tmp = (dir / ("." + file.filename().string() + ".XXXXXX")).string();
    const int fd = ::mkstemp(tmp.data());
    if (fd < 0) {
        return std::unexpected(io_error("cannot create a temporary file", dir, errno));
    }
    const auto fail = [&](std::string what, int err) -> Result<void> {
        ::close(fd);
        ::unlink(tmp.c_str());
        return std::unexpected(io_error(std::move(what), file, err));
    };
    for (std::size_t written = 0; written < text->size();) {
        const ssize_t n = ::write(fd, text->data() + written, text->size() - written);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return fail("cannot write the project", errno);
        }
        written += static_cast<std::size_t>(n);
    }
    if (::fchmod(fd, 0644) != 0 || ::fsync(fd) != 0) {
        return fail("cannot sync the project", errno);
    }
    if (::close(fd) != 0) {
        ::unlink(tmp.c_str());
        return std::unexpected(io_error("cannot close the project", file, errno));
    }
    if (::rename(tmp.c_str(), file.c_str()) != 0) {
        const int err = errno;
        ::unlink(tmp.c_str());
        return std::unexpected(io_error("cannot replace the project", file, err));
    }
    // The rename itself is durable only once the directory entry is synced.
    const int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd < 0) {
        return std::unexpected(io_error("cannot open the project folder", dir, errno));
    }
    const int synced = ::fsync(dfd);
    const int err = errno;
    ::close(dfd);
    if (synced != 0) {
        return std::unexpected(io_error("cannot sync the project folder", dir, err));
    }
    return {};
}

Result<Document> load(const std::filesystem::path& file) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec) {
        return std::unexpected(io_error("cannot open the project", file, ec.value()));
    }
    if (size > kMaxFileBytes) {
        return std::unexpected(invalid("project file too large", file.string()));
    }
    std::ifstream in(file, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!in.read(text.data(), static_cast<std::streamsize>(size))) {
        return std::unexpected(io_error("cannot read the project", file, errno));
    }
    auto doc = from_json(text, std::filesystem::absolute(file).parent_path());
    if (!doc) {
        const Error& e = doc.error();
        return std::unexpected(Error(e.code(), e.category(), e.message(),
                                     e.context().empty()
                                         ? file.string()
                                         : std::format("{}: {}", file.string(), e.context())));
    }
    return doc;
}

} // namespace oma::project
