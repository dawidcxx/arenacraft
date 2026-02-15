#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace auth
{
enum : std::uint8_t
{
  AUTH_LOGON_CHALLENGE = 0x00,
  AUTH_LOGON_PROOF     = 0x01,
  REALM_LIST           = 0x10,
};

enum : std::uint8_t
{
  WOW_SUCCESS                 = 0x00,
  WOW_FAIL_UNKNOWN_ACCOUNT    = 0x04,
  WOW_FAIL_INCORRECT_PASSWORD = 0x05,
  WOW_FAIL_VERSION_INVALID    = 0x09,
  WOW_FAIL_FAIL_NOACCESS      = 0x0D,
};

constexpr std::uint16_t WOTLK_335A_BUILD = 12340;

constexpr std::array<std::uint8_t, 1> kGenerator = {7};
constexpr std::array<std::uint8_t, 32> kModulus  = {
    0x89, 0x4B, 0x64, 0x5E, 0x89, 0xE1, 0x53, 0x5B, 0xBD, 0xAD, 0x5B, 0x8B, 0x29, 0x06, 0x50, 0x53,
    0x08, 0x01, 0xB1, 0x8E, 0xBF, 0xBF, 0x5E, 0x8F, 0xAB, 0x3C, 0x82, 0x87, 0x2A, 0x3E, 0x9B, 0xB7,
};
constexpr std::array<std::uint8_t, 16> kVersionChallenge = {
    0xBA, 0xA3, 0x1E, 0x99, 0xA0, 0x0B, 0x21, 0x57,
    0xFC, 0x37, 0x3F, 0xB3, 0x69, 0xCD, 0xD2, 0xF1,
};

struct AccountRow
{
  std::uint32_t id = 0;
  std::string username;
  std::array<std::uint8_t, 32> verifier{};
  std::array<std::uint8_t, 32> salt{};
};
} // namespace auth
