# Set GCCPATH variable before building, for example:
# export GCCPATH=/opt/gcc-arm-none-eabi-10.3-2021.10

#TARGETNAME:=$(firstword $(basename $(wildcard *.c)))
TARGETNAME:=openwg79x
SYSINC:=common
U8GPATH:=u8g
LDSCRIPTDIR:=$(SYSINC)
SRC:=main.c display.c hal/hal.c hal/hal_power.c hal/hal_sensors.c hal/hal_keyboard.c hal/hal_display.c $(wildcard $(SYSINC)/*.c) $(wildcard $(U8GPATH)/*.c)
MCPU:=cortex-m3

STARTUP:=$(wildcard $(SYSINC)/*.S)

LDSCRIPT:=$(wildcard $(LDSCRIPTDIR)/*.ld)


#================================================
# Main part of the Makefile starts here. Usually no changes are needed.

# Internal Variable Names
ELFNAME:=$(TARGETNAME).elf
BINNAME:=$(TARGETNAME).bin
HEXNAME:=$(TARGETNAME).hex
DISNAME:=$(TARGETNAME).dis
MAPNAME:=$(TARGETNAME).map
OBJ:=$(SRC:.c=.o) $(STARTUP:.S=.o)

# Replace standard build tools by avr tools
CC:=$(GCCPATH)/bin/arm-none-eabi-gcc
AR:=$(GCCPATH)/bin/arm-none-eabi-ar
AS:=$(GCCPATH)/bin/arm-none-eabi-gcc
OBJCOPY:=$(GCCPATH)/bin/arm-none-eabi-objcopy
OBJDUMP:=$(GCCPATH)/bin/arm-none-eabi-objdump
SIZE:=$(GCCPATH)/bin/arm-none-eabi-size

# Common flags
COMMON_FLAGS = -mthumb -mcpu=$(MCPU)
COMMON_FLAGS += -Wall -I. -I$(SYSINC) -I$(U8GPATH)
# default stack size is 0x0c00
COMMON_FLAGS += -D__STACK_SIZE=0x0a00
COMMON_FLAGS += -Os -flto 
COMMON_FLAGS += -ffunction-sections -fdata-sections
# Assembler flags
ASFLAGS:=$(COMMON_FLAGS) -D__STARTUP_CLEAR_BSS -D__START=main
# C flags
CFLAGS:=$(COMMON_FLAGS) -std=gnu99
# LD flags
GC:=-Wl,--gc-sections
MAP:=-Wl,-Map=$(MAPNAME)
LFLAGS:=$(COMMON_FLAGS) $(GC) $(MAP)
LDLIBS:=--specs=nano.specs -lc -lc -lnosys -L$(LDSCRIPTDIR) -T $(LDSCRIPT)

# Additional Suffixes
.SUFFIXES: .elf .hex .dis .bin

# Targets
.PHONY: all
all: $(DISNAME) $(HEXNAME) $(BINNAME)
	$(SIZE) $(ELFNAME)

.PHONY: clean
clean:
	$(RM) $(OBJ) $(HEXNAME) $(BINNAME) $(ELFNAME) $(DISNAME) $(MAPNAME)

# implicit rules
.S.o:
	$(PREPROCESS.S) $(COMMON_FLAGS) $(patsubst %.s,%.S,$<) > tmp.s
	$(COMPILE.s) -c -o $@ tmp.s
	$(RM) tmp.s

.elf.hex:
	@$(OBJCOPY) -O ihex $< $@

.elf.bin:
	@$(OBJCOPY) -O binary $< $@

# explicit rules
$(ELFNAME): $(OBJ) 
	$(LINK.o) $(LFLAGS) $(OBJ) $(LDLIBS) -o $@

$(DISNAME): $(ELFNAME)
	$(OBJDUMP) -S $< > $@
