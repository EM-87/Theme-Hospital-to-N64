# ROM del motor (fase 5). Uso, con `source tools/env.sh` antes:
#   make                 ROM normal: th64.z64
#   make PRUEBAS=1       ROM que ejecuta las pruebas del núcleo: th64_pruebas.z64
# Los assets los genera el conversor (tools/conv) en filesystem/, que no está
# en el repositorio porque sale de los datos del juego de cada usuario.
ifeq ($(N64_INST),)
  $(error N64_INST no está definido: source tools/env.sh)
endif

ifdef PRUEBAS
  ROMNAME := th64_pruebas
  BUILD_DIR := build/n64_pruebas
else
  ROMNAME := th64
  BUILD_DIR := build/n64
endif

include $(N64_INST)/include/n64.mk

# El núcleo se compila además con -Wextra; -Werror y -O2 ya los pone n64.mk.
CFLAGS += -Wextra -Icore/include -Itests

C_FILES := $(wildcard core/src/*.c) platform/n64/main.c
ifdef PRUEBAS
  C_FILES += tests/test_core.c
  CFLAGS += -DTH_PRUEBAS
endif
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))

all: $(ROMNAME).z64
.PHONY: all

$(BUILD_DIR)/$(ROMNAME).dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)

$(ROMNAME).z64: N64_ROM_TITLE = "THEME HOSPITAL 64"
$(ROMNAME).z64: N64_ROM_SAVETYPE = sram256k
$(ROMNAME).z64: $(BUILD_DIR)/$(ROMNAME).dfs

clean:
	$(RM) -r build/n64 build/n64_pruebas th64.z64 th64_pruebas.z64
.PHONY: clean

ifneq ($(wildcard $(BUILD_DIR)),)
  -include $(shell find $(BUILD_DIR) -name '*.d')
endif
