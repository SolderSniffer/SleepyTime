# button

Reads the XIAO nRF54L15 user button via a GPIO interrupt and logs press/release
events over RTT.

## How it works

The sample configures the `sw0` alias (defined in the upstream board DTS as the
XIAO user button on P1.04) as an input with edge-triggered interrupts on both
edges. A callback logs whether the button was pressed or released based on the
pin level sampled inside the ISR.

## Building

```sh
west build -p always -b xiao_nrf54l15/nrf54l15/cpuapp samples/button
```

## Flashing

```sh
west flash
```

## Expected output (RTT console)

```
[00:00:00.000,000] <inf> button_sample: Button sample started — press the user button (sw0)
[00:00:01.234,000] <inf> button_sample: Button pressed
[00:00:01.456,000] <inf> button_sample: Button released
```