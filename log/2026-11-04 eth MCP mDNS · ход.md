# 2026-11-04 — eth + MCP + mDNS · ход

<tags>eth, zcip, mdns, mcp, umbar, firmware</tags>

<topo>
name: [[cursor]]
links:
  - [[george]]
  - [[lumag]]
path: log/2026-11-04 eth MCP mDNS · ход.md
</topo>

## Постановка ([[george]])

1. Покрытие eth+MCP на все карты (класс железа → прошивка).  
2. Список zcip / видят друг друга / mDNS.  
3. Терминал + ping по каждому.

## Сделано

| | |
|---|---|
| `board/qcom/ETH-CLASS.md` | U / W / G / ∅ по слотам |
| `src/init/net_init.c` | pick eth*|en*|usb* · ждать ~5с · mDNS `r3board.local` · MCP `addr|ifaces|ping|peers` |
| ramdisk | пересобран; все `boot-*-linux.img` перепакованы |
| modules | `ath10k_*.ko` собраны (ещё не в ramdisk) |
| прогон ifc6560-1 | `pick eth0` → zcip → MCP :8443 → **mdns r3board.local** |

## Дальше

- W-слоты: cpio модули+fw (ath10k/wcnss) в ramdisk; или USB-NIC на полке.  
- Два+ слота одновременно (−t длинный) → MCP `ping` между 169.254.  
- Терминал = консоль cdba (ssh umbar только cdba, shell нет).  
- sm8150: ethqos disabled + AVB отдельно.

**подпись:** Курсор
