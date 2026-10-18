## Decision

### Types (`libs/base`, implemented in M0)
| Type | Definition | Invariants |
|---|---|---|
| `Rational` | `num/den` in `int64` | Always normalized: `den > 0`, `gcd == 1`; `INT64_MIN` rejected in both terms |
| `RationalTime` | `value * timebase` seconds | `timebase > 0` |
| `TimeRange` | `[start, start + duration)` | Same timebase for start and duration; `duration >= 0`; end computed on creation |
| `FrameRate` | fps as a `Rational` (30000/1001) | `> 0`; constants in `oma::frame_rates` |
| `SampleRate` | Hz as `int32` | `> 0`; audio is addressed in samples |

Rules:
1. **Conversions are always explicit**, with `Rounding::{Floor, Ceil, Nearest}` (Nearest
   breaks ties away from zero, like FFmpeg's `AV_ROUND_NEAR_INF`).
2. `rescale()` uses 128 bits; a result outside `int64` returns `ErrorCode::Overflow` and
   never truncates.
3. **Exact comparison across timebases** without cross products wider than 128 bits
   (fraction comparison by continued-fraction expansion, terminating like Euclid):
   `3003 @ 1/90000 == 1001 @ 1/30000`.
4. Arithmetic (`plus`, `minus`) only between times that share a timebase; mixing is an error.
5. Constructors validate and return `Result<T>`. Known constants use `Rational::literal()` /
   `FrameRate::literal()` (`consteval`): an invalid literal does not compile.
6. `double` only for display (`seconds_approx()`, `to_double_approx()`).
7. `AV_NOPTS_VALUE` becomes an empty `std::optional` at the `libs/media` boundary (M2).
8. `FrameRate` describes a nominal rate; media frames are located by PTS, never by
   `index * duration` (VFR is the normal case).

### Clip time mapping (design for M5)
- Every clip has a `TimeMap`: a **monotonic, continuous, piecewise** function from timeline
  time (local to the clip) to media time.
- Initial segments: **linear** (constant speed, including 1x) and **frozen** (freeze frame).
  Speed ramps become segments whose speed is interpolated between control points (a curve),
  evaluated with exact arithmetic at the control points and explicit rounding when
  converting to media PTS.
- Reverse playback is a decreasing segment; monotonicity holds per segment.
- The clip's timeline duration is derived from the `TimeMap`, not stored separately.
- v0.1 only creates single linear-segment maps, but the model and the project format
  already use `TimeMap`, so speed ramps (v0.2) need no structural migration.

[Return to 0002-time-representation.md](0002-time-representation.md)
