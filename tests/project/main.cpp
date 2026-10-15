#include "oma_test.hpp"

void run_document_tests();
void run_subtitle_tests();
void run_from_ir_tests();
void run_media_color_format_tests();
void run_edit_round_trip_tests();
void run_save_mode_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_document_tests();
    run_subtitle_tests();
    run_from_ir_tests();
    run_media_color_format_tests();
    run_edit_round_trip_tests();
    run_save_mode_tests();
    return cest_result();
}
