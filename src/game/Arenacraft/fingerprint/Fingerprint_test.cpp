#include <doctest/doctest.h>

#include "Fingerprint.hpp"

#include <vector>

using namespace arenacraft::fingerprint;

namespace
{
std::string const CheaterMacros = "MACRO 16777217 \"kick\" Ability_Kick\r\n"
                                  "#showtooltip Kick\r\n"
                                  "/run if UnitCastingInfo(\"focus\") then CastSpellByName(\"Kick\",\"focus\") end\r\n"
                                  "END\r\n"
                                  "MACRO 16777218 \"x\" INV_Misc_QuestionMark\r\n"
                                  "/script AutoFake_Toggle()\r\n"
                                  "END\r\n";

std::string const NormalMacros = "MACRO 16777217 \"kick\" Ability_Kick\n"
                                 "#showtooltip Kick\n"
                                 "/cast [@focus] Kick\n"
                                 "END\n";

std::string const DefaultBindings =
    "BINDINGMODE 0\nbind ESCAPE TOGGLEGAMEMENU\nbind 1 ACTIONBUTTON1\nbind 2 ACTIONBUTTON2\n";
} // namespace

TEST_CASE("fingerprint: macro bodies are parsed line by line, normalized, without the headers")
{
  auto const lines = parseMacroLines(CheaterMacros);
  CHECK(lines.size() == 3);
  CHECK(lines.contains("#showtooltip kick"));
  CHECK(lines.contains("/script autofake_toggle()"));
  CHECK_FALSE(lines.contains("end"));
}

TEST_CASE("fingerprint: bindings become key=command, unbound keys are skipped")
{
  auto const bindings = parseBindings("BINDINGMODE 0\r\nbind CTRL-F  INTERACTTARGET\r\nbind F12 NONE\r\n");
  CHECK(bindings == std::set<std::string>{"ctrl-f=interacttarget"});
}

TEST_CASE("fingerprint: blizzard addons are left out of the addon list")
{
  CHECK(parseAddons("Blizzard_AuctionUI\nGladius\nautoKickPro\n") == std::set<std::string>{"autokickpro", "gladius"});
}

TEST_CASE("fingerprint: the same setup is 100%, an unrelated one is far lower")
{
  Features const cheater = extract({CheaterMacros, DefaultBindings + "bind F5 MACRO kick\n", "Gladius\nAutoKickPro\n"});
  Features const alt     = extract({CheaterMacros, DefaultBindings + "bind F5 MACRO kick\n", "Gladius\nAutoKickPro\n"});
  Features const normal  = extract({NormalMacros, DefaultBindings, "Gladius\nOmniCC\n"});

  std::vector<Features> const corpus{cheater, normal};
  Weights const               weights(corpus);

  Similarity const same = compare(cheater, alt, weights);
  CHECK(same.overall == doctest::Approx(100.0));

  Similarity const other = compare(cheater, normal, weights);
  CHECK(other.overall < 40.0);
  CHECK(other.overall > 0.0);
}

TEST_CASE("fingerprint: entries everybody has weigh less than rare ones")
{
  Features a = extract({"", DefaultBindings, ""});
  Features b = extract({"", DefaultBindings, ""});
  Features c = extract({"", DefaultBindings + "bind F9 MACRO autofake\n", ""});

  std::vector<Features> const corpus{a, b, c};
  Weights const               weights(corpus);
  CHECK(weights.binding("escape=togglegamemenu") < weights.binding("f9=macro autofake"));
}

TEST_CASE("fingerprint: a part missing on both sides is left out, missing on one side counts as different")
{
  Features const              withAddons    = extract({"", "", "AutoKickPro\n"});
  Features const              sameAddons    = extract({"", "", "AutoKickPro\n"});
  Features const              withMacroOnly = extract({NormalMacros, "", "AutoKickPro\n"});
  std::vector<Features> const corpus{withAddons};
  Weights const               weights(corpus);

  Similarity const onlyAddons = compare(withAddons, sameAddons, weights);
  CHECK(onlyAddons.macros < 0);
  CHECK(onlyAddons.overall == doctest::Approx(100.0));

  Similarity const oneSided = compare(withAddons, withMacroOnly, weights);
  CHECK(oneSided.macros == doctest::Approx(0.0));
  CHECK(oneSided.overall < 100.0);
}
