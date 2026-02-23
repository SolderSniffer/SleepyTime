# button_wakeup

Demonstrates System Off / GPIO wakeup on the XIAO nRF54L15.

The board stays awake for 5 seconds after the last button press (or since
boot if the button is never pressed), then enters System Off. Pressing sw0
wakes it up and the cycle repeats.

NOTE: Using RTT does not allow the board to properly enter System Off, recommend to modify the sample to use serial logging instead. However, besides serial logging, can also observe the expected behaviour by watching the LED: it will blink when the board is awake, and turn off when it enters System Off. Pressing sw0 will wake the board up and blink the LED on again.

## Behaviour

```
[boot / wakeup]
      │
      ▼
  log reset cause
  configure sw0 (edge interrupt, awake-time ISR)
  start 5 s inactivity timer
      │
      ├─── sw0 pressed ──► log press, reset 5 s timer
      │
      └─── 5 s elapse ──► reconfigure sw0 as GPIO_INT_LEVEL_ACTIVE
                          LOG_PANIC() / suspend console
                          sys_poweroff()  ◄── System Off
                                │
                          [sw0 pressed] ──► SoC resets ──► [boot]
```

## System Off wakeup notes

- GPIO sense wakeup on nRF54L15 requires `GPIO_INT_LEVEL_ACTIVE`.
  Edge-triggered mode does **not** work as a wakeup source from System Off.
- The button interrupt is kept edge-triggered while the board is awake
  (for press/release logging), then switched to level-active immediately
  before `sys_poweroff()`.
- After wakeup the SoC performs a full reset. `hwinfo_get_reset_cause()`
  returns `RESET_LOW_POWER_WAKE` to distinguish this from a cold boot.

## Building

```sh
west build -p always -b xiao_nrf54l15/nrf54l15/cpuapp samples/button_wakeup
```

## Flashing

```sh
west flash
```

## Expected Log Output

```
[00:00:00.001] <inf> button_wakeup_sample: Reset cause: debugger reset
[00:00:00.001] <inf> button_wakeup_sample: Awake — will enter System Off after 5 s of inactivity
[00:00:02.100] <inf> button_wakeup_sample: Button pressed — resetting inactivity timer
[00:00:02.300] <inf> button_wakeup_sample: Button released
[00:00:07.300] <inf> button_wakeup_sample: No activity for 5 s — entering System Off
[00:00:07.300] <inf> button_wakeup_sample: Press sw0 to wake up

[-- System Off --]

[00:00:00.001] <inf> button_wakeup_sample: Reset cause: wakeup from System Off (GPIO)
[00:00:00.001] <inf> button_wakeup_sample: Awake — will enter System Off after 5 s of inactivity
...
```