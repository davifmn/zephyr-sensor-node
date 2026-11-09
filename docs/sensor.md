# THERMO-1 temperature sensor (register map)

THERMO-1 is a fictitious part, modelled on common I2C temperature sensors such as the
TI TMP117 or the ADI ADT7420. The driver (`src/drivers/thermo_i2c.c`) and the emulator
(`src/drivers/thermo_i2c_emul.c`) both follow this page.

| | |
|---|---|
| Bus | I2C, 7-bit address 0x48 (A0 low) or 0x49 (A0 high), standard or fast mode |
| Range | −256 °C to +255.99 °C, 16-bit two's complement |
| Resolution | 1/128 °C = 7.8125 m°C per LSB |

## Registers

All registers are 16 bits wide, transferred most significant byte first.

| Address | Name | Access | Reset | Contents |
|---|---|---|---|---|
| 0x00 | `TEMP` | read | 0x0000 | Last conversion, `int16`, 7.8125 m°C per LSB |
| 0x0F | `DEVICE_ID` | read | 0x5431 | Identifies the part; the driver refuses any other value |

The chip does not acknowledge other register addresses.

## Reading a register

One transaction: a write of the register address, a repeated start, a read of 2 bytes.

```
S  addr+W  A  reg  A  Sr  addr+R  A  MSB  A  LSB  NA  P
```

## Conversion

```
milli_c = raw × 1000 / 128 = raw × 125 / 16
```

| raw | °C |
|---|---|
| 0x7FFF | +255.992 |
| 0x0C80 | +25.000 |
| 0x0000 | 0.000 |
| 0xFF40 | −1.500 |
| 0x8000 | −256.000 |
