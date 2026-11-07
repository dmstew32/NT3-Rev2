# NT-3 Light: Daylight Harvesting

This document describes how daylight harvesting (DH) works in the NT-3 light
firmware, read from the source code. It has not yet been tested on real lights.
`NT3_DH_Summary.md` is the short version.

- **Main files:** `Light/nwk_sensors/dh_control.c` (the master's sampling and
  algorithm), `Light/nwk_sensors/light_sensor.c` (what surrounds it),
  `Light/hal/hal_light_sensor.c` (sensor readings),
  `Light/nwk_config/nwk_multicast.c` (the multicasts),
  `Light/nwk_mgmt/dh_debug.c` (the diagnostic unicast), `level-control.c`,
  `on-off.c` and `scenes.c` in the SDK copy (what disables DH),
  `Light/occupancy/occ_group.c` (what enables it),
  `Light/buttons/button_actions.c` (master selection).
- **Levels** are Zigbee levels 1–255. Level 0 is never sent.

---

## 1. Overview

DH only runs on an **Adaptiv network**, in a **grouped** light. There is no
calibration. One light in the group is the **DH master**, chosen with the buttons.
It is the only light whose sensor is used for control.

1. The master reads lux every 2.5 s and averages every 2 readings.
2. It runs each average through the algorithm.
3. If the level would change by more than 5 %, it multicasts a DH Level Adjust.
4. Every light that is able to follow, the master included, ramps to that level
   over 20 s.
5. Readings during the ramp are discarded.

---

## 2. Configuration

| Item | Where it is set | Values |
|---|---|---|
| Target lux | Twitch only, sent to the whole group: Adaptiv configuration cluster, attribute `ZCL_LIGHT_SENSOR_ATTRIBUTE_ID` → `save_light_lux_reference()`; saved in `TOKEN_APP_LIGHT_SENSOR_LUX` | 0 (default, DH off), 300, 500 or 800 lux. Any other value becomes 300. Only the master's target is used. A light added to the group later has 0 until the Twitch sends the target again, so if it is then selected as master, DH stays off. |
| DH master | **T×3 + D×3** buttons, flag `B_DHS_MASTER_LIGHT` (non-volatile) | Off by default. The sequence toggles it. |

| Master toggle result | Red LED | Main light |
|---|---|---|
| Now the master | Blinks 3× | Blinks 3×, then returns to its prior level |
| No longer the master | Blinks 3× | No change |
| Not in a group on an Adaptiv network | Fail indication | No change, setting unchanged |

**One master per group:** the light selected last is the master.

1. A light that becomes the master multicasts a claim (section 7), retried about
   once a second up to 5 attempts. Any other master that hears it stops being the
   master, with no indication. The new master's blink is the confirmation.
2. If a master misses the claim (powered off, message lost), both run until one
   hears a level from the other. The one with the higher node ID then stops being
   the master. In this case the light left as master may not be the one selected
   last, but the group has one working master.

---

## 3. Sampling

| Item | Value |
|---|---|
| Sensor check | every 2.5 s (`LIGHT_SENSOR_HAL_SAMPLE_TIME_MS`) |
| Measurement | about 180 ms, with the green LED off |

**The master** takes a real reading at every check. During occupancy, its green LED
goes off for about 200 ms every 2.5 s, which also shows which light is the master.
This applies whenever the master setting is on, even when DH can't currently act.

**Other lights** keep the earlier behaviour: while green is lit, one real reading
per 20 s, with the rest repeating it.

**Illuminance attribute** (endpoint 3, cluster 0x0400, Measured Value): every light
still updates this from an 8-reading average every 20 s, on every network. DH does
not use it.

**Discarded readings:** a reading is discarded, along with the first half of its
pair, if DH isn't allowed (section 5), or if since the last reading something changed
the light (a suspend) or lit the red LED, which reaches the sensor.

---

## 4. The algorithm

`dh_next_level()`, with every 2-reading average:

```
calibrated_lux = sensor_average_lux × CALIBRATED_LUX_FACTOR
desired_level  = current_level × target_lux / calibrated_lux
level_step     = (desired_level − current_level) / 2, at most ±30 % of current_level
next_level     = current_level + level_step, kept between the floor and the ceiling
```

If that changes the level by **5 % or less**, it returns the current level and
nothing is sent.

| Constant | Value |
|---|---|
| `CALIBRATED_LUX_FACTOR` | 3.5, until it is measured |
| `DEFAULT_DH_RAMP_DURATION_SEC` | 20 s |
| `DH_MAX_STEP_PERCENT` | 30 % |
| `DH_MIN_STEP_PERCENT` | 5 % |
| Floor | The min level, but at least 25 (10 %) |
| Ceiling | The master's max trim. In 50 % mode, see below |

**Behavior:**

- No gain is needed. The more of the lux that is daylight, the smaller the
  correction, and it does not overshoot.
- A target the lights can't reach leaves the level at the ceiling or floor, and
  nothing more is sent.
- 80 % down to 10 % takes at least 6 adjustments, about 2.5 minutes.
- The 5 % rule leaves about 273–333 lux at a 300 target, 455–556 at 500, and 727–889
  at 800, whatever the daylight share.
- With only 2 readings per average, noise reaches the algorithm more easily. Averaging
  4 readings is an option to revisit after testing.

**Master in 50 % mode:** the rest of the group may not be in 50 % mode, so the
master's own halved max is not the group's. It uses the stashed full max as the
ceiling. While it is held at its own limit, it continues from the level it last
sent, not from its own level.

---

## 5. When DH is allowed

`get_dhs_inhibit_bits()` returns the reasons DH is stopped. The master needs all
of them clear. A light following the master ignores bits 5 and 10, because it
needs neither a target nor to be the master.

| Bit | Inhibited when |
|---|---|
| 0 | Not on an Adaptiv network |
| 1 | **DH disabled** (section 6) |
| 2 | Light is off (On/Off attribute = 0) |
| 3 | Not in a group |
| 4 | DH suspended (`suspend_light_sensor_control()`, section 9) |
| 5 | Target lux = 0 |
| 6 | Light level below 10 % (`MIN_ALLOWABLE_LIGHT_LEVEL`, 25) |
| 7 | Occupancy at MID or lower |
| 8 | Unused |
| 9 | DH disabled while the firmware version is displayed (`B_DISABLE_LIGHT_SENSOR`) |
| 10 | Not the master |

---

## 6. Disabled and enabled

`dh_disabled` is held in RAM on each light, and every light tracks it for itself.

**Disabled by:**

| Event | Where |
|---|---|
| A received level command (Move to Level, Move, Step, Stop, with or without On/Off) | `level-control.c`, command parser |
| A received on/off command | `on-off.c`, command parser |
| A received scene recall | `scenes.c`, command parser |
| The D hold on that light | `on_hold_dim()` |

Trim, occupancy delay, sensitivity, 50 % mode and other settings do not disable it.
DH's own level changes and occupancy messages don't either.

**Enabled by:**

| Event | Where |
|---|---|
| The light was off and occupancy turns it on, from its own sensor or another light's wake | `woke_from_off` in `occ_group.c` |
| Selecting the master on that light | `dh_set_master()` |
| A target lux write | `save_light_lux_reference()` |
| A restart | `light_sensor_init()` |

- **"Off" includes vacancy that ends at mid:** in EM mode or with a min level above
  0, the occupancy state is OFF while the light stays at mid, so occupancy after that
  enables DH.
- **Switched off, then occupancy:** this enables DH, because the occupancy state is
  OFF.
- **Mid to on:** occupancy before the end of the vacancy sequence doesn't enable DH.
- **A switch turning the light on** doesn't enable DH.
- **Occupancy sensor off (delay 0):** vacancy never happens, so only the other three
  events enable DH.
- **If the master misses a command:** it keeps sending, and lights that also missed
  it follow.

---

## 7. Messages

All are manufacturer-specific, manufacturer **0x1574**, cluster-specific, client to
server, default response disabled (frame control 0x15). All values are little-endian.

**DH Level Adjust:** cluster **0x0400**, command **0xE0**
(`ZCL_DH_LEVEL_ADJUST_COMMAND_ID`), `tx_dh_level_adjust()`, multicast to every
binding of the light's endpoint, **radius 3**.

| Byte | Content |
|---|---|
| 0 | Master's current level. Receivers don't use it |
| 1 | **Target level**, 1–255 |
| 2 | **Duration**, seconds (`DEFAULT_DH_RAMP_DURATION_SEC`) |

**Master claim:** cluster **0x0400**, command **0xDF**
(`ZCL_DH_MASTER_CLAIM_COMMAND_ID`), `tx_dh_master_claim()`, no payload, same
addressing.

Commands 0xDC, 0xDD and 0xDE from earlier firmware are ignored, and older firmware
ignores 0xE0. Lights on different DH firmware do not follow each other, so upgrade a
group together.

---

## 8. The ramp timer and receiving a level

**The master** starts the ramp timer when the stack accepts the multicast, and
discards readings until it ends. The first two readings after that are the next
average, so a command goes out about every 22.5–25 s at most.

**The master moves its own light only by receiving its own multicast**, like the
occupancy keepalive. If the multicast does not come back, the master has not moved.
When the timer ends, it works out the level again and sends it, which also covers
lights that missed the first one.

**During global auto commissioning** (section 9) the master sends no level. It works
the level out again at each average, so DH resumes as soon as that ends.

**A refused send** starts no timer and isn't retried. The next average, 5 s later,
acts as the retry. Heavy broadcast traffic doesn't hold a level.

**No repeat or refresh:** a light that joins late or restarts keeps its level until
the next adjustment, which could be a long time in steady daylight.

**Receiving:** `dh_rx_level_adjust()` runs for every DH Level Adjust received,
including the master's own.

1. A master that hears a level from a lower node ID stops being the master.
2. A light with any inhibit set (section 5) ignores the level. So a light that is off,
   at MID, suspended or has DH disabled stays as it is.
3. Otherwise it calls `move_to_level_dhs(target, duration × 10)`, which limits the
   level to this light's max and does nothing while a switch dimming command is active.

---

## 9. Interaction with other features

| Event | Effect on DH |
|---|---|
| Power up | DH suspended for 20 s. |
| Upgrade from the firmware with calibration | At boot, the master setting and calibration flags are cleared once (bits 8–11). A calibration cut short by a power loss no longer leaves occupancy disabled. Select masters again. |
| Any light change by the firmware (`move_to_level_onoff()`: occupancy wake, dim to mid, off, flashing) | `suspend_all_sensors(ramp + 1 s)`: DH suspended for the ramp plus 1 s. |
| Occupancy wake | The light wakes at the wake level, not the last DH level. In a bright room DH then brings it down. |
| Occupancy at MID or OFF | Master stops, and a light at MID or OFF ignores DH levels. |
| Red LED on | The reading is not used, because the LED reaches the sensor. |
| Global auto commissioning announce (global = true) | No DH Level Adjust multicasts until its timeout ends. Master selection still works. |
| Flashes (master selected, config write, firmware version match) | The light returns to its prior level, or off if it was off. Ending auto commissioning still goes to max. |
| Max level (trim) raised, by the buttons or a config write | The light goes to the new max level, which is also the level occupancy wakes to. DH then adjusts it. A lower max only limits the level. |

`suspend_light_sensor_control()` takes **milliseconds** (`uint32_t`). A shorter suspend
does not cut a longer one short. `suspend_all_sensors()` still takes deciseconds,
because its callers pass ZCL transition times.

---

## 10. Diagnostic unicast

`dh_debug_send()` (`Light/nwk_mgmt/dh_debug.c`) sends one unicast to the debug target,
normally a Twitch that prints it on its uart. Any light with a debug target sends it.
`dhs_debug_tick()` (`light_sensor.c`) checks every 20 s:

- **Level changed since the last packet:** sent, so a ramp gives a packet every 20 s.
- **No change:** sent every 120 s.
- **Not sent** (no target, or refused): tried again at the next check, so a new target
  gets its first packet within 20 s.

A DH Level Adjust doesn't send a packet by itself, so a change shows up within 20 s.

**Frame:** cluster 0xFCDB, command 0xDB, manufacturer 0x1574, endpoint 1 to endpoint
1, APS retry and route discovery on.

**Target:** 0xFFFE by default, which sends nothing. Addresses below 0xFFFC are sent
to. It is not saved, so it returns to 0xFFFE after a restart.

| Byte | Size | Content |
|---|---|---|
| 0 | u32 | Lower 4 bytes of this light's EUI64 |
| 4 | u32 | Uptime, seconds (restarts after about 49.7 days) |
| 8 | u16 | Current lux, the average of the last 2 readings |
| 10 | u16 | Target lux |
| 12 | u8 | Current level |
| 13 | u8 | Mode flags: bit 0 = master, bit 1 = DH disabled |
| 14 | u8 | Last level this light sent to the group as master, 0 = none since a restart |

**Setting the target:** send the same cluster and command to the light, as a unicast or
multicast, with the 3-byte payload `0xAA`, then the address, little-endian. Address
0xFFFE stops the messages. A light that receives a 15-byte data message prints it.

---

## 11. Network traffic

| Situation | Multicasts per group |
|---|---|
| Level would change by 5 % or less | none |
| Daylight changing | at most one per ramp, about every 22.5–25 s |
| Target out of reach, level at the floor or ceiling | none |
| DH disabled | none |
| Other lights | none (they only receive) |

| Situation | Unicasts |
|---|---|
| Debug target set, level changing | one per 20 s |
| Debug target set, level steady | one per 120 s |

Each multicast is repeated by every router within 3 hops.
