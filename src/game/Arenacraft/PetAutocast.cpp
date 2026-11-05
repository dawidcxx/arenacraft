#include "PetAutocast.hpp"

#include "DatabaseEnv.h"
#include "Log.h"
#include "WorldScript.h"

#include <mutex>
#include <tuple>
#include <unordered_map>

namespace arenacraft::petautocast
{
namespace
{
struct Key
{
  uint32 owner;
  uint32 entry;
  uint32 spell;

  bool operator==(Key const&) const = default;
};

struct KeyHash
{
  std::size_t operator()(Key const& key) const
  {
    return std::hash<uint64>()((uint64(key.owner) << 32) ^ (uint64(key.entry) << 16) ^ key.spell);
  }
};

// Summons happen on map threads, toggles in the session update: guard the map.
std::mutex                             Lock;
std::unordered_map<Key, bool, KeyHash> Choices;

class PetAutocastLoader : public WorldScript
{
public:
  PetAutocastLoader() : WorldScript("arenacraft::petautocast::PetAutocastLoader", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override { Load(); }
};
} // namespace

void Load()
{
  std::lock_guard<std::mutex> guard(Lock);
  Choices.clear();
  if (QueryResult result = CharacterDatabase.Query("SELECT guid, entry, spell, enabled FROM character_pet_autocast"))
    do
    {
      Field* fields                                                                           = result->Fetch();
      Choices[Key{fields[0].Get<uint32>(), fields[1].Get<uint32>(), fields[2].Get<uint32>()}] = fields[3].Get<bool>();
    } while (result->NextRow());
  LOG_INFO("server.loading", "arenacraft: loaded {} guardian autocast choices", Choices.size());
}

std::optional<bool> Choice(uint32 owner, uint32 entry, uint32 spell)
{
  std::lock_guard<std::mutex> guard(Lock);
  auto                        it = Choices.find(Key{owner, entry, spell});
  if (it == Choices.end())
    return std::nullopt;
  return it->second;
}

void Remember(uint32 owner, uint32 entry, uint32 spell, bool enabled)
{
  {
    std::lock_guard<std::mutex> guard(Lock);
    auto [it, inserted] = Choices.try_emplace(Key{owner, entry, spell}, enabled);
    if (!inserted && it->second == enabled)
      return;
    it->second = enabled;
  }
  CharacterDatabase.Execute("REPLACE INTO character_pet_autocast (guid, entry, spell, enabled) VALUES ({}, {}, {}, {})",
                            owner, entry, spell, enabled ? 1 : 0);
}

void AddPetAutocastScripts() { new PetAutocastLoader(); }
} // namespace arenacraft::petautocast
