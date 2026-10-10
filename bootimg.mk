# Pack the Y220 boot image with the MTK KERNEL/ROOTFS headers.
# BOARD_CUSTOM_BOOTIMG_MK skips the default mkbootimg rules, so the
# recovery image rule has to live here too. Nothing flashes that recovery:
# the phone stays on TWRP.

$(INSTALLED_BOOTIMAGE_TARGET): $(HOST_OUT_EXECUTABLES)/mtkbootimg $(INTERNAL_BOOTIMAGE_FILES)
	@echo ----- Making MTK boot image ------
	$(HOST_OUT_EXECUTABLES)/mtkbootimg $(INTERNAL_BOOTIMAGE_ARGS) --mtk 1 --output $@
	@echo ----- Made MTK boot image -------- $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE),raw)

$(INSTALLED_RECOVERYIMAGE_TARGET): $(MKBOOTIMG) \
		$(recovery_ramdisk) \
		$(recovery_kernel)
	@echo ----- Making recovery image ------
	$(MKBOOTIMG) $(INTERNAL_RECOVERYIMAGE_ARGS) --output $@
	@echo ----- Made recovery image -------- $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE),raw)
