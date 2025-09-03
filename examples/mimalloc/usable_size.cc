#include <mimalloc.h>
#include <stdio.h>

#include <array>

void demo_usable_size() {
  void* ptr = mi_malloc(100);

  if (ptr) {
    size_t actual_size = mi_usable_size(ptr);
    size_t requested_size = 100;

    printf("Requested: %zu bytes\n", requested_size);
    printf("Actually allocated: %zu bytes\n", actual_size);
    printf("Overhead: %zu bytes (%.1f%%)\n", actual_size - requested_size,
           ((float)(actual_size - requested_size) / requested_size) * 100);

    mi_free(ptr);
  }
}

void test_various_sizes() {
  size_t test_sizes[] = {8, 16, 24, 32, 48, 64, 96, 128, 256, 512, 1024, 2048};

  printf("Size Class Analysis:\n");
  printf("Requested | Actual | Overhead | Efficiency\n");
  printf("----------|--------|----------|-----------\n");

  for (size_t i = 0; i < sizeof(test_sizes) / sizeof(test_sizes[0]); i++) {
    size_t requested = test_sizes[i];
    void* ptr = mi_malloc(requested);

    if (ptr) {
      size_t actual = mi_usable_size(ptr);
      float efficiency = (float)requested / actual * 100;

      printf("%8zu | %6zu | %8zu | %6.1f%%\n", requested, actual,
             actual - requested, efficiency);

      mi_free(ptr);
    }
  }
}

int main() { test_various_sizes(); }