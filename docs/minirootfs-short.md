# Минимальный образ загрузки

Образ собран на основе дистрибутива Alpine-Linux.  
Минимальный образ требует всего двух самодостаточных компонент: `musl`, `busybox`. Неупакованный размер такого образа около 3Мб. Минимальное окружение для запуска программ на чистом С требует только двух компонент: `ld-musl` - загрузчик и `libc.so` - библиотека языка С.

OpenRC для minirootfs **не нужен**. Достаточно BusyBox init + два скрипта на загрузку и выгрузку системы. 
Ниже — компактная схема сборки и инструкции как отлаживаться без консоли.

## Модель загрузки (три уровня, без OpenRC)

```text
ядро
  → /sbin/init          (BusyBox)
    → /etc/inittab      (описание действий)
      → sysinit:  /etc/init.d/rcS  (startRC)
      → shutdown: /etc/init.d/rcK  (stopRC)
```

| Компонент | Роль |
|-----------|------|
| **init** | читает `inittab`, исполняет сценарий `sysinit:`, при halt — `shutdown:` |
| **rcS** | запускает: `syslogd` → mount → network → ntp → dropbear → приложение |
| **rcK** | стоп сервисов → umount |
| **OpenRC** | тяжёлый менеджер зависимостей Alpine — для 4–8 МБ ramdisk лишний |

Обновление прошивки — **не** в rcS на каждом буте, а отдельный путь: команда/флаг в `/config` или процесс `update.sh`, в конце `reboot` → rcK → загрузка нового fit/boot-image.

Пример сборки образа:
```sh
ARCH=armv7 ./r3build/alpine-rootfs.sh \
  busybox dropbear dropbear-scp ca-certificates mtd-utils-flash mtd-utils-ubi
ARCH=armv7 ./install-init.sh
```
---

## Минимальные файлы

Цепочка: **ядро → `/init` → `/sbin/init` → `/etc/inittab` → `rcS`**.

`/init`:
```sh
#!/bin/sh
# /init — точка входа initramfs (ядро ищет /init)
mount -t devtmpfs devtmpfs /dev 2>/dev/null
exec 0</dev/console
exec 1>/dev/console
exec 2>/dev/console
exec /sbin/init "$@"
```


**`/etc/inittab`**
```text
::sysinit:/etc/init.d/rcS
::shutdown:/etc/init.d/rcK
::ctrlaltdel:/sbin/reboot
ttyS0::respawn:/sbin/getty -L 115200 ttyS0 vt100
```
* tty подставить свой: `ttyS0` / `ttyPS0` / `ttyAML0`

`/etc/init.d/rcS` (start RC)
```sh
#!/bin/sh
export PATH=/sbin:/bin:/usr/sbin:/usr/bin

# корень ramdisk (если уже rw — безвредно)
mount -o remount,rw / 2>/dev/null

mkdir -p /dev /proc /sys /run /tmp 
mount -t proc     proc     /proc || true
mount -t sysfs    sysfs    /sys  || true
mount -t devtmpfs devtmpfs /dev 2>/dev/null || {
  mount -t tmpfs tmpfs /dev
  mdev -s
} || true
mount -t tmpfs -o mode=0755,nosuid,nodev tmpfs /run 2>/dev/null || true
mkdir -p /dev/pts /dev/shm /var/log
mkdir -p /config /sn
# опциональное из fstab (pts, shm, tmp, run) — если fstab нет, не падает
mount -a 2>/dev/null || true

# лог в файл СРАЗУ — пока нет консоли
exec >>/var/log/boot.log 2>&1
echo "=== rcS $(date 2>/dev/null || echo start) ==="
set -x

# syslogd (BusyBox) — дублирует в сокет/файл
syslogd -n -O /var/log/messages &
sleep 1

# Setup (под свое окружение), из Ant-miner профиль
# mount -t ubifs -- разделы: config, sn, nvdata
# Перенос паролей, индивидуальная конфигурация сохраняется при перезагрузке:
# [ -f /config/passwd ] && ln -sf /config/passwd /etc/passwd
# [ -f /config/shadow ] && ln -sf /config/shadow /etc/shadow
# Настройка GPIO - индикация состояния

ip link set lo up
ip link set eth0 up 2>/dev/null
# DHCP:
udhcpc -i eth0 -b -p /run/udhcpc.pid -s /usr/share/udhcpc/default.script 2>/dev/null
# или статический IP:
# ip addr add 192.168.1.50/24 dev eth0
# ip route add default via 192.168.1.1

# DNS для ntp
echo "nameserver 8.8.8.8" >/etc/resolv.conf

# NTP служба
ntpd -q -n -p pool.ntp.org 2>/dev/null || true
ntpd -p pool.ntp.org 2>/dev/null &

# SSH сервер
mkdir -p /etc/dropbear
[ -f /etc/dropbear/dropbear_ecdsa_host_key ] || \
  dropbearkey -t ecdsa -f /etc/dropbear/dropbear_ecdsa_host_key
dropbear -R -p 22

# приложение

echo "=== rcS done ==="
```

`/etc/init.d/rcK` (kill RC)
```sh
#!/bin/sh
exec >>/var/log/boot.log 2>&1
echo "=== rcK ==="
killall dropbear ntpd gminer 2>/dev/null
killall udhcpc 2>/dev/null
umount -a -r 2>/dev/null
echo "=== rcK done ==="
```

**Обновление** (отдельный скрипт `/usr/sbin/fw-update`, не в rcS):
```sh
#!/bin/sh
# $1 = путь к fit/boot- Image, скачан и распакован BMU
flash_erase /dev/mtd1 0 0
nandwrite -p -s 0x0 /dev/mtd1 "$1"
sync
reboot
```
Перед reboot запустится сценарий shutdown → rcK.

---

## Запуск «вслепую» и как увидеть ошибки

Без UART отладка:

1. **Лог в файл** (`/var/log/boot.log`) — как выше, `exec >>` в начале rcS.  
2. **Dropbear** как можно раньше — после сети; зашёл по SSH → `cat /var/log/boot.log`.  
3. **Статический IP**, если DHCP не поднимается.  
4. **Индикатор**: мигание LED/GPIO в начале/конце rcS.  
5. Первый образ — **только** mount + syslog + static IP + dropbear.

Если init не находит `/etc/inittab` или `rcS` не executable — ядро может «зависнуть» без логов. 
Типовая причина, почему при копировании сценарии не запускаются - перевод строк, CRLF → LF.
Проверь в `rootfs/`:
```bash
chmod +x rootfs/etc/init.d/rcS rootfs/etc/init.d/rcK
ls -l rootfs/sbin/init   #  → busybox
```
Встроенный init BusyBox: `/sbin/init` applet должен быть включён в busybox. busybox- самодостаточная программа, для того чтобы она развернулась в множество утилит нужно запустить установку `/usr/busybox --install -s` или создать множество симлинков по списку апплетов `/usr/busybox --list`. 

---

### musl

В Alpine rootfs:
- `/lib/ld-musl-*.so.1` (symlink -armv7 → -armhf )
- `libc.musl-*.so`      (symlink libc.so → libc.musl-*.so )

Своди бинарники под musl кладёшь в rootfs. `alpine-base` добавляет библиотеки C++ -- это избыточно.
В системе можно установить два загрузчика и две библиотеки libc: musl и glibc, без конфликтов.
Для сборки на хосте рекомендуется установить пакет `musl-dev`.

---

### apk и скрипты post-install / `rc-update`

`rc-update` — команда **OpenRC**. Postinst Alpine делают:
- `rc-update add dropbear default`
- создание user/group
- symlink в `/etc/runlevels/...`

Без OpenRC эти скрипты **не нужны** и на foreign ARCH без эмуляции часто **падают**. Поэтому скрипт запускаем без скриптов и без кеша в rootfs:

```sh
apk --arch $ARCH --root rootfs … --no-script --no-cache add …
```

Симлинки, которые реально нужны (busybox applets), содержит сам пакет **без** post.script и уже находятся в пакетах `alpine-base`/`busybox`.

---

## Сеть и «перезапуск служб»

Без OpenRC:

| Действие | Как |
|----------|-----|
| Поднять net | `ip` + `udhcpc` в rcS |
| Перезапустить dropbear | `killall dropbear; dropbear -R -p 22` |
| После смены IP | снова udhcpc или `ip addr` |
| «Сервис упал»  | inittab `:respawn:` на один демон **или** простой watchdog-цикл в фоне |

Не обязательно имитировать `rc-service restart`.

---

## Сценарий сборки minimal rootfs

```bash
#!/bin/sh
# ARCH=armv7 ROOTFS=rootfs-armv7 APK=./apk.static ./alpine-rootfs.sh alpine-base dropbear
set -e
ARCH=${ARCH:-aarch64}
VER=${VER:-3.21}
ROOTFS=${ROOTFS:-rootfs-$ARCH}
MIRROR="https://dl-cdn.alpinelinux.org/alpine/v${VER}/main"
APK=${APK:-apk.static}

mkdir -p $ROOTFS

# 1) пустая БД apk в ROOTFS
$APK --arch $ARCH --root $ROOTFS --initdb add 2>/dev/null || true

# 2) ключи подписи пакетов Alpine
$APK --arch $ARCH --root $ROOTFS -X $MIRROR --keys-dir $ROOTFS/etc/apk/keys \
  --allow-untrusted add alpine-keys

echo $MIRROR > $ROOTFS/etc/apk/repositories

# 3) пакеты (без postinst и без кэша в дереве)
$APK --arch $ARCH --root $ROOTFS -X $MIRROR --keys-dir $ROOTFS/etc/apk/keys \
  --no-script --no-cache add "$@"

du -sh $ROOTFS
```

Дальше создать в `$ROOTFS`: `etc/inittab`, `etc/init.d/rcS`, `etc/init.d/rcK`, `passwd`, ключи `dropbear`.  
Собрать ramdisk.cpio.gz  
Упаковать в образ загрузки.

---

## Порядок отладки «без консоли»

1. Rootfs: alpine-base + dropbear, static IP, rcS только mount+ip+dropbear+log.  
2. Прошить, ждать, SSH на статический адрес.  
3. `cat /var/log/boot.log` — видно, где упало.  
4. Добавить ntp, mount config, gminer по одному шагу.

## Полный набор утилит

Мы выделяем профили сборки: `ant`, `hurd`, `swarn`, ... . 
Под каждый профиль планируется различный набор пакетов:
+ `ant`: ca-certificates dropbear dropbear-scp mtd-utils-flash mtd-utils-ubi
