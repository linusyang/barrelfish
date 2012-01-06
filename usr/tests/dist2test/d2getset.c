/**
 * \file
 * \brief Tests for dist2 get/set/del API
 */

/*
 * Copyright (c) 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <barrelfish/barrelfish.h>
#include <skb/skb.h>
#include <dist2/dist2.h>

#include "common.h"

#define STR(a) #a
#define R(var, re)  static char var##_[] = STR(re);\
const char * var = ( var##_[ sizeof(var##_) - 2] = '\0',  (var##_ + 1) );

static void get_names(void)
{
    errval_t err = SYS_ERR_OK;

    char** names = NULL;
    size_t size = 0;

    err = dist_get_names(&names, &size, "_ { weight: _ }");
    ASSERT_ERR_OK(err);
    assert(size == 3);
    ASSERT_STRING(names[0], "object2");
    ASSERT_STRING(names[1], "object3");
    ASSERT_STRING(names[2], "object4");
    dist_free_names(names, size);

    err = dist_get_names(&names, &size, "_ { attr: _, weight: %d }", 20);
    ASSERT_ERR_OK(err);
    assert(size == 1);
    ASSERT_STRING(names[0], "object4");
    dist_free_names(names, size);

    names = NULL;
    size = 0;
    err = dist_get_names(&names, &size, "asdfasd", 20);
    assert(names == NULL);
    assert(size == 0);
    ASSERT_ERR(err, DIST2_ERR_NO_RECORD);

    err = dist_get_names(&names, &size, "}+_df}");
    assert(names == NULL);
    assert(size == 0);
    ASSERT_ERR(err, DIST2_ERR_PARSER_FAIL);

    printf("get_names() done!\n");
}

static void get_records(void)
{
    errval_t err = SYS_ERR_OK;
    char* data = NULL;

    err = dist_get(&data, "recordDoesNotExist");
    ASSERT_ERR(err, DIST2_ERR_NO_RECORD);
    assert(data == NULL);

    err = dist_get(&data, "parser { error, m, }");
    ASSERT_ERR(err, DIST2_ERR_PARSER_FAIL);
    assert(data == NULL);


    err = dist_get(&data, "object1");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object1 { weight: 20 }");
    free(data);

    err = dist_get(&data, "object2 { weight: 20 }");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object2 { weight: 20 }");
    free(data);

    err = dist_get(&data, "object4");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object4 { attr: 'Somestring', weight: 20, fl: 12.0 }");
    free(data);

    err = dist_get(&data, "_ { weight: >= 10, fl: > 11.0 }");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object4 { attr: 'Somestring', weight: 20, fl: 12.0 }");
    free(data);

    err = dist_get(&data, "_ { weight: >= 10, fl: > 11.0 }");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object4 { attr: 'Somestring', weight: 20, fl: 12.0 }");
    free(data);

    err = dist_del("object4");
    ASSERT_ERR_OK(err);

    err = dist_get(&data, "object4");
    ASSERT_ERR(err, DIST2_ERR_NO_RECORD);
    //free(data); TODO??

    err = dist_set("object4 { attr: 'Somestring', weight: 20, fl: 12.0 }");
    ASSERT_ERR_OK(err);

    err = dist_get(&data, "object4");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object4 { attr: 'Somestring', weight: 20, fl: 12.0 }");
    free(data);

    err = dist_del("object1");
    ASSERT_ERR_OK(err);

    err = dist_get(&data, "object1");
    printf("data: %s\n", data);
    ASSERT_ERR(err, DIST2_ERR_NO_RECORD);
    // TODO free(data);?

    err = dist_get(&data, "object2 { weight: 20 }");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data, "object2 { weight: 20 }");
    free(data);

    err = dist_get(&data, "_ { pattern1: r'^12.*ab$' }");
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data,
            "object5 { pattern1: '123abab', pattern2: " \
            "'StringToTestRegexMatching', pattern3: '10-10-2010' }");
    printf("data: %s\n", data);
    free(data);

    // Test long regex
    R(re, "_ { pattern3: r'^(((((((0?[13578])|(1[02]))[\.\-/]?((0?[1-9])|([12]\d)|(3[01])))|(((0?[469])|(11))[\.\-/]?((0?[1-9])|([12]\d)|(30)))|((0?2)[\.\-/]?((0?[1-9])|(1\d)|(2[0-8]))))[\.\-/]?(((19)|(20))?([\d][\d]))))|((0?2)[\.\-/]?(29)[\.\-/]?(((19)|(20))?(([02468][048])|([13579][26])))))$' }");
    err = dist_get(&data, re);
    ASSERT_ERR_OK(err);
    ASSERT_STRING(data,
            "object5 { pattern1: '123abab', pattern2: " \
            "'StringToTestRegexMatching', pattern3: '10-10-2010' }");
    printf("data: %s\n", data);
    free(data);


    // TODO implement dist_del with constraints, attributes!

    printf("get_records() done!\n");
}

static void exist_records(void)
{
    errval_t err = dist_exists("././12");
    ASSERT_ERR(err, DIST2_ERR_PARSER_FAIL);

    err = dist_exists("recordDoesNotExist");
    ASSERT_ERR(err, DIST2_ERR_NO_RECORD);

    err = dist_exists("object3 { fl: > 10, weight: _ }");
    ASSERT_ERR_OK(err);
}

static void set_records(void)
{
    errval_t err = dist_set("q{weqw1,.");
    ASSERT_ERR(err, DIST2_ERR_PARSER_FAIL);

    err = dist_set("object1 { weight: %d }", 20);
    ASSERT_ERR_OK(err);

    // TODO: Do we want this?
    err = dist_set("object2 { weight: %lu, weight: %lu }", 20, 25);
    ASSERT_ERR_OK(err);

    char* str = "A text string.";
    err = dist_set("object3 { attr: '%s', weight: 9, fl: 12.0 }",
            str);
    ASSERT_ERR_OK(err);

    err = dist_set(
            "object4 { attr: 'Somestring', weight: 20, fl: %f }", 12.0);
    ASSERT_ERR_OK(err);

    char* pattern1 = "123abab";
    char* pattern2 = "StringToTestRegexMatching";
    char* pattern3 = "10-10-2010";
    err = dist_set(
            "object5 { pattern1: '%s', pattern2: '%s', pattern3: '%s' }",
            pattern1, pattern2, pattern3);
    ASSERT_ERR_OK(err);

    pattern1 = "123ababc";
    pattern3 = "21-00-2900";
    err = dist_set(
            "object6 { pattern1: '%s', pattern2: '%s', pattern3: '%s' }",
            pattern1, pattern2, pattern3);
    ASSERT_ERR_OK(err);


    printf("set_records() done!\n");
}

int main(int argc, char *argv[])
{
    dist_init();

    // Run Tests
    set_records();
    exist_records();
    get_records();
    get_names();

    printf("d2getset SUCCESS!\n");
    return EXIT_SUCCESS;
}
