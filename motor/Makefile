# ROM del motor (fase 5). Uso, con `source tools/env.sh` antes:
#   make                 ROM normal: th64.z64
#   make PRUEBAS=1       ROM que ejecuta las pruebas del núcleo: th64_pruebas.z64
#   make HORAS=N         ROM normal cuyo hospital empieza N horas adelantado:
#                        th64_horas.z64 (para capturas); con MODO=m (0 hito 4,
#                        1 hito 5, 2 vistas, 3 visor) y GIRO=r, abre ese modo
#                        con la cámara girada sin pasar por el menú
#   make MEDIR=1         ROM de medida del hito 5 (tools/hito5/medir.sh): th64_medir.z64
#   ESCENARIO=hito5      el escenario de las pruebas (por defecto hito4; conviene
#                        make clean al cambiarlo). La ROM normal los ofrece todos
#                        en su menú
# Los assets los genera el conversor (tools/conv) en filesystem/, que no está
# en el repositorio porque sale de los datos del juego de cada usuario.
ifeq ($(N64_INST),)
  $(error N64_INST no está definido: source tools/env.sh)
endif

ifdef PRUEBAS
  ROMNAME := th64_pruebas
  BUILD_DIR := build/n64_pruebas
else ifdef HORAS
  ROMNAME := th64_horas
  BUILD_DIR := build/n64_horas
else ifdef MEDIR
  ROMNAME := th64_medir
  BUILD_DIR := build/n64_medir
else
  ROMNAME := th64
  BUILD_DIR := build/n64
endif

include $(N64_INST)/include/n64.mk

# El núcleo se compila además con -Wextra; -Werror y -O2 ya los pone n64.mk.
CFLAGS += -Wextra -Icore/include -Ivista -Iui -Itests

C_FILES := $(wildcard core/src/*.c) $(wildcard vista/*.c) $(wildcard ui/*.c) $(wildcard platform/n64/*.c)
ifdef HORAS
  CFLAGS += -DTH_HOSPITAL_HORAS=$(HORAS)
endif
ifdef MODO
  CFLAGS += -DTH_MODO_INICIAL=$(MODO)
endif
ifdef GIRO
  CFLAGS += -DTH_GIRO_INICIAL=$(GIRO)
endif
ifdef ESCENARIO
  CFLAGS += -DTH_ESCENARIO='"$(ESCENARIO)"'
endif
ifdef MEDIR
  CFLAGS += -DTH_MEDIR
endif
ifdef PRUEBAS
  C_FILES += tests/test_core.c tests/test_map.c tests/test_humanoid.c tests/test_rules.c tests/test_vista.c
  CFLAGS += -DTH_PRUEBAS
endif
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))

all: $(ROMNAME).z64
.PHONY: all

# Assets: tools/conv los deja sin comprimir en assets/; aquí se comprimen
# (hojas de sprites, animaciones, niveles, reglas y textos con aPLib, -c 2) o
# se copian (paletas) a filesystem/.
MKASSET := $(N64_INST)/bin/mkasset
ASSETS := $(shell find assets -type f \( -name '*.tsh' -o -name '*.tpl' -o -name '*.tmp' -o -name '*.tan' \
  -o -name '*.trg' \) 2>/dev/null)
FS_ASSETS := $(patsubst assets/%,filesystem/%,$(ASSETS))
# Fuentes: el conversor deja BMFont (.fnt y su atlas .png) y mkfont las pasa a
# font64 en CI4 (la de más colores, FONT100M, tiene 15 y el transparente).
MKFONT := $(N64_INST)/bin/mkfont
FUENTES := $(shell find assets -type f -name '*.fnt' 2>/dev/null)
FS_ASSETS += $(patsubst assets/%.fnt,filesystem/%.font64,$(FUENTES))
# Sonido: audioconv64 pasa los WAV a wav64 en VADPCM sin Huffman (la
# decisión de la fase 4: el Huffman se descomprime en la CPU), los MIDI a
# mid64 y el SoundFont recortado a sf64.
AUDIOCONV := $(N64_INST)/bin/audioconv64
SONIDOS := $(shell find assets -type f -name '*.wav' 2>/dev/null)
MUSICA := $(shell find assets -type f -name '*.mid' 2>/dev/null)
BANCOS := $(shell find assets -type f -name '*.sf2' 2>/dev/null)
FS_ASSETS += $(patsubst assets/%.wav,filesystem/%.wav64,$(SONIDOS)) \
  $(patsubst assets/%.mid,filesystem/%.mid64,$(MUSICA)) $(patsubst assets/%.sf2,filesystem/%.sf64,$(BANCOS))

filesystem/%.tsh: assets/%.tsh
	@mkdir -p $(dir $@)
	@echo "    [ASSET] $@"
	@$(MKASSET) -c 2 -o $(dir $@) $<

filesystem/%.tmp: assets/%.tmp
	@mkdir -p $(dir $@)
	@echo "    [ASSET] $@"
	@$(MKASSET) -c 2 -o $(dir $@) $<

filesystem/%.tan: assets/%.tan
	@mkdir -p $(dir $@)
	@echo "    [ASSET] $@"
	@$(MKASSET) -c 2 -o $(dir $@) $<

filesystem/%.trg: assets/%.trg
	@mkdir -p $(dir $@)
	@echo "    [ASSET] $@"
	@$(MKASSET) -c 2 -o $(dir $@) $<

filesystem/%.font64: assets/%.fnt assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	@$(MKFONT) --format CI4 -o $(dir $@) $< > /dev/null

filesystem/%.wav64: assets/%.wav
	@mkdir -p $(dir $@)
	@$(AUDIOCONV) --wav-compress vadpcm,huffman=false -o $(dir $@) $< > /dev/null

filesystem/%.mid64: assets/%.mid
	@mkdir -p $(dir $@)
	@echo "    [MID] $@"
	@$(AUDIOCONV) -o $(dir $@) $< > /dev/null

filesystem/%.sf64: assets/%.sf2
	@mkdir -p $(dir $@)
	@echo "    [SF2] $@"
	@$(AUDIOCONV) -o $(dir $@) $< > /dev/null 2>&1

filesystem/%.tpl: assets/%.tpl
	@mkdir -p $(dir $@)
	@cp $< $@

$(BUILD_DIR)/$(ROMNAME).dfs: $(FS_ASSETS) $(wildcard filesystem/*)
$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)

$(ROMNAME).z64: N64_ROM_TITLE = "THEME HOSPITAL 64"
$(ROMNAME).z64: N64_ROM_SAVETYPE = sram256k
$(ROMNAME).z64: $(BUILD_DIR)/$(ROMNAME).dfs

clean:
	$(RM) -r build/n64 build/n64_pruebas build/n64_horas build/n64_medir th64.z64 th64_pruebas.z64 th64_horas.z64 \
	  th64_medir.z64
.PHONY: clean

ifneq ($(wildcard $(BUILD_DIR)),)
  -include $(shell find $(BUILD_DIR) -name '*.d')
endif
