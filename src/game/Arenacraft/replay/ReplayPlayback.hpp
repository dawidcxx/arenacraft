#pragma once

#include "ObjectGuid.h"
#include "ReplayFormat.hpp"

#include <cstdint>

class Battleground;
class Player;

namespace arenacraft::replay
{
// Plays a recorded match to one viewer. The viewer sits as an invisible
// spectator in a private, never-started copy of the arena; the recorded
// packets are replayed to their client on the original timeline, so the
// client renders the match as if it were live. The server never instantiates
// the recorded players - they exist only on the viewer's client.
//
// Driven from Arena::PreUpdateImpl of that private arena (world thread).
class Playback
{
public:
  Playback(ObjectGuid viewer, uint32_t replayId, ReplayData data)
      : _viewer(viewer), _replayId(replayId), _data(std::move(data))
  {
  }

  // false once the playback is over and the viewer has left; the arena can
  // then be released.
  bool update(Battleground* bg, uint32_t diff);

private:
  enum class State
  {
    WaitingForViewer,
    Playing,
    Finished, // everything sent, waiting a bit before sending the viewer home
    Done,
  };

  [[nodiscard]] Player* viewerOnMap(Battleground* bg) const;
  void                  sendDue(Player* viewer);

  ObjectGuid  _viewer;
  uint32_t    _replayId;
  ReplayData  _data;
  State       _state   = State::WaitingForViewer;
  uint32_t    _timer   = 0; // ms in the current state
  uint32_t    _startMs = 0; // getMSTime() when playback started
  std::size_t _next    = 0; // next packet to send
};

// Starts watching replay `id` for `viewer`: loads it, opens a private arena and
// moves the viewer in as a spectator. Reports problems to the viewer's chat.
void watch(Player* viewer, uint32_t id);
} // namespace arenacraft::replay
