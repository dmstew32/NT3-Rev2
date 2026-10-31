# NT-3 Light PC tests

Run everything from the project folder:

```
make test        (cmd)          .\make test      (PowerShell)
```

Needs MSYS2 MinGW gcc (`C:\msys64\mingw64\bin` on the PATH), Python and CMake,
the same tools as the Z3Switch tests.  The first run downloads GoogleTest.

## test/unit: unit tests (GoogleTest + fff)

One test program per module, like Z3Switch's `light_switch/test`:

| Test | Module |
|---|---|
| test_zcl_header | Light/rx/zcl_header.c |
| test_rx_zcl_handler | Light/rx/rx_zcl_handler.c |
| test_rx_zdo_handler | Light/rx/rx_zdo_handler.c |
| test_tx_queue | Light/tx/tx_queue.c |
| test_hal_buttons | Light/hal/hal_buttons.c |
| test_app_button | Light/buttons/app_button.c |
| test_button_actions | Light/buttons/button_actions.c |
| test_hal_leds | Light/hal/hal_leds.c |
| test_app_leds | Light/leds/app_leds.c (+ network status vs. reference/) |

How a test is built (see `CMakeLists.txt`):

- The module is compiled as C, straight from `Light/`, with `NT_3_app.h`
  force-included like the firmware build.
- `stubs/` comes first on the include path and replaces the SDK and every
  project module that is not under test.  `stubs/nt3_test_stubs.h` declares
  their types and functions.
- The test fakes (fff `FAKE_*_FUNC`) every function the module calls outside
  itself, and drives the module through its public functions.  Event handlers
  are captured from the fake `..._event_init()` and called directly.

Adding a test: write `test_<module>.cpp`, add an `nt3_test(...)` line to
`CMakeLists.txt` with the module's `.c` files, and add any missing SDK
declarations to `stubs/nt3_test_stubs.h`.  Wrap fakes in `extern "C" { }`, and
use a typedef for function pointer arguments (fff cannot take `void (*)(void)`).

Building by hand:

```
cmake -S test/unit -B test/unit/build -G "MinGW Makefiles"
cmake --build test/unit/build
ctest --test-dir test/unit/build --output-on-failure
```

## test/buttons: button equivalence test

Runs the original button code (`reference/`, before the button refactor, with
the poll-rate fix) and the current code through the same randomized button
sessions in a simulator, and compares every observable effect.  Re-run it
after any change to the button code; a mismatch means the behavior changed.
