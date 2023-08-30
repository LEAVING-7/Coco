#pragma once
#include <algorithm>
#include <cassert>
#include <mutex>
#include <utility>

template <auto Next> class Queue {};
template <typename Item, Item *Item::*next> class Queue<next> {
public:
  Queue() noexcept = default;
  Queue(Queue &&other) noexcept
      : mHead(std::exchange(other.mHead, nullptr)),
        mTail(std::exchange(other.mTail, nullptr)) {}
  auto operator=(Queue &&other) noexcept -> Queue & {
    std::swap(mHead, other.head);
    std::swap(mTail, other.tail);
    return *this;
  }
  ~Queue() noexcept {
    auto r = empty();
    assert(r);
  }

  static auto from(Item *list) noexcept -> Queue {
    Item *newHead = nullptr;
    Item *newTail = list;
    while (list != nullptr) {
      auto n = list->*next;
      list->*next = newHead;
      newHead = list;
      list = n;
    }
    auto q = Queue();
    q.mHead = newHead;
    q.mTail = newTail;
    return q;
  }

  auto empty() const noexcept -> bool { return mHead == nullptr; }
  auto popFront() noexcept -> Item * {
    if (mHead == nullptr) {
      return nullptr;
    }
    Item *item = std::exchange(mHead, mHead->*next);
    if (item->*next == nullptr) {
      mTail = nullptr;
    }
    return item;
  }

  auto pushFront(Item *item) noexcept -> void {
    item->*next = mHead;
    mHead = item;
    if (mTail == nullptr) {
      mTail = item;
    }
  }

  auto pushBack(Item *item) noexcept -> void {
    item->*next = nullptr;
    if (mTail == nullptr) {
      mHead = item;
    } else {
      mTail->*next = item;
    }
    mTail = item;
  }

  auto append(Queue other) noexcept -> void {
    if (other.empty()) {
      return;
    }
    auto *otherHead = std::exchange(other.mHead, nullptr);
    if (empty()) {
      mHead = otherHead;
    } else {
      mTail->*next = otherHead;
    }
    mTail = std::exchange(other.mTail, nullptr);
  }

  auto preappend(Queue other) noexcept -> void {
    if (other.empty()) {
      return;
    }
    other.mTail->*next = mHead;
    mHead = other.mHead;
    if (mTail == nullptr) {
      mTail = other.mTail;
    }
    other.mTail = nullptr;
    other.mHead = nullptr;
  }

  auto front() noexcept -> Item * { return mHead; }
  auto back() noexcept -> Item * { return mTail; }

private:
  Item *mHead = nullptr;
  Item *mTail = nullptr;
};
