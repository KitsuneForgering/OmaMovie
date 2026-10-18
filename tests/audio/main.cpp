#include "oma_test.hpp"

void run_ring_tests();
void run_clock_tests();
void run_output_tests();
void run_mix_tests();
void run_equalizer_tests();
void run_effect_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_ring_tests();
    run_clock_tests();
    run_output_tests();
    run_mix_tests();
    run_equalizer_tests();
    run_effect_tests();
    return cest_result();
}
