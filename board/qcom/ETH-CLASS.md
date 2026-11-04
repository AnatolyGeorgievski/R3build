# board/qcom — класс eth по слоту umbar

<tags>qcom, eth, wifi, usb-lan, firmware, zcip, mdns</tags>

Цель: каждый слот → **eth + zcip + MCP :8443** малыми усилиями.

## Классы

| класс | что на плате | путь покрытия |
|---|---|---|
| **U** | USB-LAN (онборд / NIC на полке) | драйвер в ядре; ждать enumerate; pick `eth*` |
| **W** | только WiFi | модули + firmware в ramdisk (рецепт Мити) |
| **G** | RGMII/stmmac | `status=okay` + phy (sm8150 сейчас disabled) |
| **∅** | нет dtb | нет образа |

`net_init` не поднимает `wlan0` без модулей/fw. Берёт первый `eth*`/`en*`/`usb*`, ждёт ~5с. MCP: `addr` · `ifaces` · `ping <ip>` · `peers`. mDNS: `r3board.local`.

## Слоты

| слот | класс | заметка |
|---|---|---|
| rb1-1 | W | wcn3990; USB-NIC → U |
| rb2-1 | W | wcn3990 |
| rb3-1 | U | AX88179 в прогоне |
| rb3gen2-1 | W/U | wcn6750; USB hub |
| rb5-1 | U | USB2LAN → AX88179 |
| rb5gen2-1 | W | wifi |
| db410c-1 | W | wcnss.mbn |
| db820c-1 | U | eth0 в прогоне |
| ifc6560-1 | U | eth+MCP OK |
| openq835-1 | W | wcn3990 |
| sdm845-hdk-1 | W | wcn3990 |
| sm8150-hdk-1 | G+W | ethqos disabled + AVB |
| sm8350-hdk-1 | U | lan78xx |
| sm8450-hdk-1 | W | wifi |
| db8074-1 / ifc6410-1 | ∅ | нет dtb |

**подпись:** Курсор
