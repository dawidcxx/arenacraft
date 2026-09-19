/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright
 * information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <doctest/doctest.h>

#include "Tokenize.h"

#include <string_view>

TEST_CASE("Acore::Tokenize splits on the separator")
{
  std::vector<std::string_view> tokens = Acore::Tokenize("a,b,c", ',', false);

  REQUIRE(tokens.size() == 3);
  CHECK(tokens[0] == std::string_view("a"));
  CHECK(tokens[1] == std::string_view("b"));
  CHECK(tokens[2] == std::string_view("c"));
}

TEST_CASE("Acore::Tokenize drops empty fields by default")
{
  std::vector<std::string_view> tokens = Acore::Tokenize("a,,b", ',', false);

  REQUIRE(tokens.size() == 2);
  CHECK(tokens[0] == std::string_view("a"));
  CHECK(tokens[1] == std::string_view("b"));
}

TEST_CASE("Acore::Tokenize keeps empty fields when asked")
{
  std::vector<std::string_view> tokens = Acore::Tokenize("a,,b", ',', true);

  REQUIRE(tokens.size() == 3);
  CHECK(tokens[0] == std::string_view("a"));
  CHECK(tokens[1].empty());
  CHECK(tokens[2] == std::string_view("b"));
}

TEST_CASE("Acore::Tokenize returns the whole input when the separator is absent")
{
  std::vector<std::string_view> tokens = Acore::Tokenize("hello", ',', false);

  REQUIRE(tokens.size() == 1);
  CHECK(tokens[0] == std::string_view("hello"));
}

TEST_CASE("Acore::Tokenize returns nothing for an empty input without keepEmpty")
{
  std::vector<std::string_view> tokens = Acore::Tokenize("", ',', false);

  CHECK(tokens.empty());
}
