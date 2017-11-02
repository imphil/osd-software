/* Copyright (c) 2017 by the author(s)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ============================================================================
 *
 * Author(s):
 *   Philipp Wagner <philipp.wagner@tum.de>
 */

#include <check.h>

#include <osd/osd.h>
#include <osd/packet.h>

START_TEST(test_packet_header_extractparts)
{
    osd_result rv;
    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, osd_packet_get_data_size_words_from_payload(0));
    ck_assert_int_eq(rv, OSD_OK);

    pkg->data.dest = 0xa5ab;
    pkg->data.src = 0x1234;
    pkg->data.flags = 0x5557;
    ck_assert_int_eq(osd_packet_get_dest(pkg), 0xa5ab);
    ck_assert_int_eq(osd_packet_get_src(pkg), 0x1234);
    ck_assert_int_eq(osd_packet_get_type(pkg), 0x1);
    ck_assert_int_eq(osd_packet_get_type_sub(pkg), 0x5);

    osd_packet_free(&pkg);
    ck_assert_ptr_eq(pkg, NULL);
}
END_TEST

START_TEST(test_packet_header_set)
{
    osd_result rv;
    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, osd_packet_get_data_size_words_from_payload(0));
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg, 0x1ab, 0x157, OSD_PACKET_TYPE_PLAIN, 0x5);

    ck_assert_int_eq(pkg->data.dest, 0x1ab);
    ck_assert_int_eq(pkg->data.src, 0x157);
    ck_assert_int_eq(pkg->data.flags, 0x5400);

    osd_packet_free(&pkg);
    ck_assert_ptr_eq(pkg, NULL);
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

    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
