# NT-3 Light: Daylight Harvesting

This document describes how daylight harvesting (DH) works in the NT-3 light
firmware today, read from the source code. It covers normal operation once a
group has a DH master. The calibration process that currently chooses the
master is being replaced by a direct master selection, so it is only
summarized (appendix A).

- **Firmware:** branch `main`, commit `50fc164`, plus the working-tree
  changes of November 2026 (the occupancy send retry and the version changes).
  The DH code itself is unchanged from `50fc164`.
- **Source of truth:** the code. Where this document says "today", that is
  what the code does, not necessarily what was intended. Section 10 lists
  behaviors that look unintended or worth a decision.
- **Main files:** `Light/nwk_sensors/light_sensor.c` (DH logic),
  `Light/hal/hal_light_sensor.c` (sensor sampling), `Light/nwk_config/nwk_multicast.c`
  (the DH multicast), `level-control.c` in the SDK copy (`move_to_level_dhs()`
  and the DH suspends).
- **Levels** are Zigbee/PWM levels 0–254 (254 = 100 %, 25 = 10 %).

---

## 1. Overview

Daylight harvesting only runs on an **Adaptiv network**, in a **grouped**
light. One light in the group is the **DH master**. It is the only light
whose sensor is used:

1. Every light measures lux, but only the master uses it to control.
2. Every 20 s the master compares its lux with the **lux reference** and, if
   the difference is large enough, works out a new light level.
3. The master multicasts that level to the group (one message).
4. Every light in the group, the master included, ramps to that level over
   18.5 s.

The other lights never send DH messages in normal operation.

---

## 2. Configuration

| Item | Where it is set | Values |
|---|---|---|
| Lux reference | Adaptiv configuration cluster, attribute `ZCL_LIGHT_SENSOR_ATTRIBUTE_ID` → `save_light_lux_reference()`; saved in `TOKEN_APP_LIGHT_SENSOR_LUX` | 0, 300, 500 or 800 lux. Any other value becomes 300. **0 turns DH off** (sets `B_DISABLE_LIGHT_SENSOR`). |
| DH master | Flag `B_DHS_MASTER_LIGHT` (non-volatile app flags) | Set by calibration today (`am_i_master()`). |
| Calibrated | Flag `B_DHS_CALIBRATED` | Set when calibration finishes. A light that is not calibrated neither controls nor follows. |
| Gain | `TOKEN_APP_DHS_GAIN` (int16, default 1) | Set by calibration (`am_i_master()`), see section 5. |
| Disabled | Flag `B_DISABLE_LIGHT_SENSOR` | Set by a lux reference of 0, and during calibration. |

Today, writing a non-zero lux reference **starts calibration**
(`save_light_lux_reference()` clears the master and calibrated flags and enters
`dhs_init_cal_state`). With the new master selection, writing the reference
should only change the reference.

---

## 3. Measuring lux (every light)

Defined in `Light/hal/hal_light_sensor.h`; `NEW_DHS` is defined in
`NT_3_app.h`, which selects these values:

| Item | Value |
|---|---|
| Sensor reading | every 2.5 s (`LIGHT_SENSOR_HAL_SAMPLE_TIME_MS`) |
| Average | 8 readings (`N_LIGHT_SAMPLES`) |
| DH logic runs | every **20 s** (`LIGHT_SENSOR_UPDATE_AVERAGE_MS`), when the 8-reading buffer wraps |
| Calibration sampling | every 500 ms (4 s average) |

- Readings are taken with the green LED off, so the LED does not light the
  sensor. While the green LED is lit, one dark reading per 20 s is taken and
  repeated (`light_sensor_read_handler()`).
- **Lux used by DH:** on Adaptiv, `lux = 3.5 × raw average`. (Daintree uses a
  piecewise curve, but DH does not run on Daintree.)
- **Illuminance attribute** (endpoint 3, cluster 0x0400, Measured Value):
  `10000 × log10(lux) + 1`, updated every 20 s on every network. This is for
  gateways; DH does not use it.

---

## 4. When DH is allowed

`allow_light_sensor_control()` returns true only when all of these inhibit
bits are clear (`get_dhs_inhibit_bits()`). The master checks them every 20 s.

| Bit | Inhibited when |
|---|---|
| 0 | Not on an Adaptiv network |
| 1 | Not calibrated (`B_DHS_CALIBRATED` clear) |
| 2 | Light is off (On/Off attribute = 0) |
| 3 | Not in a group |
| 4 | DH suspended (`suspend_light_sensor_control()`, section 8) |
| 5 | Lux reference = 0 |
| 6 | Light level below 10 % (`MIN_ALLOWABLE_LIGHT_LEVEL`, 25) |
| 7 | Occupancy at MID or lower |
| 8 | Dimming suspension active (`suspend_light_sensor_control_dimming()`) |
| 9 | DH disabled (`B_DISABLE_LIGHT_SENSOR`) |
| 10 | Not the master |

These only stop the **master** from sending. A light that receives a DH
message does **not** check them (section 7).

After power up the light stays in `sensor_idle_state` for one 20 s period,
then moves to `sensor_active_state` the first time DH is allowed.

---

## 5. The master's control loop

`sensor_active_state()`, every 20 s, on the master only:

1. Stop if DH is not allowed (section 4).
2. Stop if the last change is still ramping: less than `dim_time_dsec`
   (18.5 s) since the last update.
3. `error = reference − lux`. Stop if `|error|` is less than **5 % of the
   reference** (15 lux at 300, 25 at 500, 40 at 800).
4. Limit the error to **±100 lux** (`MAX_LUX_ERROR`).
5. `new level = present level + gain × error`.
6. Keep the new level between the light's **min** and **max** trim levels.
   (In 50 % mode the stashed full max is used.)
7. Stop if the level would change by less than **5 % of full scale**
   (12 levels).
8. Otherwise: ramp time = 18.5 s, record the time, and **multicast the new
   level** (section 6).

| Constant | Value | Where |
|---|---|---|
| Logic period | 20 s | `LIGHT_SENSOR_UPDATE_AVERAGE_MS` |
| Ramp time | 18.5 s (185 ds) | `(LIGHT_SENSOR_UPDATE_AVERAGE_MS − 1500) / 100` |
| Lux deadband | 5 % of reference | `sensor_active_state()` |
| Lux error limit | ±100 lux | `MAX_LUX_ERROR`, `light_sensor.h` |
| Minimum level change | 12 levels (5 % of 254) | `sensor_active_state()` |
| Gain | `TOKEN_APP_DHS_GAIN`, default 1 | set by calibration |

**Gain as calibrated today** (`am_i_master()`):
`gain = (lux at max − lux at min) / (254 − 25)`, in integer arithmetic, with a
minimum of 1. That is **lux per level step**. Section 10 explains why using it
as a multiplier in step 5 is a problem.

---

## 6. The DH message

The master sends one Adaptiv manufacturer-specific command:

- **Cluster** 0x0400 (Illuminance Measurement), **command** 0xDD
  (`ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID`), manufacturer 0x1574, cluster-specific,
  client to server, default response disabled.
- Sent by `tx_light_sensor_data()` → `tx_multicast()`, to every multicast
  binding of the light's endpoint (normally one group), **radius 3**
  (`GROUP_MULTICAST_RADIUS`).
- Sent once. There is no retry: if the stack refuses it, it is lost (but see
  section 7 for why the master then tries again 20 s later).

| Byte | Content |
|---|---|
| 0–1 | Sender ID: the first two bytes of its EUI64 |
| 2–3 | Sender's lux (16-bit, little-endian) |
| 4 | Sender's current level |
| 5 | **New level for the group** |
| 6 | **Ramp time**, deciseconds (185 = 18.5 s) |
| 7 | Low 4 bits: DH mode flags (bit 0 = sender is master). High 4 bits: calibration sample counter |

Only bytes 5–7 matter after calibration; bytes 0–4 feed the neighbor table
used by calibration.

---

## 7. Lights that receive the message

`update_light_sensor_neighbor_table()`, for every DH message received,
including the master's own (the stack delivers a light's own multicast back to
it):

1. Store the sender's lux, level and mode in the neighbor table (10 entries,
   only used by calibration).
2. If the sender is the master (mode bit 0) **and** this light is calibrated:
   `move_to_level_dhs(new level, 18.5 s)`.

`move_to_level_dhs()` (in `level-control.c`):

- Does nothing while a switch dimming command is active (`dimming_button_active`).
- Limits the level to this light's max trim level.
- Runs **Move to Level (with On/Off)**: it turns the light on if it is off.

The receiving light does **not** check its own DH inhibits (section 4): its
on/off state, occupancy state, suspensions or the DH disable flag.

**The master moves its own light only through this path.** `sensor_active_state()`
never changes the master's level directly; it relies on receiving its own
multicast. If the multicast is refused by the stack, the master does not move
either, and the next 20 s cycle simply computes the same correction again.

---

## 8. Interaction with other features

| Event | Effect on DH |
|---|---|
| Power up | DH suspended (see section 10, item 6), and on Adaptiv for 5 s by occupancy init. The first DH decision comes after one 20 s idle period. |
| Network level command (Move to Level, Move, Step, Stop, with or without On/Off) | `suspend_light_sensor_control()` is called with the unoccupied delay, then again with 5 s. The second call replaces the first, so **DH is suspended for 5 s**. |
| Switch dimming (Move or Step, with or without On/Off) | Additionally sets the *dimming* suspension for the **unoccupied delay** (inhibit bit 8). DH stops until then. |
| Any light change by the firmware (`move_to_level_onoff()`: occupancy wake, dim to mid, off, flashing) | `suspend_all_sensors(ramp + 1 s)`: DH suspended for the ramp plus 1 s. |
| Occupancy at MID or OFF | Master stops (inhibit bit 7). A DH message from a master that is still ON will still move a light at MID or OFF (section 7). |
| Light below 10 % | Master stops (inhibit bit 6). |
| 50 % mode | The master limits the group level with the full (stashed) max; each receiver then limits it with its own max. |

---

## 9. Network traffic

| Situation | DH multicasts per group |
|---|---|
| Steady daylight, lux within the deadband | none |
| Daylight changing (sunrise, sunset, clouds) | at most **one per 20 s** (3 per minute) |
| Other lights | none (they only receive) |
| Calibration | many (appendix A; being replaced) |

Each multicast is repeated by every router within 3 hops. If a group sends a
DH message every 20 s for a long time, the cause is likely the control loop
(section 10, items 1 and 2), not normal daylight change.

---

## 10. Issues found

1. **The gain is used inverted.** Calibration computes the gain as lux per
   level step, but the loop multiplies the lux error by it to get level
   steps. The effective loop gain is therefore gain², not 1. With gain = 1
   (the default, or any calibration that measured less than 2 lux per step)
   it happens to work. With gain = 2 or more, each correction is about four
   times too large: the level swings between limits and the **master sends a
   DH multicast every 20 s** for as long as daylight is in range. It should
   divide by the gain (or the gain should be stored as steps per lux).
2. **The average includes the ramp.** After a change the light ramps for
   18.5 s, and the next 20 s average is mostly taken during that ramp, so it
   sees only about half of the change and corrects again. This adds extra
   steps, and extra multicasts, to every change, even with the right gain.
   The check "let ramp finish" compares times, not which samples are in the
   average.
3. **Without calibration there is no gain.** The gain is computed only by
   calibration. With master selection replacing calibration, the gain must
   come from somewhere else (a fixed value, or measured when the master is
   chosen).
4. **Receiving lights ignore their own state.** `move_to_level_dhs()` uses
   Move to Level *with On/Off* and does not check the receiver's DH inhibits.
   A light that was switched off on its own, or is at MID because it missed
   an occupancy message, is turned back on by the master's next DH message.
5. **The master depends on hearing itself, and nothing is retried.** If the
   multicast is refused, the master corrects itself 20 s later. If it is
   sent but some lights miss it, those lights stay at the old level until
   daylight changes enough for another message, which may be a long time.
6. **Power-up suspension uses the wrong units.** `light_sensor_init()` calls
   `suspend_light_sensor_control(LIGHT_SENSOR_UPDATE_AVERAGE_MS)`, passing
   20000 (milliseconds) where deciseconds are expected: 2000 s, about
   33 minutes. On Adaptiv the occupancy init's 5 s suspension replaces it if
   it runs later; the order of the two decides which one wins.
7. **A level command's long suspension is overwritten.** The level-control
   code suspends DH for the unoccupied delay and then immediately for 5 s.
   Only the 5 s remains. If the long suspension was intended after a network
   level command, it is not in effect (switch dimming still gets it through
   the separate dimming suspension).
8. **Unused code.** `tx_light_sensor_suspend_time()` (command 0xDC) is never
   called; its receive handler remains. The neighbor table and bytes 0–4 of
   the DH message are only used by calibration.

---

## 11. Ways to reduce broadcasting

In order of expected effect:

1. **Fix the control loop** (issues 1 and 2): divide by the lux-per-step gain
   (or use a fixed, known gain), and wait for an average that starts after
   the ramp ends (skip one 20 s period after each change, or restart the
   average when the ramp finishes). A correct loop needs about one message
   per real daylight change instead of several, or one every 20 s.
2. **Limit the rate:** a minimum time between DH updates (for example 60 s)
   and/or a wider lux deadband (for example 10 %). Daylight changes slowly, so
   a 20 s response is not needed, and the worst case becomes one message per
   minute even if the loop misbehaves.
3. **Master applies its own level directly** and ignores its own multicast.
   This does not reduce traffic, but removes the dependency on hearing itself
   and allows a retry.
4. **Retry a refused send** the same way as the group occupancy messages
   (about once a second, up to 5 attempts, held while broadcast traffic is
   heavy).
5. **Remove calibration-only parts** once master selection replaces it: the
   neighbor table, bytes 0–4 and the counter of the DH message, the
   calibration states, `send_dhs_table_info()`, and the unused suspend
   command. The message only needs the new level and the ramp time.
6. **Receivers check their own state** (issue 4): ignore a DH message unless
   this light is on, at the ON occupancy level, and not suspended. This
   fixes a behavior problem rather than traffic.

---

## Appendix A: Calibration today (being replaced)

Writing a non-zero lux reference starts calibration on every light in the
group:

1. All lights go to 100 % and sample lux every 500 ms; each light multicasts
   its readings at random times every few seconds (`send_dhs_table_info()`).
2. All lights go to 10 % and do the same.
3. Each light builds the same neighbor table of every light's high and low
   lux, and picks as master the light whose readings are closest to the group
   averages (`am_i_master()`). That light stores the gain and sets
   `B_DHS_MASTER_LIGHT`; every light sets `B_DHS_CALIBRATED`.
4. The master flashes twice to show it was chosen.

The many multicasts in this process are the "lots of broadcasting" that
master selection removes.
