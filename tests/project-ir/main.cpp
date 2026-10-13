#include "oma_test.hpp"

void run_validate_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_validate_tests();
    return cest_result();
}
