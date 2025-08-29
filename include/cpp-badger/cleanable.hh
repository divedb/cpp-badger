// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <memory>

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
  Cleanable(Cleanable&&) noexcept;
  Cleanable& operator=(Cleanable&&) noexcept;

  /// Executes all the registered cleanups.
  ~Cleanable();

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

  // Move the cleanups owned by this Cleanable to another Cleanable, adding to
  // any existing cleanups it has

  /// @brief
  /// @param other
  void DelegateCleanupsTo(Cleanable* other);

  /// DoCleanup and also resets the pointers for reuse.
  inline void Reset() { DoCleanup(); }

  /// \return `true` if there is at least one cleanup function registered;
  ///         `false` otherwise.
  inline bool HasCleanups() { return cleanup_.function != nullptr; }

 protected:
  struct Cleanup {
    CleanupFunction function = nullptr;
    void* arg1;
    void* arg2;
    std::unique_ptr<Cleanup> next;
  };

  /// Register a cleanup node to be executed when this object is destroyed.
  ///
  /// Adds the given `Cleanup` node to the internal cleanup list. The registered
  /// cleanup function(s) in `c` will be called upon destruction of this object.
  ///
  /// \param c Pointer to a `Cleanup` node to register. Ownership is transferred
  ///          to this object; the caller should not delete it manually.
  void RegisterCleanup(std::unique_ptr<Cleanup> c);

  Cleanup cleanup_;

 private:
  /// Performs all the cleanups. It does not reset the pointers. Making it
  /// private to prevent misuse.
  inline void DoCleanup() {
    if (cleanup_.function == nullptr) return;

    (*cleanup_.function)(cleanup_.arg1, cleanup_.arg2);

    auto cur = std::move(cleanup_.next);

    while (cur) {
      (*cur->function)(cur->arg1, cur->arg2);
      cur = std::move(cur->next);
    }

    cleanup_.function = nullptr;
  }
};

}  // namespace badger