# board/rpi — референс стендов (Raspberry Pi)

В работе: Pi 3 B+ (полка, MES `gi1/0/10`, MAC `b8:27:eb:b7:69:31`, сейчас stub `169.254.221.82:8443`).

Профиль ног: `godminer/src/board_rpi.h`. Сборка payload — r3build (`src/init`, scripts).

## Цель модели стенда

Как umbar/**cdba+fastboot**: хост **толкает образ по сети** → плата грузится в RAM / early init → MCP.  
Не как «каждый раз вынуть SD и перешить rootfs».

| umbar / qcom | полка Pi |
|---|---|
| `cdba` → fastboot `download`+`boot` | TFTP (или USB-boot) → kernel+dtb+ramdisk |
| `boot.img` в RAM | `net_init` / netd ramdisk stay |
| сброс слота cdba | питание / reset (потом power switch на Pi) |

## Как лучше с SD (слово: могу переткнуть)

**Один** физический проход SD — под **тонкий netboot-stub**, не под полный стендовый rootfs.

На карте оставить только то, без чего BCM не стартует сеть/TFTP:

- firmware Pi (`bootcode.bin` / `start*.elf` / `fixx*.dat` по модели)
- `config.txt` / `cmdline.txt` с указанием брать ядро и initramfs с TFTP (или OTP network boot, если уже прошит)
- **не** класть меняющийся `/init` и netd на SD

Дальше итерации — только файлы на **arc2** (`out/rpi/` + TFTP/DHCP). SD не трогать, пока stub жив.

Полный образ на SD — запасной путь / офлайн, не основной цикл стенда.

## Сейчас / позже

| сейчас | позже |
|---|---|
| Pi жива на stub MCP (RST) | собрать **armhf/a64** ramdisk netd+gpio → отдать по TFTP |
| **SD stub записан** (bootcode.bin) 2026-11-04 | в Pi · питание · проверка TFTP |
| TFTP initrd = **netd-rpi+gpio** (04.11) | reboot Pi → zcip · :8443 · `gpio get -g 17` |
| power switch не спаян | reboot руками / PoE / потом gpio-питание ≈ reset слота |

## Порядок хода

1. Переткнуть SD → stub netboot (один раз).
2. На arc2: TFTP-корень из `out/rpi/` (Image/zImage + dtb + `ramdisk_*_net_init` или netd).
3. Reboot Pi → dual-check: zcip/link · MCP :8443 · `gpio get|set`.
4. Смена образа = заменить файлы на TFTP + reboot (как новый `boot.img` в cdba).

Не делать: каждый эксперимент = полный dd на SD; штурм eth umbar вместо полки.
