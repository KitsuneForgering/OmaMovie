#include "oma_test.hpp"

void run_otio_tests();
void run_otio_refusal_tests();
void run_otio_path_tests();
void run_otio_scale_tests();
void run_otio_dropped_tests();
void run_edl_tests();
void run_edl_refusal_tests();
void run_xml_tests();
void run_xml_bounds_tests();
void run_mlt_tests();
void run_mlt_timewarp_tests();
void run_mlt_timewarp_report_tests();
void run_fcpxml_tests();
void run_fcpxml_refusal_tests();

int main(int argc, char* argv[]) {
    cest_init(argc, argv);
    run_otio_tests();
    run_otio_refusal_tests();
    run_otio_path_tests();
    run_otio_scale_tests();
    run_otio_dropped_tests();
    run_edl_tests();
    run_edl_refusal_tests();
    run_xml_tests();
    run_xml_bounds_tests();
    run_mlt_tests();
    run_mlt_timewarp_tests();
    run_mlt_timewarp_report_tests();
    run_fcpxml_tests();
    run_fcpxml_refusal_tests();
    return cest_result();
}
