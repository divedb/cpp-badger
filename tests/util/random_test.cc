#include "cpp-badger/util/random.hh"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

using badger::Random;

TEST(RandomTest, Uniform) {
  const int average = 20;
  for (uint32_t seed : {0, 1, 2, 37, 4096}) {
    Random r(seed);

    for (int range : {1, 2, 8, 12, 100}) {
      std::vector<int> counts(range, 0);

      for (int i = 0; i < range * average; ++i) {
        ++counts.at(r.Uniform(range));
      }

      int max_variance = static_cast<int>(std::sqrt(range) * 2 + 4);

      for (int i = 0; i < range; ++i) {
        EXPECT_GE(counts[i], std::max(1, average - max_variance));
        EXPECT_LE(counts[i], average + max_variance + 1);
      }
    }
  }
}

TEST(RandomTest, OneIn) {
  Random r(42);

  for (int range : {1, 2, 8, 12, 100, 1234}) {
    const int average = 100;
    int count = 0;

    for (int i = 0; i < average * range; ++i) {
      if (r.OneIn(range)) {
        ++count;
      }
    }
    if (range == 1) {
      EXPECT_EQ(count, average);
    } else {
      int max_variance = static_cast<int>(std::sqrt(average) * 1.5);

      EXPECT_GE(count, average - max_variance);
      EXPECT_LE(count, average + max_variance);
    }
  }
}

TEST(RandomTest, OneInOpt) {
  Random r(42);

  for (int range : {-12, 0, 1, 2, 8, 12, 100, 1234}) {
    const int average = 100;
    int count = 0;
    for (int i = 0; i < average * range; ++i) {
      if (r.OneInOpt(range)) {
        ++count;
      }
    }
    if (range < 1) {
      EXPECT_EQ(count, 0);
    } else if (range == 1) {
      EXPECT_EQ(count, average);
    } else {
      int max_variance = static_cast<int>(std::sqrt(average) * 1.5);
      EXPECT_GE(count, average - max_variance);
      EXPECT_LE(count, average + max_variance);
    }
  }
}

TEST(RandomTest, PercentTrue) {
  Random r(42);
  for (int pct : {-12, 0, 1, 2, 10, 50, 90, 98, 99, 100, 1234}) {
    const int samples = 10000;

    int count = 0;
    for (int i = 0; i < samples; ++i) {
      if (r.PercentTrue(pct)) {
        ++count;
      }
    }
    if (pct <= 0) {
      EXPECT_EQ(count, 0);
    } else if (pct >= 100) {
      EXPECT_EQ(count, samples);
    } else {
      int est = (count * 100 + (samples / 2)) / samples;
      EXPECT_EQ(est, pct);
    }
  }
}
