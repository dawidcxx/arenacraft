#include "SoloqQueue.hpp"

#include <algorithm>

namespace arenacraft::soloq
{
namespace
{
constexpr uint8_t WarriorSpecArms = 0;
constexpr uint8_t WarriorSpecProt = 2;

// Mancleave (Arms warrior + Protection warrior + healer) is the one class stack the matchmaker allows. Constant time,
// so the enumeration cost is unchanged.
bool isMancleavePair(QueuedPlayer const& x, QueuedPlayer const& y)
{
  if (x.classId != CLASS_WARRIOR || y.classId != CLASS_WARRIOR)
    return false;
  return (x.specIndex == WarriorSpecArms && y.specIndex == WarriorSpecProt) ||
         (x.specIndex == WarriorSpecProt && y.specIndex == WarriorSpecArms);
}

bool stacksClass(QueuedPlayer const& x, QueuedPlayer const& y)
{
  return x.classId == y.classId && !isMancleavePair(x, y);
}

// a team is two DPS and one healer; healers are never warriors, so only the DPS pair can be mancleave
bool teamStacksClass(QueuedPlayer const& dps0, QueuedPlayer const& dps1, QueuedPlayer const& healer)
{
  return stacksClass(dps0, dps1) || dps0.classId == healer.classId || dps1.classId == healer.classId;
}
} // namespace

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
    matches.push_back(candidate->match);

    std::array<QueuedPlayer const*, 6> const six = playersOf(candidate->match);
    std::array<PlayerId, 6>                  ids{};
    for (std::size_t i = 0; i < ids.size(); ++i)
      ids[i] = six[i]->id;

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
                                   entry.player.rating, entry.player.mmr, windowFor(entry), entry.waited});
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

void SoloqQueue::fillStats(Candidate& candidate, std::array<Entry const*, 6> const& six)
{
  candidate.spread         = 0;
  candidate.waitTotal      = 0;
  candidate.oldestSequence = six[0]->sequence;

  uint32_t minMmr = six[0]->player.mmr;
  uint32_t maxMmr = six[0]->player.mmr;
  for (Entry const* entry : six)
  {
    minMmr = std::min(minMmr, entry->player.mmr);
    maxMmr = std::max(maxMmr, entry->player.mmr);
    candidate.waitTotal += static_cast<uint64_t>(entry->waited.count());
    candidate.oldestSequence = std::min(candidate.oldestSequence, entry->sequence);
  }
  candidate.spread = maxMmr - minMmr;
}

bool SoloqQueue::betterThan(Candidate const& left, Candidate const& right)
{
  // Queue order comes before fairness: the earliest-queued player is always
  // included when a valid set exists, otherwise long-waiting players could be
  // skipped forever by fairer sets of newer arrivals. Fairness still breaks
  // ties among sets anchored on the same oldest player.
  if (left.oldestSequence != right.oldestSequence)
    return left.oldestSequence < right.oldestSequence;
  if (left.waitTotal != right.waitTotal)
    return left.waitTotal > right.waitTotal;
  if (left.imbalance != right.imbalance)
    return left.imbalance < right.imbalance;
  return left.spread < right.spread;
}

bool SoloqQueue::teamAvoids(QueuedPlayer const& x, QueuedPlayer const& y, QueuedPlayer const& z) const
{
  return _avoids &&
         (_avoids(x.accountId, y.accountId) || _avoids(x.accountId, z.accountId) || _avoids(y.accountId, z.accountId));
}

std::optional<SoloqQueue::Candidate> SoloqQueue::makeCandidate(std::array<Entry const*, 6> const& six) const
{
  // Composition: six[0..3] are DPS (melee or caster), six[4..5] are healers.
  // Each team takes two DPS and one healer. Teams that stack a class are skipped, except Mancleave (Arms + Prot).
  uint64_t total = 0;
  for (Entry const* entry : six)
    total += entry->player.mmr;

  bool        found           = false;
  uint64_t    bestImbalance   = 0;
  std::size_t bestHealer      = 0;
  std::size_t bestDpsA        = 0;
  std::size_t bestDpsB        = 0;
  std::size_t bestComplement0 = 0;
  std::size_t bestComplement1 = 0;

  for (std::size_t healer = 0; healer < 2; ++healer)
  {
    QueuedPlayer const& healerA = six[4 + healer]->player;
    QueuedPlayer const& healerB = six[4 + (1 - healer)]->player;

    for (std::size_t a = 0; a < 4; ++a)
      for (std::size_t b = a + 1; b < 4; ++b)
      {
        std::size_t complement[2];
        std::size_t count = 0;
        for (std::size_t k = 0; k < 4; ++k)
          if (k != a && k != b)
            complement[count++] = k;

        QueuedPlayer const& dpsA0 = six[a]->player;
        QueuedPlayer const& dpsA1 = six[b]->player;
        QueuedPlayer const& dpsB0 = six[complement[0]]->player;
        QueuedPlayer const& dpsB1 = six[complement[1]]->player;

        if (teamStacksClass(dpsA0, dpsA1, healerA) || teamStacksClass(dpsB0, dpsB1, healerB))
          continue;
        if (teamAvoids(dpsA0, dpsA1, healerA) || teamAvoids(dpsB0, dpsB1, healerB))
          continue;

        uint64_t const sumA      = static_cast<uint64_t>(dpsA0.mmr) + dpsA1.mmr + healerA.mmr;
        uint64_t const doubled   = 2 * sumA;
        uint64_t const imbalance = doubled > total ? doubled - total : total - doubled;

        if (!found || imbalance < bestImbalance)
        {
          found           = true;
          bestImbalance   = imbalance;
          bestHealer      = healer;
          bestDpsA        = a;
          bestDpsB        = b;
          bestComplement0 = complement[0];
          bestComplement1 = complement[1];
        }
      }
  }

  if (!found)
    return std::nullopt;

  Candidate candidate{};
  candidate.match.a.melee  = six[bestDpsA]->player;
  candidate.match.a.caster = six[bestDpsB]->player;
  candidate.match.a.healer = six[4 + bestHealer]->player;
  candidate.match.b.melee  = six[bestComplement0]->player;
  candidate.match.b.caster = six[bestComplement1]->player;
  candidate.match.b.healer = six[4 + (1 - bestHealer)]->player;
  candidate.imbalance      = bestImbalance;
  fillStats(candidate, six);
  return candidate;
}

std::optional<SoloqQueue::Candidate> SoloqQueue::findBestMatch() const
{
  std::vector<Entry const*> dps;
  std::vector<Entry const*> healer;
  for (Entry const& entry : _entries)
  {
    if (entry.role == Role::Healer)
      healer.push_back(&entry);
    else
      dps.push_back(&entry);
  }

  if (dps.size() < 4 || healer.size() < 2)
    return std::nullopt;

  std::optional<Candidate> best;
  for (std::size_t i = 0; i < dps.size(); ++i)
    for (std::size_t j = i + 1; j < dps.size(); ++j)
      for (std::size_t k = j + 1; k < dps.size(); ++k)
        for (std::size_t l = k + 1; l < dps.size(); ++l)
          for (std::size_t hi = 0; hi < healer.size(); ++hi)
            for (std::size_t hj = hi + 1; hj < healer.size(); ++hj)
            {
              std::array<Entry const*, 6> const six = {dps[i], dps[j], dps[k], dps[l], healer[hi], healer[hj]};
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
