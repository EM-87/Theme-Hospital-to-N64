ROMNAME := th64calib
BUILD_DIR = build
include $(N64_INST)/include/n64.mk
CFLAGS += $(EXTRA_CFLAGS)
all: $(ROMNAME).z64
C_FILES := $(shell find src -name '*.c')
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))
$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)
$(ROMNAME).z64: N64_ROM_TITLE = "TH64 CALIB"
clean:
	$(RM) -r $(BUILD_DIR) *.z64
