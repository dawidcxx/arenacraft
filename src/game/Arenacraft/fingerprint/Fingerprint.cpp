#include "Fingerprint.hpp"

#include <cctype>
#include <cmath>

namespace arenacraft::fingerprint
{
namespace
{
std::string lowerCollapsed(std::string_view text)
{
  std::string out;
  out.reserve(text.size());
  bool space = false;
  for (unsigned char c : text)
  {
    if (std::isspace(c))
    {
      space = !out.empty();
      continue;
    }
    if (space)
      out += ' ';
    space = false;
    out += char(std::tolower(c));
  }
  return out;
}

template <typename Fn> void forEachLine(std::string_view text, Fn&& fn)
{
  while (!text.empty())
  {
    std::size_t const end  = text.find('\n');
    std::string_view  line = text.substr(0, end);
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    fn(line);
    if (end == std::string_view::npos)
      break;
    text.remove_prefix(end + 1);
  }
}

bool startsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }
} // namespace

// macros-cache.txt:  MACRO <id> "<name>" <icon> / body lines / END
std::set<std::string> parseMacroLines(std::string_view macrosCache)
{
  std::set<std::string> lines;
  bool                  inBody = false;
  forEachLine(macrosCache,
              [&](std::string_view line)
              {
                std::string const normalized = lowerCollapsed(line);
                if (startsWith(normalized, "macro "))
                  inBody = true;
                else if (normalized == "end")
                  inBody = false;
                else if (inBody && !normalized.empty())
                  lines.insert(normalized);
              });
  return lines;
}

// bindings-cache.txt:  BINDINGMODE 0 / bind <KEY> <COMMAND>
std::set<std::string> parseBindings(std::string_view bindingsCache)
{
  std::set<std::string> bindings;
  forEachLine(bindingsCache,
              [&](std::string_view line)
              {
                std::string const normalized = lowerCollapsed(line);
                if (!startsWith(normalized, "bind "))
                  return;
                std::string_view rest(normalized);
                rest.remove_prefix(5);
                std::size_t const space = rest.find(' ');
                if (space == std::string_view::npos)
                  return;
                std::string_view const command = rest.substr(space + 1);
                if (command.empty() || command == "none")
                  return;
                bindings.insert(std::string(rest.substr(0, space)) + "=" + std::string(command));
              });
  return bindings;
}

std::set<std::string> parseAddons(std::string_view addonList)
{
  std::set<std::string> addons;
  forEachLine(addonList,
              [&](std::string_view line)
              {
                std::string const name = lowerCollapsed(line);
                if (!name.empty() && !startsWith(name, "blizzard_"))
                  addons.insert(name);
              });
  return addons;
}

Features extract(RawFingerprint const& raw)
{
  return {parseMacroLines(raw.macros), parseBindings(raw.bindings), parseAddons(raw.addons)};
}

Weights::Weights(std::span<Features const> corpus) : _size(corpus.size())
{
  for (Features const& features : corpus)
  {
    for (std::string const& entry : features.macroLines)
      ++_macroDf[entry];
    for (std::string const& entry : features.bindings)
      ++_bindingDf[entry];
    for (std::string const& entry : features.addons)
      ++_addonDf[entry];
  }
}

double Weights::weight(Counts const& df, std::string const& entry) const
{
  auto const   it    = df.find(entry);
  double const count = it == df.end() ? 0.0 : double(it->second);
  return 1.0 + std::log((double(_size) + 1.0) / (count + 1.0));
}

double Weights::macroLine(std::string const& entry) const { return weight(_macroDf, entry); }
double Weights::binding(std::string const& entry) const { return weight(_bindingDf, entry); }
double Weights::addon(std::string const& entry) const { return weight(_addonDf, entry); }

namespace
{
// Weighted Jaccard in percent; -1 when both sets are empty.
template <typename WeightFn>
double weightedJaccard(std::set<std::string> const& a, std::set<std::string> const& b, WeightFn&& weightOf)
{
  if (a.empty() && b.empty())
    return -1;
  double shared = 0, all = 0;
  for (std::string const& entry : a)
  {
    double const w = weightOf(entry);
    all += w;
    if (b.contains(entry))
      shared += w;
  }
  for (std::string const& entry : b)
    if (!a.contains(entry))
      all += weightOf(entry);
  return all > 0 ? shared / all * 100.0 : 0.0;
}
} // namespace

Similarity compare(Features const& a, Features const& b, Weights const& weights)
{
  Similarity result;
  result.macros   = weightedJaccard(a.macroLines, b.macroLines, [&](auto const& e) { return weights.macroLine(e); });
  result.bindings = weightedJaccard(a.bindings, b.bindings, [&](auto const& e) { return weights.binding(e); });
  result.addons   = weightedJaccard(a.addons, b.addons, [&](auto const& e) { return weights.addon(e); });

  // A part only one side has (e.g. no macros synced) counts as 0% - it is a difference, not missing data. A part
  // neither side has is left out.
  double sum = 0, weightSum = 0;
  for (auto [score, weight] : {std::pair{result.macros, MacroWeight}, std::pair{result.addons, AddonWeight},
                               std::pair{result.bindings, BindingWeight}})
  {
    if (score < 0)
      continue;
    sum += score * weight;
    weightSum += weight;
  }
  result.overall = weightSum > 0 ? sum / weightSum : 0.0;
  return result;
}
} // namespace arenacraft::fingerprint
