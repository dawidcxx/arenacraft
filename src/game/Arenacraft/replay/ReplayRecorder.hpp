#pragma once

#include "ReplayConfig.hpp"
#include "ReplayFormat.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Battleground;
class Player;
class WorldSession;

namespace arenacraft::replay
{
// Packet capture for one arena match.
//
// A match is seen through one participant (the "primary"): every packet that
// describes the world (object updates, movement of others, spells, auras,
// combat logs) is copied from the stream the server sends to that player. The
// one thing that stream lacks is the primary's own movement (the server never
// echoes a player's movement back to them), which is taken from a second
// participant's stream (the "secondary"), filtered to the primary's guid.
//
// Fed from WorldSession::SendPacket, which runs on map threads and the world
// thread alike, hence the lock.
class Recording
{
public:
  Recording(uint32_t startMs, uint64_t primaryGuid) : _startMs(startMs), _primaryGuid(primaryGuid) {}

  void onPacket(bool fromPrimary, uint16_t opcode, uint8_t const* data, std::size_t size);
  // snapshot packets taken when recording starts, at offset 0
  void addInitial(uint16_t opcode, uint8_t const* data, std::size_t size);

  // Stops accepting packets and returns everything captured so far.
  std::vector<RecordedPacket> finish();

  [[nodiscard]] uint32_t startMs() const { return _startMs; }

private:
  void append(uint32_t offset, uint16_t opcode, uint8_t const* data, std::size_t size);

  uint32_t const              _startMs;
  uint64_t const              _primaryGuid;
  std::mutex                  _lock;
  std::vector<RecordedPacket> _packets;
  std::size_t                 _bytes    = 0;
  bool                        _finished = false;
};

// Opcode filters, exposed for tests.
[[nodiscard]] bool isObserverOpcode(uint16_t opcode);
[[nodiscard]] bool isMovementOpcode(uint16_t opcode);

struct Participant
{
  uint32_t    guid = 0;
  std::string name;
  uint8_t     team    = 0; // TeamId
  uint8_t     classId = 0;
};

class ReplayRecorder
{
public:
  static ReplayRecorder& instance();

  // World thread, once at startup: id counter + age based cleanup.
  void init();

  // Battleground hooks (see Battleground.cpp / Arena.cpp).
  void onGatesOpen(Battleground* bg);
  void onPlayerLeave(Battleground* bg, Player* player);
  void onArenaEnd(Battleground* bg, uint8_t winnerTeam);

  // Classifies an arena for the bracket filter.
  [[nodiscard]] static Bracket bracketOf(Battleground const* bg);

private:
  ReplayRecorder() = default;

  struct Active
  {
    std::shared_ptr<Recording> recording;
    Bracket                    bracket{};
    uint32_t                   mapId     = 0;
    uint32_t                   bgTypeId  = 0;
    uint32_t                   primary   = 0;
    uint32_t                   secondary = 0;
    float                      x = 0, y = 0, z = 0, o = 0;
    std::vector<Participant>   participants;
  };

  void save(Active active, uint8_t winnerTeam, uint32_t durationMs);

  std::mutex                           _lock;
  std::unordered_map<uint32_t, Active> _active; // by bg instance id
  std::atomic<uint32_t>                _lastId{0};
};
} // namespace arenacraft::replay
