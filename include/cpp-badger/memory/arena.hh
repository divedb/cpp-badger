#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <set>
#include <stdexcept>

namespace badger {

/// The Arena class provides a memory management system that allocates large
/// blocks of memory and serves smaller allocations from these blocks. This
/// reduces fragmentation and improves allocation performance for scenarios with
/// many small allocations.
class Arena {
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

 public:
  /// Default initial size for the first memory block (1MB).
  static constexpr size_t kDefaultInitialSize = 1024 * 1024;

  /// Constructs an Arena with the specified initial block size.
  ///
  /// \param initial_size The size of the first memory block to allocate.
  /// \throws std::bad_alloc if the initial block cannot be allocated.
  explicit Arena(size_t initial_size = kDefaultInitialSize) {
    assert(initial_size > 0 && "Initial size must be positive");

    CreateNewBlock(initial_size);
  }

  ~Arena();

  /// Move constructor.
  ///
  /// \param other The Arena to move from.
  Arena(Arena&& other) noexcept : blocks_(std::move(other.blocks_)) {}

  /// Allocates memory with the specified size and alignment.
  ///
  /// \param size The number of bytes to allocate.
  /// \param alignment The alignment requirement (must be power of two).
  /// \return Pointer to the allocated memory.
  /// \throws std::bad_alloc if memory allocation fails.
  void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
    if (size == 0) return nullptr;
    if (auto p = TryAllocateFromExistingBlock(size, alignment)) return p;

    CreateNewBlock(size, alignment);
    auto p = TryAllocateFromExistingBlock(size, alignment);

    assert(p);

    return p;
  }

  /// \tparam T The type of objects to allocate.
  /// \param count The number of objects to allocate.
  /// \return Pointer to the allocated array of objects.
  /// \throws std::bad_alloc if memory allocation fails.
  template <typename T>
  T* Allocate(size_t count = 1) {
    return static_cast<T*>(Allocate(count * sizeof(T), alignof(T)));
  }

  void Dump(std::ostream& os);

 private:
  class MemoryBlock {
   public:
    /// Constructs a MemoryBlock.
    /// \param start Pointer to the start of the memory block.
    /// \param size The total size of the memory block.
    MemoryBlock(void* start, size_t size)
        : block_start_(start),
          current_ptr_(start),
          block_end_(static_cast<uint8_t*>(start) + size) {}

    /// Checks if the block can accommodate an allocation.
    ///
    /// \param size The required allocation size.
    /// \param alignment The required alignment.
    /// \return Pointer to the potential allocation location, or nullptr if not
    ///         enough space.
    void* Peek(size_t size, size_t alignment) const {
      auto current = reinterpret_cast<std::uintptr_t>(current_ptr_);
      auto aligned = AlignUp(current, alignment);
      auto new_ptr = aligned + size;
      auto end = reinterpret_cast<std::uintptr_t>(block_end_);

      if (new_ptr > end) return nullptr;

      return reinterpret_cast<void*>(aligned);
    }

    /// Advances the current pointer to the specified location.
    ///
    /// \param p The new current pointer position.
    void Seek(void* p) {
      assert(block_start_ <= p && p <= block_end_ &&
             "Pointer out of block range");

      current_ptr_ = p;
    }

    /// \param other The other arena.
    /// \return True if the available space is less than other; otherwise false.
    bool operator<(const MemoryBlock& other) const {
      return Available() < other.Available();
    }

    void* BlockStart() const { return block_start_; }
    void* CurrentPtr() const { return current_ptr_; }
    void* BlockEnd() const { return block_end_; }

    /// \return The available space in the block.
    size_t Available() const {
      return static_cast<char*>(BlockEnd()) - static_cast<char*>(CurrentPtr());
    }

   private:
    void* block_start_;  ///< Start of the memory block.
    void* current_ptr_;  ///< Current allocation position.
    void* block_end_;    ///< End of the memory block.
  };

  /// Rounds up the given address to the nearest multiple of alignment.
  ///
  /// \param address The original value that needs to be aligned.
  /// \param alignment The alignment boundary (must be a power of two).
  /// \return The smallest value greater than or equal to size that is a
  ///         multiple of alignment.
  static uintptr_t AlignUp(uintptr_t address, size_t alignment) {
    assert(alignment > 0 && "Alignment must be positive");
    assert((alignment & (alignment - 1)) == 0 &&
           "Alignment must be power of two");

    return (address + alignment - 1) & ~(alignment - 1);
  }

  /// Create a new block with the specified size.
  ///
  /// \param size The number of bytes to allocate.
  void CreateNewBlock(size_t size);

  /// Create a new block with the specified size and alignment.
  /// Note: The alignment is not a power of 2.
  ///
  /// \param size The number of bytes to allocate.
  /// \param alignment The the minimal alignment of the allocated memory.
  void CreateNewBlock(size_t size, size_t alignment);

  /// Attempts to allocate from existing blocks.
  ///
  /// \param size The required allocation size.
  /// \param alignment The required alignment.
  /// \return Pointer to allocated memory, or nullptr if no block has enough
  ///         space.
  void* TryAllocateFromExistingBlock(size_t size, size_t alignment) {
    for (auto it = std::prev(blocks_.end());; --it) {
      if (auto p = it->Peek(size, alignment); p) {
        auto handle = blocks_.extract(it);
        handle.value().Seek(static_cast<char*>(p) + size);
        blocks_.insert(std::move(handle));
        return p;
      }

      if (it == blocks_.begin()) break;
    }

    return nullptr;
  }

  std::multiset<MemoryBlock> blocks_;
};

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
    return static_cast<T*>(arena_->Allocate(n * sizeof(T), alignof(T)));
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