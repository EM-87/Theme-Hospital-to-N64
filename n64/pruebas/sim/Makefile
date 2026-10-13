# Copia adaptada de n64/rambase/Makefile para las pruebas de herramientas.
ROMNAME := th64tools
BUILD_DIR = build

ifeq ($(N64_INST),)
  $(error N64_INST is not set)
endif
include $(N64_INST)/include/n64.mk

# Opciones extra sin pisar CFLAGS (pasar CFLAGS= en la línea de make anula las de n64.mk)
CFLAGS += $(EXTRA_CFLAGS)

all: $(ROMNAME).z64
.PHONY: all

C_FILES := $(shell find src -name '*.c')
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))

$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)

$(ROMNAME).z64: N64_ROM_TITLE = "TH64 TOOLS"

clean:
	$(RM) -r $(BUILD_DIR) *.z64
.PHONY: clean

ifneq ($(wildcard $(BUILD_DIR)),)
  -include $(shell find $(BUILD_DIR) -name '*.d')
endif

# Optimización solo para sim.c (se añade detrás de -O2 de n64.mk, así que gana)
$(BUILD_DIR)/src/sim.o: CFLAGS += $(SIM_OPT)
