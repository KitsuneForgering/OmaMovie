#include "oma/project/document.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"

#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "oma_test.hpp"

namespace fs = std::filesystem;
namespace tl = oma::timeline;
using oma::project::Document;

namespace {

oma::Rational q(std::int64_t num, std::int64_t den) {
    return oma::Rational::make(num, den).value();
}

oma::RationalTime f(std::int64_t frames) {
    return oma::RationalTime::make(frames, q(1, 30)).value();
}

oma::RationalTime mf(std::int64_t frames) {
    return oma::RationalTime::make(frames * 3000, q(1, 90000)).value();
}

// A scratch directory under the system temp, removed by the caller.
fs::path scratch(const char* name) {
    const fs::path dir =
        fs::temp_directory_path() / ("oma-project-test-" + std::to_string(::getpid())) / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void write_file(const fs::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// A project using every kind of saved state: two tracks, adjusted clips, transform keys, a
// transition, detached audio, a LUT and a marker. Media paths sit under `dir`.
Document sample(const fs::path& dir) {
    const tl::MediaId camera{7};
    const tl::LutId lut{3};
    tl::Editor ed(tl::Timeline::create(oma::frame_rates::k30, q(1, 30)).value());
    const tl::MediaInfo info{.id = camera,
                             .start = mf(0),
                             .duration = mf(300),
                             .has_video = true,
                             .has_audio = true,
                             .still = false};
    (void)ed.add_media(info);
    (void)ed.add_lut(tl::LutInfo{.id = lut, .name = "Teal \"orange\""});
    const tl::TrackId video = ed.new_track_id();
    const tl::TrackId lane = ed.new_track_id();
    (void)ed.execute(tl::edit::add_track(video, tl::TrackKind::Video, "Storyline"));
    (void)ed.execute(tl::edit::add_track(lane, tl::TrackKind::Audio, "Audio 1"));
    tl::edit::ClipSource source{.media = camera,
                                .source_in = mf(10),
                                .duration = f(40),
                                .time_map = {},
                                .video = {},
                                .audio = {}};
    source.video.opacity = 0.75F;
    source.video.crop.left = 0.125;
    source.video.color.exposure = 0.3;
    // In the order a format 3 filter and sharpness migrate to (ADR-0016), then an effect this
    // build does not know, which must survive the round trip.
    source.video.effects = {{.definition = "oma.detail",
                             .enabled = true,
                             .params = {{.name = "amount", .value = -0.4}}},
                            {.definition = "oma.look.sepia",
                             .enabled = true,
                             .params = {{.name = "amount", .value = 0.6}}},
                            {.definition = "org.example.glow",
                             .enabled = false,
                             .params = {{.name = "radius \"px\"", .value = 3}}}};
    source.video.grade.cdl.slope = {1.1, 0.95, 1.0};
    source.video.grade.curves.master = {{.x = 0.0, .y = 0.05}, {.x = 1.0, .y = 0.9}};
    source.video.grade.lut = lut;
    source.video.grade.lut_amount = 0.5;
    source.video.transform_keys = {
        {.at = mf(10), .value = {}, .interpolation = tl::Interpolation::Ease},
        {.at = mf(50),
         .value =
             {.offset_x = 12.5, .offset_y = -3, .scale_x = 1.2, .scale_y = 1.2, .rotation = 0.1},
         .interpolation = tl::Interpolation::Linear}};
    source.audio.gain = 0.5F;
    source.audio.fade_in = f(5);
    source.audio.eq = {.low_db = 3, .mid_db = -1.5F, .high_db = 0};
    const tl::ClipId a = ed.new_clip_id();
    const tl::ClipId b = ed.new_clip_id();
    (void)ed.execute(tl::edit::append(video, a, source));
    source.source_in = mf(100);
    (void)ed.execute(tl::edit::append(video, b, source));
    (void)ed.execute(tl::edit::set_transition(
        b, tl::Transition{.kind = tl::TransitionKind::Wipe, .duration = f(10)}));
    (void)ed.execute(tl::edit::detach_audio(a, lane, ed.new_clip_id()));
    (void)ed.execute(tl::edit::add_marker(ed.new_marker_id(), f(12), "Intro, \"take 2\"\n"));
    // Captions (format 5, ADR-0017): quotes, a line break and non-ASCII text survive.
    (void)ed.execute(tl::edit::add_caption({.id = ed.new_caption_id(),
                                            .start = f(0),
                                            .duration = f(30),
                                            .text = "Olá, \"mundo\"\nsegunda linha"}));
    (void)ed.execute(tl::edit::add_caption(
        {.id = ed.new_caption_id(), .start = f(45), .duration = f(15), .text = "→ fim"}));
    // A segmented time map (format 2, ADR-0013): ramp 1 -> 2, hold, then back to the start.
    const tl::ClipId c = ed.new_clip_id();
    source.source_in = mf(200);
    source.video.transform_keys.clear();
    (void)ed.execute(tl::edit::append(video, c, source));
    using Kind = tl::TimeSegment::Kind;
    (void)ed.execute(tl::edit::set_time_map(
        c,
        tl::TimeMap::segmented(
            {{.kind = Kind::Ramp, .length = 10, .from = q(1, 1), .to = q(2, 1)},
             {.kind = Kind::Freeze, .length = 5, .from = q(0, 1), .to = q(0, 1)},
             {.kind = Kind::Linear, .length = 15, .from = q(-1, 1), .to = q(-1, 1)}})
            .value(),
        true));
    // A title connected to the first storyline clip (format 2, ADR-0014).
    const tl::TrackId titles = ed.new_track_id();
    (void)ed.execute(tl::edit::add_track(titles, tl::TrackKind::Video, "Titles"));
    const tl::ClipId title = ed.new_clip_id();
    source.source_in = mf(0);
    (void)ed.execute(tl::edit::overwrite(titles, title, f(20), source));
    (void)ed.execute(tl::edit::trim_end(title, f(30), false));
    (void)ed.execute(tl::edit::connect(title, a));
    // A generated title (format 3, ADR-0015) on the same lane, with text that needs escaping.
    tl::Title text;
    text.text = "Chapter \"one\"\nnext line, é";
    text.font = "Inter";
    text.size = 0.125;
    text.color = {1.0F, 0.5F, 0.25F, 0.75F};
    text.placement = tl::TitlePlacement::Top;
    (void)ed.execute(tl::edit::overwrite(titles, ed.new_clip_id(), f(80),
                                         tl::edit::ClipSource{.media = tl::MediaId{},
                                                              .source_in = f(0),
                                                              .duration = f(15),
                                                              .time_map = {},
                                                              .video = {},
                                                              .audio = {},
                                                              .title = text}));
    Document doc;
    doc.media.push_back({.info = info,
                         .path = dir / "media" / "clip one.mp4",
                         .name = "clip one.mp4",
                         .audio_only = false,
                         .fingerprint = {.size = 1234, .hash = 0xdeadbeefcafeULL}});
    doc.luts.push_back(
        {.info = {.id = lut, .name = "Teal \"orange\""}, .path = dir / "looks" / "teal.cube"});
    doc.timeline = ed.timeline();
    doc.storyline = video;
    doc.canvas_width = 1080;
    doc.canvas_height = 1920;
    return doc;
}

void round_trips_exactly() {
    const fs::path dir = "/projects/vlog";
    const Document doc = sample(dir);
    const auto text = oma::project::to_json(doc, dir);
    expect(text.has_value()).toBeTruthy();
    auto back = oma::project::from_json(*text, dir);
    expect(back.has_value()).toBeTruthy();
    if (!back) {
        std::printf("    %s\n", back.error().summary().c_str());
        return;
    }
    expect(*oma::project::to_json(*back, dir)).toBe(*text);
    const tl::Timeline& t = *back->timeline;
    expect(t.tracks().size()).toBe(3U);
    const tl::Clip& connected = t.tracks()[2].clips.at(0);
    expect(connected.anchor.has_value()).toBeTruthy();
    expect(t.tracks()[2].clips.size()).toBe(2U);
    expect(t.tracks()[2].clips.at(1).title == doc.timeline->tracks()[2].clips.at(1).title)
        .toBeTruthy();
    expect(t.tracks()[2].clips.at(1).title->text == "Chapter \"one\"\nnext line, é").toBeTruthy();
    expect(connected.anchor == doc.timeline->tracks()[2].clips.at(0).anchor).toBeTruthy();
    expect(t.tracks()[0].clips.size()).toBe(3U);
    expect(t.tracks()[0].clips[2].time_map == doc.timeline->tracks()[0].clips[2].time_map)
        .toBeTruthy();
    expect(t.tracks()[0].clips[2].time_map.segments().size()).toBe(3U);
    const tl::Clip& a = t.tracks()[0].clips[0];
    expect(a.video == doc.timeline->tracks()[0].clips[0].video).toBeTruthy();
    expect(a.audio == doc.timeline->tracks()[0].clips[0].audio).toBeTruthy();
    expect(a.audio_detached).toBeTruthy();
    expect(t.tracks()[0].clips[1].transition_in.has_value()).toBeTruthy();
    expect(t.markers()[0].name).toBe(std::string("Intro, \"take 2\"\n"));
    expect(back->media[0].path.string()).toBe(std::string("/projects/vlog/media/clip one.mp4"));
    expect(back->media[0].fingerprint == doc.media[0].fingerprint).toBeTruthy();
    expect(static_cast<int>(back->canvas_height)).toBe(1920);
    // The file stores the media relative to the project too.
    expect(text->find("\"path\": \"media/clip one.mp4\"") != std::string::npos).toBeTruthy();
}

void new_ids_follow_the_saved_ones() {
    const Document doc = sample("/p");
    auto back = oma::project::from_json(*oma::project::to_json(doc, "/p"), "/p");
    tl::Editor ed(std::move(*back->timeline));
    std::uint64_t largest = 0;
    for (const tl::Track& t : ed.timeline().tracks()) {
        largest = std::max(largest, t.id.value());
        for (const tl::Clip& c : t.clips)
            largest = std::max(largest, c.id.value());
    }
    expect(ed.new_clip_id().value() > largest).toBeTruthy();
}

void refuses_newer_and_keeps_unknown_fields() {
    const std::string newer = R"({"format": "omamovie-project", "format_version": 99})";
    auto r = oma::project::from_json(newer, "");
    expect(!r && r.error().code() == oma::ErrorCode::Unsupported).toBeTruthy();
    const std::string extra =
        R"({"format": "omamovie-project", "format_version": 1, "future": {"a": [1, 2.5, "x"]}})";
    auto doc = oma::project::from_json(extra, "");
    expect(doc.has_value()).toBeTruthy();
    const auto again = oma::project::to_json(*doc, "");
    expect(again->find(R"("future": {"a":[1,2.5,"x"]})") != std::string::npos).toBeTruthy();
}

bool rejected(const std::string& text) {
    auto r = oma::project::from_json(text, "");
    return !r && r.error().code() == oma::ErrorCode::InvalidData;
}

void rejects_broken_files() {
    const std::string good = *oma::project::to_json(sample("/p"), "/p");
    expect(rejected(good.substr(0, good.size() / 2))).toBeTruthy(); // truncated
    expect(rejected("[]")).toBeTruthy();
    expect(rejected(R"({"format": "something else", "format_version": 1})")).toBeTruthy();
    auto replace = [&](const std::string& from, const std::string& to) {
        std::string s = good;
        const auto at = s.find(from);
        return at == std::string::npos ? good
                                       : s.replace(at, from.size(), to); // valid when not found
    };
    expect(rejected(replace("\"fit\": \"fit\"", "\"fit\": \"zoom\""))).toBeTruthy();
    expect(rejected(replace("\"timebase\": \"1/30\"", "\"timebase\": \"1/0\""))).toBeTruthy();
    expect(rejected(replace("\"media\": 7", "\"media\": 8"))).toBeTruthy(); // unknown media
    // Overlapping clips: the second clip moved onto the first.
    expect(rejected(replace("\"start\": {\n              \"value\": 40,",
                            "\"start\": {\n              \"value\": 20,")))
        .toBeTruthy();
    std::string deep(100, '[');
    expect(rejected(deep + std::string(100, ']'))).toBeTruthy();
}

void saves_atomically() {
    const fs::path dir = scratch("save");
    const fs::path file = dir / "vlog.omamovie";
    const Document doc = sample(dir);
    expect(oma::project::save(doc, file).has_value()).toBeTruthy();
    const std::string first = read_file(file);
    auto loaded = oma::project::load(file);
    expect(loaded.has_value()).toBeTruthy();
    // Replacing keeps exactly one file, no temporary left behind.
    Document changed = std::move(*loaded);
    changed.canvas_width = 1280;
    expect(oma::project::save(changed, file).has_value()).toBeTruthy();
    expect(std::distance(fs::directory_iterator(dir), fs::directory_iterator())).toBe(1L);
    // A failing save (read-only folder) leaves the previous file as it was.
    fs::permissions(dir, fs::perms::owner_read | fs::perms::owner_exec);
    const bool refused = !oma::project::save(doc, file).has_value();
    fs::permissions(dir, fs::perms::owner_all);
    if (::geteuid() != 0) { // root writes anyway
        expect(refused).toBeTruthy();
        expect(read_file(file) != first &&
               read_file(file).find("\"width\": 1280") != std::string::npos)
            .toBeTruthy();
    }
    fs::remove_all(dir.parent_path());
}

void finds_media_next_to_a_moved_project() {
    const fs::path root = scratch("moved");
    fs::create_directories(root / "before" / "media");
    write_file(root / "before" / "media" / "clip one.mp4", "not really a video");
    Document doc = sample(root / "before");
    expect(oma::project::save(doc, root / "before" / "vlog.omamovie").has_value()).toBeTruthy();
    fs::rename(root / "before", root / "after");
    auto loaded = oma::project::load(root / "after" / "vlog.omamovie");
    expect(loaded.has_value()).toBeTruthy();
    expect(loaded->media[0].path == (root / "after" / "media" / "clip one.mp4")).toBeTruthy();
    fs::remove_all(root.parent_path());
}

void fingerprints_tell_files_apart() {
    const fs::path dir = scratch("fingerprint");
    write_file(dir / "a", std::string(200000, 'x'));
    write_file(dir / "b", std::string(200000, 'x') + "y");
    write_file(dir / "c", std::string(200000, 'x'));
    const auto a = oma::project::fingerprint_file(dir / "a");
    const auto b = oma::project::fingerprint_file(dir / "b");
    const auto c = oma::project::fingerprint_file(dir / "c");
    expect(a && b && c).toBeTruthy();
    expect(*a == *c && !(*a == *b)).toBeTruthy();
    expect(oma::project::fingerprint_file(dir / "missing").has_value()).toBeFalsy();
    fs::remove_all(dir.parent_path());
}

} // namespace

// Format 1 had no `time_map`; a version 1 file reads unchanged (the 1 -> 2 migration is the
// identity, ADR-0013). The fixture is a version 2 document without segmented maps, which is
// byte-for-byte what version 1 wrote apart from the version number.
void reads_format_1() {
    Document doc = sample("/p");
    auto t = *doc.timeline;
    const tl::Clip last = t.tracks()[0].clips.back();
    const tl::Clip title = t.tracks()[2].clips.back();
    tl::Editor ed(std::move(t));
    (void)ed.execute(tl::edit::remove_clip(last.id));
    (void)ed.execute(tl::edit::remove_clip(title.id));
    doc.timeline = ed.timeline();
    std::string text = *oma::project::to_json(doc, "/p");
    const std::string current = std::format("\"format_version\": {}", oma::project::kFormatVersion);
    const auto at = text.find(current);
    expect(at != std::string::npos).toBeTruthy();
    text.replace(at, current.size(), "\"format_version\": 1");
    auto back = oma::project::from_json(text, "/p");
    expect(back.has_value()).toBeTruthy();
    if (back) {
        expect(back->timeline->tracks()[0].clips.size()).toBe(2U);
    }
}

// Migration 3 -> 4 (ADR-0016): a clip's filter and sharpness read as the same effects.
void migrates_filters_to_effects() {
    Document doc = sample("/p");
    std::string text = *oma::project::to_json(doc, "/p");
    const std::string current = std::format("\"format_version\": {}", oma::project::kFormatVersion);
    text.replace(text.find(current), current.size(), "\"format_version\": 3");
    // Every media clip of the sample has the same effects (an empty list prints as []); write
    // each list the way format 3 did.
    std::size_t lists = 0;
    for (std::size_t at = text.find("\"effects\": [\n"); at != std::string::npos;
         at = text.find("\"effects\": [\n", at)) {
        // The unknown effect closes each list, and parameter objects hold no ']'.
        const std::size_t end = text.find(']', text.find("org.example.glow", at));
        text.replace(at, end + 1 - at,
                     "\"filter\": {\"kind\": \"sepia\", \"amount\": 0.6}, \"sharpness\": -0.4");
        ++lists;
    }
    expect(lists > 0U).toBeTruthy();
    auto back = oma::project::from_json(text, "/p");
    expect(back.has_value()).toBeTruthy();
    if (!back) {
        return;
    }
    const auto& clips = back->timeline->tracks()[0].clips;
    expect(clips.empty()).toBeFalsy();
    for (const tl::Clip& c : clips) {
        expect(c.video.effects.size()).toBe(2U);
        if (c.video.effects.size() == 2U) {
            const auto expected = std::vector<tl::Effect>(
                doc.timeline->tracks()[0].clips[0].video.effects.begin(),
                doc.timeline->tracks()[0].clips[0].video.effects.begin() + 2);
            expect(c.video.effects == expected).toBeTruthy();
        }
    }
}

// Effect names are bounded before they are copied and validated against the definitions.
void rejects_bad_effects() {
    std::string text = *oma::project::to_json(sample("/p"), "/p");
    const auto broken = [&](std::string_view from, std::string_view to) {
        std::string t = text;
        t.replace(t.find(from), from.size(), to);
        return !oma::project::from_json(t, "/p").has_value();
    };
    expect(broken("\"amount\": 0.6", "\"amount\": 1.6")).toBeTruthy();      // out of range
    expect(broken("\"amount\": 0.6", "\"strength\": 0.6")).toBeTruthy();    // unknown name
    expect(broken("\"amount\": 0.6", "\"amount\": \"high\"")).toBeTruthy(); // not a number
    expect(broken("\"oma.look.sepia\"", "\"oma.detail\"")).toBeTruthy();    // applied twice
    expect(broken("\"org.example.glow\"", "\"" + std::string(100, 'x') + "\"")).toBeTruthy();
}

// Captions are bounded at the file boundary: an oversized text is refused before it is copied.
void rejects_oversized_captions() {
    std::string text = *oma::project::to_json(sample("/p"), "/p");
    const std::string from = "\"text\": \"→ fim\"";
    const auto at = text.find(from);
    expect(at != std::string::npos).toBeTruthy();
    if (at == std::string::npos) {
        return;
    }
    text.replace(at, from.size(),
                 "\"text\": \"" + std::string(tl::kMaxCaptionBytes + 1, 'x') + "\"");
    expect(oma::project::from_json(text, "/p").has_value()).toBeFalsy();
}

// Relink (CLAUDE.md §14): the moved file is found under a root by name and content; a decoy
// with the same name and size but other bytes is skipped, and nothing is found when it is gone.
void finds_relocated_media() {
    const fs::path dir = scratch("relink");
    fs::create_directories(dir / "old");
    write_file(dir / "old" / "clip.mp4", std::string(5000, 'a'));
    const auto print = oma::project::fingerprint_file(dir / "old" / "clip.mp4");
    expect(print.has_value()).toBeTruthy();
    if (!print) {
        return;
    }
    fs::create_directories(dir / "new" / "a" / "b");
    fs::create_directories(dir / "decoy");
    write_file(dir / "decoy" / "clip.mp4", std::string(5000, 'b')); // same name and size
    fs::rename(dir / "old" / "clip.mp4", dir / "new" / "a" / "b" / "clip.mp4");
    const std::array roots{dir / "decoy", dir / "new"};
    const auto found = oma::project::find_relocated(dir / "old" / "clip.mp4", *print, roots);
    expect(found.has_value()).toBeTruthy();
    if (found) {
        expect(*found == dir / "new" / "a" / "b" / "clip.mp4").toBeTruthy();
    }
    fs::remove(dir / "new" / "a" / "b" / "clip.mp4");
    expect(oma::project::find_relocated(dir / "old" / "clip.mp4", *print, roots).has_value())
        .toBeFalsy();
    fs::remove_all(dir);
}

// A title's text is bounded before it is copied (ADR-0015): a huge one is an error.
void rejects_oversized_titles() {
    const Document doc = sample("/p");
    std::string text = *oma::project::to_json(doc, "/p");
    const auto at = text.find("Chapter ");
    expect(at != std::string::npos).toBeTruthy();
    text.insert(at, std::string(tl::kMaxTitleBytes + 1, 'x'));
    expect(oma::project::from_json(text, "/p").has_value()).toBeFalsy();
}

// Segment lists from an untrusted file: bad shapes and too many segments are errors.
void rejects_bad_time_maps() {
    const std::string good = *oma::project::to_json(sample("/p"), "/p");
    // Replaces the segmented clip's whole segment array (segments hold no nested arrays).
    const auto with = [&](const std::string& replacement) {
        std::string text = good;
        const auto key = text.find("\"time_map\"");
        const auto open = text.find('[', key);
        const auto close = text.find(']', open);
        if (key == std::string::npos || open == std::string::npos || close == std::string::npos) {
            return std::string{};
        }
        text.replace(open, close - open + 1, "[" + replacement + "]");
        return text;
    };
    const auto rejected = [](const std::string& text) {
        return !text.empty() && !oma::project::from_json(text, "/p").has_value();
    };
    // The splice itself keeps a valid document when the segments are valid.
    expect(
        oma::project::from_json(with(R"({"kind": "linear", "length": 30, "speed": "1/1"})"), "/p")
            .has_value())
        .toBeTruthy();
    expect(rejected(with(R"({"kind": "ramp", "length": 10, "from": "1/1", "to": "-1/1"})")))
        .toBeTruthy();
    expect(rejected(with(R"({"kind": "linear", "length": 0, "speed": "1/1"})"))).toBeTruthy();
    expect(rejected(with(R"({"kind": "sideways", "length": 10})"))).toBeTruthy();
    std::string many;
    for (int i = 0; i < 300; ++i) {
        many += std::string(i == 0 ? "" : ", ") + R"({"kind": "freeze", "length": 1})";
    }
    expect(rejected(with(many))).toBeTruthy();
}

void run_document_tests() {
    describe("project::Document", {
        it("round-trips every saved field exactly", { round_trips_exactly(); });
        it("continues IDs after the saved ones", { new_ids_follow_the_saved_ones(); });
        it("refuses newer versions and keeps unknown fields",
           { refuses_newer_and_keeps_unknown_fields(); });
        it("rejects truncated, mistyped and inconsistent files", { rejects_broken_files(); });
        it("saves atomically and leaves the old file on failure", { saves_atomically(); });
        it("finds media next to a moved project", { finds_media_next_to_a_moved_project(); });
        it("fingerprints files", { fingerprints_tell_files_apart(); });
        it("reads format 1 projects", { reads_format_1(); });
        it("rejects malformed time maps", { rejects_bad_time_maps(); });
        it("rejects oversized titles", { rejects_oversized_titles(); });
        it("migrates format 3 filters to effects", { migrates_filters_to_effects(); });
        it("rejects malformed effects", { rejects_bad_effects(); });
        it("rejects oversized captions", { rejects_oversized_captions(); });
        it("finds moved media by name and fingerprint", { finds_relocated_media(); });
    });
}
