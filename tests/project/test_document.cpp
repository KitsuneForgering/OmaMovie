#include "oma/project/document.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"

#include <unistd.h>

#include <cstdio>
#include <filesystem>
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
    source.video.filter = {.kind = tl::FilterKind::Sepia, .amount = 0.6};
    source.video.sharpness = -0.4;
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
    expect(t.tracks().size()).toBe(2U);
    expect(t.tracks()[0].clips.size()).toBe(2U);
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
    });
}
