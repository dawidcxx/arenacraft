#include "SoloqService.hpp"

namespace arenacraft::soloq
{
std::string formatQueueStats(RoleCounts const& counts)
{
  uint16_t const total = counts.melee + counts.caster + counts.healer;

  return std::string("SoloQ Queue\n-----------------------\n\nPlayers in queue: ") + std::to_string(total) +
         "\n\nMelee (" + std::to_string(counts.melee) + ") Caster (" + std::to_string(counts.caster) + ") Healer (" +
         std::to_string(counts.healer) + ")";
}

SoloqService& SoloqService::instance()
{
  static SoloqService service;
  return service;
}

bool SoloqService::join(PlayerId id, Classes classId, uint8_t specIndex, TeamId teamId, uint32_t rating, uint32_t mmr)
{
  bool const added = _queue.playerAddToQueue(QueuedPlayer{id, classId, specIndex, rating, mmr, teamId});
  if (added)
    refreshRoleCounts();
  return added;
}

bool SoloqService::leave(PlayerId id)
{
  bool const removed = _queue.playerRemoveFromQueue(id);
  if (removed)
    refreshRoleCounts();
  return removed;
}

void SoloqService::refreshRoleCounts()
{
  _roleCounts = {};

  for (SoloqQueue::QueueSnapshot const& entry : _queue.snapshot())
  {
    switch (entry.role)
    {
    case Role::Melee:
      ++_roleCounts.melee;
      break;
    case Role::Caster:
      ++_roleCounts.caster;
      break;
    case Role::Healer:
      ++_roleCounts.healer;
      break;
    }
  }
}

std::optional<CharacterProblem> SoloqService::characterProblem(Player* player)
{
  if (!player || _skipCharacterChecks)
    return std::nullopt;

  PlayerId const id = player->GetGUID().GetRawValue();
  if (_validatedCharacters.contains(id))
    return std::nullopt;

  std::optional<CharacterProblem> const problem = checkCharacter(snapshotCharacter(player));
  if (!problem)
    _validatedCharacters.insert(id);

  return problem;
}

std::vector<Match> SoloqService::tick(std::chrono::milliseconds elapsed)
{
  std::vector<Match> matches = _queue.update(elapsed);
  if (!matches.empty())
    refreshRoleCounts();
  return matches;
}

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

std::size_t SoloqService::queueSize() const { return _queue.size(); }

bool SoloqService::inQueue(PlayerId id) const { return _queue.contains(id); }

std::vector<PlayerId> SoloqService::waitingPlayers() const { return _queue.waitingPlayers(); }

std::vector<SoloqQueue::QueueSnapshot> SoloqService::snapshot() const { return _queue.snapshot(); }

void SoloqService::setSkipCharacterChecks(bool value) { _skipCharacterChecks = value; }

std::vector<SoloqService::PendingArena> SoloqService::pendingArenas() const
{
  std::vector<PendingArena> result;
  result.reserve(_pendingMatches.size());
  for (auto const& [bgInstanceId, match] : _pendingMatches)
    result.push_back(PendingArena{bgInstanceId, match});
  return result;
}
} // namespace arenacraft::soloq
