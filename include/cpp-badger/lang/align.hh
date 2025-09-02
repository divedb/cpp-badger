#pragma once

#include <cstddef>

namespace badger {

/// valid_align_value
///
/// Returns whether an alignment value is valid. Valid alignment values are
/// powers of two representable as std::uintptr_t, with possibly additional
/// context-specific restrictions that are not checked here.
struct valid_align_value_fn {
  static_assert(sizeof(std::size_t) <= sizeof(std::uintptr_t));

  constexpr bool operator()(std::size_t align) const noexcept {
    return align && !(align & (align - 1));
  }

  constexpr bool operator()(std::align_val_t align) const noexcept {
    return operator()(static_cast<std::size_t>(align));
  }
};

inline constexpr valid_align_value_fn valid_align_value;

inline constexpr std::size_t max_align_v = alignof(std::max_align_t);

}  // namespace badger