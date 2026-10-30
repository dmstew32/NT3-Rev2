# Adaptiv changes to Silabs-provided files

This project uses Simplicity SDK **2025.6.3** in "copy" mode: the SDK files
it needs are copied into `simplicity_sdk_2025.6.3/`. Some of those files, and
some generated files in `autogen/`, contain Adaptiv changes. This file lists
them so they survive SDK upgrades and code regeneration.

## 1. SDK files: `#ifdef ADAPTIV_LIGHT`

Every Adaptiv change in an SDK file is marked like this:

```c
#ifdef ADAPTIV_LIGHT
  /* Adaptiv code */
#else  // ADAPTIV_LIGHT
  /* original Silabs code */
#endif // ADAPTIV_LIGHT
```

`ADAPTIV_LIGHT=1` is set in `NT-3-Light.slcp` (`define:`). To find all
changes: `grep -rn "ADAPTIV_LIGHT" simplicity_sdk_2025.6.3`.

| Blocks | File (under `simplicity_sdk_2025.6.3/`) |
|---:|---|
| 57 | protocol/zigbee/app/framework/plugin/level-control/level-control.c |
| 19 | protocol/zigbee/app/framework/plugin/on-off/on-off.c |
| 17 | protocol/zigbee/app/framework/plugin/ota-client/ota-client.c |
| 15 | protocol/zigbee/app/framework/plugin/network-steering/network-steering-v2.c |
| 11 | protocol/zigbee/app/framework/plugin/scenes/scenes.c |
| 7 | protocol/zigbee/app/framework/plugin/groups-server/groups-server.c |
| 7 | protocol/zigbee/app/framework/plugin/identify/identify.c |
| 7 | protocol/zigbee/app/framework/util/util.c |
| 4 | protocol/zigbee/stack/internal/src/baremetal/sl_zigbee_dynamic_commissioning_baremetal_wrapper.c |
| 3 | platform/driver/pwm/src/sl_pwm.c |
| 3 | protocol/zigbee/app/framework/cli/core-cli.c |
| 3 | protocol/zigbee/app/framework/plugin/update-tc-link-key/update-tc-link-key.c |
| 3 | protocol/zigbee/app/framework/util/attribute-table.c |
| 2 | platform/driver/gpio/src/sl_gpio.c |
| 2 | protocol/zigbee/app/framework/cli/zcl-cli.c |
| 2 | protocol/zigbee/app/framework/plugin/reporting/reporting.c |
| 2 | protocol/zigbee/stack/internal/src/baremetal/sl_zigbee_dynamic_commissioning_baremetal_callbacks.c |
| 1 | platform/service/cli/src/sl_cli.c |
| 1 | platform/service/token_manager/legacy/inc/sl_token_manufacturing_series_2.h |
| 1 | protocol/zigbee/app/framework/include/af-types.h |
| 1 | protocol/zigbee/app/framework/plugin/find-and-bind-target/find-and-bind-target.c |
| 1 | protocol/zigbee/app/framework/plugin/ota-client-policy/ota-client-policy.h |
| 1 | protocol/zigbee/app/framework/plugin/reporting/reporting-cb.c |
| 1 | protocol/zigbee/app/framework/plugin/reporting/reporting-cli.c |
| 1 | protocol/zigbee/app/framework/util/config.h |
| 1 | protocol/zigbee/app/framework/util/process-cluster-message.c |
| 1 | protocol/zigbee/app/framework/util/zcl-debug-print.h |
| 1 | protocol/zigbee/stack/security/sl_zigbee_security_manager_dlk_ecc.c |

Comment-only and whitespace-only edits are kept without markers.

Upgrading the SDK again: for each file, 3-way merge the Adaptiv version onto
the new stock file (base = old stock, theirs = new stock), then keep the
markers around the Adaptiv code. All other copied SDK files are identical to
stock 2025.6.3.

## 2. Generated files in `autogen/` edited by hand

**Studio and `slc` overwrite these files whenever they regenerate the
project**, for example after importing the project, changing a component or
saving the ZCL configurator. After any regeneration, check `git status` and
restore them:

```
git checkout -- autogen/zap-config.h autogen/zap-id.h autogen/zap-print.h \
    autogen/sl_cli_command_table.c autogen/sl_power_manager_handler.c
```

and re-check `autogen/sl_component_catalog.h` (see below).

| File | Adaptiv change |
|---|---|
| zap-config.h | Adaptiv cluster 0xFCAD (8 attributes) and Daintree cluster 0xFC01; manufacturer code 0x1574; OTA image type 2; OTA file version = `APP_FIRMWARE_VERSION_U32`; PIR delay default 0x1C20; date code; adjusted counts |
| zap-id.h | IDs for the Adaptiv/Daintree attributes and the auto-commissioning cluster 0xFCAC |
| zap-print.h | cluster names/manufacturer codes for the custom clusters |
| sl_cli_command_table.c | `sink` CLI command (`adaptiv_cli_sink_node_info_command`) |
| sl_power_manager_handler.c | `app_is_ok_to_sleep()` returns `false` |
| sl_component_catalog.h | `SL_CATALOG_ZIGBEE_R23_SUPPORT_PRESENT` and `SL_CATALOG_ZIGBEE_UPDATE_TC_LINK_KEY_PRESENT` commented out ("Per Damon") |

`zap-command.h`, `zap-cli.c`, `zap-enabled-incoming-commands.h`, `zap-event.h`
and `zigbee_zcl_callback_dispatcher.c` differ from generator output only in
order/formatting.

A permanent fix is to describe the custom clusters in a ZCL XML file
referenced by `config/zcl/zcl_config.zap`, and to move the other edits into
application code. Then regeneration no longer loses anything.

## 3. Application code (`Light/`)

- Build settings that were only in `.cproject` are now in the `.slcp`: the
  `Light/` sources and include folders, and `-include NT_3_app.h`.
- SDK 2025.6.3 no longer includes the legacy token API through `hal.h`. The 9
  files that use `TOKEN_APP_*` now `#include "sl_token_api.h"` after `af.h`.
  Token names, NVM3 keys and defaults are unchanged.
