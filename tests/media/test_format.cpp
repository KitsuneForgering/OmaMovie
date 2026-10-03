#include "oma/media/format.hpp"

#include "media_test.hpp"

#include <string>

#include "oma_test.hpp"

void run_format_tests() {
    describe("media format boundary", {
        oma::media::FfmpegFormatBackend ffmpeg;
        const oma::media::MediaImporter& importer = ffmpeg;
        const oma::media::ExportFormatChecker& output = ffmpeg;

        it("uses the existing probe DTO for imported media", {
            const auto result = importer.inspect_input(fixture("h264_30fps_aac.mp4"));
            expect(result.has_value()).toBeTruthy();
            if (result) {
                expect(result->best_video.has_value()).toBeTruthy();
                expect(result->best_audio.has_value()).toBeTruthy();
            }
        });

        it("accepts a muxer and installed video encoder that fit together", {
            oma::media::ExportFormat format;
            format.container = "mp4";
            format.video_encoder = "mpeg4";
            expect(output.check_output(format).has_value()).toBeTruthy();
        });

        it("rejects missing and incompatible output formats", {
            oma::media::ExportFormat missing;
            missing.container = "not-a-muxer";
            missing.video_encoder = "mpeg4";
            auto wrong_kind = missing;
            wrong_kind.container = "mp4";
            wrong_kind.video_encoder = "aac";
            auto incompatible = wrong_kind;
            incompatible.container = "webm";
            incompatible.video_encoder = "mpeg4";
            expect(code_of(output.check_output(missing).error()))
                .toEqual(static_cast<int>(oma::ErrorCode::Unsupported));
            expect(code_of(output.check_output(wrong_kind).error()))
                .toEqual(static_cast<int>(oma::ErrorCode::Unsupported));
            expect(code_of(output.check_output(incompatible).error()))
                .toEqual(static_cast<int>(oma::ErrorCode::Unsupported));
        });
    });
}
