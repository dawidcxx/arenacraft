#ifndef MOVEMENT_CLOCK_H
#define MOVEMENT_CLOCK_H

#include "Define.h"
#include <algorithm>
#include <bit>
#include <optional>

class MovementClock
{
public:
  void SetOffset(uint32 offset)
  {
    if (!_offset)
      _offset = offset;
    _targetOffset = offset;
  }

  uint32 Synchronize(uint32 clientTime, uint32 serverTime)
  {
    if (!_offset)
      return serverTime; // Movement can arrive before the first time-sync response.

    if (_lastClientTime)
    {
      int32 elapsed = std::bit_cast<int32>(clientTime - *_lastClientTime);
      if (elapsed > 0)
      {
        // Slew at 1% of the client timeline, not once per sync response. Limit catch-up after
        // idle periods to 10 ms per packet; negative corrections cannot reverse forward movement time.
        uint32 budget  = std::min(uint32(elapsed), 1000u) + _slewRemainder;
        int32  maxStep = int32(budget / 100);
        int32  error   = std::bit_cast<int32>(_targetOffset - *_offset);
        *_offset += uint32(std::clamp(error, -maxStep, maxStep));
        _slewRemainder  = *_offset == _targetOffset ? 0 : budget % 100;
        _lastClientTime = clientTime;
      }
    }
    else
      _lastClientTime = clientTime;

    // Both protocol clocks are uint32 milliseconds and wrap independently.
    return clientTime + *_offset;
  }

private:
  std::optional<uint32> _offset;
  std::optional<uint32> _lastClientTime;
  uint32                _targetOffset  = 0;
  uint32                _slewRemainder = 0;
};

#endif
