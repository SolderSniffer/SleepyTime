# I2C Scan Sample

Self-contained Zephyr `west` app that probes all 7-bit I2C addresses (`0x08..0x77`) and prints responding devices.

Use this sample during bring-up to verify your assembled watch can see the accelerometer (or other I2C parts), regardless of exact address.

## Build

```bash
make build-sample SAMPLE=i2c_scan
```

or directly:

```bash
west build -s samples/i2c_scan -b xiao_nrf54l15/nrf54l15/cpuapp -d build/samples/i2c_scan
```

## Flash

```bash
make flash-sample SAMPLE=i2c_scan
```

## Notes

- The sample picks the first enabled common I2C node label in this order:
  `i2c22`, `i2c30`, `i2c21`, `i2c20`, `i2c1`, `i2c0`.
- If none are enabled, provide a devicetree overlay when building.
- On LIS3DH hardware, common addresses are `0x18` (SA0 low) and `0x19` (SA0 high).

## Expected output (LIS3DH connected)

```text
*** Booting nRF Connect SDK v3.2.1-d8887f6f32df ***
*** Using Zephyr OS v4.2.99-ec78104f1569 ***
[00:00:00.007,236] <inf> i2c_scan_sample: I2C scan sample started on bus: i2c@c8000
[00:00:00.007,240] <inf> i2c_scan_sample: Scanning 7-bit addresses 0x08..0x77 every 5000 ms
[00:00:00.007,244] <inf> i2c_scan_sample: If your accelerometer is connected, it should appear (LIS3DH often at 0x18 or 0x19)
[00:00:00.008,507] <inf> i2c_scan_sample: I2C device found at 0x19
[00:00:00.015,069] <inf> i2c_scan_sample: Scan complete, 1 device(s) responded
[00:00:05.016,400] <inf> i2c_scan_sample: I2C device found at 0x19
[00:00:05.022,967] <inf> i2c_scan_sample: Scan complete, 1 device(s) responded
[00:00:10.024,274] <inf> i2c_scan_sample: I2C device found at 0x19
[00:00:10.030,843] <inf> i2c_scan_sample: Scan complete, 1 device(s) responded
```
