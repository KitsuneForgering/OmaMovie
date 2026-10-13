#include "oma_test.hpp"

void run_line_diff_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_line_diff_tests();
    return cest_result();
}
