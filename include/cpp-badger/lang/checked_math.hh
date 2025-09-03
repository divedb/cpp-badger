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

#include <concepts>
#include <limits>
#include <optional>
#include <type_traits>

namespace badger {

namespace detail {

template <std::integral T>
std::optional<T> generic_checked_add(T a, T b) {
  if constexpr (std::is_signed_v<T>) {
    if ((a >= 0 && b > std::numeric_limits<T>::max() - a) ||
        (a < 0 && b < std::numeric_limits<T>::min() - a))
      return std::nullopt;
  } else {
    if (b > std::numeric_limits<T>::max() - a) return std::nullopt;
  }

  return a + b;
}

}  // namespace detail

template <std::integral T>
std::optional<T> checked_add(T a, T b) {
  return detail::generic_checked_add(a, b);
}

}  // namespace badger