#pragma once
#include "queue.hpp"

#include <atomic>

template <auto next> class AtomicQueue;
template <typename Item, Item *Item::*next> class AtomicQueue<next> {
public:
  using node_pointer = Item *;
  using atomic_node_pointer = std::atomic<node_pointer>;

  auto empty() const noexcept -> bool {
    return mHead.load(std::memory_order_relaxed) == nullptr;
  }
  auto pushFront(node_pointer t) noexcept -> void {
    node_pointer oldHead = mHead.load(std::memory_order_relaxed);
    do {
      t->*next = oldHead;
    } while (
        !mHead.compare_exchange_weak(oldHead, t, std::memory_order_acq_rel));
  }

  auto popAll() noexcept -> Queue<next> {
    return Queue<next>::from(
        mHead.exchange(nullptr, std::memory_order_acq_rel));
  }

private:
  atomic_node_pointer mHead{nullptr};
};