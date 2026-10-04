#include "oma_test.hpp"

void run_model_tests();
void run_edit_tests();
void run_evaluate_tests();
void run_transition_tests();
void run_keyframe_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_model_tests();
    run_edit_tests();
    run_evaluate_tests();
    run_transition_tests();
    run_keyframe_tests();
    return cest_result();
}
