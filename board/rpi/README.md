# board/rpi — референс стендов (Raspberry Pi)

В работе: **три** Pi 3 B+ (stub SD → TFTP → netd MCP).

| роль | serial TFTP | MAC | IP :8443 | MES (сейчас) |
|--|--|--|--|--|
| pi-shelf | `2cb76931` | `b8:27:eb:b7:69:31` | `.82` | `gi1/0/14` |
| pi-net1 | `36f22cbe` | `b8:27:eb:f2:2c:be` | `.83` | `gi1/0/15` |
| pi-net2 | `8c24c0e8` | `b8:27:eb:24:c0:e8` | `.84` | `gi1/0/13` |

Профиль ног: `godminer/src/board_rpi.h` (BCM 17/27 LED).
MCP-конфиги: [`mcp/`](mcp/) · stub SD: [`netboot-stub/`](netboot-stub/) · payload: `scripts/mk-rpi-netd-ramdisk.sh`.

## Цель

Хост толкает образ по TFTP → RAM initrd → MCP+gpio. SD — только `bootcode.bin` (stub).

## Сейчас

| факт | дальше |
|--|--|
| TFTP+dnsmasq · initrd netd · MCP `:8443` · tool `gpio` на `.82–.84` | PPPS · I2C/реле |
| три Netac stub уже прошиты | Ethernet + питание каждой; ~20 с до link |
| IP по MAC в `/init` | i2c/реле |

## Образ

Один на всех: править TFTP `/srv/tftp/rpi` (initrd из `mk-rpi-netd-ramdisk.sh`), затем power-cycle. SD stub не трогать.
