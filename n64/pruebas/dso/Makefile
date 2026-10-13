# Basado en examples/ovldemo/Makefile de libdragon
BUILD_DIR=build
include $(N64_INST)/include/n64.mk
all: th64dso.z64
MAIN_ELF_EXTERNS := $(BUILD_DIR)/th64dso.externs
MAIN_ELF_SYMBOLS := $(BUILD_DIR)/th64dso.msym
DSO_LIST = filesystem/sim.dso filesystem/menu.dso
$(BUILD_DIR)/th64dso.dfs: $(DSO_LIST)
$(BUILD_DIR)/th64dso.elf: $(BUILD_DIR)/src/main.o $(MAIN_ELF_EXTERNS)
$(MAIN_ELF_EXTERNS): $(DSO_LIST)
filesystem/sim.dso: $(BUILD_DIR)/mod/sim.o
filesystem/menu.dso: $(BUILD_DIR)/mod/menu.o
th64dso.z64: N64_ROM_TITLE="TH64 DSO"
th64dso.z64: $(BUILD_DIR)/th64dso.dfs $(MAIN_ELF_SYMBOLS)
$(MAIN_ELF_SYMBOLS): $(BUILD_DIR)/th64dso.elf $(MAIN_ELF_EXTERNS)
clean:
	rm -rf $(BUILD_DIR) filesystem th64dso.z64
