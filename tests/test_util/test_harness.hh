#pragma once

namespace badger::test {

// Return a randomization seed for this run.  Typically returns the
// same number on repeated invocations of this binary, but automated
// runs may be able to vary the seed.
int random_seed();

}  // namespace badger::test