#include "test_harness.hh"

#include <stdlib.h>

namespace badger::test {

int random_seed() {
  const char* env = getenv("TEST_RANDOM_SEED");
  int result = (env != nullptr ? atoi(env) : 301);

  if (result <= 0) result = 301;

  return result;
}

}  // namespace badger::test