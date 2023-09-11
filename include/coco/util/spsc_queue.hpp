#include <atomic>

template <typename T, size_t Size>
class ringbuffer {
public:
  ringbuffer() : mHead(0), mTail(0) {}

  bool push(T const& value)
  {
    size_t head = mHead.load(std::memory_order_relaxed);
    size_t next_head = next(head);
    if (next_head == mTail.load(std::memory_order_acquire)) {
      return false;
    }
    ring_[head] = value;
    mHead.store(next_head, std::memory_order_release);
    return true;
  }
  
  bool pop(T& value)
  {
    size_t tail = mTail.load(std::memory_order_relaxed);
    if (tail == mHead.load(std::memory_order_acquire)) {
      return false;
    }
    value = ring_[tail];
    mTail.store(next(tail), std::memory_order_release);
    return true;
  }

private:
  size_t next(size_t current) { return (current + 1) % Size; }
  T ring_[Size];
  std::atomic<size_t> mHead, mTail;
};