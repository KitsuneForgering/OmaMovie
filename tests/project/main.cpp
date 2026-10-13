#include "oma_test.hpp"

void run_document_tests();
void run_subtitle_tests();
void run_from_ir_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_document_tests();
    run_subtitle_tests();
    run_from_ir_tests();
    return cest_result();
}
