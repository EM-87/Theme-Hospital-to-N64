ROMNAME := th64dbg
BUILD_DIR = build
include $(N64_INST)/include/n64.mk
all: $(ROMNAME).z64
OBJS := $(BUILD_DIR)/src/main.o
$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)
$(ROMNAME).z64: N64_ROM_TITLE = "TH64 DBG"
clean:
	$(RM) -r $(BUILD_DIR) *.z64
