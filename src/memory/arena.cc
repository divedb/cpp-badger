#include "cpp-badger/memory/arena.hh"

#include <mimalloc.h>

#include <iostream>

namespace badger {

Arena::~Arena() {
  for (const auto& block : blocks_) mi_free(block.BlockStart());
}

void Arena::Dump(std::ostream& os) {
  for (const auto& block : blocks_) {
    os << "<memory>: [" << block.BlockStart() << " - " << block.BlockEnd()
       << "\n<available>: " << block.Available() << " bytes\n";
  }
}

void Arena::CreateNewBlock(size_t size) {
  void* p = mi_malloc(size);

  if (nullptr == p) throw std::bad_alloc();

  size_t capacity = mi_usable_size(p);
  blocks_.emplace(p, capacity);
}

void Arena::CreateNewBlock(size_t size, size_t alignment) {
  void* p = mi_malloc_aligned(size, alignment);

  if (nullptr == p) throw std::bad_alloc();

  size_t capacity = mi_usable_size(p);
  blocks_.emplace(p, capacity);
}

}  // namespace badger