#include "oma_test.hpp"

void run_rational_tests();
void run_time_tests();
void run_error_tests();
void run_log_tests();
void run_job_tests();
void run_bounded_queue_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_rational_tests();
    run_time_tests();
    run_error_tests();
    run_log_tests();
    run_job_tests();
    run_bounded_queue_tests();
    return cest_result();
}
