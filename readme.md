v00.00.01
Initial dump that supported basic light functionality.  
Also supported; 
- Initial Adaptiv auto-commissioning.
- Initial functionality on a Daintree nwk
- Is the code sent to Kimin

v00,00.02
Continuation of functionality
- Daintree nwk all but switching of networks/gateways ie. mfg specific for max/min light levels.
- Support of light fixture button use in Adaptiv or stand alone for;
   - Max/Min light levels levels
   - Occupancy sensor delay times
   - Light temerature/color

1/27/26
Continuation of functionality
- Accommodate Twitch only supporting end point 1 for multicasts
   - Change occupancy sensor to end point 1
   - Change light sensor to end point 1
- Add mid-level brightness attribute
- Add ADAPTIV_CONFIGURATION_CLUSTER
- Add Manual-On functionality

2/15/26 v00.00.03
Manual Group function not implemented.
- Use Daintree IEEE eui to determine which nwk
- Allow color selection, from both the NT-3 & Twitch, in a Daintree nwk
- All occupancy sensors snyc'd in a group
- Support reset cmd from Twitch
- 5 second press & hold of D button also resets NT-3
- 9 presses of the D button places NT-3 in 4 hour unoccupied time out
- Modify light sensor output, prior to log10() function
- Support both % and abs levels for trim

2/19/26 v00.00.04
- Seperate state machine for occ sensor Daintree vs Apaptive vs standalone
- Reduce multicasts for occ sensor, still to many for distributed system
- Add reset command
- Add lux target support
- Change lux fudge factor from 4 to 3.78

2/26/26 v00.00.06
- Increase occupancy threshold to reduce device variability and "bounce" after lights turns off
- Initial support of dim to non-zero level, factory reset set to 0
- Support Twitch reset cmd with delay time
- Remove multiple siLab event mechanisms
- Replace events with tasks 

2/28/26 v00.00.07
- Refactor support for 3 endpoints with 3 groups and support for 1 end point for 1 group
- Still support dim to 0, but is hard coded to 5%
- Continue replacing events with tasks.

3/3/26 v00.00.08
- Support manual grouping, 5 secs press
- Support 4 scenes
- Re-install allow off
- Save last level in all situations
- Support standard zigbee attributes that are mfg specific in Adaptiv & Daintree
- Factory reset device on nwk leave
- Increase occ sensor bit time from 6uS to 12.5uS

3/5/26 v00.00.09
- Filter out Adaptiv cmds in Daintree nwk
- Respond to both write attr cmds
- Fix scenes and added color
- If off cmd, occupancy sensor suspended for total delay time
- If not off cmd, occupancy sensor suspended for zigbee attr, 0x11, default 3 secs
- Change LED behavior 1 = no nwk, 2 = joined nwk, no group
- DHS disabled
- All printf's enabled

3/7/26 v00.00.10
- Add stop step prior to off
- Add retries for reading coord eui64 to determine nwk
- DHS control disabled, still reports lux
- Add holding D button ~6 secs, factory reset.  Red led will flash rapidly, let go.
- Daintree, if joined, and not on a schedule?, enable occ sensor control with 2 hour delay
- Adaptiv, occupancy acts like Lutron
- Adaptiv, configuration cmds flashes all grouped lights once.
- Adaptiv, Manual mode flashes light, (in manual mode), till grouped or timed out, NOTE:time out = occ time out
- Adaptiv, Add 2 hr delay option.  7 button presses

3/11/26 v00.00.11
- Check for bad occupancy sensor, (NT-2), default to unoccupied
- Daintree, all NT-3 buttons disabled, set color via twitch only
- Adaptiv, add new, delete old auto-commissioning cmds
- Adaptiv, another try at Lutron behavior for occupancy sensor

3/17/26 v00.00.12
- If valid IEEE response, stick with nwk response, even if unknown.
- SL_CATALOG_ZIGBEE_R23_SUPPORT_PRESENT not supported, compatable with older stack?  Revisit!
- 5x On/Off to reset, relax timing, On time to < 8 seconds, Off time meaningless.
- Bad occupancy sensor will rapidly flash green LED, (temp)
- Adjust occupancy sensor's low level set-up times to add margin
- Adaptiv, disable occupancy sensors control if joined but not grouped
- Adaptiv, push out OTA server request, random time between 5-10 minutes
- Adaptiv, attempt to reduce NT-3's broadcast Permit Join Requests

3/25/26 v00.00.13
- Bug fix related to changing nwks with power cycle
- Clean up lossless compression 3:1

4/5/26 v00.00.14
- On a new join, all button are disabled
- On a new join, all configurations are reset to defaults
- Occupancy sensor sample time reduced to 75mS  (150mS)
- Reduce Join attempt period to random between 3 to 8 seconds
- Remove check for install code during joining
- Stand alone, Occupancy delay, trim levels, color buttons supported
- Adaptiv, once grouped, all button configurations are allowed
- Adaptiv, after 3 hours of unoccupied, occupancy trip will go to max allowable light level
- Adaptiv, off button press = a hard coded delay of 5 minutes, sliding window
- Adaptiv, add to group allowed for any light on nwk, grouped and not grouped
- Daintree, Once grouped, only color configuration is enabled
- Daintree, if not grouped, Occupancy sensor enabled, unoccupied delay is 2 hours, sliding window

4/12/26 v00.00.15
- Adaptiv, Any configuration parameters mods will flash lights once and place them at max light level
- Adaptiv, Fix bug of delayed occ command during twitch commands
- Adaptiv, Manual add only if on Adaptiv nwk and not grouped

4/24/26 v00.01.01
- Change occupancy sensor trip level to 80
- Change light sensor gain 0.845
- 5x5 power reset window 3.5 - 6.5 seconds
- Daintree, identify command restores original level, not min level
- Daintree, color set to default after leaving nwk, then rejoining
- Daintree & Adaptiv write to differnt attributes for same parameters, fix logic bug

6/2/26 v00.01.02
- Remove 50% mode, add holding D button to continuously dim light, for scenes
- Don't leave network on TC link key failure
- 5x5 power reset window 2.0 - 8.0 seconds
- Daintree, no comms for 30 minutes, lights go to max, stay on nwk, switch cmds valid
- Occupacy sensor sensitivity 100 -> 80
- Daylight Harvester updated at 30 seconds
- Set minimum energy mode to 0, EM0.  Disable allow sleep
- Add latest light sensor piece-wise linear equation
- Clean up light sensor calibration
- Add sink/source ota debug, cli initiated

6/3/26 v00.01.03
- Allow occupancy sensor to function again. (Bug: Was disabled for dhs testing)
- Add new button(s) functionality to match documentation
- Add static 50% mode for Cali
- DHS enabled, will calibrate on every target change
- Comment out 1 line to allow ota

6/28/26 v00.01.04
- Bug fix on 10-10-0% trim
- Add ota image policy id now 2
- Add allow for ota down grades
- Add allow dhs master also be in 50% mode
- Add increase slew rate for large dhs change values
- Add (temp?) ignore occ sensor for 3 seconds after lights go off, (supply issue?)
- Bug fix 2+ off button presses will now keep 1st saved level
- Add reset occ timers during active config
- Bug fix on saving last level during occ event dimming to mid

7/5/26 v00.01.05

7/26/26 v00.01.06
- Remove Occ sensor disabled for 3 seconds after an unoccupied to off
- DHS in a group will all get same level and ramp values
- Remove trim option #9, original EM_MODE
- EM_MODE now enabled/disabled with 4 double button presses
- Add local auto commissioning via NT-3, 6 double button presses, which also puts into manual add
- Adaptiv DHS now 4.5 X,  Daintree is 1.72 and piece wise linear

8/1/26 v77.01.07
- Hard code unocc -> occ delay to 3 seconds
- Remove common code for unocc -> occ delay on Adaptiv,(ep 1) vs Daintree,(ep 2) nwks.

8/20/26 v04.01.01
- Press both buttons 8 times for version display
- Adaptiv DHS has exponential decay from linear
- Adaptiv DHS inhibits updates when LEDs are on
- Adaptiv DHS gain 3.5 X
- OTA_CLIENT_QUERY_DELAY_MINUTES = 60 minutes
- DHS if dim up/dn cmd inhibit dhs for occ sensor delay (differnt from occ cmd inhibit)
- Adaptiv, remove occ broadcasts when powered on (and already on a nwk) and when joining a nwk
- Adaptiv, remove occ broadcasts when joining a nwk

9/4/26 v01.01.02
- Adaptiv unoccupied -> occupied delay is 0
- Adaptive add custom cmd to flash lights on match/mis-match of fw versions 
- Bug fix macro to gen 32 bit work from an array

9/x/26 v01.01.03
- Limit occ and light sensors multi-casts to 2 hops
- Limit NT-3 auto-commissioning broadcast to 12 hops

10/31/26 v1.01.04
- OTA queries now between 59 - 61 minutes
- OTA server disco changed from 10 minutes to 4 hours
- Occ sensor button presses: 10 = 120 minutes delay,  11 = 80 threshold,  12 = 60 threshold
- Bug fix, NT-3 button press for f/w version, not re-enabling occ snesor
- Support R55 removal, power on sequence of light
- Occ and DHS multi-cast radis now 3
- Go back to original DHS calibration
- DHS uses multi-cast during calibration
