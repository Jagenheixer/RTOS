#include <gtest/gtest.h>
#include "TimeParser.h"

// --- HYVÄKSYTYT SYÖTÖT ---
TEST(TimeParserTest, ValidTimeInputs) {
    char t1[] = "000120"; // 1m 20s = 80s
    EXPECT_EQ(time_parse(t1), 80);

    char t2[] = "000005"; // 5s
    EXPECT_EQ(time_parse(t2), 5);

    char t3[] = "235959"; // 59m 59s = 3599s
    EXPECT_EQ(time_parse(t3), 3599);
}

// --- BOUNDARY VALUE -TESTIT ---
TEST(TimeParserTest, BoundaryHours) {
    char t1[] = "240000"; // Tunnit yli 23
    EXPECT_EQ(time_parse(t1), TIME_VALUE_ERROR);
}

TEST(TimeParserTest, BoundaryMinutes) {
    char t1[] = "006000"; // Minuutit yli 59
    EXPECT_EQ(time_parse(t1), TIME_VALUE_ERROR);
}

TEST(TimeParserTest, BoundarySeconds) {
    char t1[] = "000060"; // Sekunnit yli 59
    EXPECT_EQ(time_parse(t1), TIME_VALUE_ERROR);
}

// --- LISÄTESTIKEISSIT ---
TEST(TimeParserTest, NullPointerCheck) {
    EXPECT_EQ(time_parse(NULL), TIME_NULL_ERROR);
}

TEST(TimeParserTest, InvalidLength) {
    char t1[] = "12345";   // Liian lyhyt
    char t2[] = "1234567"; // Liian pitkä
    EXPECT_EQ(time_parse(t1), TIME_LEN_ERROR);
    EXPECT_EQ(time_parse(t2), TIME_LEN_ERROR);
}

TEST(TimeParserTest, NonDigitCharacters) {
    char t1[] = "00A120";
    char t2[] = "12:34:";
    EXPECT_EQ(time_parse(t1), TIME_DIGIT_ERROR);
    EXPECT_EQ(time_parse(t2), TIME_DIGIT_ERROR);
}

TEST(TimeParserTest, ZeroTimeNotAllowed) {
    char t1[] = "000000";
    EXPECT_EQ(time_parse(t1), TIME_ZERO_ERROR);
}