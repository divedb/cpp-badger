#pragma once

#include <mimalloc.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>

namespace badger {

class Arena {
 public:
  static constexpr size_t kDefaultInitialSize = 1024 * 1024;  // 1MB

  explicit Arena(size_t initial_size = kDefaultInitialSize) {
    assert(initial_size_ > 0);

    allocate_new_block(initial_size_);
  }

  // Destructor - free all allocated blocks
  ~Arena() {}

  // Copy and move operations
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  Arena(Arena&& other) noexcept
      : blocks_(std::move(other.blocks_)),
        total_allocated_(other.total_allocated_) {
    other.total_allocated_ = 0;
  }

  // Main allocation function
  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
    if (size == 0) return nullptr;
    if (auto p = TryAllocateFromExistingBlock(size, alignment)) return p;

    size_t aligned = AlignUp(size, alignment);

    if (auto p = AllocateNewBlock(aligned); p) return p;

    throw std::bad_alloc();
  }

  // Template version for type-safe allocation
  template <typename T>
  T* allocate(size_t count = 1) {
    return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
  }

  // Statistics
  size_t get_total_memory() const {
    size_t total = 0;
    for (const auto& block : blocks_) {
      total += block.size;
    }
    return total;
  }

  size_t get_allocated_memory() const { return total_allocated_; }

 private:
  class MemoryBlock {
   public:
    MemoryBlock(void* start, size_t size)
        : block_start_(start),
          current_ptr_(start),
          block_end_(static_cast<uint8_t*>(start) + size) {}

    /// @brief
    /// @param size
    /// @param alignment
    /// @return
    void* Peek(size_t size, size_t alignment) const {
      auto current = reinterpret_cast<std::uintptr_t>(current_ptr_);
      auto aligned = AlignUp(current, alignment);
      auto new_ptr = aligned + size;
      auto end = reinterpret_cast<std::uintptr_t>(block_end_);

      if (new_ptr > end) return nullptr;
      return reinterpret_cast<void*>(aligned);
    }

    /// @brief
    /// @param ptr
    void Seek(void* ptr) {
      assert(block_start_ <= ptr && ptr <= block_end_ &&
             "Pointer out of block range");

      current_ptr_ = ptr;
    }

    bool operator<(const MemoryBlock& other) const {
      return Available() < other.Available();
    }

   private:
    size_t Available() const {
      return static_cast<char*>(block_end_) - static_cast<char*>(current_ptr_);
    }

    void* block_start_;
    void* current_ptr_;
    void* block_end_;
  };

  /// Rounds up the given size to the nearest multiple of alignment.
  ///
  /// \param size The original value that needs to be aligned.
  /// \param alignment The alignment boundary (must be a power of two).
  /// \return The smallest value greater than or equal to size that is a
  ///         multiple of alignment.
  static uintptr_t AlignUp(uintptr_t address, size_t alignment) {
    assert(alignment > 0 && "Alignment must be positive");
    assert((alignment & (alignment - 1)) == 0 &&
           "Alignment must be power of two");

    return (address + alignment - 1) & ~(alignment - 1);
  }

  /// Allocate a new memory block of the given size.
  ///
  /// \param size The number of bytes to allocate.
  /// \return True if allocation succeeded, false otherwise.
  bool AllocateNewBlock(size_t size) {
    void* ptr = mi_malloc(size);

    if (nullptr == ptr) return false;

    size_t actual = mi_usable_size(ptr);
    blocks_.emplace(ptr, ptr, ptr + actual);

    return true;
  }

  void* TryAllocateFromExistingBlock(size_t size, size_t alignment) {
    for (auto it = blocks_.end(); it != blocks_.begin(); --it) {
      if (auto ptr = it->Peek(size, alignment); ptr) {
        auto handle = blocks_.extract(it);
        handle.value().Seek(static_cast<char*>(ptr) + size);
        blocks_.insert(std::move(handle));

        return ptr;
      }
    }

    return nullptr;
  }

  std::set<MemoryBlock> blocks_;
  size_t total_allocated_;
};

// Utility function to create arena-allocated objects
template <typename T, typename... Args>
T* make_arena_object(Arena& arena, Args&&... args) {
  void* memory = arena.allocate(sizeof(T), alignof(T));
  try {
    return new (memory) T(std::forward<Args>(args)...);
  } catch (...) {
    // If constructor throws, we need to handle this properly
    // In arena allocator, we typically don't free individual allocations,
    // but we need to ensure the memory is available for reuse after reset
    throw;
  }
}

// Arena-based allocator for STL containers
template <typename T>
class ArenaSTL {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template <typename U>
  struct rebind {
    using other = ArenaSTL<U>;
  };

  explicit ArenaSTL(Arena& arena) : arena_(&arena) {}

  template <typename U>
  ArenaSTL(const ArenaSTL<U>& other) : arena_(other.arena_) {}

  T* allocate(size_t n) {
    return static_cast<T*>(arena_->allocate(n * sizeof(T), alignof(T)));
  }

  void deallocate(T* p, size_t n) noexcept {
    // No-op for arena allocator - memory is freed on reset
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    new (p) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

  Arena* get_arena() const { return arena_; }

 private:
  Arena* arena_;

  template <typename U>
  friend class ArenaSTL;
};

template <typename T, typename U>
bool operator==(const ArenaSTL<T>& lhs, const ArenaSTL<U>& rhs) {
  return lhs.get_arena() == rhs.get_arena();
}

template <typename T, typename U>
bool operator!=(const ArenaSTL<T>& lhs, const ArenaSTL<U>& rhs) {
  return !(lhs == rhs);
}

}  // namespace badger