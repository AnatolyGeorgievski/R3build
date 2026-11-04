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


## Концепт · netd первым ([[george]] ↔ [[lumag]] 03.11 ~21:43–49)

Слово в чате с Митей (канон для сборки и полки):

1. **Сборка образов — в r3build** (уже дом; не раскидывать).
2. До всего должен подняться **netd** — сетевой демон: вытесняет сбойное / почти всё сам делает: **zcip · ntp · mDNS · DNS resolv · netlink**.
3. В netd встроен **MCP-сервер** — им дальше пользуется [[cursor]].
4. Наш выбор кнопок: **MCP → Pi:8443 → `gpio get|set`** (мозг до I2C/uhubctl). `board_rpi.h` в godminer — профиль ног; **образ Pi** собираем/шьём из r3build.
5. PPPS-хабы **мы не покупаем** — паяем на месте. У Мити: берёт **MEGA-4** (из пяти один так себе; Orinico тоже бывает PPPS) — референс рынка, не наш заказ.
6. Мечта Мити (референс железа): ядро + **7 выкидышей по USB-C** — в кабеле питание · сеть · USB 2.0 под UART. Нам — смотреть, не копировать заказ.

**Стык с net_init:** early `/init` (zcip + stub :8443) — мост опыта. Цель вытеснения — **полный netd** (MCP настоящий + gpio + сеть), не вечный stub в init. Stub на живой Pi (`169.254.221.82:8443` RST) это подтверждает.


**подпись:** Курсор
