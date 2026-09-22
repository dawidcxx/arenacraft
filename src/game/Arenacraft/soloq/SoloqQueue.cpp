#include "SoloqQueue.hpp"

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

  _entries.push_back(Entry{player, *role, std::chrono::milliseconds::zero(), _nextSequence++});
  return true;
}

bool SoloqQueue::playerRemoveFromQueue(PlayerId id)
{
  std::size_t const removed = std::erase_if(_entries, [id](Entry const& entry) { return entry.player.id == id; });
  return removed > 0;
}

std::vector<Match> SoloqQueue::update(std::chrono::milliseconds elapsed)
{
  for (Entry& entry : _entries)
    entry.waited += elapsed;

  std::vector<Match> matches;
  while (std::optional<Candidate> const candidate = findBestMatch())
  {
    matches.push_back(buildMatch(*candidate));

    std::array<PlayerId, 6> ids{};
    for (std::size_t i = 0; i < ids.size(); ++i)
      ids[i] = candidate->players[i].id;

    removePlayers(ids);
  }

  return matches;
}

std::size_t SoloqQueue::size() const { return _entries.size(); }

bool SoloqQueue::empty() const { return _entries.empty(); }

bool SoloqQueue::contains(PlayerId id) const
{
  return std::any_of(_entries.begin(), _entries.end(), [id](Entry const& entry) { return entry.player.id == id; });
}

std::vector<PlayerId> SoloqQueue::waitingPlayers() const
{
  std::vector<PlayerId> ids;
  ids.reserve(_entries.size());
  for (Entry const& entry : _entries)
    ids.push_back(entry.player.id);
  return ids;
}

std::vector<SoloqQueue::QueueSnapshot> SoloqQueue::snapshot() const
{
  std::vector<QueueSnapshot> result;
  result.reserve(_entries.size());
  for (Entry const& entry : _entries)
    result.push_back(QueueSnapshot{entry.player.id, entry.player.classId, entry.player.specIndex, entry.role,
                                   entry.player.rating, entry.player.mmr, entry.waited});
  return result;
}

uint32_t SoloqQueue::windowFor(Entry const& entry)
{
  uint64_t const steps =
      static_cast<uint64_t>(entry.waited.count()) / static_cast<uint64_t>(tuning::StepIntervalMs.count());
  uint64_t const window = static_cast<uint64_t>(tuning::InitialWindow) + steps * static_cast<uint64_t>(tuning::MmrStep);
  return static_cast<uint32_t>(std::min<uint64_t>(window, tuning::MaxWindow));
}

bool SoloqQueue::compatible(Entry const& left, Entry const& right)
{
  uint32_t const gap =
      left.player.mmr > right.player.mmr ? left.player.mmr - right.player.mmr : right.player.mmr - left.player.mmr;
  return gap <= std::max(windowFor(left), windowFor(right));
}

bool SoloqQueue::allCompatible(std::array<Entry const*, 6> const& six) const
{
  for (std::size_t i = 0; i < six.size(); ++i)
    for (std::size_t j = i + 1; j < six.size(); ++j)
      if (!compatible(*six[i], *six[j]))
        return false;

  return true;
}

std::optional<SoloqQueue::Candidate> SoloqQueue::makeCandidate(std::array<Entry const*, 6> const& six) const
{
  Candidate candidate{};
  for (std::size_t i = 0; i < six.size(); ++i)
    candidate.players[i] = six[i]->player;

  QueuedPlayer const& m0 = candidate.players[0];
  QueuedPlayer const& m1 = candidate.players[1];
  QueuedPlayer const& c0 = candidate.players[2];
  QueuedPlayer const& c1 = candidate.players[3];
  QueuedPlayer const& h0 = candidate.players[4];
  QueuedPlayer const& h1 = candidate.players[5];

  uint64_t const total = static_cast<uint64_t>(m0.mmr) + m1.mmr + c0.mmr + c1.mmr + h0.mmr + h1.mmr;

  // partition p: team A = {m0, caster[p], healer[p]}, team B takes the rest.
  // Only partitions where neither team stacks a class are eligible.
  bool        found         = false;
  uint64_t    bestImbalance = 0;
  std::size_t bestPartition = 0;
  for (std::size_t p = 0; p < 4; ++p)
  {
    QueuedPlayer const& casterA = (p == 0 || p == 1) ? c0 : c1;
    QueuedPlayer const& healerA = (p == 0 || p == 2) ? h0 : h1;
    QueuedPlayer const& casterB = (p == 0 || p == 1) ? c1 : c0;
    QueuedPlayer const& healerB = (p == 0 || p == 2) ? h1 : h0;

    if (m0.classId == casterA.classId || m0.classId == healerA.classId || casterA.classId == healerA.classId)
      continue;
    if (m1.classId == casterB.classId || m1.classId == healerB.classId || casterB.classId == healerB.classId)
      continue;

    uint64_t const sumA      = static_cast<uint64_t>(m0.mmr) + casterA.mmr + healerA.mmr;
    uint64_t const doubled   = 2 * sumA;
    uint64_t const imbalance = doubled > total ? doubled - total : total - doubled;

    if (!found || imbalance < bestImbalance)
    {
      found         = true;
      bestImbalance = imbalance;
      bestPartition = p;
    }
  }

  if (!found)
    return std::nullopt;

  candidate.partition   = bestPartition;
  candidate.imbalance   = bestImbalance;
  candidate.spread      = 0;
  candidate.waitTotal   = 0;
  candidate.minSequence = six[0]->sequence;

  uint32_t minMmr = six[0]->player.mmr;
  uint32_t maxMmr = six[0]->player.mmr;
  for (std::size_t i = 0; i < six.size(); ++i)
  {
    minMmr = std::min(minMmr, six[i]->player.mmr);
    maxMmr = std::max(maxMmr, six[i]->player.mmr);
    candidate.waitTotal += static_cast<uint64_t>(six[i]->waited.count());
    candidate.minSequence = std::min(candidate.minSequence, six[i]->sequence);
  }
  candidate.spread = maxMmr - minMmr;

  return candidate;
}

Match SoloqQueue::buildMatch(Candidate const& candidate)
{
  bool const casterFirstInA = candidate.partition == 0 || candidate.partition == 1;
  bool const healerFirstInA = candidate.partition == 0 || candidate.partition == 2;

  Match match;
  match.a.melee  = candidate.players[0];
  match.a.caster = candidate.players[casterFirstInA ? 2 : 3];
  match.a.healer = candidate.players[healerFirstInA ? 4 : 5];
  match.b.melee  = candidate.players[1];
  match.b.caster = candidate.players[casterFirstInA ? 3 : 2];
  match.b.healer = candidate.players[healerFirstInA ? 5 : 4];
  return match;
}

std::optional<SoloqQueue::Candidate> SoloqQueue::findBestMatch() const
{
  std::vector<Entry const*> melee;
  std::vector<Entry const*> caster;
  std::vector<Entry const*> healer;
  for (Entry const& entry : _entries)
  {
    switch (entry.role)
    {
    case Role::Melee:
      melee.push_back(&entry);
      break;
    case Role::Caster:
      caster.push_back(&entry);
      break;
    case Role::Healer:
      healer.push_back(&entry);
      break;
    }
  }

  if (melee.size() < 2 || caster.size() < 2 || healer.size() < 2)
    return std::nullopt;

  auto const betterThan = [](Candidate const& left, Candidate const& right)
  {
    if (left.imbalance != right.imbalance)
      return left.imbalance < right.imbalance;
    if (left.spread != right.spread)
      return left.spread < right.spread;
    if (left.waitTotal != right.waitTotal)
      return left.waitTotal > right.waitTotal;
    return left.minSequence < right.minSequence;
  };

  std::optional<Candidate> best;
  for (std::size_t mi = 0; mi < melee.size(); ++mi)
    for (std::size_t mj = mi + 1; mj < melee.size(); ++mj)
      for (std::size_t ci = 0; ci < caster.size(); ++ci)
        for (std::size_t cj = ci + 1; cj < caster.size(); ++cj)
          for (std::size_t hi = 0; hi < healer.size(); ++hi)
            for (std::size_t hj = hi + 1; hj < healer.size(); ++hj)
            {
              std::array<Entry const*, 6> const six = {melee[mi],  melee[mj],  caster[ci],
                                                       caster[cj], healer[hi], healer[hj]};
              if (!allCompatible(six))
                continue;

              std::optional<Candidate> const candidate = makeCandidate(six);
              if (candidate && (!best || betterThan(*candidate, *best)))
                best = candidate;
            }

  return best;
}

void SoloqQueue::removePlayers(std::array<PlayerId, 6> const& ids)
{
  std::erase_if(_entries,
                [&ids](Entry const& entry) { return std::find(ids.begin(), ids.end(), entry.player.id) != ids.end(); });
}
} // namespace arenacraft::soloq
