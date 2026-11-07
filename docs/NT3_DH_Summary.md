# NT-3 Light: Daylight Harvesting Summary

How daylight harvesting (DH) works now. For more detail, see
`NT3_Daylight_Harvesting.md`. This firmware has not yet been tested on real lights.

## How it works

DH runs only on an Adaptiv network, in a grouped light. One light is the
**master**, and only its sensor is used:

1. **Sampling:** the master reads lux every **2.5 s**. Its green LED goes off for
   about 200 ms for each reading during occupancy, which also shows which light is
   the master.
2. **Average:** every 2 readings (5 s) it averages them and runs the algorithm.
3. **Send:** if the level would change by more than 5 %, it multicasts a DH Level
   Adjust to the group.
4. **Ramp:** every light that is able to follow, including the master when it hears
   its own multicast, ramps there over **20 s** (`DEFAULT_DH_RAMP_DURATION_SEC`).
5. **Ramp timer:** readings during the ramp are discarded. The next two readings
   after it ends are the next average, so a command goes out about every 22.5–25 s
   at most.

During global auto commissioning (an announce with global = true), no DH Level
Adjust is sent until its timeout ends.

There is no repeat or refresh. A light that joins late or restarts keeps its level
until the next adjustment. A refused send isn't retried, because the next average
5 s later acts as the retry.

## Setting it up

- **Master:** press **T×3 + D×3** on one light in an Adaptiv group. This toggles
  the setting, which is off by default. Red blinks 3× either way, and the main
  light also blinks 3× when master turns on. Outside an Adaptiv group the light
  shows the fail blink and nothing changes.
- **Target lux:** set by the Twitch only, sent to the whole group: 0 (default,
  DH off), 300, 500 or 800. Only the master's target is used. A light added to
  the group later has 0 until the Twitch sends the target again.
- **Upgrades:** the first boot after upgrading from the calibration firmware clears
  the old master and calibration flags. Lights on different DH firmware don't follow
  each other, so upgrade a group together.

## The algorithm

```
calibrated_lux = sensor_average_lux × CALIBRATED_LUX_FACTOR (3.5 for now)
desired_level  = current_level × target_lux / calibrated_lux
level_step     = (desired_level − current_level) / 2, at most ±30 % of current_level
next_level     = current_level + level_step, kept between the floor and the ceiling
```

If that changes the level by **5 % or less**, nothing is sent.

- **Floor:** the min level, but at least 25 (10 %).
- **Ceiling:** the master's max trim.
- **Target out of reach:** the level stays at the floor or ceiling, and nothing is
  sent.
- **In-range window:** the 5 % rule leaves about 273–333 lux at a 300 target,
  455–556 at 500, and 727–889 at 800.
- **Large changes:** 80 % down to 10 % takes at least 6 adjustments, about 2.5 minutes.
- **Discarded readings:** any reading in which something changed the light or lit
  the red LED, along with the first half of its pair.

## When DH is disabled

Disabled is held in RAM on each light. A disabled master sends nothing, and a
disabled light ignores DH levels.

| DH is disabled by | DH is enabled again by |
|---|---|
| A received level command (Move to Level, Move, Step, Stop) | The light was off and occupancy turns it on (its own or another light's) |
| A received on/off command | Selecting the master on that light |
| A received scene recall | A target lux write |
| The D hold on that light | A restart |

- **Doesn't disable it:** trim, occupancy delay, sensitivity, 50 % mode and other
  settings.
- **Vacancy that ends at mid:** in EM mode or with a min level above 0, "off" means
  the end of the vacancy sequence, so occupancy after that enables DH.
- **Mid to on:** occupancy before the end of the vacancy sequence doesn't enable DH.
- **A switch turning the light on** doesn't enable DH.
- **If the master misses a command:** it keeps sending and the rest of the group
  follows, unless those lights received the command themselves.

## When a light ignores a level

A light also ignores a DH level if it's off, at mid or lower, below 10 %, suspended,
or not on an Adaptiv network or in a group. A firmware light change (occupancy wake,
dim to mid, off, flash) suspends DH for the ramp plus 1 s, and power-up suspends it
for 20 s. A shorter suspend doesn't cut a longer one short. Occupancy wakes to the
normal wake level, not the last DH level.

## One master per group

The light selected last is the master:

1. **Claim:** a new master multicasts a claim, retried if refused, and any other
   master that hears it stops being master.
2. **Safety net:** if a master missed the claim, it can still hear a DH level from
   the other. The one with the higher node ID then stops being master. In this
   case the remaining master may not be the one selected last.

## Flashes and trim

| Event | Now |
|---|---|
| Any flash (master selected, config write, firmware version flash) | Returns to the prior level, or off if the light was off |
| Trim raised, by button or config write | Goes to the new max once any flash ends, and that also becomes the wake level |
| Trim lowered | The level is only limited if it's above the new max; otherwise it stays put |
| Trim written with the same value | No change |
| Power-up | Still goes to max |
| Auto-commissioning ending | Still goes to max |

## 50 % mode

**T×2 + D×2** toggles this mode on one light. Nothing is sent to the group.

- **Turning it on:** halves that light's max, mid and min levels (max and mid not
  below 25) and halves its present level (not below 25).
- **Turning it off:** restores the saved levels. The max has risen, so the light
  goes to it.
- **If the master is in 50 % mode:** its own limit doesn't hold back the group. It
  uses the full saved max as the ceiling for levels it sends, and while held at
  its own limit it continues from the level it last sent. Other lights can rise
  above it, up to their own max.

## Messages on the air

All three commands are manufacturer-specific, with manufacturer ID **0x1574**,
client to server, default response disabled (frame control **0x15**). All values
are little-endian.

### DH Level Adjust and master claim

Multicast to the group, radius 3, on cluster **0x0400** (Illuminance Measurement).

| Command | Payload |
|---|---|
| **0xE0**, DH Level Adjust | Master's current level (u8), target level (u8, 1–255), duration in seconds (u8) |
| **0xDF**, master claim | None |

Receivers don't use the master's current level. Commands `0xDC`, `0xDD` and `0xDE`
from earlier firmware are ignored.

### Diagnostic command

Sent as a unicast to the debug target, normally a Twitch that prints it on its uart,
from any light with a debug target. It's checked every 20 s:
- **Level changed since the last packet:** sent, so a ramp gives a packet every 20 s.
- **No change:** sent every 120 s.

| Field | Value |
|---|---|
| Cluster | **0xFCDB** |
| Command | **0xDB** |
| Manufacturer ID | **0x1574** |
| Endpoints | 1 to 1 |
| APS options | Retry and route discovery on |

Payload, **15 bytes**:

| Byte | Type | Content |
|---|---|---|
| 0 | u32 | Lower 4 bytes of the light's EUI64 |
| 4 | u32 | Uptime, seconds (wraps after about 49.7 days) |
| 8 | u16 | Current lux, the average of the last 2 readings |
| 10 | u16 | Target lux |
| 12 | u8 | Current level |
| 13 | u8 | Mode flags: bit 0 = master, bit 1 = DH disabled |
| 14 | u8 | Last level this light sent to the group as master, 0 = none since a restart |

**Setting the debug target:** send the same cluster and command to the light, as
a unicast or multicast, with the 3-byte payload `0xAA`, then the address (u16).

- The target defaults to **0xFFFE**, which sends nothing. Sending 0xFFFE stops the
  messages.
- Only addresses below **0xFFFC** are sent to.
- The target isn't saved, so it returns to 0xFFFE after a restart.
- A light can tell the two payloads apart by length. One that receives a 15-byte
  message prints it.
