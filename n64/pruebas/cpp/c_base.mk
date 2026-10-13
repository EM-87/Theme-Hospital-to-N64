ROMNAME = c_base
BUILD_DIR = build/c_base
include $(N64_INST)/include/n64.mk
all: c_base.z64
$(BUILD_DIR)/c_base.elf: $(BUILD_DIR)/csrc/main.o
c_base.z64: N64_ROM_TITLE = "TH64 C"
