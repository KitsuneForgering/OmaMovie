#include "oma/media/probe.hpp"

#include "oma_test.hpp"

void run_probe_regression_tests() {
    describe("media probe regressions", {
        // Found by fuzz_probe (2026-10-13): a crafted Matroska declares -2 ms for the file and
        // every stream. The probe reports no length rather than a negative one.
        it("treats a negative declared duration as unknown", {
            auto info = oma::media::probe("tests/fuzz/corpus/probe/negative-duration.mkv");
            if (!info)
                return; // refusing the file is also safe
            expect(!info->duration || info->duration->value() >= 0).toBeTruthy();
            bool negative = false;
            for (const auto& s : info->streams)
                negative = negative || (s.duration && s.duration->value() < 0);
            expect(negative).toBeFalsy();
        });
    });
}
