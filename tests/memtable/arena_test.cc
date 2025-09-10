#include "cpp-badger/memtable/arena.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace badger;

class ArenaTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(ArenaTest, BasicAllocation) {
  Arena arena(1024);
  void* ptr1 = arena.Allocate(100);

  EXPECT_NE(ptr1, nullptr);

  void* ptr2 = arena.Allocate(200);

  EXPECT_NE(ptr2, nullptr);
  EXPECT_NE(ptr1, ptr2);
}

TEST_F(ArenaTest, TypeSafeAllocation) {
  Arena arena;

  // Test various types
  int* int_array = arena.Allocate<int>(10);
  EXPECT_NE(int_array, nullptr);

  double* double_val = arena.Allocate<double>();
  EXPECT_NE(double_val, nullptr);

  char* char_array = arena.Allocate<char>(256);
  EXPECT_NE(char_array, nullptr);

  // Test array access
  for (int i = 0; i < 10; ++i) {
    int_array[i] = i;
    EXPECT_EQ(int_array[i], i);
  }
}

TEST_F(ArenaTest, Alignment) {
  Arena arena;

  char* c = arena.Allocate<char>(1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(c) % alignof(char), 0);

  int* i = arena.Allocate<int>(1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(i) % alignof(int), 0);

  double* d = arena.Allocate<double>(1);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(d) % alignof(double), 0);

  void* aligned_16 = arena.Allocate(100, 16);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_16) % 16, 0);

  void* aligned_64 = arena.Allocate(200, 64);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_64) % 64, 0);

  void* aligned_128 = arena.Allocate(300, 128);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned_128) % 128, 0);
}

TEST_F(ArenaTest, MultipleBlocks) {
  Arena arena(128);

  // First allocation should fit in first block
  void* ptr1 = arena.Allocate(100);
  EXPECT_NE(ptr1, nullptr);

  void* ptr3 = arena.Allocate(16);
  EXPECT_NE(ptr3, nullptr);

  // This should trigger a new block
  void* ptr2 = arena.Allocate(200);
  EXPECT_NE(ptr2, nullptr);

  // Verify they are in different memory regions
  uintptr_t addr1 = reinterpret_cast<uintptr_t>(ptr1);
  uintptr_t addr2 = reinterpret_cast<uintptr_t>(ptr2);

  // They should be far apart (different blocks)
  EXPECT_GT(std::abs(static_cast<long>(addr1 - addr2)), 100);

  arena.Dump(std::cout);
}

TEST_F(ArenaTest, ZeroAllocation) {
  Arena arena;
  void* ptr = arena.Allocate(0);

  EXPECT_EQ(ptr, nullptr);
  EXPECT_NO_THROW(arena.Allocate(0));
}

TEST_F(ArenaTest, MoveSemantics) {
  Arena arena1;
  void* original_ptr = arena1.Allocate(100);
  Arena arena2 = std::move(arena1);
  void* new_ptr = arena2.Allocate(50);

  EXPECT_NE(new_ptr, nullptr);
  EXPECT_NE(new_ptr, original_ptr);
}