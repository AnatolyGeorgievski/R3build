# 2026-11-03 — одно место: boot umbar / net_init / dtb

<tags>r3build, umbar, net_init, dtb, не-раскидывать</tags>

<topo>
name: [[cursor]]
links:
  - [[george]]
path: log/2026-11-03 одно место · boot umbar.md
</topo>

Слово [[george]]: полезное по стыку Мити / загрузке **не раскидывать** по godminer · R3-herd · r3build.

## Дом = r3build

| что | где |
|---|---|
| dts/dtb слотов | `board/qcom/<BOARD_ID>/` + `DTB-INDEX.md` |
| conf слотов | `board/qcom/*.conf` |
| mkbootimg | `scripts/mk-android-bootimg.sh` |
| save dtb | `scripts/save-board-dtb.sh` |
| net_init источники | `src/init/` (`net_init.c`, `zcip.c`, заголовки) |
| сборка /init + ramdisk | `scripts/mk-net-init-ramdisk.sh` → `out/` |
| boot.img / ramdisk / init | `out/` |
| ход стыка | `log/2026-11-03 Митя boot image · постановка.md` |

godminer — монолит netd (исходник апплета там же по жизни репо). Для **образа и лабы** рабочая копия и скрипты — только здесь.

**подпись:** Курсор

## Роль net_init ([[george]] 03.11 ~21:32)

Канон: `src/init/README.md`.

Кратко: **здесь** собираем `/init` для boot.img; в godminer — апплет монолита. Роль на стенде — early init + stay → MCP, не ant/rcS и не U-Boot.

Передышка: день за два; когда скучно — сказать. Сейчас — координация и этот канон.

