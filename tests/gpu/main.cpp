#include "oma_test.hpp"

void run_device_tests();
void run_resource_tests();
void release_gpu_test_device();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_device_tests();
    run_resource_tests();
    release_gpu_test_device();
    return cest_result();
}
