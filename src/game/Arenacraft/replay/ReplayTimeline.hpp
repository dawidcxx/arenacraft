#pragma once

#include "ReplayFormat.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Pure timeline logic for replay seeking (no game world; unit-tested in ReplayTimeline_test.cpp).
//
// The viewer's client cannot be rewound: a seek forward replays the packets up to the target at once, a seek back
// first destroys every recorded object on the client and then replays from the start. While jumping, one-shot
// effects (spell visuals, combat log lines, sounds) are dropped and only the last movement packet of each object is
// kept, so the client lands on the state at the target without replaying the whole fight.
namespace arenacraft::replay
{
// Parses a seek command argument into an absolute timeline position in ms, clamped to [0, durationMs]:
//   "95" / "95s" -> 95 s, "1:35" -> 95 s, "+10" / "-10" -> relative to currentMs.
std::optional<uint32_t> parseSeekTarget(std::string_view text, uint32_t currentMs, uint32_t durationMs);

// Packets that only matter at the moment they happen (visuals, sounds, combat log); skipped while jumping.
[[nodiscard]] bool isTransientOpcode(uint16_t opcode);

// Indices of the packets to send to jump from packet `from` to `targetMs`, in sending order: every lasting packet
// in recording order, then the last movement packet of each object. `next` receives the index of the first packet
// after `targetMs`.
std::vector<std::size_t> planJump(std::vector<RecordedPacket> const& packets, std::size_t from, uint32_t targetMs,
                                  std::size_t& next);

// Every object guid the recording mentions in a way that can be parsed cheaply (movement, splines, auras, spell
// casts, destroys). A seek back destroys these on the client before replaying from the start.
std::vector<uint64_t> collectObjectGuids(std::vector<RecordedPacket> const& packets);
} // namespace arenacraft::replay
