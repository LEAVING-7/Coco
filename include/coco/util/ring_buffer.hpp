#pragma once

#include <optional>
// fifo ring buffer
// TODO: make it lock-free
template <typename T, std::uint32_t N>
class RingBuffer {
public:
  using value_type = std::remove_cv_t<T>;

  RingBuffer() : mOut(0), mSize(0) {}

  auto pop() noexcept -> std::optional<T>
  {
    if (mSize == 0) {
      return std::nullopt;
    }
    std::optional<T> result = mData[mOut];
    mOut = (mOut + 1) % N;
    mSize -= 1;
    return result;
  }

  auto push(T&& item) noexcept -> bool
  {
    if (mSize == N) {
      return false;
    }
    mData[(mOut + mSize) % N] = std::move(item);
    mSize += 1;
    return true;
  }

  auto push(T const& item) noexcept -> bool
  {
    if (mSize == N) {
      return false;
    }
    mData[(mOut + mSize) % N] = item;
    mSize += 1;
    return true;
  }

  auto empty() noexcept -> bool { return mSize == 0; }
  auto full() noexcept -> bool { return mSize == N; }

private:
  std::array<T, N> mData;
  std::uint32_t mOut;
  std::uint32_t mSize;
};
