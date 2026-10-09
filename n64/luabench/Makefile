# ROM de microbenchmarks de Lua. Uso:
#   make LUA_DIR=<ruta a lua-5.x.y/src> ROMNAME=luabench55
# (tools/luabench.sh prepara las fuentes de Lua 5.4.6 y 5.5.0 y lo llama).
ROMNAME ?= luabench
BUILD_DIR = build/$(ROMNAME)
LUA_DIR ?= $(error indica LUA_DIR=<ruta a lua-5.x.y/src>)

include $(N64_INST)/include/n64.mk

# Núcleo y bibliotecas de Lua, sin el intérprete, el compilador ni las
# bibliotecas que dependen del sistema operativo (io, os, package).
LUA_EXCLUDE := lua.c luac.c onelua.c linit.c liolib.c loslib.c loadlib.c
LUA_SRCS := $(filter-out $(addprefix $(LUA_DIR)/,$(LUA_EXCLUDE)),$(wildcard $(LUA_DIR)/*.c))
LUA_OBJS := $(patsubst $(LUA_DIR)/%.c,$(BUILD_DIR)/lua/%.o,$(LUA_SRCS))

N64_CFLAGS += -I$(LUA_DIR) -Wno-error

all: $(ROMNAME).z64
.PHONY: all

$(BUILD_DIR)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "    [CC] lua/$*.c"
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/$(ROMNAME).dfs: filesystem/run.lua
	@mkdir -p $(dir $@)
	@echo "    [DFS] $@"
	$(N64_MKDFS) $@ filesystem >/dev/null

$(BUILD_DIR)/$(ROMNAME).elf: $(BUILD_DIR)/main.o $(LUA_OBJS)
$(BUILD_DIR)/main.o: src/main.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(ROMNAME).z64: N64_ROM_TITLE = "TH64 luabench"
$(ROMNAME).z64: $(BUILD_DIR)/$(ROMNAME).dfs

clean:
	$(RM) -r build *.z64
.PHONY: clean
