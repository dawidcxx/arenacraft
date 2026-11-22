#!/usr/bin/env bash
# Release notes for launcher releases, generated from the commits touching
# apps/launcher since the previous launcher-* tag.
#
# usage: changelog.sh <launcher-x.y.z>
set -euo pipefail

tag="${1:?usage: changelog.sh <launcher-x.y.z>}"
version="${tag#launcher-}"

if [ -n "${GITHUB_REPOSITORY:-}" ]; then
  repo="$GITHUB_REPOSITORY"
else
  repo="$(git remote get-url origin | sed -E 's#^.*github\.com[:/]([^/]+/[^/.]+)(\.git)?$#\1#')"
fi

prev="$(git describe --tags --abbrev=0 --match 'launcher-*' "$tag^" 2>/dev/null || true)"
if [ -n "$prev" ]; then
  range="$prev..$tag"
  intro="Changes since \`$prev\`:"
else
  range="$tag"
  intro="Initial launcher release."
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
: > "$tmpdir/features"
: > "$tmpdir/fixes"
: > "$tmpdir/improvements"
: > "$tmpdir/other"

count=0
while IFS=$'\t' read -r sha subject; do
  [ -n "$sha" ] || continue
  short="${sha:0:7}"
  entry="- $subject ([$short](https://github.com/$repo/commit/$sha))"
  case "$subject" in
    feat*) file=features ;;
    fix*) file=fixes ;;
    perf*|refactor*) file=improvements ;;
    docs*|chore*|ci*|build*|test*) continue ;;
    *) file=other ;;
  esac
  printf '%s\n' "$entry" >> "$tmpdir/$file"
  count=$((count + 1))
done <<< "$(git log --no-merges --reverse --format='%H%x09%s' "$range" -- apps/launcher)"

echo "# Arenacraft Launcher $version"
echo
echo "$intro"
echo

if [ "$count" -eq 0 ]; then
  echo "No changes touching the launcher."
else
  for section in features:Features fixes:Fixes improvements:Improvements other:Other; do
    file="${section%%:*}"
    title="${section#*:}"
    if [ -s "$tmpdir/$file" ]; then
      echo "## $title"
      echo
      cat "$tmpdir/$file"
      echo
    fi
  done
fi

if [ -n "$prev" ]; then
  echo "**Full diff**: https://github.com/$repo/compare/$prev...$tag"
fi
