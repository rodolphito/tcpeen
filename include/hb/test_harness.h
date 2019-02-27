#ifndef HB_TEST_HARNESS_H
#define HB_TEST_HARNESS_H

#include "aws/testing/aws_test_harness.h"
#include "aws/common/common.h"

#define HB_TEST_HARNESS
#define HB_TEST_CASE AWS_TEST_CASE
#define HB_TEST_CASE_PARAMS struct aws_allocator *allocator, void *ctx

#define HB_TEST_CASE_BEGIN(fn) static int fn (HB_TEST_CASE_PARAMS) \
{ \
    (void)ctx; \
    (void)allocator; \

#define HB_TEST_CASE_END }

#include "hb/error.h"



#endif