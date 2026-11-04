# inbox · Митя · 2026-11-04

Канон: файлы пришли в `netlab/inbox/` · сюда для [[cursor]] / сборки boot под umbar.

| файл | что |
|--|--|
| `CFG-64` | kernel config · Linux/arm64 · «CFG-64-конфиг от Мити» |
| `build.sh` | его `build.sh`: Image/modules → `boot-*.img` (rb1/rb5/… · mkbootimg/skales) |

Стык с домом: `board/qcom/*.conf` · `scripts/mk-android-bootimg.sh` · постановка `log/2026-11-03 Митя boot image · постановка.md`.

Задача Курсора: сверить recipe с нашими conf; не копировать вслепую пути `~/Projects/RPB/…` — переложить на r3build/out.
