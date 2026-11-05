/*
 * This file is part of the Arenacraft Project. See AUTHORS file for Copyright
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

#include "SqlStatementSplitter.h"

using Acore::Database::SplitSqlStatements;

TEST_CASE("SplitSqlStatements splits simple statements")
{
  auto const statements = SplitSqlStatements("SELECT 1; SELECT 2;\n");
  REQUIRE(statements.size() == 2);
  CHECK(statements[0] == "SELECT 1");
  CHECK(statements[1] == "SELECT 2");
}

TEST_CASE("SplitSqlStatements keeps semicolons inside quotes")
{
  auto const statements = SplitSqlStatements("INSERT INTO t VALUES ('a;b', \"c;d\", `e;f`);");
  REQUIRE(statements.size() == 1);
  CHECK(statements[0] == "INSERT INTO t VALUES ('a;b', \"c;d\", `e;f`)");
}

TEST_CASE("SplitSqlStatements handles escaped and doubled quotes")
{
  auto const statements = SplitSqlStatements("SELECT 'it\\'s', 'it''s'; SELECT 2;");
  REQUIRE(statements.size() == 2);
  CHECK(statements[0] == "SELECT 'it\\'s', 'it''s'");
  CHECK(statements[1] == "SELECT 2");
}

TEST_CASE("SplitSqlStatements strips line and block comments")
{
  auto const statements = SplitSqlStatements("-- leading comment\n"
                                             "SELECT 1; # trailing comment\n"
                                             "/* block ; comment */ SELECT 2;\n");
  REQUIRE(statements.size() == 2);
  CHECK(statements[0] == "SELECT 1");
  CHECK(statements[1] == "SELECT 2");
}

TEST_CASE("SplitSqlStatements preserves mysql conditional comments")
{
  auto const statements = SplitSqlStatements("/*!40101 SET NAMES utf8mb4 */;\nSELECT 1;");
  REQUIRE(statements.size() == 2);
  CHECK(statements[0] == "/*!40101 SET NAMES utf8mb4 */");
  CHECK(statements[1] == "SELECT 1");
}

TEST_CASE("SplitSqlStatements discards empty statements")
{
  auto const statements = SplitSqlStatements(";;;  ;\n-- nothing\n;");
  CHECK(statements.empty());
}
