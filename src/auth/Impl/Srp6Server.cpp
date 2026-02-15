#include "Srp6Server.h"

#include <algorithm>
#include <vector>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "AuthProtocol.h"

namespace auth
{
namespace
{
std::string to_upper_ascii(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                 {
                   if (c >= 'a' && c <= 'z')
                     return static_cast<char>(c - ('a' - 'A'));
                   return static_cast<char>(c);
                 });
  return s;
}

std::array<std::uint8_t, SHA_DIGEST_LENGTH> sha1_of(const std::vector<std::uint8_t>& data)
{
  std::array<std::uint8_t, SHA_DIGEST_LENGTH> out{};
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
  EVP_DigestUpdate(ctx, data.data(), data.size());
  unsigned int out_len = 0;
  EVP_DigestFinal_ex(ctx, out.data(), &out_len);
  EVP_MD_CTX_free(ctx);
  return out;
}

template <typename... Chunks>
std::array<std::uint8_t, SHA_DIGEST_LENGTH> sha1_concat(const Chunks&... chunks)
{
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
  (EVP_DigestUpdate(ctx, chunks.data(), chunks.size()), ...);
  std::array<std::uint8_t, SHA_DIGEST_LENGTH> out{};
  unsigned int out_len = 0;
  EVP_DigestFinal_ex(ctx, out.data(), &out_len);
  EVP_MD_CTX_free(ctx);
  return out;
}

template <std::size_t N>
BIGNUM* bn_from_le(const std::array<std::uint8_t, N>& le)
{
  std::array<std::uint8_t, N> be{};
  std::reverse_copy(le.begin(), le.end(), be.begin());
  return BN_bin2bn(be.data(), static_cast<int>(be.size()), nullptr);
}

template <std::size_t N>
std::array<std::uint8_t, N> bn_to_le(const BIGNUM* bn)
{
  std::array<std::uint8_t, N> be{};
  BN_bn2binpad(bn, be.data(), static_cast<int>(N));
  std::array<std::uint8_t, N> le{};
  std::reverse_copy(be.begin(), be.end(), le.begin());
  return le;
}

std::array<std::uint8_t, 40> interleaved_session_key(const std::array<std::uint8_t, 32>& s)
{
  std::array<std::uint8_t, 16> even{};
  std::array<std::uint8_t, 16> odd{};
  for (std::size_t i = 0; i < 16; ++i)
  {
    even[i] = s[2 * i + 0];
    odd[i]  = s[2 * i + 1];
  }

  std::size_t p = 0;
  while (p < s.size() && s[p] == 0)
    ++p;
  if ((p & 1U) != 0)
    ++p;
  p /= 2;

  std::vector<std::uint8_t> even_tail(even.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(p, even.size())),
                                      even.end());
  std::vector<std::uint8_t> odd_tail(odd.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(p, odd.size())), odd.end());
  auto hash_even = sha1_of(even_tail);
  auto hash_odd  = sha1_of(odd_tail);

  std::array<std::uint8_t, 40> k{};
  for (std::size_t i = 0; i < SHA_DIGEST_LENGTH; ++i)
  {
    k[2 * i + 0] = hash_even[i];
    k[2 * i + 1] = hash_odd[i];
  }

  return k;
}
} // namespace

Srp6Server::Srp6Server(const AccountRow& account)
    : username_upper_(to_upper_ascii(account.username)), salt_(account.salt), verifier_(account.verifier)
{
  RAND_bytes(private_b_.data(), static_cast<int>(private_b_.size()));
}

std::array<std::uint8_t, 32> Srp6Server::server_public_b()
{
  BIGNUM* bn_N = bn_from_le(kModulus);
  BIGNUM* bn_g = bn_from_le(kGenerator);
  BIGNUM* bn_v = bn_from_le(verifier_);
  BIGNUM* bn_b = bn_from_le(private_b_);

  BN_CTX* ctx    = BN_CTX_new();
  BIGNUM* g_pow_b = BN_new();
  BIGNUM* kv      = BN_new();
  BIGNUM* B       = BN_new();

  BN_mod_exp(g_pow_b, bn_g, bn_b, bn_N, ctx);
  BN_copy(kv, bn_v);
  BN_mul_word(kv, 3);
  BN_mod(kv, kv, bn_N, ctx);
  BN_mod_add(B, g_pow_b, kv, bn_N, ctx);

  server_B_ = bn_to_le<32>(B);

  BN_free(B);
  BN_free(kv);
  BN_free(g_pow_b);
  BN_CTX_free(ctx);
  BN_free(bn_b);
  BN_free(bn_v);
  BN_free(bn_g);
  BN_free(bn_N);

  return server_B_;
}

bool Srp6Server::verify_client_proof(const std::array<std::uint8_t, 32>& client_A,
                                     const std::array<std::uint8_t, 20>& client_M,
                                     std::array<std::uint8_t, 20>& out_M2,
                                     std::array<std::uint8_t, 40>& out_session_key)
{
  if (used_)
    return false;
  used_ = true;

  BIGNUM* bn_N = bn_from_le(kModulus);
  BIGNUM* bn_A = bn_from_le(client_A);
  BIGNUM* bn_v = bn_from_le(verifier_);
  BIGNUM* bn_b = bn_from_le(private_b_);
  BN_CTX* ctx  = BN_CTX_new();

  BIGNUM* A_mod_N = BN_new();
  BN_mod(A_mod_N, bn_A, bn_N, ctx);
  if (BN_is_zero(A_mod_N))
  {
    BN_free(A_mod_N);
    BN_CTX_free(ctx);
    BN_free(bn_b);
    BN_free(bn_v);
    BN_free(bn_A);
    BN_free(bn_N);
    return false;
  }

  const auto u_digest = sha1_concat(client_A, server_B_);
  BIGNUM* bn_u = bn_from_le(u_digest);

  BIGNUM* vu  = BN_new();
  BIGNUM* avu = BN_new();
  BIGNUM* S   = BN_new();

  BN_mod_exp(vu, bn_v, bn_u, bn_N, ctx);
  BN_mod_mul(avu, bn_A, vu, bn_N, ctx);
  BN_mod_exp(S, avu, bn_b, bn_N, ctx);

  const auto S_bytes = bn_to_le<32>(S);
  out_session_key    = interleaved_session_key(S_bytes);

  const auto hash_N = sha1_of(std::vector<std::uint8_t>(kModulus.begin(), kModulus.end()));
  const auto hash_g = sha1_of(std::vector<std::uint8_t>(kGenerator.begin(), kGenerator.end()));
  std::array<std::uint8_t, 20> ng_xor{};
  for (std::size_t i = 0; i < ng_xor.size(); ++i)
    ng_xor[i] = hash_N[i] ^ hash_g[i];

  const auto hash_I = sha1_of(std::vector<std::uint8_t>(username_upper_.begin(), username_upper_.end()));

  const auto expected_M = sha1_concat(ng_xor, hash_I, salt_, client_A, server_B_, out_session_key);
  if (expected_M != client_M)
  {
    BN_free(S);
    BN_free(avu);
    BN_free(vu);
    BN_free(bn_u);
    BN_free(A_mod_N);
    BN_CTX_free(ctx);
    BN_free(bn_b);
    BN_free(bn_v);
    BN_free(bn_A);
    BN_free(bn_N);
    return false;
  }

  out_M2 = sha1_concat(client_A, client_M, out_session_key);

  BN_free(S);
  BN_free(avu);
  BN_free(vu);
  BN_free(bn_u);
  BN_free(A_mod_N);
  BN_CTX_free(ctx);
  BN_free(bn_b);
  BN_free(bn_v);
  BN_free(bn_A);
  BN_free(bn_N);
  return true;
}
} // namespace auth
