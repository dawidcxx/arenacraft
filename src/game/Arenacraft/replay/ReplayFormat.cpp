#include "ReplayFormat.hpp"

#include <cstring>
#include <zlib.h>

namespace arenacraft::replay
{
namespace
{
constexpr uint32_t RawMagic        = 0x31524341; // "ACR1"
constexpr uint32_t CompressedMagic = 0x5A524341; // "ACRZ"
// refuse to inflate anything absurd (corrupt row)
constexpr uint32_t MaxRawSize = 512u * 1024u * 1024u;

class Writer
{
public:
  void u8(uint8_t v) { _out.push_back(v); }
  void u16(uint16_t v) { put(&v, sizeof(v)); }
  void u32(uint32_t v) { put(&v, sizeof(v)); }
  void f32(float v) { put(&v, sizeof(v)); }
  void bytes(std::vector<uint8_t> const& v) { _out.insert(_out.end(), v.begin(), v.end()); }

  std::vector<uint8_t>& out() { return _out; }

private:
  // little endian hosts only (x86_64 / aarch64), same as the world protocol
  void put(void const* p, std::size_t n)
  {
    auto const* b = static_cast<uint8_t const*>(p);
    _out.insert(_out.end(), b, b + n);
  }

  std::vector<uint8_t> _out;
};

class Reader
{
public:
  Reader(uint8_t const* data, std::size_t size) : _data(data), _size(size) {}

  bool u16(uint16_t& v) { return get(&v, sizeof(v)); }
  bool u32(uint32_t& v) { return get(&v, sizeof(v)); }
  bool f32(float& v) { return get(&v, sizeof(v)); }
  bool bytes(std::vector<uint8_t>& v, uint32_t n)
  {
    if (_size - _pos < n)
      return false;
    v.assign(_data + _pos, _data + _pos + n);
    _pos += n;
    return true;
  }
  [[nodiscard]] bool atEnd() const { return _pos == _size; }

private:
  bool get(void* p, std::size_t n)
  {
    if (_size - _pos < n)
      return false;
    std::memcpy(p, _data + _pos, n);
    _pos += n;
    return true;
  }

  uint8_t const* _data;
  std::size_t    _size;
  std::size_t    _pos = 0;
};
} // namespace

std::vector<uint8_t> encode(ReplayData const& replay)
{
  Writer raw;
  raw.u32(RawMagic);
  raw.u32(replay.recordStartMs);
  raw.u32(replay.mapId);
  raw.f32(replay.x);
  raw.f32(replay.y);
  raw.f32(replay.z);
  raw.f32(replay.o);
  raw.u32(static_cast<uint32_t>(replay.packets.size()));
  for (RecordedPacket const& p : replay.packets)
  {
    raw.u32(p.offsetMs);
    raw.u16(p.opcode);
    raw.u32(static_cast<uint32_t>(p.data.size()));
    raw.bytes(p.data);
  }

  std::vector<uint8_t> const& src = raw.out();
  uLongf                      len = compressBound(src.size());

  Writer out;
  out.u32(CompressedMagic);
  out.u32(static_cast<uint32_t>(src.size()));
  std::vector<uint8_t>& buf    = out.out();
  std::size_t const     header = buf.size();
  buf.resize(header + len);
  if (compress2(buf.data() + header, &len, src.data(), src.size(), Z_BEST_SPEED) != Z_OK)
    return {};
  buf.resize(header + len);
  return std::move(buf);
}

std::optional<ReplayData> decode(std::vector<uint8_t> const& blob)
{
  Reader   head(blob.data(), blob.size());
  uint32_t magic = 0, rawSize = 0;
  if (!head.u32(magic) || magic != CompressedMagic || !head.u32(rawSize) || rawSize > MaxRawSize)
    return std::nullopt;

  std::vector<uint8_t> raw(rawSize);
  uLongf               len = rawSize;
  if (uncompress(raw.data(), &len, blob.data() + 8, blob.size() - 8) != Z_OK || len != rawSize)
    return std::nullopt;

  Reader     r(raw.data(), raw.size());
  ReplayData replay;
  uint32_t   count = 0;
  if (!r.u32(magic) || magic != RawMagic || !r.u32(replay.recordStartMs) || !r.u32(replay.mapId) || !r.f32(replay.x) ||
      !r.f32(replay.y) || !r.f32(replay.z) || !r.f32(replay.o) || !r.u32(count))
    return std::nullopt;

  replay.packets.reserve(count);
  for (uint32_t i = 0; i < count; ++i)
  {
    RecordedPacket p;
    uint32_t       size = 0;
    if (!r.u32(p.offsetMs) || !r.u16(p.opcode) || !r.u32(size) || !r.bytes(p.data, size))
      return std::nullopt;
    replay.packets.push_back(std::move(p));
  }
  if (!r.atEnd())
    return std::nullopt;
  return replay;
}

std::optional<uint64_t> readPackedGuid(std::vector<uint8_t> const& data, std::size_t& pos)
{
  if (pos >= data.size())
    return std::nullopt;
  uint8_t const mask = data[pos++];
  uint64_t      guid = 0;
  for (int i = 0; i < 8; ++i)
  {
    if (!(mask & (1u << i)))
      continue;
    if (pos >= data.size())
      return std::nullopt;
    guid |= uint64_t(data[pos++]) << (i * 8);
  }
  return guid;
}

bool shiftMovementTime(std::vector<uint8_t>& data, uint32_t delta)
{
  std::size_t pos = 0;
  if (!readPackedGuid(data, pos))
    return false;
  pos += 4 + 2; // flags, flags2
  if (data.size() < pos + 4)
    return false;

  uint32_t time = 0;
  std::memcpy(&time, data.data() + pos, 4);
  time += delta;
  std::memcpy(data.data() + pos, &time, 4);
  return true;
}
} // namespace arenacraft::replay
