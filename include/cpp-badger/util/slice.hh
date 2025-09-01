// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Slice is a simple structure containing a pointer into some external
// storage and a size.  The user of a Slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// Multiple threads can invoke const methods on a Slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Slice must use
// external synchronization.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include "cpp-badger/util/cleanable.hh"

namespace badger {

class Slice {
 public:
  /// Create an empty slice.
  Slice() : data_(""), size_(0) {}

  /// Create a slice that refers to a memory region [d, d + n).
  ///
  /// \note The slice does not copy the data; it only stores the pointer and
  ///       size.
  ///
  /// \param d Pointer to the beginning of the character sequence.
  /// \param n Length of the sequence.
  Slice(const char* d, size_t n) : data_(d), size_(n) {}

  /// Construct a slice that refers to the contents of a std::string.
  ///
  /// \note The string must remain valid for the lifetime of this slice.
  ///
  /// \param s Reference to the source std::string.
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}

  /// Construct a slice that refers to the contents of a std::string_view.
  ///
  /// \param sv Reference to the source std::string_view.
  Slice(std::string_view sv) : data_(sv.data()), size_(sv.size()) {}

  /// Construct a slice that refers to a NULL-terminated C string. The size of
  /// the slice is determined using `strlen(s)`. If `s` is nullptr, the slice
  /// will be empty (size = 0).
  ///
  /// \param s Pointer to a null-terminated C string.
  Slice(const char* s) : data_(s) { size_ = (s == nullptr) ? 0 : strlen(s); }

  /// Construct a slice by concatenating multiple SliceParts into a single
  /// buffer. The resulting slice will reference the contents stored in `buf`.
  ///
  /// \param parts A collection of SliceParts to be concatenated.
  /// \param buf   Destination buffer where the concatenated data is stored.
  Slice(const struct SliceParts& parts, std::string* buf);

  /// \return Return a pointer to the beginning of the referenced data.
  const char* data() const { return data_; }

  /// \return Return the length (in bytes) of the referenced data.
  size_t size() const { return size_; }

  /// \return Return true iff the length of the referenced data is zero
  bool IsEmpty() const { return size_ == 0; }

  /// Access a character at the specified position in the slice.
  /// Performs a bounds check with `assert(n < size())`.
  ///
  /// \param n Zero-based index of the character to access.
  /// \return The character at position `n`.
  char operator[](size_t n) const {
    assert(n < size());

    return data_[n];
  }

  /// Change this slice to refer to an empty array
  void Clear() {
    data_ = "";
    size_ = 0;
  }

  /// Remove the first `n` characters from the slice.
  ///
  /// \param n Number of characters to remove.
  void RemovePrefix(size_t n) {
    assert(n <= size());

    data_ += n;
    size_ -= n;
  }

  /// Remove the last `n` characters from the slice.
  ///
  /// \param n Number of characters to remove.
  void RemoveSuffix(size_t n) {
    assert(n <= size());

    size_ -= n;
  }

  /// Returns a string containing a copy of the referenced data.
  /// If `hex` is true, the string will be hex-encoded, doubling its length
  /// (characters 0-9, A-F).
  ///
  /// \param hex If true, output the data in hexadecimal representation;
  ///            otherwise, output raw data.
  /// \return A std::string containing the copied data (hex-encoded if `hex` is
  ///         true)
  std::string ToString(bool hex = false) const;

  /// \return Return a string_view that references the same data as this slice.
  std::string_view ToStringView() const {
    return std::string_view(data_, size_);
  }

  /// Decodes the current slice, interpreted as a hexadecimal string,
  /// into `result`. The slice is expected to contain an even number of
  /// hexadecimal characters (0-9, A-F), and lowercase letters (a-f) are also
  /// accepted. If the slice is not a valid hex string (e.g., not produced by
  /// `Slice::ToString(true)`), decoding fails.
  ///
  /// \param result Reference to a std::string where the decoded bytes will be
  ///               stored.
  /// \return true if decoding was successful; false otherwise.
  bool DecodeHex(std::string& result) const;

  /// Performs a three-way comparison between this slice and another
  /// slice `b`. Returns a value indicating their relative order:
  ///   - <  0 if "*this" <  `b`
  ///   - == 0 if "*this" == `b`
  ///   - >  0 if "*this" >  `b`
  ///
  /// \param b The slice to compare against.
  /// \return An integer representing the comparison result: negative, zero, or
  ///         positive.
  int Compare(const Slice& b) const;

  /// Checks if this slice begins with the contents of another slice `x`.
  ///
  /// \param x The slice to check as the prefix.
  /// \return true if this slice starts with `x`; false otherwise.
  bool StartsWith(const Slice& x) const {
    return ((size_ >= x.size_) && (memcmp(data_, x.data_, x.size_) == 0));
  }

  /// Checks if this slice ends with the contents of another slice `x`.
  ///
  /// \param x The slice to check as the suffix.
  /// \return true if this slice ends with `x`; false otherwise.
  bool EndsWith(const Slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_ + size_ - x.size_, x.data_, x.size_) == 0));
  }

  /// Finds the first position at which this slice and another slice `b` differ.
  ///
  /// \param b The slice to compare against.
  /// \return The zero-based index of the first differing byte.
  size_t DifferenceOffset(const Slice& b) const;

 protected:
  const char* data_;
  size_t size_;
};

/// A likely more efficient alternative to std::optional<Slice>. For example,
/// an empty key might be distinct from "not specified" (and Slice* as an
/// optional is more troublesome to deal with).
class OptSlice {
 public:
  OptSlice() : slice_(nullptr, SIZE_MAX) {}
  /* implicit */ OptSlice(const Slice& s) : slice_(s) {}
  /* implicit */ OptSlice(const std::string& s) : slice_(s) {}
  /* implicit */ OptSlice(std::string_view sv) : slice_(sv) {}
  /* implicit */ OptSlice(const char* c_str) : slice_(c_str) {}

  // For easier migrating from APIs uing Slice* as an optional type.
  // CAUTION: OptSlice{nullptr} is "no value" while Slice{nullptr} is "empty"
  /* implicit */ OptSlice(std::nullptr_t) : OptSlice() {}

  bool HasValue() const noexcept { return slice_.size() != SIZE_MAX; }
  explicit operator bool() const noexcept { return HasValue(); }

  const Slice& Value() const noexcept {
    assert(HasValue());

    return slice_;
  }

  const Slice& operator*() const noexcept { return Value(); }
  const Slice* operator->() const noexcept { return &Value(); }
  const Slice* AsPtr() const noexcept { return HasValue() ? &slice_ : nullptr; }

  // Populate from an optional pointer. This is a very explicit conversion
  // to minimize risk of bugs as in
  //   Slice start, limit;
  //   RangeOpt rng = {&start, &limit};
  //   start = ...;  // BUG: would not affect rng
  static OptSlice CopyFromPtr(const Slice* ptr) {
    return ptr ? OptSlice{*ptr} : OptSlice{};
  }

 protected:
  Slice slice_;
};

/// A Slice that can be pinned with some cleanup tasks, which will be run upon
/// ::Reset() or object destruction, whichever is invoked first. This can be
/// used to avoid memcpy by having the PinnableSlice object referring to the
/// data that is locked in the memory and release them after the data is
/// consumed.
class PinnableSlice : public Slice, public Cleanable {
 public:
  PinnableSlice() { buf_ = &self_space_; }
  explicit PinnableSlice(std::string* buf) { buf_ = buf; }

  PinnableSlice(PinnableSlice&& other);
  PinnableSlice& operator=(PinnableSlice&& other);

  /// No copy constructor and copy assignment allowed.
  PinnableSlice(PinnableSlice&) = delete;
  PinnableSlice& operator=(PinnableSlice&) = delete;

  inline void PinSlice(const Slice& s, CleanupFunction f, void* arg1,
                       void* arg2) {
    assert(!pinned_);

    pinned_ = true;
    data_ = s.data();
    size_ = s.size();
    RegisterCleanup(f, arg1, arg2);
  }

  inline void PinSlice(const Slice& s, Cleanable* cleanable) {
    assert(!pinned_);

    pinned_ = true;
    data_ = s.data();
    size_ = s.size();

    if (cleanable != nullptr) cleanable->DelegateCleanupsTo(this);
  }

  inline void PinSelf(const Slice& slice) {
    assert(!pinned_);

    buf_->assign(slice.data(), slice.size());
    data_ = buf_->data();
    size_ = buf_->size();
  }

  inline void PinSelf() {
    assert(!pinned_);

    data_ = buf_->data();
    size_ = buf_->size();
  }

  void RemoveSuffix(size_t n) {
    assert(n <= size());

    if (pinned_)
      size_ -= n;
    else {
      buf_->erase(size() - n, n);
      PinSelf();
    }
  }

  void RemovePrefix(size_t n) {
    assert(n <= size());

    if (pinned_) {
      data_ += n;
      size_ -= n;
    } else {
      buf_->erase(0, n);
      PinSelf();
    }
  }

  void Reset() {
    Cleanable::Reset();
    pinned_ = false;
    size_ = 0;
  }

  inline std::string* GetSelf() { return buf_; }

  inline bool IsPinned() const { return pinned_; }

 private:
  std::string self_space_;
  std::string* buf_;
  bool pinned_ = false;
};

/// A set of Slices that are virtually concatenated together.  'parts' points
/// to an array of Slices.  The number of elements in the array is 'num_parts'.
struct SliceParts {
  const Slice* parts = nullptr;
  int num_parts = 0;
};

inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) { return !(x == y); }

inline int Slice::Compare(const Slice& b) const {
  assert(data_ != nullptr && b.data_ != nullptr);

  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);

  if (r == 0) {
    if (size_ < b.size_)
      r = -1;
    else if (size_ > b.size_)
      r = +1;
  }

  return r;
}

inline size_t Slice::DifferenceOffset(const Slice& b) const {
  size_t off = 0;
  const size_t len = (size_ < b.size_) ? size_ : b.size_;

  for (; off < len; off++)
    if (data_[off] != b.data_[off]) break;

  return off;
}

}  // namespace badger