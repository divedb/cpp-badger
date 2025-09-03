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
#include <stdexcept>
#include <vector>

namespace badger {

class Arena {
 public:
  static constexpr size_t kDefaultInitialSize = 1024 * 1024;  // 1MB

  enum class GrowthPolicy {
    kFixed,       ///< No growth - throw on exhaustion.
    kLinear,      ///< Grow by initial size each time.
    kExponential  ///< Double size each time.
  };

  explicit Arena(size_t initial_size = kDefaultInitialSize) {
    assert(initial_size_ > 0);

    allocate_new_block(initial_size_);
  }

  // Destructor - free all allocated blocks
  ~Arena() {
    reset();
    for (auto& block : blocks_) {
      std::free(block.ptr);
    }
  }

  // Copy and move operations
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  Arena(Arena&& other) noexcept
      : blocks_(std::move(other.blocks_)),
        current_block_(other.current_block_),
        current_offset_(other.current_offset_),
        total_allocated_(other.total_allocated_) {
    other.current_block_ = nullptr;
    other.current_offset_ = 0;
    other.total_allocated_ = 0;
  }

  Arena& operator=(Arena&& other) noexcept {
    if (this != &other) {
      reset();
      for (auto& block : blocks_) {
        std::free(block.ptr);
      }

      blocks_ = std::move(other.blocks_);
      current_block_ = other.current_block_;
      current_offset_ = other.current_offset_;
      total_allocated_ = other.total_allocated_;

      other.current_block_ = nullptr;
      other.current_offset_ = 0;
      other.total_allocated_ = 0;
    }
    return *this;
  }

  // Main allocation function
  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
    if (size == 0) return nullptr;

    size_t aligned_offset = RoundUp(current_offset_, alignment);
    size_t required_size = aligned_offset + size;

    // Check if we need a new block
    if (!current_block_ || required_size > current_block_->size) {
      if (!allocate_new_block_for_size(size, alignment)) {
        throw std::bad_alloc();
      }
      // Recalculate after new block allocation
      aligned_offset = align_up(current_offset_, alignment);
      required_size = aligned_offset + size;
    }

    // Perform allocation
    void* ptr =
        reinterpret_cast<uint8_t*>(current_block_->ptr) + aligned_offset;
    current_offset_ = aligned_offset + size;
    total_allocated_ += size;

    return ptr;
  }

  // Template version for type-safe allocation
  template <typename T>
  T* allocate(size_t count = 1) {
    return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
  }

  // Reset the arena - free all allocations but keep the memory blocks
  void reset() {
    for (auto& block : blocks_) {
      block.used = 0;
    }
    if (!blocks_.empty()) {
      current_block_ = &blocks_.front();
      current_offset_ = 0;
    } else {
      current_block_ = nullptr;
      current_offset_ = 0;
    }
    total_allocated_ = 0;
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

  size_t get_used_memory() const {
    size_t used = 0;
    for (const auto& block : blocks_) {
      used += block.used;
    }
    return used;
  }

  size_t get_block_count() const { return blocks_.size(); }

  GrowthPolicy get_growth_policy() const { return growth_policy_; }

 private:
  struct MemoryBlock {
    void* ptr;
    size_t used;
    size_t size;
  };

  /// Rounds up the given offset to the nearest multiple of alignment.
  ///
  /// \param offset The original value that needs to be aligned.
  /// \param alignment The alignment boundary (must be a power of two).
  /// \return The smallest value greater than or equal to offset that is a
  ///         multiple of alignment.
  static size_t RoundUp(size_t offset, size_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
  }

  bool CanReuseCurrentBlock(size_t size, size_t alignment) {
    if (!current_block_) return false;

    size_t aligned_offset = RoundUp(current_offset_, alignment);
    size_t required_size = aligned_offset + size;
  }

  /// Allocate a new memory block of the given size.
  ///
  /// \param size The number of bytes to allocate.
  /// \return True if allocation succeeded, false otherwise.
  bool AllocateNewBlock(size_t size) {
    void* ptr = mi_malloc(size);

    if (nullptr == ptr) return false;

    blocks_.push_back({.ptr = ptr, .used = 0, .size = size});
    ResetCurrentBlock();

    return true;
  }

  // Allocate new block for requested size with alignment
  bool AllocateNewBlockForSize(size_t size, size_t alignment) {
    // Calculate minimum required size for the allocation
    size_t min_required = size + alignment - 1;

    size_t next_size = calculate_next_block_size(min_required);
    if (next_size == 0) {
      return false;
    }

    return allocate_new_block(next_size);
  }

  /// Reset the current block pointer and offset to the most recently allocated
  /// block.
  void ResetCurrentBlock() {
    assert(!blocks_.empty());

    current_block_ = &blocks_.back();
    current_offset_ = 0;
  }

  std::vector<MemoryBlock> blocks_;
  MemoryBlock* current_block_;
  size_t current_offset_;
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