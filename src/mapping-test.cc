#include "mapping.hh"
#include "gtest/gtest.h"

TEST(mapper_test, test_real_data) {
  fs::Mapper mapper;
  mapper.open("../test-data/map-file.txt");

  ASSERT_EQ(mapper.getItem(0), "string_0");
  ASSERT_EQ(mapper.getItem(35), "string_35");
  ASSERT_EQ(mapper.getItem(-1), "");
  ASSERT_EQ(mapper.getNumRows(), 100);
}
