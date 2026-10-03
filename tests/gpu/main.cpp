#include "oma_test.hpp"

void run_device_tests();
void release_gpu_test_device();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_device_tests();
    release_gpu_test_device();
    return cest_result();
}
