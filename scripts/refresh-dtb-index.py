#!/usr/bin/env python3
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1] / 'board' / 'qcom'
rows = []
for d in sorted(ROOT.iterdir()):
    if not d.is_dir() or not (d / 'SOURCE.txt').exists():
        continue
    dts = next(iter(d.glob('*.dts')), None)
    dtb = next(iter(d.glob('*.dtb')), None)
    text = (d / 'SOURCE.txt').read_text(encoding='utf-8', errors='ignore')
    if 'STATUS=missing' in text:
        st = 'missing'
    elif 'STATUS=saved-dts-only' in text:
        st = 'dts-only'
    elif dts or dtb:
        st = 'saved'
    else:
        st = 'missing'
    rows.append((d.name, dts.name if dts else '—', dtb.name if dtb else '—', st))
lines = [
    '# board/qcom — dts/dtb по слотам umbar', '',
    '<tags>qcom, dtb, dts, umbar, cdba</tags>', '',
    'Каждая плата: `board/qcom/<BOARD_ID>/` — `.dts` + `.dtb` + `SOURCE.txt`.', '',
    '| BOARD_ID | dts | dtb | status |', '|---|---|---|---|',
]
for b, dts, dtb, st in rows:
    lines.append(f'| {b} | {dts} | {dtb} | {st} |')
lines += ['', '```bash', './scripts/save-board-dtb.sh [/path/to/arm64/boot/dts/qcom]', '```', '']
(ROOT / 'DTB-INDEX.md').write_text('\n'.join(lines), encoding='utf-8')
assert b'\x00' not in (ROOT / 'DTB-INDEX.md').read_bytes()
print(f'I: DTB-INDEX.md ({len(rows)} boards)')
