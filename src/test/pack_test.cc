#include <gtest/gtest.h>

#include "../pack.h"

TEST(pack_test, handmade_1) {
  Pack pack(9, 5, 0, 3);
  ASSERT_TRUE(pack.GetRotated(0) == Pack(9, 5, 0, 3));
}

TEST(pack_test, handmade_2) {
  Pack pack(9, 5, 0, 3);
  ASSERT_TRUE(pack.GetRotated(1) == Pack(0, 9, 3, 5));
}

TEST(pack_test, handmade_3) {
  Pack pack(9, 5, 0, 3);
  ASSERT_TRUE(pack.GetRotated(2) == Pack(3, 0, 5, 9));
}

TEST(pack_test, handmade_4) {
  Pack pack(9, 5, 0, 3);
  ASSERT_TRUE(pack.GetRotated(3) == Pack(5, 3, 9, 0));
}

TEST(pack_test, handmade_5) {
  Pack::Init();

  Pack pack(9, 1, 0, 5);
  ASSERT_TRUE(pack.IsFlammable());
}

TEST(pack_test, handmade_6) {
  Pack::Init();

  Pack pack(0, 0, 0, 0);
  ASSERT_TRUE(!pack.IsFlammable());
}

TEST(pack_test, handmade_7) {
  Pack::Init();

  Pack pack(5, 5, 5, 5);
  ASSERT_TRUE(pack.IsFlammable());
}

TEST(pack_test, handmade_8) {
  Pack::Init();

  Pack pack(9, 8, 7, 6);
  ASSERT_TRUE(!pack.IsFlammable());
}

TEST(pack_test, handmade_9) {
  Pack::Init();

  Pack pack(1, 9, 2, 8);
  ASSERT_TRUE(pack.IsFlammable());
}

TEST(pack_test, handmade_10) {
  Pack::Init();

  Pack pack(1, 9, 2, 8);
  ASSERT_TRUE(pack.Count(1) == 1);
  ASSERT_TRUE(pack.Count(2) == 1);
  ASSERT_TRUE(pack.Count(3) == 0);
  ASSERT_TRUE(pack.Count(4) == 0);
  ASSERT_TRUE(pack.Count(5) == 0);
  ASSERT_TRUE(pack.Count(6) == 0);
  ASSERT_TRUE(pack.Count(7) == 0);
  ASSERT_TRUE(pack.Count(8) == 1);
  ASSERT_TRUE(pack.Count(9) == 1);
}

TEST(pack_test, handmade_11) {
  Pack::Init();

  Pack pack(1, 1, 1, 1);
  ASSERT_TRUE(pack.Count(1) == 4);
  ASSERT_TRUE(pack.Count(2) == 0);
  ASSERT_TRUE(pack.Count(3) == 0);
  ASSERT_TRUE(pack.Count(4) == 0);
  ASSERT_TRUE(pack.Count(5) == 0);
  ASSERT_TRUE(pack.Count(6) == 0);
  ASSERT_TRUE(pack.Count(7) == 0);
  ASSERT_TRUE(pack.Count(8) == 0);
  ASSERT_TRUE(pack.Count(9) == 0);
}
