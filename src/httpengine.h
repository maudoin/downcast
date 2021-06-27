#pragma once

#include <optional>
#include <functional>
#include <cstdint>
#include <string>

struct HttpEngine
{
  struct Result
  {
    std::string error;
    operator bool()const{return error.empty();}
  };

  static std::optional<std::string> get(std::string const& url);
  static Result get(
      std::string const& url,
      std::function<bool(const char *data, size_t data_length)> const& receiver,
      std::function<void()> const& restart,
      std::function<bool(uint64_t current, uint64_t total)> const& progressHandler,
      int initialTryCount = 2);
};
