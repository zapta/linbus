#
# Makefile for LIN injector development 
# Execute with NMAKE.EXE from MS Visual Studio
#
# MCU type: atmega328p
#
# Fuses: L  0xFF
#        H  0xDA
#        E  0x05
#        LB 0x3F
#
# Assumes Arduino Mini Pro bootloader is present (5V, 16 MHz)
#

all: arduino.hex

OBJS = \
   arduino.o          \
   avr_util.o         \
   custom_config.o    \
   custom_injector.o  \
   custom_module.o    \
   custom_signals.o   \
   hardware_clock.o   \
   leds.o             \
   lin_frame.o        \
   lin_processor.o    \
   sio.o              \
   system_clock.o

HDRS = \
   action_led.h         \
   arduino.h            \
   avr_util.h           \
   custom_config.h      \
   custom_defs.h        \
   custom_injector.h    \
   custom_module.h      \
   custom_signals.h     \
   debouncer.h          \
   hardware_clock.h     \
   injector_actions.h   \
   io_pins.h            \
   leds.h               \
   lin_frame.h          \
   lin_processor.h      \
   passive_timer.h      \
   signal_tracker.h     \
   sio.h                \
   system_clock.h       \
   WString.h

.cpp.o:
   avr-gcc -g -mmcu=$(MCU) -DF_CPU=16000000 -I. -Wall -O2 -c $*.cpp

MCU = atmega328p

arduino.hex: makefile $(OBJS) $(HDRS)
	avr-gcc -g -mmcu=$(MCU) -Wall -o arduino.out -Wall $(OBJS)
	avr-objcopy -R .eeprom -O ihex arduino.out arduino.hex
	avr-size -A arduino.out
	avrdude -carduino -pm328p -Pcom16 -b57600 -U flash:w:arduino.hex
