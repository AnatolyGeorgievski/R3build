# board/rpi/mcp — конфиги MCP стенда

<tags>mcp, rpi, стенд, config, манифест</tags>

<topo>
name: board/rpi/mcp
links:
  - ../netboot-stub
  - ../../../scripts/mk-rpi-netd-ramdisk.sh
path: board/rpi/mcp/README.md
</topo>

## Где лежит канон

| файл | роль | куда попадает |
|--|--|--|
| `mcp_netd.json` | манифест tools MCP (в т.ч. `gpio`) | initrd: `/mcp_netd.json` и `/config/mcp.json` |
| `config.json` | минимум для netd (`mcp.tcp`) | initrd: `/config/config.json` |
| `hosts.json` | MAC → IP → serial → MES (люди и `/init`) | **не** в initrd; править и сверять с `mk-rpi-netd-ramdisk.sh` case MAC |

Источник правды для стенда Pi — **эта папка**, не `godminer/src/`.  
godminer держит общий `src/mcp_netd.json` для майнера; сюда копируют/сверяют, когда меняют набор tools стенда.

## Правила

1. [[cursor]] меняет tools → правит `board/rpi/mcp/mcp_netd.json` → гоняет `scripts/mk-rpi-netd-ramdisk.sh` → кладёт `out/rpi/ramdisk_*.cpio.gz` в `/srv/tftp/rpi/initrd.img` (и serial-dirs).
2. Новый host на полке → строка в `hosts.json` **и** ветка `case` MAC в `mk-rpi-netd-ramdisk.sh` **и** `dhcp-host` в `/etc/dnsmasq.d/rpi-netboot.conf`.
3. Порт MCP стенда — **8443**, пока слово [[george]] не сменит.
4. Без `config/mcp.json` + `config/config.json` applet-путь netd не стартует (проверка `access`).

## Сборка payload (кто / что)

**Кто:** [[cursor]] на arc2.  
**Что:** `bash scripts/mk-rpi-netd-ramdisk.sh`  
**Откуда берёт:** `NETD=out/rpi/netd-rpi.armhf`, `MANIFEST=board/rpi/mcp/mcp_netd.json`, `CONFIG=board/rpi/mcp/config.json`.  
**Готово:** файл `out/rpi/ramdisk_netd_armhf.cpio.gz`; после копирования на TFTP — power-cycle Pi.
