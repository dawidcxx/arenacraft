#include "SoloqService.hpp"

namespace arenacraft::soloq
{
SoloqService& SoloqService::instance()
{
  static SoloqService service;
  return service;
}

bool SoloqService::join(PlayerId id, Classes classId, uint8_t specIndex, TeamId teamId, uint32_t rating, uint32_t mmr)
{
  return _queue.playerAddToQueue(QueuedPlayer{id, classId, specIndex, rating, mmr, teamId});
}

bool SoloqService::leave(PlayerId id) { return _queue.playerRemoveFromQueue(id); }

std::vector<Match> SoloqService::tick(std::chrono::milliseconds elapsed) { return _queue.update(elapsed); }

void SoloqService::registerMatch(uint32 bgInstanceId, Match const& match) { _pendingMatches[bgInstanceId] = match; }

std::optional<Match> SoloqService::takeMatch(uint32 bgInstanceId)
{
  auto const it = _pendingMatches.find(bgInstanceId);
  if (it == _pendingMatches.end())
    return std::nullopt;

  Match const match = it->second;
  _pendingMatches.erase(it);
  return match;
}

void SoloqService::forgetMatch(uint32 bgInstanceId) { _pendingMatches.erase(bgInstanceId); }

std::size_t SoloqService::queueSize() const { return _queue.size(); }

bool SoloqService::inQueue(PlayerId id) const { return _queue.contains(id); }

std::vector<PlayerId> SoloqService::waitingPlayers() const { return _queue.waitingPlayers(); }

std::vector<SoloqQueue::QueueSnapshot> SoloqService::snapshot() const { return _queue.snapshot(); }

void SoloqService::setEnforceTeamFaction(bool value) { _queue.setEnforceTeamFaction(value); }

bool SoloqService::enforceTeamFaction() const { return _queue.enforceTeamFaction(); }

std::vector<SoloqService::PendingArena> SoloqService::pendingArenas() const
{
  std::vector<PendingArena> result;
  result.reserve(_pendingMatches.size());
  for (auto const& [bgInstanceId, match] : _pendingMatches)
    result.push_back(PendingArena{bgInstanceId, match});
  return result;
}
} // namespace arenacraft::soloq
