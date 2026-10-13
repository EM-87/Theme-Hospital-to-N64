# ROM de medida de la fase 4b: RAM libre con 4 MiB según vídeo y audio.
# Uso: make V=nombre DEFS="-DNBUF=2 -DAFREQ=22050"
V ?= base
DEFS ?=
ROMNAME := ram4_$(V)
BUILD_DIR = build/$(V)
include $(N64_INST)/include/n64.mk
CFLAGS += $(DEFS)
all: $(ROMNAME).z64
$(BUILD_DIR)/$(ROMNAME).dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/$(ROMNAME).elf: $(BUILD_DIR)/src/main.o
$(ROMNAME).z64: N64_ROM_TITLE = "TH64 RAM4"
$(ROMNAME).z64: $(BUILD_DIR)/$(ROMNAME).dfs
