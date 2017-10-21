#include <check.h>

#include <osd/osd.h>

START_TEST(test_util)
{
    const struct osd_version *v = osd_version_get();

    ck_assert_int_eq(v->major, OSD_VERSION_MAJOR);
    ck_assert_int_eq(v->minor, OSD_VERSION_MINOR);
    ck_assert_int_eq(v->micro, OSD_VERSION_MICRO);
    ck_assert_str_eq(v->suffix, OSD_VERSION_SUFFIX);
}
END_TEST

Suite * suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("libosd-util");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_util);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
