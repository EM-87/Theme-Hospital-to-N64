# ROM de medida (fase 5): toca las 8 piezas de música con el banco recortado,
# con la misma configuración de sonido que el motor, para grabar en ares lo que
# suena en la N64 y compararlo con fluidsynth (tools/musica/grabar_ares.sh).
# Los ficheros (MID64 y SF64) son los que genera el Makefile del motor en
# motor/filesystem/musica; hay que compilar el motor antes. Con
# MUSICA=<dir> (ruta absoluta) toca los .mid64 de otro directorio, que debe
# tener también banco.sf64; conviene `make clean` al cambiarlo.
ROMNAME := th64musica
BUILD_DIR = build
MUSICA ?= ../../../motor/filesystem/musica
N64_MKDFS_ROOT := $(MUSICA)

ifeq ($(N64_INST),)
  $(error N64_INST is not set)
endif
include $(N64_INST)/include/n64.mk

all: $(ROMNAME).z64
.PHONY: all

C_FILES := $(shell find src -name '*.c')
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))

$(BUILD_DIR)/$(ROMNAME).dfs: $(wildcard $(N64_MKDFS_ROOT)/*)
$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)

$(ROMNAME).z64: N64_ROM_TITLE = "TH64 MUSICA"
$(ROMNAME).z64: $(BUILD_DIR)/$(ROMNAME).dfs

clean:
	$(RM) -r $(BUILD_DIR) *.z64
.PHONY: clean

ifneq ($(wildcard $(BUILD_DIR)),)
  -include $(shell find $(BUILD_DIR) -name '*.d')
endif
