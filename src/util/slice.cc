#include "cpp-badger/util/slice.hh"

#include <absl/strings/escaping.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/string_view.h>

namespace badger {

Slice::Slice(const SliceParts& parts, std::string* buf) {
  size_t length = 0;

  for (int i = 0; i < parts.num_parts; ++i) length += parts.parts[i].size();

  buf->reserve(length);

  for (int i = 0; i < parts.num_parts; ++i)
    buf->append(parts.parts[i].data(), parts.parts[i].size());

  data_ = buf->data();
  size_ = buf->size();
}

std::string Slice::ToString(bool hex) const {
  if (hex) return absl::BytesToHexString(absl::string_view(data_, size_));

  return std::string(data_, size_);
}

bool Slice::DecodeHex(std::string* result) const {
  if (!result) return false;

  std::string::size_type len = size_;

  // Hex string must be even number of hex digits to get complete bytes back
  if (len & 0x01) return false;

  result->clear();
  result->reserve(len / 2);

  for (size_t i = 0; i < len;) {
    int h1 = fromHex(data_[i++]);
    if (h1 < 0) {
      return false;
    }
    int h2 = fromHex(data_[i++]);
    if (h2 < 0) {
      return false;
    }
    result->push_back(static_cast<char>((h1 << 4) | h2));
  }
  return true;
}

PinnableSlice::PinnableSlice(PinnableSlice&& other) {
  *this = std::move(other);
}

PinnableSlice& PinnableSlice::operator=(PinnableSlice&& other) {
  if (this != &other) {
    Cleanable::Reset();
    Cleanable::operator=(std::move(other));
    size_ = other.size_;
    pinned_ = other.pinned_;
    if (pinned_) {
      data_ = other.data_;
      // When it's pinned, buf should no longer be of use.
    } else {
      if (other.buf_ == &other.self_space_) {
        self_space_ = std::move(other.self_space_);
        buf_ = &self_space_;
        data_ = buf_->data();
      } else {
        buf_ = other.buf_;
        data_ = other.data_;
      }
    }
    other.self_space_.clear();
    other.buf_ = &other.self_space_;
    other.pinned_ = false;
    other.PinSelf();
  }
  return *this;
}

}  // namespace badger