#include "oma_test.hpp"

void run_model_tests();
void run_edit_tests();
void run_evaluate_tests();
void run_transition_tests();
void run_keyframe_tests();
void run_time_map_tests();
void run_anchor_tests();
void run_title_tests();
void run_effect_tests();
void run_caption_tests();
void run_canvas_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_model_tests();
    run_edit_tests();
    run_evaluate_tests();
    run_transition_tests();
    run_keyframe_tests();
    run_time_map_tests();
    run_anchor_tests();
    run_title_tests();
    run_effect_tests();
    run_caption_tests();
    run_canvas_tests();
    return cest_result();
}
