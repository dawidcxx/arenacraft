#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "AuthProtocol.h"

namespace auth
{
class Srp6Server
{
public:
  explicit Srp6Server(const AccountRow& account);

  const std::array<std::uint8_t, 32>& salt() const { return salt_; }
  std::array<std::uint8_t, 32> server_public_b();

  bool verify_client_proof(const std::array<std::uint8_t, 32>& client_A,
                           const std::array<std::uint8_t, 20>& client_M,
                           std::array<std::uint8_t, 20>& out_M2,
                           std::array<std::uint8_t, 40>& out_session_key);

private:
  bool                         used_ = false;
  std::string                  username_upper_;
  std::array<std::uint8_t, 32> salt_{};
  std::array<std::uint8_t, 32> verifier_{};
  std::array<std::uint8_t, 32> private_b_{};
  std::array<std::uint8_t, 32> server_B_{};
};
} // namespace auth
