#pragma once
#include "coco/__preclude.hpp"

#include <coco/runtime.hpp>
#include <coco/util/ring_buffer.hpp>

namespace coco::sync {
template <typename T, std::uint32_t N>
class Channel;
namespace detail {
template <typename T, std::uint32_t N>
struct ChannelReadAwaiter {
  auto await_ready() noexcept -> bool { return false; }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept;
  auto await_resume() noexcept -> std::optional<T>;

  std::optional<T> mVal;
  Channel<T, N>& mChannel;
};
template <typename T, std::uint32_t N>
struct ChannelWriteAwaiter {
  auto await_ready() noexcept -> bool { return false; }
  template <typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> handle) noexcept;
  auto await_resume() noexcept -> bool;

  T const* mVal;
  Channel<T, N>& mChannel;
};
}; // namespace detail

template <typename T, std::uint32_t N>
class Channel {
public:
  Channel() : mBuffer() {}

  auto read() noexcept { return detail::ChannelReadAwaiter<T, N>{std::nullopt, *this}; }
  auto write(T&& in) noexcept { return detail::ChannelWriteAwaiter<T, N>{&in, *this}; }
  auto write(T const& in) noexcept { return detail::ChannelWriteAwaiter<T, N>{&in, *this}; }

  auto empty() noexcept -> bool { return mBuffer.empty(); }
  auto full() noexcept -> bool { return mBuffer.full(); }
  auto close() noexcept
  {
    std::scoped_lock lk(mMt);
    mClosed = true;
    Proactor::get().execute(std::move(mReaders));
    Proactor::get().execute(std::move(mWriter));
  }

private:
  friend struct detail::ChannelReadAwaiter<T, N>;
  friend struct detail::ChannelWriteAwaiter<T, N>;

  std::mutex mMt;
  WorkerJobQueue mReaders;
  WorkerJobQueue mWriter;

  RingBuffer<T, N> mBuffer;

  bool mClosed = false;
};

namespace detail {
template <typename T, std::uint32_t N>
template <typename Promise>
auto ChannelReadAwaiter<T, N>::await_suspend(std::coroutine_handle<Promise> handle) noexcept
{
  std::scoped_lock lk(mChannel.mMt);
  if (mChannel.mClosed) {
    return false;
  }
  if (mChannel.mBuffer.empty()) {
    mChannel.mReaders.pushBack(handle.promise().getThisJob());
    while (auto job = mChannel.mWriter.popFront()) {
      Proactor::get().execute(std::move(job));
    }
    return true;
  } else {
    mVal = mChannel.mBuffer.pop();
    assert(mVal.has_value());
    while (auto job = mChannel.mWriter.popFront()) {
      Proactor::get().execute(std::move(job));
    }
    return false;
  }
}

template <typename T, std::uint32_t N>
auto ChannelReadAwaiter<T, N>::await_resume() noexcept -> std::optional<T>
{
  std::scoped_lock lk(mChannel.mMt);
  if (mChannel.mClosed) {
    return std::nullopt;
  }
  if (!mVal.has_value()) {
    mVal = mChannel.mBuffer.pop();
    assert(mVal.has_value());
  }
  return std::move(mVal);
}

// ChannelWriteAwaiter

template <typename T, std::uint32_t N>
template <typename Promise>
auto ChannelWriteAwaiter<T, N>::await_suspend(std::coroutine_handle<Promise> handle) noexcept
{
  std::scoped_lock lk(mChannel.mMt);
  if (mChannel.mClosed) {
    return false;
  }
  if (mChannel.mBuffer.full()) {
    mChannel.mWriter.pushBack(handle.promise().getThisJob());
    while (auto job = mChannel.mReaders.popFront()) {
      Proactor::get().execute(std::move(job));
    }
    return true;
  } else {
    mChannel.mBuffer.push(std::move(*mVal));
    mVal = nullptr;
    while (auto job = mChannel.mReaders.popFront()) {
      Proactor::get().execute(std::move(job));
    }
    return false;
  }
}

template <typename T, std::uint32_t N>
auto ChannelWriteAwaiter<T, N>::await_resume() noexcept -> bool
{
  std::scoped_lock lk(mChannel.mMt);
  if (mChannel.mClosed) {
    return false;
  }
  if (mVal != nullptr) {
    mChannel.mBuffer.push(std::move(*mVal));
    mVal = nullptr;
  }
  return true;
}
} // namespace detail
}; // namespace coco::sync