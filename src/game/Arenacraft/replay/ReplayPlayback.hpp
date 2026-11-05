#pragma once

#include "ObjectGuid.h"
#include "ReplayFormat.hpp"

#include <cstdint>
#include <memory>
#include <string>

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
  Playback(ObjectGuid viewer, uint32_t replayId, ReplayData data);
  ~Playback();

  // false once the playback is over and the viewer has left; the arena can
  // then be released.
  bool update(Battleground* bg, uint32_t diff);

  // Viewer controls (world thread). false while the playback has not started or is over.
  bool setPaused(Player* viewer, bool paused);
  bool seek(Player* viewer, uint32_t targetMs);
  // Puts the viewer's camera on recorded player `target`, like Bind Sight does in a live arena. An empty guid, or the
  // player already watched, puts it back on the viewer. false if `target` is no player of the recording.
  bool view(Player* viewer, ObjectGuid target);
  // Sends the addon state message (see ai-doc/replay.md, "Addon API").
  void sendState(Player* viewer) const;

  [[nodiscard]] uint32_t positionMs() const { return _posMs; }
  [[nodiscard]] uint32_t durationMs() const { return _durationMs; }
  [[nodiscard]] bool     paused() const { return _paused; }

private:
  enum class State
  {
    WaitingForViewer,
    Playing,
    Finished, // everything sent, waiting a bit before sending the viewer home
    Done,
  };

  [[nodiscard]] Player* viewerOnMap(Battleground* bg) const;
  [[nodiscard]] bool    controllable() const { return _state == State::Playing || _state == State::Finished; }
  void                  sendPacket(Player* viewer, std::size_t index) const;
  void                  sendJumpPacket(Player* viewer, std::size_t index, uint32_t targetMs) const;
  void                  sendUpTo(Player* viewer, uint32_t limitMs);
  void                  destroyRecordedObjects(Player* viewer) const;
  void                  tickState(Player* viewer, uint32_t diff);

  ObjectGuid            _viewer;
  uint32_t              _replayId;
  ReplayData            _data;
  std::vector<uint64_t> _objects;                  // recorded objects destroyed on a seek back
  bool                  _hasSpectatorData = false; // matches recorded with the spectator addon data
  uint32_t              _durationMs       = 0;     // offset of the last packet
  State                 _state            = State::WaitingForViewer;
  uint32_t              _timer            = 0; // ms in the current state
  uint32_t              _posMs            = 0; // timeline position
  uint32_t              _stateTimer       = 0; // ms since the last addon state message
  std::size_t           _next             = 0; // next packet to send
  bool                  _paused           = false;
  ObjectGuid            _viewed;          // recorded player the camera is on (empty: the viewer)
  uint32_t              _reviewTimer = 0; // ms until the camera goes back on _viewed after a seek back
};

// The playback `viewer` is watching, if any (world thread).
std::shared_ptr<Playback> playbackOf(ObjectGuid viewer);

// Starts watching replay `id` for `viewer`: loads it, opens a private arena and
// moves the viewer in as a spectator. Reports problems to the viewer's chat.
void watch(Player* viewer, uint32_t id);

// `.spect watch <name>` while watching a replay: the camera goes on that recorded player (see Playback::view).
// false if `viewer` watches no replay; problems are reported to the viewer's chat.
bool viewByName(Player* viewer, std::string const& name);

// Why `player` cannot watch a replay right now, or an empty string.
std::string cannotWatchReason(Player* player);
} // namespace arenacraft::replay
