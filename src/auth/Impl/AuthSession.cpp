#include "AuthSession.h"

#include <cstring>

#include <boost/cobalt/op.hpp>

#include "AuthProtocol.h"
#include "ByteWriter.h"
#include "Logging.h"
#include "MySqlAsync.h"
#include "Srp6Server.h"

namespace auth
{
AuthSession::AuthSession(tcp::socket socket, std::shared_ptr<MySqlAsync> db, std::shared_ptr<AppConfig> cfg)
    : socket_(std::move(socket)), db_(std::move(db)), cfg_(std::move(cfg))
{
}

boost::cobalt::task<void> AuthSession::run()
{
  try
  {
    for (;;)
    {
      auto cmd = co_await read_u8();

      switch (cmd)
      {
        case AUTH_LOGON_CHALLENGE:
          co_await handle_logon_challenge();
          break;
        case AUTH_LOGON_PROOF:
          co_await handle_logon_proof();
          break;
        case REALM_LIST:
          co_await handle_realm_list();
          break;
        default:
          co_return;
      }
    }
  }
  catch (const std::exception& ex)
  {
    logging::write(logging::Level::error, std::string("auth session closed: ") + ex.what());
  }
}

boost::cobalt::task<std::uint8_t> AuthSession::read_u8()
{
  std::array<std::uint8_t, 1> b{};
  co_await asio::async_read(socket_, asio::buffer(b), boost::cobalt::use_op);
  co_return b[0];
}

boost::cobalt::task<std::vector<std::uint8_t>> AuthSession::read_exact(std::size_t n)
{
  std::vector<std::uint8_t> out(n);
  if (n > 0)
    co_await asio::async_read(socket_, asio::buffer(out), boost::cobalt::use_op);
  co_return out;
}

boost::cobalt::task<void> AuthSession::write_packet(const std::vector<std::uint8_t>& packet)
{
  co_await asio::async_write(socket_, asio::buffer(packet), boost::cobalt::use_op);
}

boost::cobalt::task<void> AuthSession::send_auth_error(std::uint8_t cmd, std::uint8_t code)
{
  ByteWriter w;
  w.u8(cmd);
  if (cmd == AUTH_LOGON_CHALLENGE)
    w.u8(0x00);
  w.u8(code);
  co_await write_packet(w.bytes);
}

boost::cobalt::task<void> AuthSession::handle_logon_challenge()
{
  logging::write(logging::Level::info, "Received logon challenge");
  const auto hdr          = co_await read_exact(3);
  const auto payload_size = static_cast<std::uint16_t>(hdr[1] | (static_cast<std::uint16_t>(hdr[2]) << 8));
  const auto payload      = co_await read_exact(payload_size);

  if (payload.size() < 30)
  {
    co_await send_auth_error(AUTH_LOGON_CHALLENGE, WOW_FAIL_FAIL_NOACCESS);
    co_return;
  }

  const auto build = static_cast<std::uint16_t>(payload[7] | (static_cast<std::uint16_t>(payload[8]) << 8));
  const auto login_len = payload[29];
  if (payload.size() < static_cast<std::size_t>(30 + login_len))
  {
    co_await send_auth_error(AUTH_LOGON_CHALLENGE, WOW_FAIL_FAIL_NOACCESS);
    co_return;
  }

  login_ = std::string(reinterpret_cast<const char*>(payload.data() + 30), login_len);

  if (build != WOTLK_335A_BUILD)
  {
    co_await send_auth_error(AUTH_LOGON_CHALLENGE, WOW_FAIL_VERSION_INVALID);
    co_return;
  }

  auto account = co_await db_->async_fetch_account(login_, boost::cobalt::use_op);
  if (!account)
  {
    co_await send_auth_error(AUTH_LOGON_CHALLENGE, WOW_FAIL_UNKNOWN_ACCOUNT);
    co_return;
  }

  account_ = *account;
  srp_.emplace(*account_);
  auto B = srp_->server_public_b();

  ByteWriter w;
  w.u8(AUTH_LOGON_CHALLENGE);
  w.u8(0x00);
  w.u8(WOW_SUCCESS);
  w.append(B);
  w.u8(1);
  w.append(kGenerator);
  w.u8(32);
  w.append(kModulus);
  w.append(srp_->salt());
  w.append(kVersionChallenge);
  w.u8(0x00);

  co_await write_packet(w.bytes);
}

boost::cobalt::task<void> AuthSession::handle_logon_proof()
{
  const auto body = co_await read_exact(74);
  if (!srp_ || !account_)
  {
    co_await send_auth_error(AUTH_LOGON_PROOF, WOW_FAIL_UNKNOWN_ACCOUNT);
    co_return;
  }

  std::array<std::uint8_t, 32> A{};
  std::array<std::uint8_t, 20> M1{};
  std::memcpy(A.data(), body.data(), A.size());
  std::memcpy(M1.data(), body.data() + 32, M1.size());

  std::array<std::uint8_t, 20> M2{};
  std::array<std::uint8_t, 40> K{};
  if (!srp_->verify_client_proof(A, M1, M2, K))
  {
    co_await send_auth_error(AUTH_LOGON_PROOF, WOW_FAIL_INCORRECT_PASSWORD);
    co_return;
  }

  boost::system::error_code ec;
  const auto                remote    = socket_.remote_endpoint(ec);
  const std::string         remote_ip = ec ? std::string("0.0.0.0") : remote.address().to_string();
  (void)co_await db_->async_update_session_key(account_->id, K, remote_ip, boost::cobalt::use_op);

  ByteWriter w;
  w.u8(AUTH_LOGON_PROOF);
  w.u8(WOW_SUCCESS);
  w.append(M2);
  w.u32(0x00800000);
  w.u32(0x00000000);
  w.u16(0x0000);
  co_await write_packet(w.bytes);

  authed_ = true;
}

boost::cobalt::task<void> AuthSession::handle_realm_list()
{
  (void)co_await read_exact(4);

  if (!authed_)
  {
    co_await send_auth_error(REALM_LIST, WOW_FAIL_FAIL_NOACCESS);
    co_return;
  }

  ByteWriter realms;
  realms.u8(cfg_->realm_type);
  realms.u8(0);
  realms.u8(cfg_->realm_flags);
  realms.cstr(cfg_->realm_name);
  realms.cstr(cfg_->realm_address);
  realms.f32(cfg_->realm_pop);
  realms.u8(0);
  realms.u8(cfg_->realm_tz);
  realms.u8(cfg_->realm_id);
  realms.u8(0x10);
  realms.u8(0x00);

  ByteWriter list_size;
  list_size.u32(0);
  list_size.u16(1);

  ByteWriter out;
  out.u8(REALM_LIST);
  out.u16(static_cast<std::uint16_t>(list_size.bytes.size() + realms.bytes.size()));
  out.append(list_size.bytes);
  out.append(realms.bytes);

  co_await write_packet(out.bytes);
}
} // namespace auth
