Opensource firmware for Worx robotic mower model Landroid wg794 with circuit board db275 for now. Hopefully all WG79x-models in the future.
Based on the LandLord project: https://github.com/Damme/LandLord/

Setup build environment:
Use a linux machine, for example Ubuntu 20.04.
Install gcc-arm:
Download gcc-arm binaries for your host computer architecture from https://developer.arm.com/downloads/-/gnu-rm/10-3-2021-10
Unpack downloaded file, for example with:
sudo tar -xvf gcc-arm-none-eabi-10.3-2021.10-aarch64-linux.tar.bz2 -C /opt/

Build firmware:
Set GCCPATH variable before building, for example:
export GCCPATH=/opt/gcc-arm-none-eabi-10.3-2021.10
cd Hello_World
make

Load firmware in lawnmower:
Option 1: Use USB-stick
Place built file Hello_World.bin on USB-stick and rename it to DB275_GRAF.bin
Insert USB-stick in lawnmower and flash as usual firmware.

Option 2: Open the lawnmower and solder connections for an ST-Link v2 programmer
Connect wires according to this table (signal names are printed on bottom side of pcb):
ST-link V2	JTAG on worx
reset       RST
swdio       TMS
swim        TDO
swclk       TCK
3.3v        +3.3v
GND         GND

First backup existing firmware including bootloader:
openocd -f interface/stlink.cfg -c "transport select hla_swd"  -f target/lpc17xx.cfg -c "adapter speed 100" -c init -c "dump_image img.bin 0 0x100000" -c shutdown

Then program new firmware file:
openocd -f interface/stlink.cfg -c "transport select hla_swd"  -f target/lpc17xx.cfg -c "adapter speed 100" -c init -c "program Hello_World.bin 0x9000" -c shutdown
