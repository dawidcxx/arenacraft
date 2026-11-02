#include "PatrolRules.hpp"

#include <algorithm>
#include <cctype>

namespace arenacraft::patrol
{
std::optional<Category> parseCategory(std::string_view text)
{
  std::string lower;
  for (char c : text)
    if (c != ' ' && c != '-' && c != '_')
      lower += char(std::tolower(static_cast<unsigned char>(c)));

  for (CategoryInfo const& entry : Categories)
    if (lower == entry.key)
      return entry.category;

  if (lower == "cheat" || lower == "hack" || lower == "hacks" || lower == "speedhack")
    return Category::Cheating;
  if (lower == "bot" || lower == "botting" || lower == "autokick" || lower == "autofake" || lower == "script" ||
      lower == "scripts")
    return Category::Automation;
  if (lower == "grief" || lower == "afk" || lower == "throwing" || lower == "feeding")
    return Category::Griefing;
  if (lower == "wintrade" || lower == "wintrades")
    return Category::WinTrading;
  return std::nullopt;
}

CategoryInfo const& info(Category category) { return Categories[std::size_t(category)]; }

std::string labels(uint8_t mask)
{
  std::string out;
  for (CategoryInfo const& entry : Categories)
  {
    if (!(mask & bit(entry.category)))
      continue;
    if (!out.empty())
      out += ", ";
    out += entry.label;
  }
  return out;
}

uint8_t guiltyCategories(std::span<uint8_t const> yesMasks, std::size_t verdictsNeeded, std::size_t yesToBan)
{
  if (yesMasks.size() < verdictsNeeded || yesToBan == 0)
    return 0;

  uint8_t guilty = 0;
  for (CategoryInfo const& entry : Categories)
  {
    std::size_t const yes =
        std::count_if(yesMasks.begin(), yesMasks.end(), [&](uint8_t mask) { return mask & bit(entry.category); });
    if (yes >= yesToBan)
      guilty |= bit(entry.category);
  }
  return guilty;
}

uint8_t parseCategoryMask(std::string_view list, std::string* unknown)
{
  uint8_t mask = 0;
  while (!list.empty())
  {
    std::size_t const      comma = list.find(',');
    std::string_view const token = list.substr(0, comma);
    list                         = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);

    std::string trimmed;
    for (char c : token)
      if (c != ' ' && c != '\t')
        trimmed += char(std::tolower(static_cast<unsigned char>(c)));
    if (trimmed.empty())
      continue;
    if (trimmed == "all")
    {
      mask = AllCategories;
      continue;
    }
    if (std::optional<Category> category = parseCategory(trimmed))
      mask |= bit(*category);
    else if (unknown && unknown->empty())
      *unknown = trimmed;
  }
  return mask;
}
} // namespace arenacraft::patrol
