# R3 · статус полки umbar (qcom)

<tags>umbar, cdba, mitya, status, alpine, stay, rootfs</tags>

Срез для Мити и для себя. Канал: `cdba -b <slot> -h umbar.lab`.  
Ядро: qualcomm-linux **7.4.0-rc1** + **CFG-64**.  
`boot.img`: pagesize **2048**, base `0x80000000` (как в его `build.sh`).

Два наших payload:

| режим | ramdisk | цель |
|---|---|---|
| **stay** | `net_init` + `stay` | Linux · zcip · MCP :8443 |
| **Alpine** | minirootfs aarch64 + ant + netd | getty `ttyMSM0` · `root` / `admin` |

Журнал хода: `log/2026-11-04 Мите · матрица umbar boot.md`, `log/2026-11-05 полка · sm8150 sm8350 · Мите.md`.  
Conf на слот: `board/qcom/<slot>.conf`.


## Таблица слотов

| слот | stay Linux | Alpine login | eth+MCP (U) | заметка |
|---|---|---|---|---|
| rb1-1 | yes | — | — (W) | |
| rb2-1 | yes | — | — (W) | |
| rb3-1 | yes | **yes** | **yes** | |
| rb3gen2-1 | yes | — | — | |
| rb5-1 | yes | **yes** | no в том прогоне | |
| rb5gen2-1 | yes | — | — | |
| db410c-1 | yes | — | — | |
| db820c-1 | yes | **yes** | **yes** | |
| ifc6560-1 | yes | init, login не поймали | **yes** | |
| openq835-1 | yes | — | — | dtb `msm8998-mtp` |
| sdm845-hdk-1 | yes | — | — | dtb `sdm845-mtp` |
| sm8150-hdk-1 | см. ниже | **нет** | — | |
| sm8350-hdk-1 | yes | **yes** | **yes** | |
| sm8450-hdk-1 | yes | — | — | |
| db8074-1 | нет образа | — | — | нет dtb в 7.4 |
| ifc6410-1 | нет образа | — | — | нет dtb в 7.4 |

**stay:** 13/14 с образом. **Alpine login поймали:** rb3, rb5, db820c, sm8350. **U eth+MCP:** 4/5 (rb5 без eth в том прогоне). WiFi/fw в ramdisk не клали.


### sm8150

Packing `Image.gz+dtb` как у Мити. ABL пишет «no appended DTB» — то же на живых платах.  
Без earlycon: после Exit EBS тишина (казалось «Linux не стартовал»).  
С `earlycon=qcom_geni,0xa90000`: Linux **SM8150 HDK** есть; до `Run /init` не дошли — RPMH/cpufreq timeout. Этап закрыт без новых вариантов.


## Как грузит Митя · диск (открыто)

В его `build.sh` cmdline по умолчанию:

```text
root=PARTLABEL=rootfs rootwait
```

Не SD «с ноутбука». Метка GPT **`rootfs`** на **встроенном** носителе платы (UFS / eMMC — что у слота).  
Пара `do_image …` / `do_image …-rootfs`: второй образ с **пустым** ramdisk — ядро+dtb, корень уже на разделе.  
Firmware/modules он дописывает в ramdisk из RPB (`initramfs-firmware-<board>-…`).

У нас сейчас: `rdinit=/init root=/dev/ram0` (всё в boot.img).  
Дальше: какой блок на каждой плате несёт `PARTLABEL=rootfs`, чем его прошить, и нужен ли нам его rootfs-диск или остаёмся на ramdisk.

**подпись:** Курсор · 2026-11-05
