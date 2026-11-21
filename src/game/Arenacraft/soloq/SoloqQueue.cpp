#include "SoloqQueue.hpp"

#include "Roles.hpp"

#include <algorithm>

namespace arenacraft::soloq
{
bool SoloqQueue::playerAddToQueue(QueuedPlayer player)
{
  std::optional<Role> const role = roleFor(player.classId, player.specIndex);
  if (!role)
    return false;

  if (contains(player.id))
    return false;

  _entries.push_back(QueueEntry{player, *role, std::chrono::milliseconds::zero(), _nextSequence++});
  return true;
}

bool SoloqQueue::playerRemoveFromQueue(PlayerId id)
{
  std::size_t const removed = std::erase_if(_entries, [id](QueueEntry const& entry) { return entry.player.id == id; });
  return removed > 0;
}

std::vector<Match> SoloqQueue::update(std::chrono::milliseconds elapsed)
{
  for (QueueEntry& entry : _entries)
    entry.waited += elapsed;

  std::vector<Match> matches;
  while (std::optional<Match> const match = _matchmaker->findBestMatch(_entries, _tourneyMode, _avoids))
  {
    matches.push_back(*match);

    std::array<QueuedPlayer const*, 6> const six = playersOf(*match);
    std::array<PlayerId, 6>                  ids{};
    for (std::size_t i = 0; i < ids.size(); ++i)
      ids[i] = six[i]->id;

    removePlayers(ids);
  }

  return matches;
}

void SoloqQueue::setMode(SoloqMode mode)
{
  if (_matchmaker->mode() == mode)
    return;

  _matchmaker = makeMatchmaker(mode);
}

void SoloqQueue::clear() { _entries.clear(); }

bool SoloqQueue::canFormMatch() const
{
  return _matchmaker->findBestMatch(_entries, _tourneyMode, _avoids).has_value();
}

std::size_t SoloqQueue::size() const { return _entries.size(); }

bool SoloqQueue::empty() const { return _entries.empty(); }

bool SoloqQueue::contains(PlayerId id) const
{
  return std::any_of(_entries.begin(), _entries.end(), [id](QueueEntry const& entry) { return entry.player.id == id; });
}

std::vector<PlayerId> SoloqQueue::waitingPlayers() const
{
  std::vector<PlayerId> ids;
  ids.reserve(_entries.size());
  for (QueueEntry const& entry : _entries)
    ids.push_back(entry.player.id);
  return ids;
}

std::vector<SoloqQueue::QueueSnapshot> SoloqQueue::snapshot() const
{
  std::vector<QueueSnapshot> result;
  result.reserve(_entries.size());
  for (QueueEntry const& entry : _entries)
    result.push_back(QueueSnapshot{entry.player.id, entry.player.classId, entry.player.specIndex, entry.role,
                                   entry.player.rating, entry.player.mmr, mmrWindowFor(entry, _tourneyMode),
                                   entry.waited});
  return result;
}

void SoloqQueue::removePlayers(std::array<PlayerId, 6> const& ids)
{
  std::erase_if(_entries, [&ids](QueueEntry const& entry)
                { return std::find(ids.begin(), ids.end(), entry.player.id) != ids.end(); });
}
} // namespace arenacraft::soloq
