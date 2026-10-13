BUILD_DIR = build
include $(N64_INST)/include/n64.mk
TIPOS = none eeprom4k eeprom16k sram256k sram768k sram1m flashram
all: $(foreach t,$(TIPOS),save_$(t).z64) save_cpak.z64
OBJS = $(BUILD_DIR)/src/main.o
$(foreach t,$(TIPOS),$(eval save_$(t).z64: N64_ROM_SAVETYPE = $(t)))
$(foreach t,$(TIPOS),$(eval $(BUILD_DIR)/save_$(t).elf: $(OBJS)))
save_cpak.z64: N64_ROM_SAVETYPE = none
save_cpak.z64: N64_ROM_CONTROLLER1 = n64,pak=controller
$(BUILD_DIR)/save_cpak.elf: $(OBJS)
%.z64: N64_ROM_TITLE = "TH64 SAVE"
clean:
	rm -rf $(BUILD_DIR) *.z64 *.eeprom *.ram *.flash *.pak
