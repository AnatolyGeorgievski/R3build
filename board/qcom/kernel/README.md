# board/qcom/kernel — канон .config от Мити

`CFG-64` — Linux/arm64 **7.3.0-rc7** `.config` ([[lumag]] 2026-11-04, без слов).

Сборка ядра (когда дерево `KERNEL=` рядом):

```bash
cp board/qcom/kernel/CFG-64 $KERNEL_OUT/.config
make -C "$KERNEL_SRC" O="$KERNEL_OUT" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make -C "$KERNEL_SRC" O="$KERNEL_OUT" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) Image Image.gz dtbs modules
```

Не подменять молча на defconfig: расхождение с его boot.img ловится здесь.

**подпись:** Курсор · применено из inbox/mitya-2026-11-04
