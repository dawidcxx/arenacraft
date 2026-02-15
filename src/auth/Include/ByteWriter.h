#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace auth
{
struct ByteWriter
{
  std::vector<std::uint8_t> bytes;

  void u8(std::uint8_t v) { bytes.push_back(v); }

  void u16(std::uint16_t v)
  {
    bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  }

  void u32(std::uint32_t v)
  {
    bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  }

  void f32(float v)
  {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t raw = 0;
    std::memcpy(&raw, &v, sizeof(raw));
    u32(raw);
  }

  template <std::size_t N> void append(const std::array<std::uint8_t, N>& arr)
  {
    bytes.insert(bytes.end(), arr.begin(), arr.end());
  }

  void append(const std::vector<std::uint8_t>& in) { bytes.insert(bytes.end(), in.begin(), in.end()); }

  void cstr(const std::string& s)
  {
    bytes.insert(bytes.end(), s.begin(), s.end());
    bytes.push_back(0);
  }
};
} // namespace auth
