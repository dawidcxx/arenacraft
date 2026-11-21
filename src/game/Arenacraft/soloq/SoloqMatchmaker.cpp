#include "SoloqMatchmaker.hpp"

#include <algorithm>
#include <cstddef>

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

// ASCII-only comparison, so mode parsing does not depend on the locale.
bool equalsIgnoreCase(std::string_view value, std::string_view expected)
{
  if (value.size() != expected.size())
    return false;

  for (std::size_t i = 0; i < value.size(); ++i)
  {
    char c = value[i];
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
    if (c != expected[i])
      return false;
  }
  return true;
}

// Flex: each team takes any two DPS (melee or caster) plus one healer. six[0..3]
// are the DPS, six[4..5] the healers. Teams that stack a class are skipped,
// except Mancleave (Arms + Prot).
class FlexMatchmaker final : public SoloqMatchmaker
{
public:
  [[nodiscard]] SoloqMode mode() const override { return SoloqMode::Flex; }

  [[nodiscard]] bool requirementsMet(RoleCounts const& counts) const override
  {
    return counts.melee + counts.caster >= 4 && counts.healer >= 2;
  }

  [[nodiscard]] std::optional<Match> findBestMatch(std::vector<QueueEntry> const& entries, bool tourneyMode,
                                                   AvoidCheck const& avoids) const override
  {
    std::vector<QueueEntry const*> dps;
    std::vector<QueueEntry const*> healer;
    for (QueueEntry const& entry : entries)
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
                std::array<QueueEntry const*, 6> const six = {dps[i], dps[j], dps[k], dps[l], healer[hi], healer[hj]};
                if (!allCompatible(six, tourneyMode))
                  continue;

                std::optional<Candidate> const candidate = makeCandidate(six, avoids);
                if (candidate && (!best || betterThan(*candidate, *best)))
                  best = candidate;
              }

    if (!best)
      return std::nullopt;

    return best->match;
  }

private:
  static std::optional<Candidate> makeCandidate(std::array<QueueEntry const*, 6> const& six, AvoidCheck const& avoids)
  {
    uint64_t total = 0;
    for (QueueEntry const* entry : six)
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
          if (teamAvoids(dpsA0, dpsA1, healerA, avoids) || teamAvoids(dpsB0, dpsB1, healerB, avoids))
            continue;

          uint64_t const sumA      = static_cast<uint64_t>(dpsA0.mmr) + dpsA1.mmr + healerA.mmr;
          uint64_t const imbalance = imbalanceOf(sumA, total);

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
};

// MCH ("melee / caster / healer"): every team fields exactly one melee, one
// caster and one healer. six[0..1] are the melee pair, six[2..3] the caster
// pair and six[4..5] the healer pair. Because the roles are disjoint, a class
// can only stack across roles (e.g. a priest healer with a shadow priest caster),
// which is still rejected like every other composition.
class MchMatchmaker final : public SoloqMatchmaker
{
public:
  [[nodiscard]] SoloqMode mode() const override { return SoloqMode::Mch; }

  [[nodiscard]] bool requirementsMet(RoleCounts const& counts) const override
  {
    return counts.melee >= 2 && counts.caster >= 2 && counts.healer >= 2;
  }

  [[nodiscard]] std::optional<Match> findBestMatch(std::vector<QueueEntry> const& entries, bool tourneyMode,
                                                   AvoidCheck const& avoids) const override
  {
    std::vector<QueueEntry const*> melee;
    std::vector<QueueEntry const*> caster;
    std::vector<QueueEntry const*> healer;
    for (QueueEntry const& entry : entries)
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

    std::optional<Candidate> best;
    for (std::size_t mi = 0; mi < melee.size(); ++mi)
      for (std::size_t mj = mi + 1; mj < melee.size(); ++mj)
        for (std::size_t ci = 0; ci < caster.size(); ++ci)
          for (std::size_t cj = ci + 1; cj < caster.size(); ++cj)
            for (std::size_t hi = 0; hi < healer.size(); ++hi)
              for (std::size_t hj = hi + 1; hj < healer.size(); ++hj)
              {
                std::array<QueueEntry const*, 6> const six = {melee[mi],  melee[mj],  caster[ci],
                                                              caster[cj], healer[hi], healer[hj]};
                if (!allCompatible(six, tourneyMode))
                  continue;

                std::optional<Candidate> const candidate = makeCandidate(six, avoids);
                if (candidate && (!best || betterThan(*candidate, *best)))
                  best = candidate;
              }

    if (!best)
      return std::nullopt;

    return best->match;
  }

private:
  static std::optional<Candidate> makeCandidate(std::array<QueueEntry const*, 6> const& six, AvoidCheck const& avoids)
  {
    QueuedPlayer const& m0 = six[0]->player;
    QueuedPlayer const& m1 = six[1]->player;
    QueuedPlayer const& c0 = six[2]->player;
    QueuedPlayer const& c1 = six[3]->player;
    QueuedPlayer const& h0 = six[4]->player;
    QueuedPlayer const& h1 = six[5]->player;

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
      if (teamAvoids(m0, casterA, healerA, avoids) || teamAvoids(m1, casterB, healerB, avoids))
        continue;

      uint64_t const sumA      = static_cast<uint64_t>(m0.mmr) + casterA.mmr + healerA.mmr;
      uint64_t const imbalance = imbalanceOf(sumA, total);

      if (!found || imbalance < bestImbalance)
      {
        found         = true;
        bestImbalance = imbalance;
        bestPartition = p;
      }
    }

    if (!found)
      return std::nullopt;

    bool const casterFirstInA = bestPartition == 0 || bestPartition == 1;
    bool const healerFirstInA = bestPartition == 0 || bestPartition == 2;

    Candidate candidate{};
    candidate.match.a.melee  = m0;
    candidate.match.a.caster = casterFirstInA ? c0 : c1;
    candidate.match.a.healer = healerFirstInA ? h0 : h1;
    candidate.match.b.melee  = m1;
    candidate.match.b.caster = casterFirstInA ? c1 : c0;
    candidate.match.b.healer = healerFirstInA ? h1 : h0;
    candidate.imbalance      = bestImbalance;
    fillStats(candidate, six);
    return candidate;
  }
};
} // namespace

std::string_view modeName(SoloqMode mode)
{
  switch (mode)
  {
  case SoloqMode::Flex:
    return "flex";
  case SoloqMode::Mch:
    return "mch";
  }
  return "?";
}

std::optional<SoloqMode> parseMode(std::string_view name)
{
  if (equalsIgnoreCase(name, "flex"))
    return SoloqMode::Flex;
  if (equalsIgnoreCase(name, "mch"))
    return SoloqMode::Mch;
  return std::nullopt;
}

std::string_view modeRequirement(SoloqMode mode)
{
  switch (mode)
  {
  case SoloqMode::Flex:
    return "need at least 4 DPS (melee/caster) and 2 healers to form a match";
  case SoloqMode::Mch:
    return "need at least 2 melee, 2 casters and 2 healers to form a match";
  }
  return "";
}

uint32_t mmrWindowFor(QueueEntry const& entry, bool tourneyMode)
{
  uint32_t const initial = tourneyMode ? tuning::TourneyInitialWindow : tuning::InitialWindow;
  uint32_t const cap     = tourneyMode ? tuning::TourneyMaxWindow : tuning::MaxWindow;
  auto const     step    = tourneyMode ? tuning::TourneyStepIntervalMs : tuning::StepIntervalMs;

  uint64_t const steps  = static_cast<uint64_t>(entry.waited.count()) / static_cast<uint64_t>(step.count());
  uint64_t const window = static_cast<uint64_t>(initial) + steps * static_cast<uint64_t>(tuning::MmrStep);
  return static_cast<uint32_t>(std::min<uint64_t>(window, cap));
}

bool SoloqMatchmaker::compatible(QueueEntry const& left, QueueEntry const& right, bool tourneyMode)
{
  uint32_t const gap =
      left.player.mmr > right.player.mmr ? left.player.mmr - right.player.mmr : right.player.mmr - left.player.mmr;
  return gap <= std::max(mmrWindowFor(left, tourneyMode), mmrWindowFor(right, tourneyMode));
}

bool SoloqMatchmaker::allCompatible(std::array<QueueEntry const*, 6> const& six, bool tourneyMode)
{
  for (std::size_t i = 0; i < six.size(); ++i)
    for (std::size_t j = i + 1; j < six.size(); ++j)
      if (!compatible(*six[i], *six[j], tourneyMode))
        return false;

  return true;
}

bool SoloqMatchmaker::teamAvoids(QueuedPlayer const& x, QueuedPlayer const& y, QueuedPlayer const& z,
                                 AvoidCheck const& avoids)
{
  return avoids &&
         (avoids(x.accountId, y.accountId) || avoids(x.accountId, z.accountId) || avoids(y.accountId, z.accountId));
}

uint64_t SoloqMatchmaker::imbalanceOf(uint64_t sumA, uint64_t total)
{
  uint64_t const doubled = 2 * sumA;
  return doubled > total ? doubled - total : total - doubled;
}

void SoloqMatchmaker::fillStats(Candidate& candidate, std::array<QueueEntry const*, 6> const& six)
{
  candidate.spread         = 0;
  candidate.waitTotal      = 0;
  candidate.oldestSequence = six[0]->sequence;

  uint32_t minMmr = six[0]->player.mmr;
  uint32_t maxMmr = six[0]->player.mmr;
  for (QueueEntry const* entry : six)
  {
    minMmr = std::min(minMmr, entry->player.mmr);
    maxMmr = std::max(maxMmr, entry->player.mmr);
    candidate.waitTotal += static_cast<uint64_t>(entry->waited.count());
    candidate.oldestSequence = std::min(candidate.oldestSequence, entry->sequence);
  }
  candidate.spread = maxMmr - minMmr;
}

bool SoloqMatchmaker::betterThan(Candidate const& left, Candidate const& right)
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

std::unique_ptr<SoloqMatchmaker> makeMatchmaker(SoloqMode mode)
{
  switch (mode)
  {
  case SoloqMode::Flex:
    return std::make_unique<FlexMatchmaker>();
  case SoloqMode::Mch:
    return std::make_unique<MchMatchmaker>();
  }
  return std::make_unique<FlexMatchmaker>();
}
} // namespace arenacraft::soloq
