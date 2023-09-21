#pragma once

#include <array>

namespace coco::util {
template <typename T, std::size_t N>
class FixedVec {
public:
  constexpr FixedVec() noexcept(noexcept(std::declval<T>())) = default;
  constexpr auto begin() noexcept { return mData.begin(); }
  constexpr auto end() noexcept { return mData.begin() + mSize; }
  constexpr auto data() noexcept { return mData.data(); }
  constexpr auto push_back(T const& item) -> bool
  {
    if (mSize == N) [[unlikely]] {
      return false;
    }
    mData[mSize++] = item;
    return true;
  }
  constexpr auto push_back(T&& item) -> bool
  {
    if (mSize == N) [[unlikely]] {
      return false;
    }
    mData[mSize++] = std::move(item);
    return true;
  }
  template <typename... Args>
  constexpr auto emplace_back(Args&&... args) -> bool
  {
    if (mSize == N) [[unlikely]] {
      return false;
    }
    std::construct_at(mData.data() + mSize, std::forward<Args>(args)...);
    mSize += 1;
    return true;
  }
  constexpr auto pop_back() -> bool
  {
    if (mSize == 0) {
      return false;
    }
    mSize -= 1;
    std::destroy_at(mData.begin() + mSize);
    return true;
  }
  constexpr auto size() noexcept -> std::size_t { return mSize; }
  constexpr auto clear() -> void
  {
    std::destroy_n(mData.data(), mSize);
    mSize = 0;
  }
  constexpr auto operator[](std::size_t i) noexcept -> T& { return mData[i]; }
  constexpr auto operator[](std::size_t i) const noexcept -> T const& { return mData[i]; }

private:
  std::size_t mSize = 0;
  std::array<T, N> mData;
};
} // namespace coco::util