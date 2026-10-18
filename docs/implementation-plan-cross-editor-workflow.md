### Cross-editor workflow gap review (2026-10-11)

This comparison is about completing an edit, not matching feature counts. Official product
guides establish that the workflows exist elsewhere; they do not establish demand, speed or
quality on OmaMovie's target machine. The code and milestones below determine OmaMovie's
current state. No competitor task was run in this review.

| User task | Comparator evidence | OmaMovie now | Plan consequence |
|---|---|---|---|
| Speed up a talking clip and keep its speech | [Final Cut Pro pitch preservation](https://support.apple.com/guide/final-cut-pro/preserve-pitch-in-retimed-clips-verb6aca45d/mac), [Premiere Speed/Duration](https://helpx.adobe.com/sg/premiere/desktop/edit-projects/change-clip-speed/change-clip-speed-using-the-speedduration-option.html) | `TimelineAudio::mix_clip` returns silence outside constant 1×; Speed is a video-only menu | M8 retimed-audio correctness before presenting speed as a complete audiovisual edit |
| Move or hand off a project with its source media | [Final Cut Pro consolidation](https://support.apple.com/en-au/guide/final-cut-pro/ver9c7660349/mac), [Premiere project copy](https://helpx.adobe.com/premiere/desktop/organize-media/create-projects/copy-project.html) | Native project saves references and can relink missing files, but versions/autosaves do not copy sources | M7 portable-project pilot; keep recovery and media backup distinct |
| Deliver dialogue and music separately | [Final Cut Pro roles and separate files](https://support.apple.com/en-ca/guide/final-cut-pro/verc9c84cdbc/mac), [Resolve Fairlight stems](https://documents.blackmagicdesign.com/UserManuals/DaVinci-Resolve-18-Fairlight-Audio-Post.pdf) | Tracks and one 48 kHz stereo AAC program mix; no named audio groups or stem export | M9 conditional role/stem pilot after mix correctness |
| Choose a destination and still control the encoding | [Kdenlive render profiles](https://docs.kdenlive.org/en/exporting/render.html), [Premiere color/output setup](https://helpx.adobe.com/premiere/desktop/correct-color/set-up-color-management/about-color-management.html) | Four explicit H.264/AAC, HEVC/AAC and VP9/Vorbis container profiles exist; resolution, rate and quality remain fixed | M7 named output-intent pilot; M9 color management remains a separate gate |

Already planned, so not added again: proxies, HDR/color management, multicam/takes,
captions, relink, nesting, queueing, loudness analysis and long-form performance. The
portable-project task extends existing relink; it is not a second relink implementation.

Algorithmic check (code-derived, not measured here): for each audio block,
`TimelineAudio::render` visits every track, binary-searches its ordered clips and mixes
the overlapping ones. With tracks `T`, clips `C_t` and active clips `A_t` on track `t`,
clip discovery is O(sum over tracks of log C_t + A_t); decoded samples and effects add
work proportional to active audio. `ClipGain::at` also binary-searches `K` volume keys
for each mixed sample, O(log K) per sample. The existing interval search is sound for
long timelines. A stem implementation should measure real block deadlines and decoder
reuse before adding parallel mixes or a routing cache. The already planned trim-preview
and recordings-directory gates remain the more direct UI scaling checks.

[← Implementation plan](implementation-plan.md)
