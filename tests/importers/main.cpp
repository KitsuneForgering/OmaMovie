#include "oma_test.hpp"

void run_otio_tests();
void run_otio_refusal_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_otio_tests();
    run_otio_refusal_tests();
    return cest_result();
}
