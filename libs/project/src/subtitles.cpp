#include "oma/project/subtitles.hpp"

#include <algorithm>
#include <format>
#include <optional>

namespace oma::project {

namespace {

Error invalid(std::string message, std::size_t line) {
    return {ErrorCode::InvalidData, Category::Project, std::move(message),
            std::format("line {}", line)};
}

// The next line (without '\r'); advances `at`.
std::string_view next_line(std::string_view text, std::size_t& at) {
    const std::size_t end = text.find('\n', at);
    std::string_view line =
        text.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
    at = end == std::string_view::npos ? text.size() : end + 1;
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    return line;
}

std::string_view trim(std::string_view s) {
    // '\r' too: a line of stray carriage returns is blank, not a caption line.
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r';
    };
    while (!s.empty() && space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && space(s.back()))
        s.remove_suffix(1);
    return s;
}

// A number of exactly `digits` digits (or at least 1 and at most `digits` when `loose`).
std::optional<std::int64_t> number(std::string_view& s, std::size_t digits, bool loose = false) {
    std::size_t n = 0;
    while (n < s.size() && n < (loose ? 9 : digits) && s[n] >= '0' && s[n] <= '9')
        ++n;
    if (n == 0 || (!loose && n != digits))
        return std::nullopt;
    std::int64_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
        v = (v * 10) + (s[i] - '0'); // at most 9 digits: no overflow
    s.remove_prefix(n);
    return v;
}

// [HH:]MM:SS[,.]mmm (hours: any number of digits up to 9).
std::optional<std::int64_t> timestamp(std::string_view& s) {
    std::string_view probe = s;
    auto first = number(probe, 9, true);
    if (!first || probe.empty() || probe.front() != ':')
        return std::nullopt;
    probe.remove_prefix(1);
    auto second = number(probe, 2);
    if (!second)
        return std::nullopt;
    std::int64_t hours = 0;
    std::int64_t minutes = *first;
    std::int64_t seconds = 0;
    if (!probe.empty() && probe.front() == ':') { // HH:MM:SS
        probe.remove_prefix(1);
        auto third = number(probe, 2);
        if (!third)
            return std::nullopt;
        hours = *first;
        minutes = *second;
        seconds = *third;
    } else {
        seconds = *second;
    }
    if (probe.empty() || (probe.front() != ',' && probe.front() != '.'))
        return std::nullopt;
    probe.remove_prefix(1);
    auto ms = number(probe, 3);
    if (!ms || minutes > 59 || seconds > 59)
        return std::nullopt;
    s = probe;
    return (((hours * 60) + minutes) * 60 + seconds) * 1000 + *ms;
}

// "start --> end[ settings]".
std::optional<std::pair<std::int64_t, std::int64_t>> timing(std::string_view line) {
    line = trim(line);
    auto start = timestamp(line);
    line = trim(line);
    if (!start || !line.starts_with("-->"))
        return std::nullopt;
    line.remove_prefix(3);
    line = trim(line);
    auto end = timestamp(line);
    if (!end || (!line.empty() && line.front() != ' ' && line.front() != '\t'))
        return std::nullopt;
    return std::pair(*start, *end);
}

std::string format_time(std::int64_t ms, char separator) {
    return std::format("{:02}:{:02}:{:02}{}{:03}", ms / 3600000, (ms / 60000) % 60,
                       (ms / 1000) % 60, separator, ms % 1000);
}

} // namespace

Result<std::vector<Cue>> parse_subtitles(std::string_view text) {
    if (text.size() > kMaxSubtitleBytes) {
        return std::unexpected(invalid("caption file larger than 16 MiB", 1));
    }
    if (text.starts_with("\xEF\xBB\xBF"))
        text.remove_prefix(3);
    std::size_t at = 0;
    std::size_t line_no = 0;
    const bool vtt = text.starts_with("WEBVTT");
    if (vtt) { // the header block ends at the first blank line
        while (at < text.size()) {
            ++line_no;
            if (trim(next_line(text, at)).empty())
                break;
        }
    }
    std::vector<Cue> cues;
    while (at < text.size()) {
        // A block: blank lines, then lines up to the next blank line.
        ++line_no;
        std::string_view line = next_line(text, at);
        if (trim(line).empty())
            continue;
        const std::size_t block_line = line_no;
        if (vtt &&
            (line.starts_with("NOTE") || line.starts_with("STYLE") || line.starts_with("REGION"))) {
            while (at < text.size()) {
                ++line_no;
                if (trim(next_line(text, at)).empty())
                    break;
            }
            continue;
        }
        if (line.find("-->") == std::string_view::npos) { // an SRT index or a VTT identifier
            if (at >= text.size())
                break;
            ++line_no;
            line = next_line(text, at);
        }
        const auto times = timing(line);
        if (!times) {
            return std::unexpected(
                invalid("expected a timing line like 00:00:01,000 --> 00:00:02,000", line_no));
        }
        if (times->second <= times->first) {
            return std::unexpected(invalid("a caption must end after it starts", line_no));
        }
        Cue cue{.start_ms = times->first, .end_ms = times->second, .text = {}};
        while (at < text.size()) {
            const std::size_t mark = at;
            ++line_no;
            const std::string_view body = next_line(text, at);
            if (trim(body).empty())
                break;
            if (body.find("-->") != std::string_view::npos &&
                timing(body)) { // no blank line between cues
                at = mark;
                --line_no;
                break;
            }
            if (cue.text.size() + body.size() + 1 > kMaxCueBytes) {
                return std::unexpected(invalid("caption text longer than 1000 bytes", block_line));
            }
            if (!cue.text.empty())
                cue.text += '\n';
            cue.text += body;
        }
        if (cues.size() == kMaxCues) {
            return std::unexpected(invalid("more than 10,000 captions", block_line));
        }
        cues.push_back(std::move(cue));
    }
    std::ranges::stable_sort(cues, {}, &Cue::start_ms);
    for (std::size_t i = 0; i + 1 < cues.size(); ++i) {
        cues[i].end_ms = std::min(cues[i].end_ms, cues[i + 1].start_ms);
    }
    std::erase_if(cues, [](const Cue& c) { return c.end_ms <= c.start_ms; }); // two at one start
    return cues;
}

std::string write_srt(std::span<const Cue> cues) {
    std::string out;
    std::size_t index = 0;
    for (const Cue& c : cues) {
        out += std::format("{}\n{} --> {}\n{}\n\n", ++index, format_time(c.start_ms, ','),
                           format_time(c.end_ms, ','), c.text);
    }
    return out;
}

std::string write_vtt(std::span<const Cue> cues) {
    std::string out = "WEBVTT\n\n";
    for (const Cue& c : cues) {
        out += std::format("{} --> {}\n{}\n\n", format_time(c.start_ms, '.'),
                           format_time(c.end_ms, '.'), c.text);
    }
    return out;
}

} // namespace oma::project
