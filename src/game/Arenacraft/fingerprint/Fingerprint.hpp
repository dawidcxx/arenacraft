#pragma once

#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

// Player fingerprints for anti-cheat (see ai-doc/fingerprint.md). A fingerprint is what the 3.3.5 client itself
// keeps on the server: its macros and key bindings (account data, synced on every change) and the addon list it
// reports at login. GMs snapshot a known cheater's fingerprint (.fingerprint touch) and compare other players
// against the stored snapshots (.fingerprint compare). Pure parsing and scoring here, unit-tested.
namespace arenacraft::fingerprint
{
// The raw texts, as stored on the server.
struct RawFingerprint
{
  std::string macros;   // macros-cache.txt (account + character), concatenated
  std::string bindings; // bindings-cache.txt (account + character), concatenated
  std::string addons;   // one addon name per line
};

// Normalized feature sets.
struct Features
{
  std::set<std::string> macroLines; // each non-empty macro body line, lowercased, whitespace collapsed
  std::set<std::string> bindings;   // "key=command"
  std::set<std::string> addons;     // addon names, lowercased, Blizzard_ addons left out
};

Features extract(RawFingerprint const& raw);

std::set<std::string> parseMacroLines(std::string_view macrosCache);
std::set<std::string> parseBindings(std::string_view bindingsCache);
std::set<std::string> parseAddons(std::string_view addonList);

// Rarer entries weigh more: an entry present in `df` of `corpusSize` fingerprints weighs 1 + ln((corpusSize + 1) /
// (df + 1)). Shared defaults (stock key bindings, popular addons) barely count, unusual macros/addons dominate.
class Weights
{
public:
  explicit Weights(std::span<Features const> corpus);
  [[nodiscard]] double macroLine(std::string const& entry) const;
  [[nodiscard]] double binding(std::string const& entry) const;
  [[nodiscard]] double addon(std::string const& entry) const;

private:
  using Counts               = std::unordered_map<std::string, uint32_t>;
  std::size_t          _size = 0;
  Counts               _macroDf, _bindingDf, _addonDf;
  [[nodiscard]] double weight(Counts const& df, std::string const& entry) const;
};

struct Similarity
{
  // 0..100, or -1 when neither side has anything of that kind
  double macros   = -1;
  double bindings = -1;
  double addons   = -1;
  double overall  = 0; // weighted mean of the parts both sides have
};

// Weight of each part in `overall` (renormalized over the parts both sides have).
inline constexpr double MacroWeight   = 0.45;
inline constexpr double AddonWeight   = 0.35;
inline constexpr double BindingWeight = 0.20;

Similarity compare(Features const& a, Features const& b, Weights const& weights);
} // namespace arenacraft::fingerprint
