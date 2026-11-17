# Teensy 3.6 US-to-JIS Keyboard Converter Makefile
TARGET = us2jpk
MCU = MK66FX1M0
CPUARCH = cortex-m4
F_CPU = 180000000

# Teensyduino Paths
TEENSY_APP = /Applications/Teensyduino.app/Contents/Java
TOOLSPATH = $(TEENSY_APP)/hardware/tools
COMPILERPATH = $(TOOLSPATH)/arm/bin
COREPATH = $(TEENSY_APP)/hardware/teensy/avr/cores/teensy3

CC = $(COMPILERPATH)/arm-none-eabi-gcc
CXX = $(COMPILERPATH)/arm-none-eabi-g++
OBJCOPY = $(COMPILERPATH)/arm-none-eabi-objcopy
SIZE = $(COMPILERPATH)/arm-none-eabi-size

# Options & Flags
# USB_SERIAL_HID: Serial + Keyboard + Mouse + Joystick (enables KEYBOARD_INTERFACE)
OPTIONS = -DF_CPU=$(F_CPU) -DUSB_SERIAL_HID -DLAYOUT_US_ENGLISH \
          -D__$(MCU)__ -DARDUINO=10805 -DTEENSYDUINO=144 \
          -DKEYBOARD_INTERFACE=2 -DSHOW_KEYBOARD_DATA

INCLUDES = -Isrc -IUSBHost_t36 -I$(COREPATH) -I$(TEENSY_APP)/hardware/teensy/avr/libraries/EEPROM

CPPFLAGS = -Wall -g -Os -mcpu=$(CPUARCH) -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
           -fsingle-precision-constant -ffunction-sections -fdata-sections \
           -MMD $(OPTIONS) $(INCLUDES)

CFLAGS = 
CXXFLAGS = -std=gnu++14 -felide-constructors -fno-exceptions -fno-rtti

LDFLAGS = -Os -Wl,--gc-sections,--defsym=__rtc_localtime=0 --specs=nano.specs \
          -mcpu=$(CPUARCH) -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
          -T$(COREPATH)/mk66fx1m0.ld
LIBS = -larm_cortexM4lf_math -lm

# Build Directory
BUILDDIR = build

# Source Files
SRC_APP_CPP = src/app.cpp src/us_jis_converter.cpp src/bluetooth_stubs.cpp
SRC_APP_C = src/usb_desc.c

SRC_USBHOST = USBHost_t36/enumeration.cpp \
              USBHost_t36/ehci.cpp \
              USBHost_t36/memory.cpp \
              USBHost_t36/hid.cpp \
              USBHost_t36/hub.cpp \
              USBHost_t36/keyboard.cpp \
              USBHost_t36/keyboardHIDExtras.cpp \
              USBHost_t36/mouse.cpp \
              USBHost_t36/print.cpp

# Exclude core's default usb_desc.c in favor of src/usb_desc.c (extended Usage Maximum to 255)
SRC_CORE_C = $(filter-out $(COREPATH)/usb_desc.c, $(wildcard $(COREPATH)/*.c))
SRC_CORE_CPP = $(wildcard $(COREPATH)/*.cpp)
SRC_CORE_S = $(wildcard $(COREPATH)/*.S)

# Object Files
OBJS = $(patsubst src/%.cpp, $(BUILDDIR)/app/%.o, $(SRC_APP_CPP)) \
       $(patsubst src/%.c, $(BUILDDIR)/app/%.o, $(SRC_APP_C)) \
       $(patsubst USBHost_t36/%.cpp, $(BUILDDIR)/usbhost/%.o, $(SRC_USBHOST)) \
       $(patsubst $(COREPATH)/%.c, $(BUILDDIR)/core/%.o, $(SRC_CORE_C)) \
       $(patsubst $(COREPATH)/%.cpp, $(BUILDDIR)/core/%.o, $(SRC_CORE_CPP)) \
       $(patsubst $(COREPATH)/%.S, $(BUILDDIR)/core/%.o, $(SRC_CORE_S))

all: $(BUILDDIR)/$(TARGET).hex

$(BUILDDIR)/$(TARGET).hex: $(BUILDDIR)/$(TARGET).elf
	@echo "Creating HEX file: $@"
	$(SIZE) $<
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	@echo "Linking: $@"
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

# Compile rules
$(BUILDDIR)/app/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/app/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/usbhost/%.o: USBHost_t36/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/core/%.o: $(COREPATH)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/core/%.o: $(COREPATH)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/core/%.o: $(COREPATH)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -c $< -o $@

flash: $(BUILDDIR)/$(TARGET).hex
	teensy_loader_cli --mcu=TEENSY36 -w -v $(BUILDDIR)/$(TARGET).hex

clean:
	rm -rf $(BUILDDIR)

.PHONY: all flash clean
