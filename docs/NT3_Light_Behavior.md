# NT-3 Light: Behavior by Network Configuration

This document describes how the NT-3 light firmware behaves today, read from
the source code. It covers not joined, Adaptiv and Daintree networks, and the
difference between *joined* and *joined and grouped*.

- **Firmware:** branch `occupancy_improvement`, commit `8efba86`
  (includes the new Adaptiv group occupancy model, the Daintree level mapping
  and the LED/light-sensor changes).
- **Source of truth:** the code. Where this document says "today", that is
  what the code does, not necessarily what was intended. Section 11 lists
  behaviors that look unintended or worth a decision.
- **Levels** are Zigbee/PWM levels 0–255 unless a percentage is given
  (255 = 100 %, 204 = 80 %, 51 = 20 %, 25 = 10 %). The level control
  plugin's maximum level is configured as 255.

---

## 1. Configurations

### 1.1 Network types

The light decides which network it is on by asking the coordinator for its
IEEE address after joining, and checking the manufacturer prefix (OUI). It
retries every 5 s until it knows. The result is saved in flash
(`TOKEN_APP_WHICH_NETWORK`), so it is known at power up.

| Network type | How it is detected | Code name |
|---|---|---|
| **Not joined** | Not on any network | `eNO_NWK` |
| **Adaptiv** | Coordinator IEEE starts with `8C:1F:64` | `eADAPTIV_NWK` |
| **Daintree** | Coordinator IEEE starts with `00:22:81` | `eDAINTREE_NWK` |
| **Unknown** | Joined, but the coordinator is neither | `eUNKNOWN_NWK` |

"Standalone" (`eSTAND_ALONE`) exists in the code and is checked in many
places, but **nothing ever sets it**. In practice, standalone = not joined.
This document uses "not joined" for both.

### 1.2 Joined vs. joined and grouped

A light is **grouped** when it has at least one **multicast binding** on the
light endpoint (`device_is_in_a_group()`). How it gets one:

- **Adaptiv:** auto-commissioning by the Twitch commissioning tool, or manual
  add (section 8.3). The light adds the group and a binding to it.
- **Daintree:** the Daintree gateway creates the group and binding.

The difference matters a lot:

| | Joined, not grouped | Joined and grouped |
|---|---|---|
| Network status LED | red: 2 blinks every 6 s | red off |
| Adaptiv occupancy control | **none** (sensor runs, LED shows motion) | group occupancy (section 5.3) |
| Adaptiv button settings | rejected, except the always-on ones | allowed, sent to the whole group |
| Adaptiv daylight harvesting | no | yes (if a master and a target are set) |
| Daintree occupancy | light controls itself, 2 h timeout | sensor only reports, gateway controls the light |
| Messages to other lights | none (nothing to send to) | settings and occupancy multicast to the group |

### 1.3 Joining

- A light that is not joined keeps trying to join (network steering), with a
  random 0.3–8 s delay after the network goes down.
- When a light **joins from not joined**, all light settings are reset to
  defaults (`set_light_configurations_to_default`): levels, occupancy delays,
  colour, allow off, flags (section 9).
- A Daintree light can also rejoin a known network from saved commissioning
  data (the `B_SWITCH_NWK` path).

---

## 2. Summary matrix

| Feature | Not joined | Adaptiv joined | Adaptiv grouped | Daintree joined | Daintree grouped | Unknown |
|---|---|---|---|---|---|---|
| On/off and level commands | – | yes | yes | yes | yes | yes |
| Level = | output (≤ max) | output (≤ max) | output (≤ max) | **mapped onto min..max** | **mapped onto min..max** | output (≤ max) |
| Occupancy turns light on/off | yes, local | **no** | yes, group | yes, local (2 h) | **no** (reports only) | **no** |
| Dim to mid on vacancy | yes | – | yes | no (on/off only) | – | – |
| Occupancy LED (green) | yes | yes | yes | yes | yes | yes |
| Daylight harvesting | no | no | yes | no | no | no |
| Button settings (D×n, T×n) | yes | rejected | yes (group) | rejected | rejected | rejected |
| Colour hold (T) | yes | no change | yes (group) | no change | yes (group) | no change |
| Manual dim hold (D) | yes | yes | yes | yes | yes | yes |
| 50 % / EM / factory reset / version | yes | yes | yes | yes | yes | yes |
| Manual add (T×6+D×6, T+D hold) | rejected | **yes** | rejected | rejected | rejected | rejected |
| DH master (T×3+D×3) | rejected | rejected | **yes** | rejected | rejected | rejected |
| Allow off | – | – | – | yes | yes | – |
| No-comms fallback (30 min → full) | – | – | – | yes | yes | – |

---

## 3. Power up

1. Both LEDs on while the firmware initialises, then off.
2. The PWM starts at the saved max level and saved colour.
3. The light goes to its **max level** 1 s after start-up
   (`save_max_light_level` at init schedules the move): the trim max on
   Adaptiv and not joined, Zigbee level 255 (= the Daintree max) on Daintree
  (see 4.2).
4. Occupancy starts in the ON state with a full timer. **No multicast** is
   sent at power up or on joining.
5. Light sensor: readings start; daylight harvesting control is suspended
   for the first 20 s.
6. Network status LED: first check 750 ms after init.
7. Adaptiv auto-commissioning: after a **power cycle** (not a soft reset),
   1–3 s later, a light that was told to join a group by the Twitch adds
   that group (section 8.3).

---

## 4. Light level control

### 4.1 Commands

The light accepts standard Zigbee **On/Off** (on, off, toggle) and **Level
Control** (move to level, move, step, stop, with and without on/off) on
endpoint 1, on every network type.

- **Adaptiv and unknown networks:** the level is the light output. Commands
  are limited to **25 .. max** (25 = `MIN_LEVEL`, 10 %). Dimming down stops
  at 25 and stays on; only an off command turns the light off.
- **Daintree:** see 4.2.

### 4.2 Daintree level mapping

Daintree writes three attributes in its configuration cluster (0xFC01,
manufacturer 0x103A): **min** (0x0010), **max** (0x0011) and **allow off**
(0xFC01).

- Writes are **stored exactly as written** (attribute, flash and internal
  value) and **never move the light**. The new range takes effect on the next
  on/off or level command.
- The Zigbee level **1..255 maps in a straight line onto min..max**:
  `output = min + (level − 1) × (max − min) / 254`, rounded. Example with
  min 20, max 200: level 255 → 200, 254 → 199, 128 → 110, 1 → 20.
- Level 255 is stored as 255 in CurrentLevel (and OnLevel). With min 0 and
  max 255, every level drives the light at that level.
- **Level 0 = off.** Move to Level *with On/Off* to 0 turns the light off.
  **Level 1 stays on** (at min). Move and Step down stop at 1.
- A plain Move to Level (without on/off) to 0 makes the light dark but
  leaves the OnOff attribute on (ZCL behavior).
- CurrentLevel, OnLevel and the min/max attributes always hold the Zigbee
  values; the mapping only affects the PWM output.
- **Allow off = false:** off commands are blocked; the light goes to level 1
  (= min) and stays on.
- **No communication for 30 minutes:** the light goes to full (level 255 =
  max) over 1 s, once, until communication resumes.

### 4.3 PWM output

- Two LED channels (warm and cool). The output level is split between them
  by colour temperature: 3000 K = all warm, 4000 K = 50/50, 5000 K = all cool
  (500 K steps).
- The light driver is unstable below PWM 10, so **every channel that is on
  gets at least 10**; 0 stays off. A channel with a share of the mix never
  rounds down to 0 while the light is on.

### 4.4 Light settings

| Setting | Default | Set by |
|---|---|---|
| Max (trim) | 204 (80 %) | T×1–6, Twitch, Daintree (0x0011) |
| Mid (dim level) | 51 (20 %) | T×1–6, Twitch |
| Min | 0 | T×1–6, Twitch, Daintree (0x0010) |
| Colour | 4000 K | T hold, Twitch |
| Allow off | true | Daintree (0xFC01) |

In an Adaptiv group, settings changed with the buttons are multicast to the
whole group.

### 4.5 Modes

- **50 % mode (T×2 + D×2):** halves max, mid and min (not below 25 for max
  and mid) and halves the present level. Toggling again restores the saved
  levels. Works on every network type.
- **EM mode (T×4 + D×4):** after vacancy the light stays at **mid instead of
  turning off**. Occupancy still wakes it. Works on every network type.

---

## 5. Occupancy sensor

### 5.1 Sensor and LED (all networks)

- PIR sensor sampled every **75 ms**. Motion = the change from the running
  average exceeds the **threshold** (default 80; D×11 = 80, D×12 = 60, lower
  is more sensitive). A reading outside the valid range marks the sensor as
  bad.
- **Green LED:** on while motion is detected, off 1.25 s after the last
  detection. A bad sensor makes green flicker. Green blink patterns (firmware
  version) take priority; occupancy resumes after them.
- **Light sensor readings and green:** each lux reading needs green off for
  about 181 ms. While green is lit, a real reading is taken only once per
  20 s (one short blink every 20 s while occupied); otherwise the last reading
  is reused.
- The sensor's occupancy attribute is reported on endpoint 2.

### 5.2 When occupancy does nothing

| Condition | Effect |
|---|---|
| Unoccupied delay = 0 (D×9, or a delay write) | Sensor **stopped**. Green off. The light stays where it is; nothing turns it on or off. A stopped sensor reports "occupied". |
| Firmware version display (`B_DISABLE_OCC_SENSOR`) | Sensor still samples and green follows motion, but occupancy does not control the light, and Adaptiv group messages are ignored. |
| Adaptiv, not grouped | No occupancy control (green still follows motion). |
| Daintree, grouped | Sensor only reports; it never turns the light on or off. |
| Unknown network | No occupancy control. |
| Short pauses after every level change (Adaptiv) | Occupancy ignored for the transition time + 1 s (2–3 s after on/off commands), so the light's own change is not seen as motion. |

### 5.3 Adaptiv, grouped: group occupancy

Every light keeps its own timer; the group stays in sync with two multicasts
(Adaptiv manufacturer-specific, occupancy sensing cluster 0x0406):

- **Keepalive (0xDF):** time until expiration (s, 2 bytes), state, reason
  (wake / refresh), level.
- **Level change (0xE0):** state (ON / MID / OFF), level.

A light ignores its own multicasts, and ignores these messages when its
sensor is off (delay 0) or while the firmware version is displayed.

**States:** ON → *(dim delay, no motion)* → MID → *(off delay, no motion)* → OFF.

| Event | What happens |
|---|---|
| Motion while ON | Restart own timer. Nothing sent. |
| Motion while MID or OFF | A **wake** keepalive goes out at once, at the sensor sample that saw the motion (`OCC_GROUP_WAKE_BACKOFF_MS` = 0). Lights that are already on when they see the person send nothing, so usually only the first one or two lights to see someone send a wake. The light that saw the motion does **not** turn on by itself: it goes ON (to the wake level) when its own wake comes back from the stack, the same moment the rest of the group receives it, so the group comes on together. If the stack accepted the wake but it does not come back within 500 ms, the light goes ON anyway. If the wake cannot be sent at all (held or refused, see "Busy network"), the light stays where it is, like the rest of the group; another light that sees the motion will try too. |
| Near the end of the ON timer (last 60 s, or timeout/4 if shorter) | One light sends a **refresh** keepalive with its remaining time, at a random point in the first 2/3 of that window. Others cancel theirs and sync their timers to it. |
| A light has more than the margin (window/4, 15 s) more time left than a received refresh | It replies within 0–5 s with its own (longer) time: someone near it is still there. The group syncs to the longer time and stays on. |
| ON timer expires | Random 0–5 s wait, then the light dims to mid and sends **level change MID**, unless another light's command arrives first (then it follows that). A light that missed the command changes on its own timer anyway. |
| MID timer expires | Same, with **level change OFF**: off, or stays at mid in EM mode or if min > 0. No keepalive is sent before off. |
| Keepalive received while ON | Timer sync only. **The level is not changed** (daylight harvesting keeps its level). |
| Keepalive received while MID or OFF | Light goes ON (someone has occupancy). |
| Level change received while seeing motion right now | Light stays ON and sends a wake, bringing the group back. |
| Switch on/off or level command | The ON timer restarts (all lights get the command, so they stay in sync). No message. |

**Wake level:** the last level set from a switch (a network level command
that finished or was stopped), trim max if none. After 3 hours off, the wake
level returns to max.

**Message count (simulation):** a vacant group sends about one refresh, one
dim and one off multicast per vacancy cycle. While occupied, about two
multicasts per dim period (one refresh and one reply).

**Busy network:** these multicasts go out with a radius of 3 hops
(`GROUP_MULTICAST_RADIUS`). If the stack refuses one (for example, the
broadcast table is full), the light tries again about once a second (±250 ms),
up to 5 attempts (about 4 s). While broadcast traffic is heavy (60% or more of
the broadcast table in use), the first 3 attempts are held and the last 2 are
sent anyway. A light whose level change is held does not dim or turn off until
the command really goes out. A message from another light that arrives during
the wait still cancels this light's message, so a busy network does not make
every light in the group send its own copy (10 lights with heavy traffic,
simulated: still about one refresh, one dim and one off per cycle).

**Special cases (Adaptiv):**

| Case | Behavior |
|---|---|
| Off from a switch | Occupancy may not turn the lights back on for **5 minutes**; every motion restarts the 5 minutes. |
| Manual-on mode (`B_MANUAL_ON_MODE`) | Occupancy never turns lights on from OFF (a switch must). It still wakes them from MID. |
| EM mode, or min level > 0 | Vacancy ends at mid instead of off. |
| 2-hour delay (D×10) | Temporary: returns to the saved delay on the next on/off command or once the lights are off. |
| Settle time after off | Occupancy can turn lights on 250 ms after they turned off. |

### 5.4 Not joined: local occupancy

The light controls only itself, with the same delays as Adaptiv (dim and off
delays from the D button), and no messages.

| State | Next |
|---|---|
| ON (max level) | No motion change for 3 s and no motion → waiting |
| Waiting | Motion → back to **max**. No motion for the **dim delay** → dim to **mid** |
| Mid | Motion → **max**. No motion for the **off delay** → **off** (or stays at mid in EM mode or if min > 0) |
| Off | Motion → **max** (after 250 ms) |

It always returns to **max**, not to a previous level, and there are no pauses
after level changes.

### 5.5 Daintree: occupancy

| Daintree, not grouped | Daintree, grouped |
|---|---|
| The light controls itself: motion → on (OnLevel); no motion for **2 hours** → off. No mid step. | The sensor **only reports** occupancy; the gateway controls the light. The occupancy attribute changes after the **PIR occupied→unoccupied delay** (endpoint 2 attribute, set by Daintree; 60 s if it can't be read). |

The D-button delays do not apply on Daintree (button settings are rejected).

---

## 6. Daylight harvesting (Adaptiv only)

- The group's **master** is selected with the buttons (T×3 + D×3, off by
  default). The light selected last is the master: it tells the group, and any
  other master stops. The **target lux** is set by the Twitch only: 0 (off), 300, 500 or
  800. There is no calibration.
- The master runs only when **all** of these are true: Adaptiv network, grouped,
  a target that is not 0, light on, occupancy state ON (not mid or off), not
  suspended, and level at least the minimum allowed.
- The master averages its lux readings (8 readings over 20 s). At most once
  every 60 s, if the mean is more than 50 lux from the target, it multicasts a
  new level: halfway to `level × target / lux`, at most 30 % of the level per
  update. The group ramps over 18.5 s. The last new level is sent once more for a
  light that missed it, and the present level is sent again about every 10
  minutes for a light that restarted.
- A light that is off, at mid, suspended or recently dimmed by a switch ignores
  the level. Details: `NT3_Daylight_Harvesting.md`.
- Not used on Daintree, unknown or not-joined lights (readings are still
  taken; on Daintree the lux value is reported as an attribute).

---

## 7. LEDs

| Indication | LED | Pattern |
|---|---|---|
| Starting up | both | on during init, then off |
| Not joined, first check after boot | red | one long blink (2 s) |
| Not joined | red | 1 blink (500/500 ms) every 6 s |
| Joined, not grouped | red | 2 blinks every 6 s |
| Joined and grouped | red | off (checked every 12 s) |
| Button setting accepted | red | 1 blink per press, 250/250 ms |
| Button input rejected | red | 1 long blink (1.5 s) |
| Manual dim step | red | 1 blink (500/50 ms) per step |
| Press sequence finished | both | off |
| Factory reset running | red | solid until the reset is done |
| Firmware version | red, green, red | major, middle, minor digit, 500/500 ms |
| Motion | green | on, off 1.25 s after the last motion |
| Bad occupancy sensor | green | fast flicker |

Priorities: factory reset > button feedback > network status (button
feedback interrupts a status blink after a 500 ms gap). Green blink patterns
> lux reading > occupancy.

---

## 8. Buttons

Two buttons: **T** (button 0) and **D** (button 1, nearest the sensor).

- **Press sequences:** press and release; the sequence ends **1.5 s** after
  the last release. Presses are debounced (75 ms). Both buttons can be part
  of one sequence.
- **Holds:** start after **4 s** of holding.
- **Feedback:** accepted = one red blink per press; rejected = one long red
  blink.

### 8.1 Press sequences

**Always available (any network):**

| Presses | Action |
|---|---|
| T×10 + D×10 (±1 each) | **Factory reset** (section 10) |
| T×2 + D×2 | Toggle **50 % mode** |
| T×3 + D×3 | Toggle **daylight harvesting master** (Adaptiv, in a group; otherwise fail). Red blinks 3×; the main light also blinks 3× when this light becomes the master |
| T×4 + D×4 | Toggle **EM mode** |
| T×8 + D×8 | Show **firmware version** on the LEDs |
| T×6 + D×6 | **Local auto-commission** (manual add + announce): **Adaptiv, not grouped only**; rejected elsewhere |

**Only when settings are allowed: not joined, or Adaptiv grouped.** Rejected
on Adaptiv not grouped, Daintree and unknown networks. In an Adaptiv group
the new setting is multicast to every light in the group.

| D presses (T = 0) | Dim delay (ON → mid) | Off delay (mid → off) |
|---|---|---|
| 1 | 10 min | 5 min |
| 2 | 20 min | 5 min |
| 3 | 30 min (default) | 5 min |
| 4 | 45 min | 5 min |
| 5 | 60 min | 5 min |
| 8 | 30 s (test) | 30 s |
| 9 | 0 = **occupancy sensor off** | 31 s |
| 10 | 2 h (temporary, see 5.3) | 5 min |
| 11 | sensitivity: threshold 80 (normal) | – |
| 12 | sensitivity: threshold 60 (more sensitive) | – |
| 6, 7 | rejected | |

| T presses (D = 0) | Max | Mid | Min |
|---|---|---|---|
| 1 | 100 % (255) | 20 % | 0 |
| 2 | 80 % | 20 % | 0 |
| 3 | 60 % | 20 % | 0 |
| 4 | 40 % | 20 % | 0 |
| 5 | 20 % | 10 % | 0 |
| 6 | 10 % | 10 % | 0 |

Any other combination is rejected.

A higher max level moves the light to the new max, which is also the level
occupancy then wakes to. A lower max only limits the level.

Flashes from selecting the DH master or from a Twitch config write end at the
prior level, or off if the light was off.

### 8.2 Holds

| Hold | Action | Where |
|---|---|---|
| T held 4 s | Colour temperature +500 K every 2.5 s (3000 → 5000 K, then wraps). Multicast to the group. | Changes colour only when grouped (Adaptiv or Daintree) or not joined; elsewhere the hold runs but nothing changes. |
| D held 4 s | Manual dim: steps down about every 200 ms; below 25 it jumps back to max. | Every network type |
| T + D held 4 s | Manual add to group (8.3), repeats every 4 s while held | **Adaptiv, not grouped only** |

### 8.3 Adaptiv commissioning (auto and manual add)

- **Auto-commissioning (Twitch):** the Twitch sends an "announce group"
  command with a group ID. The light saves it, and after the **next power
  cycle** (1–3 s after start-up) adds that group.
- **Manual add mode** (T+D hold, or a Twitch command): the light ramps up and
  down continuously until it is added to a group, or the time-out expires
  (5 min by default, or the Twitch's value). Being added to a group stops it
  and returns the light to max.
- **Local auto-commission** (T×6 + D×6): manual add mode, plus the light
  broadcasts the announce itself 3 times, 3 s apart (doing the Twitch's job).

---

## 9. Defaults

Applied on joining from not joined, and by factory reset.

| Setting | Default |
|---|---|
| Max / mid / min level | 204 (80 %) / 51 (20 %) / 0 |
| Dim delay (ON → mid) | 30 min |
| Off delay (mid → off) | 5 min |
| Unoccupied → occupied delay | 0 s (250 ms settle) |
| Occupancy threshold | 80 |
| Colour | 4000 K |
| Allow off | true |
| Lux target | 0 (daylight harvesting off) |
| Daylight harvesting master | off |

---

## 10. Factory reset

T×10 + D×10 (±1). Red LED solid until done. Steps, 500 ms apart:

1. Leave the network.
2. Clear the reporting table, groups, bindings and scenes.
3. Reset settings to defaults (section 9) and the network type to not joined.
   A normal reset also clears the mode flags (50 %, EM, manual-on, DH master,
   2 h delay, manual add).
4. Restart the chip.

---

## 11. Observations and open questions

Behaviors in today's code that look unintended or need a decision:

1. **`eSTAND_ALONE` is never set.** Standalone and not joined are the same
   today; the separate standalone branches in the code are unused.
2. **Unknown networks have no occupancy control.** Occupancy is initialised
   as for Daintree, but the sample handler has no case for unknown networks,
   so the sensor never turns the light on or off.
3. **Daintree max defaults to 204 (80 %)** after joining, until the gateway
   writes its own max. Until then, level 255 maps to 80 %.
4. **D×9 sets an off delay of 31 s** with the sensor off; the 31 has no
   effect.
5. **50 % mode, EM mode and manual dim work on Daintree**, while the
   other button settings are rejected there. On Daintree, 50 % mode halves the
   Daintree min/max range.
6. **Daintree grouped lights never control themselves from occupancy;** the
   gateway must. Not-grouped Daintree lights use a fixed 2-hour timeout.
7. **Not-joined occupancy always wakes to max**, while Adaptiv wakes to the
   last switch level.
8. **A stopped sensor (delay 0) reports "occupied"** in its attribute.
9. **The colour hold on a not-grouped Adaptiv or Daintree light** runs (no
   feedback) but changes nothing. On a grouped Daintree light it changes the
   colour and multicasts it to the group.
