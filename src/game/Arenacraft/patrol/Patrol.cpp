#include "Patrol.hpp"

#include "AccountMgr.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "BanMgr.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PatrolConfig.hpp"
#include "PatrolRules.hpp"
#include "Player.h"
#include "ReplayConfig.hpp"
#include "ReplayPlayback.hpp"
#include "SoloqTeam.hpp"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace arenacraft::patrol
{
namespace
{
constexpr char const* BanAuthor = "Patrol";

enum CaseStatus : uint8
{
  CASE_OPEN    = 0,
  CASE_BANNED  = 1,
  CASE_CLEARED = 2,
  CASE_FLAGGED = 3, // guilty, but Patrol.Punishment = none: left to staff
};

// One open review per patroller. Lives from .patrol accept until the verdict, a skip or a logout.
struct Review
{
  uint32                                      caseId   = 0;
  uint32                                      replayId = 0;
  ObjectGuid                                  suspect;
  std::unordered_map<ObjectGuid, std::string> aliases; // every participant of the match -> shown name
  bool                                        answering = false;
  std::vector<Category>                       questions;    // enabled categories when the case was accepted
  std::size_t                                 question = 0; // next question index while answering
  uint8                                       yesMask  = 0;
  bool                                        follow   = true; // camera locked on the suspect
};

// Name query responses are answered outside the world thread, so the map is guarded.
std::mutex                             ReviewsMutex;
std::unordered_map<ObjectGuid, Review> Reviews;

std::optional<Review> FindReview(ObjectGuid viewer)
{
  std::lock_guard lock(ReviewsMutex);
  auto            it = Reviews.find(viewer);
  return it == Reviews.end() ? std::nullopt : std::optional<Review>(it->second);
}

template <typename Fn> bool UpdateReview(ObjectGuid viewer, Fn&& fn)
{
  std::lock_guard lock(ReviewsMutex);
  auto            it = Reviews.find(viewer);
  if (it == Reviews.end())
    return false;
  fn(it->second);
  return true;
}

void Tell(Player* player, std::string const& text)
{
  if (player && player->GetSession())
    ChatHandler(player->GetSession()).SendSysMessage(text);
}

ObjectGuid PlayerGuid(uint32 low) { return ObjectGuid::Create<HighGuid::Player>(low); }

// SMSG_NAME_QUERY_RESPONSE with `name`, same layout as WorldSession::SendNameQueryOpcode. Sent unsolicited it
// also overwrites a name the client already cached.
void SendName(Player* viewer, ObjectGuid guid, std::string const& name)
{
  CharacterCacheEntry const* entry = sCharacterCache->GetCharacterCacheByGuid(guid);
  WorldPacket                data(SMSG_NAME_QUERY_RESPONSE, 8 + 1 + name.size() + 1 + 4);
  data << guid.WriteAsPacked();
  data << uint8(0); // name known
  data << name;
  data << uint8(0); // realm
  data << uint8(entry ? entry->Race : 0);
  data << uint8(entry ? entry->Sex : 0);
  data << uint8(entry ? entry->Class : 0);
  data << uint8(0); // not declined
  viewer->GetSession()->SendPacket(&data);
}

void PushAliases(Player* viewer, Review const& review)
{
  for (auto const& [guid, alias] : review.aliases)
    SendName(viewer, guid, alias);
}

void SetFollow(Player* viewer, Review const& review, bool follow)
{
  viewer->SetGuidValue(PLAYER_FARSIGHT, follow ? review.suspect : ObjectGuid::Empty);
}

void AskQuestion(Player* viewer, Review const& review)
{
  CategoryInfo const& category = info(review.questions[review.question]);
  Tell(viewer, Acore::StringFormat("Patrol case #{} - question {}/{} about the player marked Suspect:", review.caseId,
                                   review.question + 1, review.questions.size()));
  Tell(viewer, Acore::StringFormat("  {}  Answer .patrol yes or .patrol no", category.question));
}

// Ends the review of `viewer` (whatever its state) and gives the client the real names back.
void EndReview(ObjectGuid viewerGuid)
{
  std::optional<Review> review;
  {
    std::lock_guard lock(ReviewsMutex);
    auto            it = Reviews.find(viewerGuid);
    if (it == Reviews.end())
      return;
    review = std::move(it->second);
    Reviews.erase(it);
  }

  Player* viewer = ObjectAccessor::FindConnectedPlayer(viewerGuid);
  if (!viewer || !viewer->GetSession())
    return;

  if (viewer->GetGuidValue(PLAYER_FARSIGHT) == review->suspect)
    viewer->SetGuidValue(PLAYER_FARSIGHT, ObjectGuid::Empty);
  for (auto const& [guid, alias] : review->aliases)
    viewer->GetSession()->SendNameQueryOpcode(guid);
}

uint32 SoloqSeasonGames(Player* player)
{
  if (ArenaTeam* team = soloq::FindSoloqTeam(player))
    if (ArenaTeamMember* member = team->GetMember(player->GetGUID()))
      return member->SeasonGames;
  return 0;
}

// Solo queue rating of any character, online or not (arena teams are all loaded at startup). 0 = no soloq team.
uint32 SoloqRating(ObjectGuid guid)
{
  if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamByCaptain(guid, ARENA_TYPE_5v5))
    return team->GetRating();
  return 0;
}

std::vector<Category> EnabledCategories()
{
  std::vector<Category> out;
  for (CategoryInfo const& entry : Categories)
    if (config().categories & bit(entry.category))
      out.push_back(entry.category);
  return out;
}

// Staff skip the games / rating requirements when Patrol.StaffExempt is on.
bool IsExemptStaff(Player* player)
{
  return config().staffExempt && !AccountMgr::IsPlayerAccount(player->GetSession()->GetSecurity());
}

// Why `player` can't patrol right now (empty = can).
std::string CannotPatrolReason(Player* player)
{
  if (!config().enabled)
    return "Patrol is disabled on this server.";
  if (!replay::config().enabled)
    return "Patrol is unavailable: replays are disabled.";
  if (!IsExemptStaff(player) && SoloqSeasonGames(player) < config().minGames)
    return Acore::StringFormat("Patrol needs at least {} solo queue games this season (you have {}).",
                               config().minGames, SoloqSeasonGames(player));
  return {};
}

// Applies Patrol.Punishment to the suspect and closes the case.
void CloseCase(uint32 caseId, ObjectGuid suspect, std::string const& suspectName, uint8 guilty)
{
  if (!guilty)
  {
    CharacterDatabase.DirectExecute(
        "UPDATE patrol_case SET status = {}, closed_at = NOW() WHERE id = {} AND status = {}", uint8(CASE_CLEARED),
        caseId, uint8(CASE_OPEN));
    LOG_INFO("server", "patrol: case #{} cleared ({})", caseId, suspectName);
    return;
  }

  Config const& cfg  = config();
  std::string   name = suspectName;
  sCharacterCache->GetCharacterNameByGuid(suspect, name); // follows renames
  std::string const reason = "Patrol: " + labels(guilty);

  if (cfg.punishment == Punishment::None)
  {
    CharacterDatabase.DirectExecute(
        "UPDATE patrol_case SET status = {}, guilty = {}, closed_at = NOW() WHERE id = {} AND status = {}",
        uint8(CASE_FLAGGED), guilty, caseId, uint8(CASE_OPEN));
    LOG_INFO("server", "patrol: case #{} found {} guilty ({}), flagged for staff", caseId, name, reason);
    return;
  }

  BanReturn const result = cfg.punishment == Punishment::BanAccount
                               ? sBan->BanAccountByPlayerName(name, cfg.banDuration, reason, BanAuthor)
                               : sBan->BanCharacter(name, cfg.banDuration, reason, BanAuthor);

  CharacterDatabase.DirectExecute(
      "UPDATE patrol_case SET status = {}, guilty = {}, closed_at = NOW() WHERE id = {} AND status = {}",
      uint8(CASE_BANNED), guilty, caseId, uint8(CASE_OPEN));
  LOG_INFO("server", "patrol: case #{} {} -> {} ({}), ban result {}", caseId, name, describePunishment(cfg), reason,
           uint32(result));
}

void SubmitVerdict(Player* patroller, Review const& review)
{
  uint32 const accountId = patroller->GetSession()->GetAccountId();

  QueryResult state =
      CharacterDatabase.Query("SELECT status, suspect_name FROM patrol_case WHERE id = {}", review.caseId);
  if (!state || (*state)[0].Get<uint8>() != CASE_OPEN)
  {
    Tell(patroller, "Thanks - this case was already decided by other patrol players.");
    return;
  }
  std::string const suspectName = (*state)[1].Get<std::string>();

  CharacterDatabase.DirectExecute(
      "INSERT IGNORE INTO patrol_verdict (case_id, account_id, guid, skipped, yes_mask) VALUES ({}, {}, {}, 0, {})",
      review.caseId, accountId, patroller->GetGUID().GetCounter(), review.yesMask);
  Tell(patroller, "Thanks - your verdict is recorded.");

  std::vector<uint8> masks;
  if (QueryResult verdicts = CharacterDatabase.Query(
          "SELECT yes_mask FROM patrol_verdict WHERE case_id = {} AND skipped = 0", review.caseId))
    do
      masks.push_back((*verdicts)[0].Get<uint8>());
    while (verdicts->NextRow());

  Config const& cfg = config();
  if (masks.size() >= cfg.verdicts)
    CloseCase(review.caseId, review.suspect, suspectName, guiltyCategories(masks, cfg.verdicts, cfg.yesToBan));
}
} // namespace

// --- commands --------------------------------------------------------------------------------------------

void Report(ChatHandler* handler, Player* reporter, uint32 replayId, std::string_view suspectName,
            std::string_view categoryText)
{
  Config const& cfg = config();
  if (!cfg.enabled)
  {
    handler->SendSysMessage("Reports are disabled on this server.");
    return;
  }

  std::optional<Category> const category = parseCategory(categoryText);
  if (!category || !(cfg.categories & bit(*category)))
  {
    std::string keys;
    for (Category entry : EnabledCategories())
      keys += (keys.empty() ? "" : "|") + std::string(info(entry).key);
    handler->PSendSysMessage("Usage: .replay report <replay id> <name> <{}>", keys);
    return;
  }

  if (cfg.reportWindowHours)
  {
    QueryResult age =
        CharacterDatabase.Query("SELECT created_at > NOW() - INTERVAL {} HOUR FROM arena_replay WHERE id = {}",
                                cfg.reportWindowHours, replayId);
    if (age && (*age)[0].Get<int64>() == 0)
    {
      handler->PSendSysMessage("Replay #{} is too old: matches can be reported up to {} hours after they ended.",
                               replayId, cfg.reportWindowHours);
      return;
    }
  }

  QueryResult players =
      CharacterDatabase.Query("SELECT guid, name FROM arena_replay_player WHERE replay_id = {}", replayId);
  if (!players)
  {
    handler->PSendSysMessage("Replay #{} does not exist (or was already deleted).", replayId);
    return;
  }

  std::string wanted(suspectName);
  normalizePlayerName(wanted);

  uint32 const reporterLow = reporter->GetGUID().GetCounter();
  bool         reporterIn  = false;
  uint32       suspectLow  = 0;
  std::string  suspectRealName;
  do
  {
    uint32 const      guid = (*players)[0].Get<uint32>();
    std::string const name = (*players)[1].Get<std::string>();
    if (guid == reporterLow)
      reporterIn = true;
    else if (name == wanted)
    {
      suspectLow      = guid;
      suspectRealName = name;
    }
  } while (players->NextRow());

  if (!reporterIn)
  {
    handler->SendSysMessage("You can only report players of a match you played in.");
    return;
  }
  if (!suspectLow)
  {
    handler->PSendSysMessage("{} did not play in replay #{} (or that is you).", wanted, replayId);
    return;
  }

  uint32 const account = reporter->GetSession()->GetAccountId();
  if (QueryResult recent = CharacterDatabase.Query(
          "SELECT COUNT(*) FROM patrol_report WHERE reporter_account = {} AND created_at > NOW() - INTERVAL 1 DAY",
          account))
    if (cfg.maxReportsPerDay && (*recent)[0].Get<uint64>() >= cfg.maxReportsPerDay)
    {
      handler->PSendSysMessage("You can file at most {} reports per day.", cfg.maxReportsPerDay);
      return;
    }

  uint8 const categoryBit = bit(*category);
  uint32      caseId      = 0;
  if (QueryResult existing = CharacterDatabase.Query(
          "SELECT id, status FROM patrol_case WHERE replay_id = {} AND suspect_guid = {}", replayId, suspectLow))
  {
    caseId = (*existing)[0].Get<uint32>();
    if ((*existing)[1].Get<uint8>() != CASE_OPEN)
    {
      handler->PSendSysMessage("{} was already reviewed for this match.", suspectRealName);
      return;
    }
    CharacterDatabase.DirectExecute("UPDATE patrol_case SET categories = categories | {} WHERE id = {}", categoryBit,
                                    caseId);
  }
  else
  {
    std::string escaped = suspectRealName;
    CharacterDatabase.EscapeString(escaped);
    // the rating at report time: patrollers are matched against it (Patrol.RatingWindow)
    CharacterDatabase.DirectExecute("INSERT IGNORE INTO patrol_case (replay_id, suspect_guid, suspect_name, "
                                    "suspect_rating, categories) VALUES ({}, {}, '{}', {}, {})",
                                    replayId, suspectLow, escaped, SoloqRating(PlayerGuid(suspectLow)), categoryBit);
    if (QueryResult created = CharacterDatabase.Query(
            "SELECT id FROM patrol_case WHERE replay_id = {} AND suspect_guid = {}", replayId, suspectLow))
      caseId = (*created)[0].Get<uint32>();
  }

  if (!caseId)
  {
    handler->SendSysMessage("Could not file the report, try again later.");
    return;
  }

  if (CharacterDatabase.Query("SELECT 1 FROM patrol_report WHERE case_id = {} AND reporter_account = {}", caseId,
                              account))
  {
    handler->PSendSysMessage("You already reported {} for this match.", suspectRealName);
    return;
  }

  CharacterDatabase.DirectExecute(
      "INSERT IGNORE INTO patrol_report (case_id, reporter_account, reporter_guid, category) VALUES ({}, {}, {}, {})",
      caseId, account, reporterLow, uint8(*category));
  handler->PSendSysMessage("Report filed: {} in replay #{} ({}). Patrol players will review it anonymously.",
                           suspectRealName, replayId, info(*category).label);
}

void Accept(ChatHandler* handler, Player* patroller)
{
  if (std::optional<Review> open = FindReview(patroller->GetGUID()))
  {
    // a review whose replay never started (or was left before loading) is retried, not stuck
    if (!open->answering && !patroller->IsSpectator())
    {
      if (std::string const problem = replay::cannotWatchReason(patroller); !problem.empty())
      {
        handler->PSendSysMessage("Patrol: {}", problem);
        return;
      }
      handler->PSendSysMessage("Patrol case #{}: loading the replay again.", open->caseId);
      replay::watch(patroller, open->replayId);
      return;
    }
    handler->SendSysMessage("You already have a patrol case open. Finish it (.patrol yes/no) or .patrol skip it.");
    return;
  }

  if (std::string const problem = CannotPatrolReason(patroller); !problem.empty())
  {
    handler->SendSysMessage(problem);
    return;
  }

  if (std::string const problem = replay::cannotWatchReason(patroller); !problem.empty())
  {
    handler->PSendSysMessage("Patrol: {}", problem);
    return;
  }

  Config const& cfg     = config();
  uint32 const  account = patroller->GetSession()->GetAccountId();

  // Patrol.RatingWindow: only suspects rated within +-window of the patroller's solo queue rating.
  std::string ratingFilter;
  if (cfg.ratingWindow && !IsExemptStaff(patroller))
  {
    uint32 const rating = SoloqRating(patroller->GetGUID());
    ratingFilter =
        Acore::StringFormat("AND c.suspect_rating BETWEEN {} AND {} ",
                            rating > cfg.ratingWindow ? rating - cfg.ratingWindow : 0, rating + cfg.ratingWindow);
  }

  // A random open case this account has not judged, did not report and did not play in.
  QueryResult picked = CharacterDatabase.Query(
      "SELECT c.id, c.replay_id, c.suspect_guid FROM patrol_case c "
      "WHERE c.status = {} {}"
      "AND EXISTS (SELECT 1 FROM arena_replay r WHERE r.id = c.replay_id) "
      "AND (SELECT COUNT(*) FROM patrol_verdict v WHERE v.case_id = c.id AND v.skipped = 0) < {} "
      "AND NOT EXISTS (SELECT 1 FROM patrol_verdict v WHERE v.case_id = c.id AND v.account_id = {}) "
      "AND NOT EXISTS (SELECT 1 FROM patrol_report rp WHERE rp.case_id = c.id AND rp.reporter_account = {}) "
      "AND NOT EXISTS (SELECT 1 FROM arena_replay_player p JOIN characters ch ON ch.guid = p.guid "
      "WHERE p.replay_id = c.replay_id AND ch.account = {}) "
      "ORDER BY RAND() LIMIT 1",
      uint8(CASE_OPEN), ratingFilter, cfg.verdicts, account, account, account);
  if (!picked)
  {
    handler->SendSysMessage("No patrol case is waiting for you right now. Thanks for checking!");
    return;
  }

  Review review;
  review.caseId    = (*picked)[0].Get<uint32>();
  review.replayId  = (*picked)[1].Get<uint32>();
  review.suspect   = PlayerGuid((*picked)[2].Get<uint32>());
  review.questions = EnabledCategories();

  uint32 index = 0;
  if (QueryResult players = CharacterDatabase.Query(
          "SELECT guid FROM arena_replay_player WHERE replay_id = {} ORDER BY team, guid", review.replayId))
    do
    {
      ObjectGuid const guid = PlayerGuid((*players)[0].Get<uint32>());
      review.aliases[guid]  = guid == review.suspect ? "Suspect" : Acore::StringFormat("Player{}", ++index);
    } while (players->NextRow());

  {
    std::lock_guard lock(ReviewsMutex);
    Reviews[patroller->GetGUID()] = review;
  }

  handler->PSendSysMessage("Patrol case #{}: watch the player named Suspect (names are hidden). The camera follows "
                           "them; .patrol camera frees it. Questions come when the replay ends (.replay leave ends it "
                           "early, .patrol skip passes on the case).",
                           review.caseId);
  replay::watch(patroller, review.replayId);
}

void Answer(ChatHandler* handler, Player* patroller, bool yes)
{
  std::optional<Review> done;
  bool                  waiting = false;
  bool const            found   = UpdateReview(patroller->GetGUID(),
                                               [&](Review& review)
                                               {
                                    if (!review.answering)
                                    {
                                      waiting = true;
                                      return;
                                    }
                                    if (yes)
                                      review.yesMask |= bit(review.questions[review.question]);
                                    ++review.question;
                                    if (review.question >= review.questions.size())
                                      done = review;
                                  });

  if (!found)
  {
    handler->SendSysMessage("You have no patrol case open. .patrol accept starts one.");
    return;
  }
  if (waiting)
  {
    handler->SendSysMessage("Watch the replay first; the questions come when it ends (.replay leave ends it early).");
    return;
  }

  if (done)
  {
    SubmitVerdict(patroller, *done);
    EndReview(patroller->GetGUID());
    return;
  }

  if (std::optional<Review> review = FindReview(patroller->GetGUID()))
    AskQuestion(patroller, *review);
}

void Skip(ChatHandler* handler, Player* patroller)
{
  std::optional<Review> review = FindReview(patroller->GetGUID());
  if (!review)
  {
    handler->SendSysMessage("You have no patrol case open.");
    return;
  }

  CharacterDatabase.DirectExecute(
      "INSERT IGNORE INTO patrol_verdict (case_id, account_id, guid, skipped, yes_mask) VALUES ({}, {}, {}, 1, 0)",
      review->caseId, patroller->GetSession()->GetAccountId(), patroller->GetGUID().GetCounter());
  EndReview(patroller->GetGUID());
  if (patroller->IsSpectator())
    patroller->TeleportToEntryPoint();
  handler->SendSysMessage("Case skipped; it won't be offered to you again.");
}

void Status(ChatHandler* handler, Player* patroller)
{
  std::optional<Review> review = FindReview(patroller->GetGUID());
  if (!review)
  {
    if (std::string const problem = CannotPatrolReason(patroller); !problem.empty())
      handler->PSendSysMessage("No patrol case open. {}", problem);
    else
      handler->SendSysMessage("No patrol case open. You can patrol: .patrol accept");
    return;
  }
  if (review->answering)
  {
    handler->PSendSysMessage("Patrol case #{}: {} of {} questions answered.", review->caseId, review->question,
                             review->questions.size());
    AskQuestion(patroller, *review);
  }
  else
    handler->PSendSysMessage("Patrol case #{}: watching the replay.", review->caseId);
}

void ToggleFollow(ChatHandler* handler, Player* patroller)
{
  std::optional<Review> review;
  UpdateReview(patroller->GetGUID(),
               [&](Review& r)
               {
                 r.follow = !r.follow;
                 review   = r;
               });
  if (!review)
  {
    handler->SendSysMessage("You have no patrol case open.");
    return;
  }
  SetFollow(patroller, *review, review->follow);
  handler->SendSysMessage(review->follow ? "Camera follows the Suspect." : "Camera is free.");
}

void ListOpenCases(ChatHandler* handler)
{
  Config const& cfg = config();
  handler->PSendSysMessage("Patrol is {}: {} verdicts per case, guilty at {} yes -> {}.", cfg.enabled ? "on" : "off",
                           cfg.verdicts, cfg.yesToBan, describePunishment(cfg));

  QueryResult cases =
      CharacterDatabase.Query("SELECT c.id, c.replay_id, c.suspect_name, c.suspect_rating, c.categories, "
                              "(SELECT COUNT(*) FROM patrol_verdict v WHERE v.case_id = c.id AND v.skipped = 0), "
                              "(SELECT COUNT(*) FROM patrol_report r WHERE r.case_id = c.id) "
                              "FROM patrol_case c WHERE c.status = {} ORDER BY c.id LIMIT 30",
                              uint8(CASE_OPEN));
  if (!cases)
    handler->SendSysMessage("No open patrol cases.");
  else
  {
    handler->SendSysMessage("Open cases (id, replay, suspect, rating, reported for, verdicts, reports):");
    do
    {
      Field* f = cases->Fetch();
      handler->PSendSysMessage("  #{} replay #{} {} ({}) - {} - {}/{} verdicts, {} reports", f[0].Get<uint32>(),
                               f[1].Get<uint32>(), f[2].Get<std::string>(), f[3].Get<uint32>(),
                               labels(f[4].Get<uint8>()), f[5].Get<uint64>(), cfg.verdicts, f[6].Get<uint64>());
    } while (cases->NextRow());
  }

  // Patrol.Punishment = none: guilty verdicts wait for staff
  if (QueryResult flagged = CharacterDatabase.Query("SELECT id, replay_id, suspect_name, guilty FROM patrol_case "
                                                    "WHERE status = {} ORDER BY id DESC LIMIT 30",
                                                    uint8(CASE_FLAGGED)))
  {
    handler->SendSysMessage("Found guilty, not punished (id, replay, suspect, guilty of):");
    do
    {
      Field* f = flagged->Fetch();
      handler->PSendSysMessage("  #{} replay #{} {} - {}", f[0].Get<uint32>(), f[1].Get<uint32>(),
                               f[2].Get<std::string>(), labels(f[3].Get<uint8>()));
    } while (flagged->NextRow());
  }
}

// --- hooks -----------------------------------------------------------------------------------------------

void OnReplayStarted(Player* viewer, uint32 replayId)
{
  std::optional<Review> review = FindReview(viewer->GetGUID());
  if (!review || review->replayId != replayId)
    return;

  PushAliases(viewer, *review);
  if (review->follow)
    SetFollow(viewer, *review, true);
}

void OnReplayEnded(ObjectGuid viewerGuid, uint32 replayId, bool watched)
{
  if (!watched)
  {
    // never got to see it: keep the review, .patrol accept retries the same case
    if (std::optional<Review> pending = FindReview(viewerGuid); pending && pending->replayId == replayId)
      Tell(ObjectAccessor::FindConnectedPlayer(viewerGuid),
           "Patrol: the replay did not start. Type .patrol accept to try again or .patrol skip.");
    return;
  }

  std::optional<Review> review;
  UpdateReview(viewerGuid,
               [&](Review& r)
               {
                 if (r.replayId != replayId || r.answering)
                   return;
                 r.answering = true;
                 review      = r;
               });
  if (!review)
    return;

  Player* viewer = ObjectAccessor::FindConnectedPlayer(viewerGuid);
  if (!viewer)
    return;
  if (viewer->GetGuidValue(PLAYER_FARSIGHT) == review->suspect)
    viewer->SetGuidValue(PLAYER_FARSIGHT, ObjectGuid::Empty);
  AskQuestion(viewer, *review);
}

void OnReplaySeeked(Player* viewer, uint32 replayId)
{
  std::optional<Review> review = FindReview(viewer->GetGUID());
  if (!review || review->replayId != replayId || review->answering || !review->follow)
    return;
  // a seek back destroyed and re-created the suspect on the client; point the camera at the new object again
  SetFollow(viewer, *review, false);
  SetFollow(viewer, *review, true);
}

void OnLogout(Player* player) { EndReview(player->GetGUID()); }

std::optional<std::string> AliasFor(ObjectGuid viewer, ObjectGuid target)
{
  std::lock_guard lock(ReviewsMutex);
  auto            it = Reviews.find(viewer);
  if (it == Reviews.end())
    return std::nullopt;
  auto alias = it->second.aliases.find(target);
  if (alias == it->second.aliases.end())
    return std::nullopt;
  return alias->second;
}

bool IsReviewing(ObjectGuid viewer)
{
  std::lock_guard lock(ReviewsMutex);
  return Reviews.contains(viewer);
}
} // namespace arenacraft::patrol
