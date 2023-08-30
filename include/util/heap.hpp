#pragma once
#include <cstdint>
#include <vector>

template <typename T, size_t N, typename P = std::less<T>>
class Heap {
public:
  Heap() = default;
  Heap(std::size_t capacity) { mData.reserve(capacity); }
  ~Heap() = default;

  auto size() const -> std::size_t { return mData.size(); }
  auto empty() const -> bool { return mData.empty(); }
  auto insert(T&& val) -> void
  {
    mData.push_back(std::forward<T>(val));
    siftUp(mData.size() - 1);
  }
  auto pop(T& out) -> bool
  {
    if (mData.empty()) {
      return false;
    }
    out = mData[0];
    mData[0] = mData.back();
    mData.pop_back();
    siftDown(0);
    return true;
  }
  auto pop() -> bool
  {
    if (mData.empty()) {
      return false;
    }
    mData[0] = mData.back();
    mData.pop_back();
    siftDown(0);
    return true;
  }
  auto top() -> T& { return mData[0]; }

private:
  auto siftUp(std::size_t idx) -> void
  {
    if (idx == 0) {
      return;
    }
    auto parent = (idx - 1) / N;
    if (P()(mData[idx], mData[parent])) {
      std::swap(mData[idx], mData[parent]);
      siftUp(parent);
    }
  }
  auto siftDown(std::size_t idx) -> void
  {
    auto sonStart = idx * N + 1;
    auto sonEnd = std::min(sonStart + N, mData.size());
    auto maxSon = idx;
    for (auto i = sonStart; i < sonEnd; i++) {
      if (P()(mData[i], mData[maxSon])) {
        maxSon = i;
      }
    }
    if (maxSon != idx) {
      std::swap(mData[maxSon], mData[idx]);
      siftDown(maxSon);
    }
  }

private:
  std::vector<T> mData;
};
