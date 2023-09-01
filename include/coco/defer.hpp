#pragma once
#include "coco/__preclude.hpp"

template <class F>
class Defer {
public:
  Defer(F f) noexcept : mFn(std::move(f)), mInvoke(true) {}
  Defer(Defer&& other) noexcept : mFn(std::move(other.mFn)), mInvoke(other.mInvoke) { other.mInvoke = false; }
  Defer(Defer const&) = delete;
  Defer& operator=(Defer const&) = delete;
  ~Defer() noexcept
  {
    if (mInvoke) {
      mFn();
    }
  }

private:
  F mFn;
  bool mInvoke;
};

template <class F>
inline Defer<F> deferFn(F const& f) noexcept
{
  return Defer<F>(f);
}

template <class F>
inline Defer<F> deferFn(F&& f) noexcept
{
  return Defer<F>(std::forward<F>(f));
}

#define concat1(a, b) a##b
#define concat2(a, b) concat1(a, b)
#define _deferObject concat2(_finally_object_, __COUNTER__)

#define defer Defer _deferObject = [&]()
#define defer2(func) Defer _deferObject = deferFn(func)
