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

#include "SqlStatementSplitter.h"

namespace
{
std::size_t FindQuoteEnd(std::string_view text, std::size_t start, char quote)
{
  std::size_t i = start + 1;
  while (i < text.size())
  {
    if (text[i] == '\\')
    {
      i += 2; // escaped char
      continue;
    }

    if (text[i] == quote)
    {
      if (i + 1 < text.size() && text[i + 1] == quote)
      {
        i += 2; // doubled quote, stays inside
        continue;
      }

      return i + 1;
    }

    ++i;
  }

  return text.size();
}
} // namespace

namespace Acore::Database
{
std::vector<std::string> SplitSqlStatements(std::string_view sqlText)
{
  std::vector<std::string> statements;
  std::string              current;
  std::size_t              i = 0;

  auto pushStatement = [&]()
  {
    std::size_t first = current.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
      current.clear();
      return;
    }

    std::size_t last = current.find_last_not_of(" \t\r\n");
    statements.emplace_back(current.substr(first, last - first + 1));
    current.clear();
  };

  while (i < sqlText.size())
  {
    char const c    = sqlText[i];
    char const next = (i + 1 < sqlText.size()) ? sqlText[i + 1] : '\0';

    // line comments
    if ((c == '-' && next == '-') || c == '#')
    {
      std::size_t eol = sqlText.find('\n', i);
      i               = (eol == std::string_view::npos) ? sqlText.size() : eol;
      current += ' ';
      continue;
    }

    // mysql conditional comments are directives and must survive
    if (c == '/' && next == '*' && i + 2 < sqlText.size() && sqlText[i + 2] == '!')
    {
      std::size_t end  = sqlText.find("*/", i + 3);
      std::size_t stop = (end == std::string_view::npos) ? sqlText.size() : end + 2;
      current.append(sqlText.substr(i, stop - i));
      i = stop;
      continue;
    }

    // plain block comments
    if (c == '/' && next == '*')
    {
      std::size_t end = sqlText.find("*/", i + 2);
      i               = (end == std::string_view::npos) ? sqlText.size() : end + 2;
      current += ' ';
      continue;
    }

    // quoted regions are copied verbatim
    if (c == '\'' || c == '"' || c == '`')
    {
      std::size_t end = FindQuoteEnd(sqlText, i, c);
      current.append(sqlText.substr(i, end - i));
      i = end;
      continue;
    }

    if (c == ';')
    {
      pushStatement();
      ++i;
      continue;
    }

    current += c;
    ++i;
  }

  pushStatement();
  return statements;
}
} // namespace Acore::Database
