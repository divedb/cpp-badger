/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <absl/container/internal/compressed_tuple.h>

#include <boost/intrusive/slist.hpp>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <utility>

#include "cpp-badger/lang/align.hh"

namespace badger {

/// Simple arena: allocate memory which gets freed when the arena gets
/// destroyed.
///
/// The arena itself allocates memory using a custom allocator which conforms
/// to the C++ concept Allocator.
///
///   http://en.cppreference.com/w/cpp/concept/Allocator
///
/// You may also specialize ArenaAllocatorTraits for your allocator type to
/// provide:
///
///   size_t GoodSize(const Allocator& alloc, size_t size) const;
///      Return a size (>= the provided size) that is considered "good" for your
///      allocator (for example, if your allocator allocates memory in 4MB
///      chunks, size should be rounded up to 4MB).  The provided value is
///      guaranteed to be rounded up to a multiple of the maximum alignment
///      required on your system; the returned value must be also.
///
/// An implementation that uses malloc() / free() is defined below, see
/// SysArena.
template <typename Alloc>
struct ArenaAllocatorTraits;

template <typename Alloc>
class Arena {
  using AllocTraits =
      typename std::allocator_traits<Alloc>::template rebind_traits<char>;

  using BlockLink = boost::intrusive::slist_member_hook<>;

  struct alignas(max_align_v) Block {
    BlockLink link;

    char* Start() { return reinterpret_cast<char*>(this + 1); }
  };

  struct alignas(max_align_v) LargeBlock {
    BlockLink link;
    const size_t size;

    explicit LargeBlock(size_t s) : size(s) {}
    char* Start() { return reinterpret_cast<char*>(this + 1); }
  };

  // cache_last<true> makes the list keep a pointer to the last element, so we
  // have push_back() and constant time splice_after()
  using BlockList = boost::intrusive::slist<
      Block, boost::intrusive::member_hook<Block, BlockLink, &Block::link>,
      boost::intrusive::constant_time_size<false>,
      boost::intrusive::cache_last<true>>;

  using LargeBlockList = boost::intrusive::slist<
      LargeBlock,
      boost::intrusive::member_hook<LargeBlock, BlockLink, &LargeBlock::link>,
      boost::intrusive::constant_time_size<false>,
      boost::intrusive::cache_last<true>>;

 public:
  static constexpr size_t kDefaultMinBlockSize = 4096 - sizeof(Block);
  static constexpr size_t kNoSizeLimit = 0;
  static constexpr size_t kDefaultMaxAlign = alignof(Block);
  static constexpr size_t kBlockOverhead = sizeof(Block);

  explicit Arena(const Alloc& alloc,
                 size_t min_block_size = kDefaultMinBlockSize,
                 size_t size_limit = kNoSizeLimit,
                 size_t max_align = kDefaultMaxAlign)
      : alloc_and_size_(alloc, min_block_size),
        current_block_(blocks_.last()),
        size_limit_(size_limit),
        max_align_(max_align),
  {
    if (!valid_align_value(max_align)) {
      throw_exception<std::invalid_argument>(
          folly::to<std::string>("Invalid maxAlign: ", maxAlign_));
    }
  }

  ~Arena() {
    FreeBlocks();
    FreeLargeBlocks();
  }

  void* allocate(size_t size) {
    size = roundUp(size);
    bytesUsed_ += size;

    assert(ptr_ <= end_);
    if (FOLLY_LIKELY((size_t)(end_ - ptr_) >= size)) {
      // Fast path: there's enough room in the current block
      char* r = ptr_;
      ptr_ += size;
      assert(isAligned(r));
      return r;
    }

    if (canReuseExistingBlock(size)) {
      currentBlock_++;
      char* r = align(currentBlock_->start());
      ptr_ = r + size;
      end_ = currentBlock_->start() + blockGoodAllocSize() - sizeof(Block);
      assert(ptr_ <= end_);
      assert(isAligned(r));
      return r;
    }

    // Not enough room in the current block
    void* r = allocateSlow(size);
    assert(isAligned(r));
    return r;
  }

  void deallocate(void* /* p */, size_t = 0) {
    // Deallocate? Never!
  }

  // Transfer ownership of all memory allocated from "other" to "this".
  void merge(Arena&& other);

  void clear() {
    bytesUsed_ = 0;
    freeLargeBlocks();  // We don't reuse large blocks
    if (blocks_.empty()) {
      return;
    }
    currentBlock_ = blocks_.begin();
    ptr_ = align(currentBlock_->start());
    end_ = currentBlock_->start() + blockGoodAllocSize() - sizeof(Block);
    assert(ptr_ <= end_);
  }

  // Gets the total memory used by the arena
  size_t totalSize() const { return totalAllocatedSize_ + sizeof(Arena); }

  // Gets the total number of "used" bytes, i.e. bytes that the arena users
  // allocated via the calls to `allocate`. Doesn't include fragmentation, e.g.
  // if block size is 4KB and you allocate 2 objects of 3KB in size,
  // `bytesUsed()` will be 6KB, while `totalSize()` will be 8KB+.
  size_t bytesUsed() const { return bytesUsed_; }

  // not copyable or movable
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;
  Arena(Arena&&) = delete;
  Arena& operator=(Arena&&) = delete;

 private:
  constexpr size_t blockGoodAllocSize() {
    return ArenaAllocatorTraits<Alloc>::goodSize(
        alloc(), roundUp(sizeof(Block)) + minBlockSize());
  }

  bool canReuseExistingBlock(size_t size) {
    if (size > minBlockSize()) {
      // We don't reuse large blocks
      return false;
    }
    if (blocks_.empty() || currentBlock_ == blocks_.last()) {
      // No regular blocks to reuse
      return false;
    }
    return true;
  }

  void freeBlocks() {
    blocks_.clear_and_dispose([this](Block* b) {
      b->~Block();
      AllocTraits::deallocate(alloc(), reinterpret_cast<char*>(b),
                              blockGoodAllocSize());
    });
  }

  void freeLargeBlocks() {
    largeBlocks_.clear_and_dispose([this](LargeBlock* b) {
      auto size = b->allocSize;
      totalAllocatedSize_ -= size;
      b->~LargeBlock();
      AllocTraits::deallocate(alloc(), reinterpret_cast<char*>(b), size);
    });
  }

 private:
  bool isAligned(uintptr_t address) const {
    return (address & (maxAlign_ - 1)) == 0;
  }
  bool isAligned(void* p) const {
    return isAligned(reinterpret_cast<uintptr_t>(p));
  }

  // Round up size so it's properly aligned
  size_t roundUp(size_t size) const {
    auto maxAl = maxAlign_ - 1;
    size_t realSize;
    if (!checked_add<size_t>(&realSize, size, maxAl)) {
      throw_exception<std::bad_alloc>();
    }
    return realSize & ~maxAl;
  }

  char* align(char* ptr) { return align_ceil(ptr, maxAlign_); }

  void* allocateSlow(size_t size);

  // Empty member optimization: package Alloc with a non-empty member
  // in case Alloc is empty (as it is in the case of SysAllocator).
  struct AllocAndSize : public Alloc {
    explicit AllocAndSize(const Alloc& a, size_t s)
        : Alloc(a), minBlockSize(s) {}

    size_t minBlockSize;
  };

  size_t minBlockSize() const { return allocAndSize_.minBlockSize; }
  Alloc& alloc() { return allocAndSize_; }
  const Alloc& alloc() const { return allocAndSize_; }

  absl::container_internal::CompressedTuple<Alloc, size_t> alloc_and_size_;
  BlockList blocks_;
  LargeBlockList large_blocks_;

  typename BlockList::iterator current_block_;
  char* ptr_ = nullptr;
  char* end_ = nullptr;
  size_t total_allocated_size_ = 0;
  size_t bytes_used_ = 0;

  const size_t size_limit_;
  const size_t max_align_;
};

template <class Alloc>
struct AllocatorHasTrivialDeallocate<Arena<Alloc>> : std::true_type {};

/**
 * By default, don't pad the given size.
 */
template <class Alloc>
struct ArenaAllocatorTraits {
  static size_t goodSize(const Alloc& /* alloc */, size_t size) { return size; }
};

template <>
struct ArenaAllocatorTraits<SysAllocator<char>> {
  static size_t goodSize(const SysAllocator<char>& /* alloc */, size_t size) {
    return goodMallocSize(size);
  }
};

/**
 * Arena that uses the system allocator (malloc / free)
 */
class SysArena : public Arena<SysAllocator<char>> {
 public:
  explicit SysArena(size_t minBlockSize = kDefaultMinBlockSize,
                    size_t sizeLimit = kNoSizeLimit,
                    size_t maxAlign = kDefaultMaxAlign)
      : Arena<SysAllocator<char>>({}, minBlockSize, sizeLimit, maxAlign) {}
};

template <>
struct AllocatorHasTrivialDeallocate<SysArena> : std::true_type {};

template <typename T, typename Alloc>
using ArenaAllocator = CxxAllocatorAdaptor<T, Arena<Alloc>>;

template <typename T>
using SysArenaAllocator = ArenaAllocator<T, SysAllocator<char>>;

template <typename T, typename Alloc>
using FallbackArenaAllocator =
    CxxAllocatorAdaptor<T, Arena<Alloc>, /* FallbackToStdAlloc */ true>;

template <typename T>
using FallbackSysArenaAllocator = FallbackArenaAllocator<T, SysAllocator<char>>;

}  // namespace badger

#include <folly/memory/Arena-inl.h>