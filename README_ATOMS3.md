# ArduinoGotchi for M5Stack AtomS3

A real Tamagotchi P1 emulator ported to run on the M5Stack AtomS3.

## Features

This version takes full advantage of the M5Stack AtomS3 hardware:

- **Display**: Uses the built-in 240x240 LCD display with scaled Tamagotchi graphics (8x scaling)
- **Sound**: Built-in speaker for authentic Tamagotchi sounds with volume control
- **Controls**: 5-way side buttons (BtnA, BtnB, BtnC) for game interaction
- **Storage**: SPIFFS filesystem for persistent game state storage
- **Performance**: ESP32-S3 dual-core processor allows for smoother emulation (10 FPS vs 3 FPS on Arduino UNO)
- **Visual Feedback**: Screen flashes green when manually saving

## Hardware Requirements

- M5Stack AtomS3 unit (with or without camera)
- USB-C cable for programming and power

## Button Mapping

| AtomS3 Button | Tamagotchi Function |
|---------------|---------------------|
| BtnA (top)    | Left button         |
| BtnB (middle) | Middle button / Long press (2s) for manual save |
| BtnC (bottom) | Right button        |

## Prerequisites

### Software

- **Arduino IDE** - [Download and Install](https://www.arduino.cc/en/software)
- **M5Unified Library** - Install via Library Manager:
  - Sketch → Include Library → Manage Libraries
  - Search "M5Unified" and install "M5Unified" by M5Stack
- **Git** - To clone the repository

### Installation Steps

1. Clone this repository:
```bash
git clone https://github.com/GaryZ88/ArduinoGotchi
cd ArduinoGotchi
```

2. Open Arduino IDE

3. Add M5Stack board support:
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Go to Tools → Board → Board Manager
   - Search "esp32" and install "esp32 by Espressif Systems"

4. Select your board:
   - Tools → Board → esp32 → M5Stack AtomS3

5. Open `ArduinoGotchi.ino` in Arduino IDE

6. Install required libraries if not already installed:
   - M5Unified library (via Library Manager)

7. Connect your M5Stack AtomS3 via USB-C

8. Click Upload

## Configuration

You can adjust these settings in the main program:

```cpp
/***** M5Stack AtomS3 Display Settings *****/
#define DISPLAY_SCALE 8          // Scale factor (8x for 240x240 display)
#define DISPLAY_OFFSET_X 48      // Horizontal centering
#define DISPLAY_OFFSET_Y 64      // Vertical centering
/****************************************/

/***** Tama Setting and Features *****/
#define TAMA_DISPLAY_FRAMERATE  10  // Frames per second (higher than Arduino)
#define ENABLE_TAMA_SOUND         // Enable built-in speaker
#define ENABLE_AUTO_SAVE_STATUS   // Auto-save enabled
#define AUTO_SAVE_MINUTES 60      // Save interval in minutes
#define ENABLE_LOAD_STATE_FROM_SPIFFS  // Load from SPIFFS storage
/***************************/
```

## Usage Notes

- **First Run**: On first run, you'll need to configure the clock by pressing the middle button (BtnB)
- **Auto-Save**: The game automatically saves every 60 minutes to preserve your pet's progress
- **Manual Save**: Long press the middle button (BtnB) for 2 seconds to manually save - screen will flash green as confirmation
- **Sound**: The built-in speaker produces authentic Tamagotchi beeps with adjustable volume
- **Display**: The Tamagotchi screen is centered and scaled 8x for optimal viewing on the 240x240 display

## Differences from Arduino UNO Version

| Feature | Arduino UNO | M5Stack AtomS3 |
|---------|-------------|----------------|
| Display | External SSD1306 OLED | Built-in 240x240 LCD |
| Controls | 3 external buttons | Built-in 5-way buttons |
| Sound | External buzzer | Built-in speaker |
| Storage | EEPROM | SPIFFS filesystem |
| Framerate | 3 FPS | 10 FPS |
| Timestamp | millis() * 1000 | micros() |
| Library | U8g2 | M5Unified |
| Power | 5V USB | USB-C or battery |

## Troubleshooting

### SPIFFS Mount Failed
- Try formatting SPIFFS: Create a test sketch with `SPIFFS.format()` in setup()

### Display Not Working
- Ensure M5Unified library is properly installed
- Check that you've selected the correct board (M5Stack AtomS3)
- Verify the ESP32 board package is installed

### Buttons Not Responding
- Make sure `M5.update()` is being called (it is by default in loop())
- Check button mapping in the configuration section

### No Sound
- Verify `ENABLE_TAMA_SOUND` is uncommented
- Check volume settings (default is 64)
- Ensure speaker is initialized properly

## Technical Details

### Display Rendering
The Tamagotchi's native 32x16 LCD is scaled 8x to 256x128 pixels, then centered on the 240x240 AtomS3 display. A 1-bit sprite buffer is used for efficient rendering with M5Unified's LGFX_Sprite class.

### State Storage
Game state is stored in SPIFFS as `/tamagotchi_state.bin`, containing:
- CPU state structure
- Emulator memory (MEMORY_SIZE bytes)

### Audio
Uses the M5.Speaker class from M5Unified for tone generation at the original Tamagotchi frequencies. Volume is set to 64 (out of 255) by default.

### Library Changes
This version uses **M5Unified** instead of M5AtomS3 library because:
- M5Unified is the official unified library for all M5Stack devices
- Better support for AtomS3's specific hardware features
- More consistent API across different M5Stack products
- Active development and maintenance

## License

ArduinoGotchi is distributed under the GPLv2 license. See the LICENSE file for more information.

## Credits

- Original ArduinoGotchi: Gary Kwok
- TamaLib: Jean-Christophe Rona
- M5Stack AtomS3 Port with M5Unified: Adapted for ESP32-S3 platform

## Resources

- [M5Stack AtomS3 Documentation](https://docs.m5stack.com/en/core/AtomS3)
- [M5Unified Library GitHub](https://github.com/m5stack/M5Unified)
- [Tamagotchi P1 Wiki](https://tamagotchi.fandom.com/wiki/Tamagotchi_(1996_Pet))
- [TamaLib GitHub](https://github.com/jcrona/tamalib)
