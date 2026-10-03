#include "oma/base/log.hpp"

#include "oma_test.hpp"

void run_timeline_audio_tests();

int main(int argc, char* argv[]) {
    oma::set_log_level(oma::Category::Media, oma::LogLevel::Off);
    cest_init(argc, argv);
    run_timeline_audio_tests();
    return cest_result();
}
