#pragma once

#include <cstdint>

using HandleId = std::uint64_t;
enum class HandleState : std::uint8_t {
  Suspend,
  Canceled,
  Ready,
  Running,
  Finished,
};

class Handle {
public:
  Handle() noexcept : mId(smHandleIdGen++) {}
  virtual ~Handle() = default;
  virtual auto run() -> void = 0;

  auto setState(HandleState state) -> void { mState = state; }
  auto id() -> HandleId { return mId; }

private:
  HandleId mId;
  inline static auto smHandleIdGen = 0;

protected:
  HandleState mState = HandleState::Ready;
};

class CoroHanle : Handle {
public:
private:
};