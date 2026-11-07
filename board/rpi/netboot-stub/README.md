# netboot-stub — правила сборки SD

<tags>rpi, stub, SD, TFTP, bootcode, стенд</tags>

<topo>
name: board/rpi/netboot-stub
links:
  - ../mcp
  - ../../../scripts/write-rpi-netboot-stub.sh
path: board/rpi/netboot-stub/README.md
</topo>

## Зачем stub

На SD только то, без чего Pi3 ROM не начинает сеть. Ядро · dtb · initrd · config — **с TFTP** (`/srv/tftp/rpi`). Полный rootfs на карту не кладём.

## Состав карты (канон)

| на FAT | обязательно |
|--|--|
| `bootcode.bin` | да — из этой папки |
| `STUB.txt` | да — метка PI1/PI2/PI3 + указатель TFTP (пишет скрипт) |
| `start.elf` / `kernel` / `initrd` / `config.txt` | **нет** — с host |

Раздел: один primary FAT32 ~256 MiB, label `PI1BOOT` / `PI2BOOT` / `PI3BOOT`.

## Правила сборки

1. **Носитель:** целый блок (`/dev/sdX`), не раздел. Скрипт отказывает `sda`–`sdc` и nvme0.
2. **Размер:** 8–64 ГиБ (коридор SD стенда).
3. **Кто жжёт:** человек у reader на arc2 (слово [[george]] / дежурный).  
   **Команда:** `scripts/write-rpi-netboot-stub.sh /dev/sdX PI1|PI2|PI3`
4. **Что делает скрипт:** `wipefs` → msdos → FAT → копирует только `board/rpi/netboot-stub/bootcode.bin` + `STUB.txt`.
5. **После прошивки карту не трогать**, пока stub жив. Меняется payload на TFTP, не SD.
6. **Три карты Netac** уже размечены PI1/PI2/PI3 (2026-11-06). Повторный burn — только если stub сбит.

## Связь с MCP-образом

Stub не содержит MCP. После питания: TFTP → initrd из `mk-rpi-netd-ramdisk.sh` (конфиги — `board/rpi/mcp/`).

## Сервер host

- dnsmasq: `/etc/dnsmasq.d/rpi-netboot.conf`
- tftp-root: `/srv/tftp/rpi`
- serial-dirs: `2cb76931` · `36f22cbe` · `8c24c0e8` (совпадают с `mcp/hosts.json`)

## Приёмка stub

1. Карта в Pi, Ethernet, питание.  
2. В `journalctl -u dnsmasq` — PXE этого MAC и `sent …/kernel7.img`.  
3. MCP на IP из `mcp/hosts.json` · `:8443`.

**подпись:** канон stub · 2026-11-07
