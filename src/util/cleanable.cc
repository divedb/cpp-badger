#include "cpp-badger/util/cleanable.hh"

#include <atomic>
#include <cassert>
#include <utility>

namespace badger {

// If the entire linked list was on heap we could have simply add attach one
// link list to another. However the head is an embeded object to avoid the cost
// of creating objects for most of the use cases when the Cleanable has only one
// Cleanup to do. We could put evernything on heap if benchmarks show no
// negative impact on performance.
// Also we need to iterate on the linked list since there is no pointer to the
// tail. We can add the tail pointer but maintainin it might negatively impact
// the perforamnce for the common case of one cleanup where tail pointer is not
// needed. Again benchmarks could clarify that.
// Even without a tail pointer we could iterate on the list, find the tail, and
// have only that node updated without the need to insert the Cleanups one by
// one. This however would be redundant when the source Cleanable has one or a
// few Cleanups which is the case most of the time.
// TODO(myabandeh): if the list is too long we should maintain a tail pointer
// and have the entire list (minus the head that has to be inserted separately)
// merged with the target linked list at once.
void Cleanable::DelegateCleanupsTo(Cleanable* other) {
  assert(other != nullptr);

  other->cleanups_.insert(other->cleanups_.end(),
                          std::make_move_iterator(cleanups_.begin()),
                          std::make_move_iterator(cleanups_.end()));
  DoCleanup();
}

void Cleanable::RegisterCleanup(CleanupFunction func, void* arg1, void* arg2) {
  assert(func != nullptr);

  cleanups_.emplace_back(func, arg1, arg2);
}

struct SharedCleanablePtr::Impl : public Cleanable {
  std::atomic<unsigned> ref_count{1};

  void Ref() { ref_count.fetch_add(1, std::memory_order_relaxed); }

  void Unref() {
    if (ref_count.fetch_sub(1, std::memory_order_relaxed) == 1) delete this;
  }

  static void UnrefWrapper(void* arg1, void* /*arg2*/) {
    static_cast<SharedCleanablePtr::Impl*>(arg1)->Unref();
  }
};

void SharedCleanablePtr::Reset() {
  if (ptr_) {
    ptr_->Unref();
    ptr_ = nullptr;
  }
}

void SharedCleanablePtr::Allocate() {
  Reset();
  ptr_ = new Impl();
}

SharedCleanablePtr::SharedCleanablePtr(const SharedCleanablePtr& from) {
  *this = from;
}

SharedCleanablePtr::SharedCleanablePtr(SharedCleanablePtr&& from) noexcept {
  *this = std::move(from);
}

SharedCleanablePtr& SharedCleanablePtr::operator=(
    const SharedCleanablePtr& from) {
  if (this != &from) {
    Reset();
    ptr_ = from.ptr_;

    if (ptr_) ptr_->Ref();
  }

  return *this;
}

SharedCleanablePtr& SharedCleanablePtr::operator=(
    SharedCleanablePtr&& from) noexcept {
  assert(this != &from);

  Reset();
  ptr_ = from.ptr_;
  from.ptr_ = nullptr;

  return *this;
}

SharedCleanablePtr::~SharedCleanablePtr() { Reset(); }

Cleanable& SharedCleanablePtr::operator*() { return *ptr_; }

Cleanable* SharedCleanablePtr::operator->() { return ptr_; }

Cleanable* SharedCleanablePtr::get() { return ptr_; }

void SharedCleanablePtr::RegisterCopyWith(Cleanable* target) {
  if (ptr_) {
    ptr_->Ref();
    target->RegisterCleanup(&Impl::UnrefWrapper, ptr_, nullptr);
  }
}

void SharedCleanablePtr::MoveAsCleanupTo(Cleanable* target) {
  if (ptr_) {
    target->RegisterCleanup(&Impl::UnrefWrapper, ptr_, nullptr);
    ptr_ = nullptr;
  }
}

}  // namespace badger
