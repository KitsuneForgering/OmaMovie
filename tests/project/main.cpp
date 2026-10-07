#include "oma_test.hpp"

void run_document_tests();
void run_subtitle_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_document_tests();
    run_subtitle_tests();
    return cest_result();
}
