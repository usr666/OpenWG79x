# Opensource firmware for Worx Landroid wg79x robotic mowers
For now only model wg794 with circuit board db275 is supported, hopefully all WG79x-models in the future.
Based on the LandLord project: https://github.com/Damme/LandLord/

## Current project status
Use at your own risk! Only use it in a safe area. Always keep the mower under strict observation and be prepared to press the stop button or lift mower if it does something unexpected.
This software does not fulfil any safety standards.

### What is working
- Basic driving inside wire area
- Reverse when hitting obstacles
- Turn when hitting wire (very basic, needs improvement)
- Tilt detection
- Wire following and detection of charging station
- Battery charging
- Display output and keyboard input
- Wheel and disc motor control
- Bump and lift sensor detection
- Basic wire sensors
- Stop button
- Storing of current state in non volatile memory
- Scheduling

### What is not implemented/working
- Detect when mower is stuck and wheels just spin
- Detect when mower is stuck and just moves back and forth in a small area
- Detect when motors are blocked and wheel/disc does not move
- Detect motor overcurrent?
- Pin code
  
## Setup build environment
Use a linux machine, for example Ubuntu 20.04.  
Install gcc-arm:  
Download gcc-arm binaries for your host computer architecture from https://developer.arm.com/downloads/-/gnu-rm/10-3-2021-10  
Unpack downloaded file, for example with:  
sudo tar -xvf gcc-arm-none-eabi-10.3-2021.10-aarch64-linux.tar.bz2 -C /opt/
set GCCPATH variable to point to compiler install directory, for example:
export GCCPATH=/opt/gcc-arm-none-eabi-10.3-2021.10

## Build firmware
type command "make" from top directory

## Load firmware in lawnmower
### Option 1: Use USB-stick
Place built file DB275_GRAF.bin on an empty FAT32-formatted USB drive
Insert USB-stick in lawnmower, press and hold power-on button for 10 seconds.

### Option 2: Open the lawnmower and solder connections for an ST-Link v2 programmer
Connect wires according to this table (signal names are printed on bottom side of pcb):

|ST-link V2 |JTAG on worx|
|---------- |------------|
|reset      |RST
|swdio      |TMS
|swim       |TDO
|swclk      |TCK
|3.3v       |+3.3v
|GND        |GND

First backup existing firmware including bootloader  
```
openocd -f interface/stlink.cfg -c "transport select hla_swd"  -f target/lpc17xx.cfg -c "adapter speed 100" -c init -c "dump_image img.bin 0 0x100000" -c shutdown
```

Then program new firmware file  
```
openocd -f interface/stlink.cfg -c "transport select hla_swd"  -f target/lpc17xx.cfg -c "adapter speed 100" -c init -c "program openwg79x.bin 0x9000" -c shutdown
```
