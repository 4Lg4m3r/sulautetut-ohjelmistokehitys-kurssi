
#include <gtest/gtest.h>
#include "../TimeParser.h"

// Test suite: TimeParserTest

// 1. Correct time
TEST(TimeParserTest, TestCaseCorrectTime) {
    char time1[] = "000005";
    EXPECT_EQ(time_parse(time1), 5);

    char time2[] = "000015";
    EXPECT_EQ(time_parse(time2), 15);

    char time3[] = "010000";
    EXPECT_EQ(time_parse(time3), 3600);

    char time4[] = "123456";
    EXPECT_EQ(time_parse(time4), 45296);
}

// 2. Incorrect time values
TEST(TimeParserTest, TestCaseIncorrectTime) {
    char time1[] = "000067";
    EXPECT_EQ(time_parse(time1), TIME_VALUE_ERROR);

    char time2[] = "006700";
    EXPECT_EQ(time_parse(time2), TIME_VALUE_ERROR);

    char time3[] = "250000";
    EXPECT_EQ(time_parse(time3), TIME_VALUE_ERROR);
}

// 3. Negative and non-numeric values
TEST(TimeParserTest, TestCaseNegativeTime) {
    char time1[] = "-00067";
    EXPECT_EQ(time_parse(time1), TIME_VALUE_ERROR);

    char time2[] = "0-6700";
    EXPECT_EQ(time_parse(time2), TIME_VALUE_ERROR);

    char time3[] = "00A005";
    EXPECT_EQ(time_parse(time3), TIME_VALUE_ERROR);
}

// 4. Null input
TEST(TimeParserTest, TestCaseNullTime) {
    EXPECT_EQ(time_parse(nullptr), TIME_ARRAY_ERROR);
}

// 5. Zero time
TEST(TimeParserTest, TestCaseZeroTime) {
    char time[] = "000000";
    EXPECT_EQ(time_parse(time), TIME_VALUE_ERROR);
}

// 6. Incorrect string length
TEST(TimeParserTest, TestCaseIncorrectLength) {
    char time1[] = "12345";
    EXPECT_EQ(time_parse(time1), TIME_LEN_ERROR);

    char time2[] = "1234567";
    EXPECT_EQ(time_parse(time2), TIME_LEN_ERROR);

    char time3[] = "";
    EXPECT_EQ(time_parse(time3), TIME_LEN_ERROR);
}

// 7. Boundary values
TEST(TimeParserTest, TestCaseBoundaryValues) {
    char time1[] = "235959";
    EXPECT_EQ(time_parse(time1), 86399);

    char time2[] = "000001";
    EXPECT_EQ(time_parse(time2), 1);

    char time3[] = "005959";
    EXPECT_EQ(time_parse(time3), 359);
}
