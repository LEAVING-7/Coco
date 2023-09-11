#pragma once

#include <optional>
// fifo ring buffer
template <typename T, std::uint32_t N>
class RingBuffer {
public:
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

  auto pop(T& value) -> bool
  {
    if (mSize == 0) {
      return false;
    }
    value = std::move(mData[mOut]);
    mOut = (mOut + 1) % N;
    mSize -= 1;
    return true;
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

  auto size() const noexcept -> std::uint32_t { return mSize; }
  auto empty() const noexcept -> bool { return size() == 0; }
  auto full() const noexcept -> bool { return size() == N; }

private:
  std::array<T, N> mData;
  std::uint32_t mOut;
  std::uint32_t mSize;
};
