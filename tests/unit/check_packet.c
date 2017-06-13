#include <check.h>

#include <osd/osd.h>
#include "../../src/libosd/packet.h"

START_TEST(test_packet_header_extractparts)
{
    osd_result rv;
    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, 0);
    ck_assert_int_eq(rv, OSD_OK);

    pkg->data.dp_header_1 = 0xa5ab;
    pkg->data.dp_header_2 = 0x1234;
    pkg->data.dp_header_3 = 0x5557;
    ck_assert_int_eq(osd_packet_get_dest(pkg), 0xa5ab);
    ck_assert_int_eq(osd_packet_get_src(pkg), 0x1234);
    ck_assert_int_eq(osd_packet_get_type(pkg), 0x1);
    ck_assert_int_eq(osd_packet_get_type_sub(pkg), 0x5);

    osd_packet_free(pkg);
}
END_TEST

START_TEST(test_packet_header_set)
{
    osd_result rv;
    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, 0);
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg, 0x1ab, 0x157, OSD_PACKET_TYPE_PLAIN, 0x5);

    ck_assert_int_eq(pkg->data.dp_header_1, 0x1ab);
    ck_assert_int_eq(pkg->data.dp_header_2, 0x157);
    ck_assert_int_eq(pkg->data.dp_header_3, 0x5400);

    osd_packet_free(pkg);
}
END_TEST

Suite * packet_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("libosd-packet");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_packet_header_set);
    tcase_add_test(tc_core, test_packet_header_extractparts);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = packet_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
