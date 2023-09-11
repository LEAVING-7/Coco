#pragma once
#include <concepts>
#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace coco::util {
struct PanicDynamicStringView {
  template <class T>
    requires std::convertible_to<T, std::string_view>
  PanicDynamicStringView(T const& s, std::source_location loc = std::source_location::current()) noexcept
      : str(s), loc(loc)
  {
  }

  std::string_view str;
  std::source_location loc;
};

template <typename... Args>
struct PanicFormat {
  template <class T>
  consteval PanicFormat(T const& s, std::source_location loc = std::source_location::current()) noexcept
      : str(s), loc(loc)
  {
  }

  std::format_string<Args...> str;
  std::source_location loc;
};

[[noreturn]] auto panicImpl(char const* s) noexcept -> void;

[[noreturn]] inline auto panic(PanicDynamicStringView s) noexcept -> void
{
  auto msg = std::format("{}:{} panic: {}\n", s.loc.file_name(), s.loc.line(), s.str);
  panicImpl(msg.c_str());
}

template <class... Args>
  requires(sizeof...(Args) > 0)
[[noreturn]] auto panic(PanicFormat<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept -> void
{
  auto msg = std::format("{}:{} panic: {}\n", fmt.loc.file_name(), fmt.loc.line(),
                         std::format(fmt.fmt, std::forward<Args>(args)...));
  panicImpl(msg.c_str());
}

[[noreturn]] inline auto panicImpl(char const* s) noexcept -> void
{
  std::fputs(s, stderr);
  std::abort();
}
} // namespace coco::util