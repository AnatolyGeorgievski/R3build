#!/bin/sh
# fw-update — скрипт пост-обработки сетевого обновления прошивки
# Запускается из директории, куда BMU распаковала компоненты

# Перенаправляем вывод в системный лог обновлений для отладки
exec >>/var/log/fw-update.log 2>&1
echo "=== Starting firmware update: $(date 2>/dev/null || echo 'now') ==="
set -x

# 1. СКАНИРОВАНИЕ И ОПРЕДЕЛЕНИЕ ФАЙЛОВ ЯДРА/RAMDISK
FIT_IMAGE=""
if [ -f "fitImage_new" ]; then
    FIT_IMAGE="fitImage_new"
elif [ -f "datafile" ]; then
    FIT_IMAGE="datafile"
elif [ -f "fitImage" ]; then
    FIT_IMAGE="fitImage"
fi

# 2. ОБНОВЛЕНИЕ ЗАГРУЗЧИКА (ОДНА КОПИЯ, МТD0)
if [ -f "BOOT.bin" ]; then
    echo "Updating bootloader (BOOT.bin) on /dev/mtd0..."
    flash_erase /dev/mtd0 0 0 >/dev/null
    nandwrite -p -s 0x0 /dev/mtd0 BOOT.bin
else
    echo "No BOOT.bin found. Skipping bootloader update."
fi

# 3. ОБНОВЛЕНИЕ ЯДРА И RAMDISK (FIT-ОБРАЗ, MTD1)
if [ -n "$FIT_IMAGE" ]; then
    echo "Updating kernel and ramdisk ($FIT_IMAGE) on /dev/mtd1..."
    flash_erase /dev/mtd1 0 0 >/dev/null
    nandwrite -p -s 0x0 /dev/mtd1 "$FIT_IMAGE"
else
    echo "No kernel/fitImage components found. Skipping kernel update."
fi

# 4. ЗАВЕРШЕНИЕ И ПЕРЕЗАГРУЗКА
# echo "Flushing file buffers to NAND..."
sync

# Проверяем, обновилось ли хоть что-то перед перезагрузкой
if [ -f "BOOT.bin" ] || [ -n "$FIT_IMAGE" ]; then
    echo "Update successful! Rebooting system..."
    sleep 1
    reboot
else
    echo "ERROR: Nothing to update. No valid components found."
    exit 1
fi
