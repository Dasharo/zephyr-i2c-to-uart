# README

## Env setup

* Follow
  [Zephyr Getting Started](https://docs.zephyrproject.org/latest/develop/getting_started/installation_linux.html#installation-linux)
* [Install Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html#install-zephyr-sdk-on-linux)
* [Install west](https://docs.zephyrproject.org/latest/develop/west/install.html)

## Building

```
west init -m https://github.com/Dasharo/zephyr-i2c-to-uart.git --mr master
west update
west build --board nucleo_l432kc zephyr-i2c-to-uart/app/
```

## Flashing

```
west flash
```

## Using

By default, I2C is exposed on PA9 (`CLOCK`) and PA10 (`DATA`) pads. See the
pinout
[here](https://os.mbed.com/platforms/ST-Nucleo-L432KC/#arduino-nano-compatible-headers).
All data coming from I2C is forwarded to virtual UART which works over USB
(CDC-ACM).

When using with Clevo laptops, following connection must be made:
* `SMC` - `PA9`
* `SMD` - `PA10`
