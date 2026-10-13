ROMNAME ?= th64cpp
BUILD_DIR = build/$(ROMNAME)
include $(N64_INST)/include/n64.mk
CXXFLAGS += $(EXTRA_CXXFLAGS)
all: $(ROMNAME).z64
OBJS := $(BUILD_DIR)/src/main.o
$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)
$(ROMNAME).z64: N64_ROM_TITLE = "TH64 CPP"
clean:
	$(RM) -r build *.z64
