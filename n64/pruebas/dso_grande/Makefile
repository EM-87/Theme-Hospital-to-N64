# Basado en examples/ovldemo/Makefile de libdragon. Uso: make (DSO) o make estatico
BUILD_DIR = build
include $(N64_INST)/include/n64.mk
LUA_DIR ?= $(TH64_WORK)/src/lua55/lua-5.5.0/src
LUA_EXCLUDE := lua.c luac.c onelua.c linit.c liolib.c loslib.c loadlib.c
LUA_SRCS := $(filter-out $(addprefix $(LUA_DIR)/,$(LUA_EXCLUDE)),$(wildcard $(LUA_DIR)/*.c))
LUA_OBJS := $(patsubst $(LUA_DIR)/%.c,$(BUILD_DIR)/lua/%.o,$(LUA_SRCS))
CFLAGS += -I$(LUA_DIR) -Wno-error
$(BUILD_DIR)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

all: th64dso.z64
MAIN_ELF_EXTERNS := $(BUILD_DIR)/th64dso.externs
MAIN_ELF_SYMBOLS := $(BUILD_DIR)/th64dso.msym
DSO_LIST = filesystem/ventana.dso filesystem/lua.dso
$(BUILD_DIR)/th64dso.dfs: $(DSO_LIST)
$(BUILD_DIR)/th64dso.elf: $(BUILD_DIR)/src/main.o $(MAIN_ELF_EXTERNS)
$(MAIN_ELF_EXTERNS): $(DSO_LIST)
filesystem/ventana.dso: $(BUILD_DIR)/mod/ventana.o
filesystem/lua.dso: $(BUILD_DIR)/mod/luaglue.o $(LUA_OBJS)
th64dso.z64: N64_ROM_TITLE="TH64 DSO"
th64dso.z64: $(BUILD_DIR)/th64dso.dfs $(MAIN_ELF_SYMBOLS)
$(MAIN_ELF_SYMBOLS): $(BUILD_DIR)/th64dso.elf $(MAIN_ELF_EXTERNS)

# Variante estática: el mismo código enlazado en el ejecutable
estatico: th64est.z64
$(BUILD_DIR)/est/main.o: src/main.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -DESTATICO -o $@ $<
EST_LUA_OBJS := $(patsubst $(LUA_DIR)/%.c,$(BUILD_DIR)/est/lua/%.o,$(LUA_SRCS))
$(BUILD_DIR)/est/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<
$(BUILD_DIR)/th64est.elf: $(BUILD_DIR)/est/main.o $(BUILD_DIR)/est/luaglue.o $(EST_LUA_OBJS)
$(BUILD_DIR)/est/luaglue.o: mod/luaglue.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<
th64est.z64: N64_ROM_TITLE="TH64 EST"
.PHONY: all estatico
