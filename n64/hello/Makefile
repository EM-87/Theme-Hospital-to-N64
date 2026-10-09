ROMNAME := th64hello
BUILD_DIR = build

ifeq ($(N64_INST),)
  $(error N64_INST is not set)
endif
include $(N64_INST)/include/n64.mk

all: $(ROMNAME).z64
.PHONY: all

C_FILES := $(shell find src -name '*.c')
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))

$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)

$(ROMNAME).z64: N64_ROM_TITLE = "TH64 Hola"

clean:
	$(RM) -r $(BUILD_DIR) *.z64
.PHONY: clean

ifneq ($(wildcard $(BUILD_DIR)),)
  -include $(shell find $(BUILD_DIR) -name '*.d')
endif
