BUILD_DIR = build
include $(N64_INST)/include/n64.mk
all: th64grande.z64
$(BUILD_DIR)/th64grande.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/th64grande.elf: $(BUILD_DIR)/src/main.o
th64grande.z64: N64_ROM_TITLE = "TH64 GRANDE"
th64grande.z64: $(BUILD_DIR)/th64grande.dfs
