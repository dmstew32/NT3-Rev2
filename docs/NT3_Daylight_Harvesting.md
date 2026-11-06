# NT-3 Light: Daylight Harvesting

This document describes how daylight harvesting (DH) works in the NT-3 light
firmware, read from the source code.

- **Main files:** `Light/nwk_sensors/dh_control.c` (the master's control logic),
  `Light/nwk_sensors/light_sensor.c` (what surrounds it),
  `Light/hal/hal_light_sensor.c` (sensor sampling),
  `Light/nwk_config/nwk_multicast.c` (the DH multicast), `level-control.c` in the
  SDK copy (`move_to_level_dhs()` and the suspends),
  `Light/buttons/button_actions.c` (master selection).
- **Levels** are Zigbee/PWM levels 0–254 (254 = 100 %, 25 = 10 %).

---

## 1. Overview

Daylight harvesting only runs on an **Adaptiv network**, in a **grouped** light.
There is no calibration. One light in the group is the **DH master**, chosen
with the buttons. It is the only light whose sensor is used:

1. Every light measures lux, but only the master uses it to control.
2. At most once every 60 s, the master compares its lux with the **target lux**.
   If it is more than 50 lux away, it works out a new level.
3. The master multicasts that level to the group.
4. Every light in the group that is on at its normal level, the master included,
   ramps to that level over 18.5 s.

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

1. A light that becomes the master multicasts a claim (section 6), retried like
   a level. Any other master that hears it stops being the master, with no
   indication. The new master's blink is the confirmation.
2. If a master misses the claim (powered off, message lost), both run until one
   hears a level from the other. The one with the higher node ID then stops
   being the master. In this case the light left as master may not be the one
   selected last, but the group has one working master.

While lux is within the deadband neither master sends, so a conflict goes
unnoticed until one of them sends a level. Both are silent until then.

---

## 3. Measuring lux (every light)

| Item | Value |
|---|---|
| Sensor reading | every 2.5 s (`LIGHT_SENSOR_HAL_SAMPLE_TIME_MS`) |
| Average | 8 readings (`N_LIGHT_SAMPLES`) |
| New average | every 20 s (`LIGHT_SENSOR_UPDATE_AVERAGE_MS`) |

- Readings are taken with the green LED off, so the LED does not light the
  sensor. While the green LED is lit, one dark reading per 20 s is taken and
  repeated (`light_sensor_read_handler()`).
- **Lux used by DH:** on Adaptiv, `lux = 3.5 × raw average`.
- **Illuminance attribute** (endpoint 3, cluster 0x0400, Measured Value):
  `10000 × log10(lux) + 1`, updated every 20 s on every network. This is for
  gateways; DH does not use it.

---

## 4. When DH is allowed

`get_dhs_inhibit_bits()` returns the reasons DH is stopped. The master needs all
of them clear. A light following the master ignores bits 5 and 10, because it
needs neither a target nor to be the master.

| Bit | Inhibited when |
|---|---|
| 0 | Not on an Adaptiv network |
| 1 | Unused |
| 2 | Light is off (On/Off attribute = 0) |
| 3 | Not in a group |
| 4 | DH suspended (`suspend_light_sensor_control()`, section 8) |
| 5 | Target lux = 0 |
| 6 | Light level below 10 % (`MIN_ALLOWABLE_LIGHT_LEVEL`, 25) |
| 7 | Occupancy at MID or lower |
| 8 | Dimming suspension active (`suspend_light_sensor_control_dimming()`) |
| 9 | DH disabled while the firmware version is displayed (`B_DISABLE_LIGHT_SENSOR`) |
| 10 | Not the master |

---

## 5. The master's control loop

`dh_master_on_average()`, with every 20 s average. No gain or calibration is
needed: lux is daylight plus light from the group, and the light from the group
follows the level, so the master scales the level by how far lux is from the
target.

1. **Not allowed** (section 4), or something changed the light or lit the red
   LED during this average: do nothing, drop any pending repeat, and skip the next
   average as well, because it may still include the change.
2. **Waiting:** two averages are skipped after each send (section 7).
3. **Within 50 lux of the target** (`DH_LUX_DEADBAND`): no new level.
4. **Otherwise** (`dh_next_level()`):

   ```
   desired = level × target / lux
   step    = (desired − level) / 2, at most ±30 % of the level
   next    = level + step, kept between the floor and the ceiling
   ```

5. If the level changes, send it. If not, and the last new level has not been
   repeated yet, send that again (section 7). Otherwise, about every 10 minutes,
   send the present level as a refresh.

| Constant | Value | Why |
|---|---|---|
| Update period | 60 s | One message per minute at most |
| Ramp time | 18.5 s (185 ds) | `DH_RAMP_DS` |
| Lux deadband | ±50 lux | `DH_LUX_DEADBAND` |
| Step limit | 30 % of the level, per update | `DH_MAX_STEP_PERCENT` |
| Correction factor | 1/2 | Stays stable if light output is not linear with the level |
| Floor | The min level, but at least 25 (10 %) | |
| Ceiling | The master's max level | In 50 % mode, see below |

**Behavior of the equation:**

- In a dark room, the level moves directly toward the target, half the remaining
  distance per update.
- The more of the lux that is daylight, the smaller each correction, and it does
  not overshoot.
- A target the lights can't reach leaves the level at the ceiling or floor. The
  level no longer changes there, so only the refresh is sent.
- A large change takes minutes: 80 % down to 10 % takes at least 6 updates.

**Master in 50 % mode:** the rest of the group may not be in 50 % mode, so the
master's own halved max is not the group's. It uses the stashed full max as the
ceiling. While it is held at its own limit, it continues from the level it last
sent, not from its own level.

**The numbers come from a simulation** of rooms with assumed fixture output and
response curves (`test_dh_control` runs a smaller version). They have not been
measured on a real group yet.

---

## 6. The DH message

The master sends one Adaptiv manufacturer-specific command:

- **Cluster** 0x0400 (Illuminance Measurement), **command** 0xDE
  (`ZCL_DH_LEVEL_COMMAND_ID`), manufacturer 0x1574, cluster-specific, client to
  server, default response disabled.
- Sent by `tx_dh_level()`, to every multicast binding of the light's endpoint
  (normally one group), **radius 3** (`GROUP_MULTICAST_RADIUS`).

| Byte | Content |
|---|---|
| 0 | **New level for the group** |
| 1 | **Ramp time**, deciseconds (185 = 18.5 s) |

A light selected as master sends **command 0xDF**
(`ZCL_DH_MASTER_CLAIM_COMMAND_ID`) the same way, with no payload
(`tx_dh_master_claim()`).

Commands 0xDC and 0xDD from the firmware with calibration are ignored, and older
firmware ignores 0xDE and 0xDF. Lights with old and new firmware in one group do not
follow each other, so upgrade a group together.

---

## 7. Sending

- **Timing:** a send happens when an average ends, so the 18.5 s ramp falls in
  the next average. The second average is free of the ramp, and the third, 60 s
  after the send, is the one used.
- **Refused by the stack:** tried again about once a second (1 s ± 250 ms), up to
  5 attempts, the same as the group occupancy messages. While broadcast traffic
  is heavy the first 3 attempts are held. If all fail, the level is worked out
  again at the next average. If DH stops being allowed meanwhile, the send is
  dropped.
- **Repeat:** a new level is sent once more at the next update if there is no
  newer level, for a light that missed it.
- **Refresh:** after about 10 minutes (`DH_REFRESH_AVERAGES`) with no send, the
  present level is sent again, not repeated. A light that restarted or missed
  both sends comes back to the group's level within that time. The time only
  runs while DH is allowed.

---

## 8. Lights that receive the message

`dh_rx_level_command()` runs for every DH message received, including the
master's own. **The master moves its own light only this way**: the stack
delivers its own multicast back to it, like the occupancy keepalive.

A light follows only if its inhibits are clear (section 4). A light that is off,
at MID, suspended, or recently dimmed by a switch stays as it is. A master that
hears a level from another light resolves the conflict first (section 2), whether
or not it follows the level.

`move_to_level_dhs()` (in `level-control.c`):

- Does nothing while a switch dimming command is active (`dimming_button_active`).
- Limits the level to this light's max level.
- Runs **Move to Level (with On/Off)**.

---

## 9. Interaction with other features

| Event | Effect on DH |
|---|---|
| Power up | DH suspended for 20 s. The master's first level comes after at least two 20 s averages. |
| Upgrade from the firmware with calibration | At boot, the master setting and calibration flags are cleared once (bits 8–11). A calibration cut short by a power loss no longer leaves occupancy disabled. Select masters again. |
| Network level command (Move to Level, Move, Step, Stop, with or without On/Off) | DH suspended for 2 minutes (`DH_SUSPEND_AFTER_LEVEL_CMD_MS`). DH then adjusts the level again. |
| Switch dimming (Move or Step, with or without On/Off) | Additionally suspended for the **unoccupied delay** (inhibit bit 8), so DH does not fight the person who set the level. |
| Any light change by the firmware (`move_to_level_onoff()`: occupancy wake, dim to mid, off, flashing) | `suspend_all_sensors(ramp + 1 s)`: DH suspended for the ramp plus 1 s. |
| Occupancy wake | The light wakes at the wake level, not the last DH level. In a bright room DH then brings it down over a few minutes. |
| Occupancy at MID or OFF | Master stops, and a light at MID or OFF ignores DH messages. |
| Red LED on | The average is not used, because the LED reaches the sensor. |
| Flashes (master selected, config write, firmware version match) | The light returns to its prior level, or off if it was off. Ending auto commissioning still goes to max. |
| Max level (trim) raised, by the buttons or a config write | The light goes to the new max level, which is also the level occupancy wakes to. DH then adjusts it. A lower max only limits the level. |

`suspend_light_sensor_control()` and `suspend_light_sensor_control_dimming()` take
**milliseconds** (`uint32_t`). A shorter `suspend_light_sensor_control()` does not
cut a longer one short. `suspend_all_sensors()` still takes deciseconds,
because its callers pass ZCL transition times.

---

## 10. Debug unicast

Each time the master sends a DH level, including a repeat, it also calls
`dh_debug_send()` (`Light/nwk_mgmt/dh_debug.c`). That sends one unicast to the
debug target, normally a Twitch that prints it on its uart.

- **Cluster** 0xFCDB, **command** 0xDB, manufacturer 0x1574, cluster-specific,
  client to server, default response disabled, endpoint 1 to endpoint 1.
- **Target:** 0xFFFE by default, which sends nothing. Addresses below 0xFFFC are
  sent to. It is not saved, so it returns to 0xFFFE after a restart.

| Byte | Size | Content |
|---|---|---|
| 0 | u32 | Lower 4 bytes of this light's EUI64 |
| 4 | u32 | Uptime, seconds (restarts after about 49.7 days) |
| 8 | u16 | Current lux |
| 10 | u16 | Target lux |
| 12 | u8 | Current level, when the DH level is sent, so before that level's ramp |
| 13 | u8 | Mode flags, bit 0 = master |
| 14 | u8 | New level, the one this DH level message sent to the group |

All values are little-endian.

**Setting the target:** send the same cluster and command to the master, as a
unicast or multicast, with the 3-byte payload `0xAA`, then the address,
little-endian. Address 0xFFFE stops the messages. A light that receives a
15-byte data message prints it.

---

## 11. Network traffic

| Situation | DH multicasts per group |
|---|---|
| Steady daylight, lux within the deadband | one refresh per 10 minutes |
| Daylight changing (sunrise, sunset, clouds) | at most **one per 60 s** |
| After the last change | one repeat |
| Target out of reach, level at the floor or ceiling | one refresh per 10 minutes |
| Other lights | none (they only receive) |
| Debug target set | one unicast per DH level, including repeats and refreshes, from the master |

Each multicast is repeated by every router within 3 hops.
