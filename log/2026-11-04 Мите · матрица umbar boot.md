# 2026-11-04 — Мите · матрица umbar boot (завершение этапа)

<tags>mitya, umbar, cdba, boot, matrix, net_init</tags>

<topo>
name: [[cursor]]
links:
  - [[lumag]]
  - [[george]]
path: log/2026-11-04 Мите · матрица umbar boot.md
</topo>

## Что гоняли

Ядро: qualcomm-linux **7.4.0-rc1** + твой **CFG-64** (`olddefconfig`).

Ramdisk: наш `net_init` **stay** · pagesize **2048** · base `0x80000000`.

Канал: `cdba -b <slot> -h umbar.lab -c 2` · `out/boot-<slot>-linux.img`.


## Таблица

| слот | образ | итог | Linux | model | stay | eth | MCP | примечание |
|---|---|---|---|---|---|---|---|---|
| rb1-1 | `boot-rb1-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. Robotics RB1 | yes | none | none |  |
| rb2-1 | `boot-rb2-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. QRB4210 RB2 | yes | none | none |  |
| rb3-1 | `boot-rb3-1-linux.img` | **OK** | 7.4.0-rc1 | Thundercomm Dragonboard 845c | yes | 169.254.221.82/16 | 169.254.221.82:8443 |  |
| rb3gen2-1 | `boot-rb3gen2-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. Robotics RB3gen2 | yes | none | none |  |
| rb5-1 | `boot-rb5-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. Robotics RB5 | yes | 169.254.221.82/16 | 169.254.221.82:8443 | в матрице окно рано; отдельный прогон OK+MCP |
| rb5gen2-1 | `boot-rb5gen2-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. QCS8550 RB5Gen2 | yes | none | none |  |
| db410c-1 | `boot-db410c-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. APQ 8016 SBC | yes | none | none |  |
| db820c-1 | `boot-db820c-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. DB820c | yes | 169.254.221.82/16 | 169.254.221.82:8443 |  |
| ifc6560-1 | `boot-ifc6560-1-linux.img` | **OK** | 7.4.0-rc1 | Inforce 6560 Single Board Computer | yes | 169.254.221.82/16 | 169.254.221.82:8443 |  |
| openq835-1 | `boot-openq835-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. MSM8998 v1 MTP | yes | none | none |  |
| sdm845-hdk-1 | `boot-sdm845-hdk-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. SDM845 MTP | yes | none | none |  |
| sm8150-hdk-1 | `boot-sm8150-hdk-1-linux.img` | **download без Linux** | — | — | — | — | — | fastboot download, Linux не стартовал |
| sm8350-hdk-1 | `boot-sm8350-hdk-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. SM8350 HDK | yes | 169.254.221.82/16 | 169.254.221.82:8443 |  |
| sm8450-hdk-1 | `boot-sm8450-hdk-1-linux.img` | **OK** | 7.4.0-rc1 | Qualcomm Technologies, Inc. SM8450 HDK | yes | none | none |  |
| db8074-1 | `—` | **нет образа** | — | — | — | — | — | нет dtb в ядре 7.4 / дереве |
| ifc6410-1 | `—` | **нет образа** | — | — | — | — | — | нет dtb в ядре 7.4 / дереве |

## Сводка

| | |
|---|---|
| слотов umbar | 16 |
| образов собрано | 14 |
| Linux + stay | **13** |
| Linux (без stay в логе) | 0 |
| нет образа | db8074-1, ifc6410-1 (нет dtb в 7.4 arm64) |
| download без Linux | sm8150-hdk-1 (AVB/dtbo mismatch в логе) |
| openq835 | dtb `msm8998-mtp` (не lantronix openq) → model MTP |
| sdm845-hdk | dtb `sdm845-mtp` (в сборке нет `sdm845-hdk.dtb`) |

RPB firmware cpio в ramdisk не клали — missing mbn/sqe/qca в dmesg ожидаемы.


Репозиторий: `AnatolyGeorgievski/R3build` · conf на слот в `board/qcom/<slot>.conf`.


**подпись:** Курсор · для [[lumag]]

