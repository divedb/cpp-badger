// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <utility>
#include <vector>

namespace badger {

class Cleanable {
  Cleanable(Cleanable&) = delete;
  Cleanable& operator=(Cleanable&) = delete;

 public:
  /// Clients are allowed to register function/arg1/arg2 triples that will be
  /// invoked when this iterator is destroyed.
  ///
  /// Note: Unlike all of the preceding methods, this method is abstract and
  ///       therefore clients should not override it.
  using CleanupFunction = void (*)(void* arg1, void* arg2);

  Cleanable() = default;
  Cleanable(Cleanable&& other) noexcept = default;
  Cleanable& operator=(Cleanable&& other) noexcept = default;

  /// Executes all the registered cleanups.
  ~Cleanable() { DoCleanup(); }

  /// Register a cleanup function that will be invoked when this object is
  /// destroyed.
  ///
  /// The client can register multiple cleanups. Each cleanup consists of a
  /// function and two arguments (`arg1` and `arg2`). When this object is
  /// destroyed, all registered functions are called in the order they were
  /// added.
  ///
  /// \param function Pointer to the cleanup function to be called.
  ///                 The function should have the signature `void(void* arg1,
  ///                 void* arg2)`.
  /// \param arg1 First argument to pass to the cleanup function.
  /// \param arg2 Second argument to pass to the cleanup function.
  void RegisterCleanup(CleanupFunction function, void* arg1, void* arg2);

  /// Move the cleanups owned by this Cleanable to another Cleanable, adding to
  /// any existing cleanups it has
  ///
  /// \param other Pointer to the target `Cleanable` that will take ownership
  ///              of the cleanups. After this call, this object no longer
  ///              owns those cleanups.
  void DelegateCleanupsTo(Cleanable* other);

  /// DoCleanup and also resets the pointers for reuse.
  inline void Reset() { DoCleanup(); }

  /// \return `true` if there is at least one cleanup function registered;
  ///         `false` otherwise.
  inline bool HasCleanups() { return !cleanups_.empty(); }

 protected:
  struct Cleanup {
    Cleanup(const Cleanup&) = default;
    Cleanup& operator=(const Cleanup&) = delete;

    Cleanup() = default;
    Cleanup(CleanupFunction func, void* a1, void* a2)
        : function(func), arg1(a1), arg2(a2) {}
    Cleanup(Cleanup&& other) noexcept
        : function(std::exchange(other.function, nullptr)),
          arg1(std::exchange(other.arg1, nullptr)),
          arg2(std::exchange(other.arg2, nullptr)) {}

    ~Cleanup() {
      if (function != nullptr) (*function)(arg1, arg2);
    }

    Cleanup& operator=(Cleanup&& other) noexcept {
      if (this != &other) {
        function = std::exchange(other.function, nullptr);
        arg1 = std::exchange(other.arg1, nullptr);
        arg2 = std::exchange(other.arg2, nullptr);
      }

      return *this;
    }

    CleanupFunction function = nullptr;
    void* arg1;
    void* arg2;
  };

  std::vector<Cleanup> cleanups_;

 private:
  /// Performs all the cleanups. It does not reset the pointers. Making it
  /// private to prevent misuse.
  inline void DoCleanup() { cleanups_.clear(); }
};

/// A copyable, reference-counted pointer to a simple Cleanable that only
/// performs registered cleanups after all copies are destroy. This is like
/// shared_ptr<Cleanable> but works more efficiently with wrapping the pointer
/// in an outer Cleanable (see RegisterCopyWith() and MoveAsCleanupTo()).
/// WARNING: if you create a reference cycle, for example:
///   SharedCleanablePtr scp;
///   scp.Allocate();
///   scp.RegisterCopyWith(&*scp);
/// It will prevent cleanups from ever happening!
class SharedCleanablePtr {
 public:
  /// Default constructor. Creates an empty (null) shared pointer.
  SharedCleanablePtr() = default;

  /// Copy and move constructors and assignment.
  ///
  /// \param from The SharedCleanablePtr to copy from.
  SharedCleanablePtr(const SharedCleanablePtr& from);

  /// Move constructor. Transfers ownership from another SharedCleanablePtr.
  ///
  /// \param from The SharedCleanablePtr to move from.
  SharedCleanablePtr(SharedCleanablePtr&& from) noexcept;

  /// Copy assignment operator. Shares ownership of the Cleanable from another
  /// SharedCleanablePtr.
  ///
  /// \param from The SharedCleanablePtr to copy from.
  /// \return Reference to this SharedCleanablePtr.
  SharedCleanablePtr& operator=(const SharedCleanablePtr& from);

  /// Move assignment operator. Transfers ownership from another
  /// SharedCleanablePtr.
  ///
  /// \param from The SharedCleanablePtr to move from.
  /// \return Reference to this SharedCleanablePtr.
  SharedCleanablePtr& operator=(SharedCleanablePtr&& from) noexcept;

  /// Destructor (decrement refcount if non-null)
  ~SharedCleanablePtr();

  /// Create a new simple Cleanable and make this assign this pointer to it.
  /// (Reset()s first if necessary.)
  void Allocate();

  /// Reset to empty/null (decrement refcount if previously non-null)
  void Reset();

  /// Dereference to pointed-to Cleanable.
  Cleanable& operator*();
  Cleanable* operator->();

  /// \return A raw pointer to Cleanable
  Cleanable* get();

  /// Register a virtual copy of this SharedCleanablePtr with a target
  /// Cleanable. Ensures cleanups in this Cleanable run only after the target's
  /// cleanups.
  ///
  /// \param target The target Cleanable to register this copy with.
  ///               No-op if this pointer is null.
  void RegisterCopyWith(Cleanable* target);

  /// Move this SharedCleanablePtr as a cleanup to the target Cleanable.
  /// Performance-optimized version of RegisterCopyWith using move semantics.
  ///
  /// \param target The target Cleanable to move this pointer as a cleanup to.
  ///               No-op if this pointer is null.
  void MoveAsCleanupTo(Cleanable* target);

 private:
  struct Impl;
  Impl* ptr_ = nullptr;
};

}  // namespace badger