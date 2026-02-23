# Blink Sample

Self-contained Zephyr `west` application that blinks `led0`.

## Build

```bash
west build -s samples/blink -b xiao_nrf54l15/nrf54l15/cpuapp -d build/samples/blink
```

## Flash

```bash
west flash -d build/samples/blink
```

## Notes

- Requires a board devicetree alias: `led0`.
- If your board has no `led0`, add an overlay when building:

```bash
west build -s samples/blink -b <your_board> -d build/samples/blink -- -DDTC_OVERLAY_FILE=<path/to/overlay.overlay>
```
