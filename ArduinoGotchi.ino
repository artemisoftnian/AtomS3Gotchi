/*
 * ArduinoGotchi - A real Tamagotchi emulator for M5Stack AtomS3
 *
 * Copyright (C) 2022 Gary Kwok
 * Copyright (C) 2024 Adapted for M5Stack AtomS3
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// M5Stack AtomS3 specific libraries - Uses M5Unified library
#include <M5Unified.h>
#include <SPIFFS.h>
#include "tamalib.h"
#include "hw.h"
#include "bitmaps.h"
#include "hardcoded_state.h"

/***** M5Stack AtomS3 Display Settings *****/
// AtomS3 has a 240x240 LCD display
// Tamagotchi P1 native resolution is 32x16 pixels
#define TAMA_LCD_WIDTH  32   // Native Tamagotchi width
#define TAMA_LCD_HEIGHT 16   // Native Tamagotchi height  
#define DISPLAY_SCALE 8      // Scale factor: 32*8=256, 16*8=128
#define DISPLAY_OFFSET_X ((240 - (TAMA_LCD_WIDTH * DISPLAY_SCALE)) / 2)   // 48 - Center horizontally
#define DISPLAY_OFFSET_Y ((240 - (TAMA_LCD_HEIGHT * DISPLAY_SCALE)) / 2)  // 64 - Center vertically
/****************************************/

/***** Tama Setting and Features *****/
#define TAMA_DISPLAY_FRAMERATE  10  // Higher framerate for ESP32-S3
#define ENABLE_TAMA_SOUND
#define ENABLE_AUTO_SAVE_STATUS
#define AUTO_SAVE_MINUTES 60    // Auto save interval
#define ENABLE_LOAD_STATE_FROM_SPIFFS
//#define ENABLE_DUMP_STATE_TO_SERIAL_WHEN_START
//#define ENABLE_SERIAL_DEBUG_INPUT
//#define ENABLE_LOAD_HARCODED_STATE_WHEN_START
/***************************/

/***** M5Stack AtomS3 Button Mapping *****/
// Using the 5-way button on the side (UP/DOWN/LEFT/RIGHT/CENTER)
// BtnA = Left/Up, BtnB = Center, BtnC = Right/Down
#define BTN_LEFT_PIN      M5.BtnA    // Left button
#define BTN_MIDDLE_PIN    M5.BtnB    // Middle button (center)
#define BTN_RIGHT_PIN     M5.BtnC    // Right button
#define BTN_SAVE_PIN      M5.BtnA    // Can also use combination for manual save
/******************************************/

// M5Stack AtomS3 display object using M5Unified
LGFX_Sprite sprite;
uint16_t tama_palette[2] = {0x0000, 0xFFFF}; // Black and white palette

/**** TamaLib Specific Variables ****/
static uint16_t current_freq = 0; 
static bool_t matrix_buffer[LCD_HEIGHT][LCD_WIDTH/8] = {{0}};
static byte runOnceBool = 0;
static bool_t icon_buffer[ICON_NUM] = {0};
static cpu_state_t cpuState;
static unsigned long lastSaveTimestamp = 0;
/************************************/

static void hal_halt(void) {
  //Serial.println("Halt!");
}

static void hal_log(log_level_t level, char *buff, ...) {
  Serial.println(buff); 
}

static void hal_sleep_until(timestamp_t ts) {
  // ESP32-S3 can sleep, but we'll keep it simple for now
  //int32_t remaining = (int32_t) (ts - hal_get_timestamp());
  //if (remaining > 0) {
    //delayMicroseconds(1);
    //delay(1);
  //}
}

static timestamp_t hal_get_timestamp(void) {
  return micros();  // Use microseconds for better precision on ESP32-S3
}

static void hal_update_screen(void) {
  displayTama();
} 

static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val) {
  uint8_t mask;
  if (val) {
    mask = 0b10000000 >> (x % 8);
    matrix_buffer[y][x/8] = matrix_buffer[y][x/8] | mask;   
  } else { 
    mask = 0b01111111;
    for(byte i=0;i<(x % 8);i++) {
      mask = (mask >> 1) | 0b10000000;
    }
    matrix_buffer[y][x/8] = matrix_buffer[y][x/8] & mask;  
  }
}

static void hal_set_lcd_icon(u8_t icon, bool_t val) {
  icon_buffer[icon] = val;
}

static void hal_set_frequency(u32_t freq) {
  current_freq = freq;
}

static void hal_play_frequency(bool_t en) {
#ifdef ENABLE_TAMA_SOUND 
  if (en) {
    // Use M5Stack AtomS3 built-in speaker with M5Unified
    M5.Speaker.tone(current_freq);
  } else {
    M5.Speaker.stop();
  }
#endif  
}

static bool_t button4state = 0;

static int hal_handler(void) {
#ifdef ENABLE_SERIAL_DEBUG_INPUT 
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();
    Serial.println(incomingByte, DEC);
    if (incomingByte==49) {
      hw_set_button(BTN_LEFT, BTN_STATE_PRESSED );
    } else if (incomingByte==50) {
      hw_set_button(BTN_LEFT, BTN_STATE_RELEASED );
    } else if (incomingByte==51) {
      hw_set_button(BTN_MIDDLE, BTN_STATE_PRESSED );
    } else if (incomingByte==52) {
      hw_set_button(BTN_MIDDLE, BTN_STATE_RELEASED );
    } else if (incomingByte==53) {
      hw_set_button(BTN_RIGHT, BTN_STATE_PRESSED );
    } else if (incomingByte==54) {
      hw_set_button(BTN_RIGHT, BTN_STATE_RELEASED );
    }  
  } 
#else  
  // Use M5Stack AtomS3 5-way buttons with M5Unified
  // BtnA = Left/Up, BtnB = Center, BtnC = Right/Down
  M5.update();  // Update button states
  if (M5.BtnA.wasPressed()) {
    hw_set_button(BTN_LEFT, BTN_STATE_PRESSED );
  } else if (M5.BtnA.wasReleased()) {
    hw_set_button(BTN_LEFT, BTN_STATE_RELEASED );
  }
  if (M5.BtnB.wasPressed()) {
    hw_set_button(BTN_MIDDLE, BTN_STATE_PRESSED );
  } else if (M5.BtnB.wasReleased()) {
    hw_set_button(BTN_MIDDLE, BTN_STATE_RELEASED );
  }
  if (M5.BtnC.wasPressed()) {
    hw_set_button(BTN_RIGHT, BTN_STATE_PRESSED );
  } else if (M5.BtnC.wasReleased()) {
    hw_set_button(BTN_RIGHT, BTN_STATE_RELEASED );
  }
  #ifdef ENABLE_AUTO_SAVE_STATUS 
    // Use long press of BtnB (center) for manual save
    if (M5.BtnB.pressingFor(2000)) {
      if (button4state==0) {
        saveStateToSPIFFS();
        // Visual feedback
        M5.Display.fillScreen(TFT_GREEN);
        delay(200);
        M5.Display.fillScreen(TFT_BLACK);
      }
      button4state = 1;
    } else {
      button4state = 0;
    }
  #endif
#endif  
  return 0;
}

static hal_t hal = {
  .halt = &hal_halt,
  .log = &hal_log,
  .sleep_until = &hal_sleep_until,
  .get_timestamp = &hal_get_timestamp,
  .update_screen = &hal_update_screen,
  .set_lcd_matrix = &hal_set_lcd_matrix,
  .set_lcd_icon = &hal_set_lcd_icon,
  .set_frequency = &hal_set_frequency,
  .play_frequency = &hal_play_frequency,
  .handler = &hal_handler,
};

void drawTriangle(uint8_t x, uint8_t y) {
  // Draw scaled triangle for selection indicator (no offset needed, sprite handles it)
  sprite.drawLine(x+1,y+1,x+5,y+1);
  sprite.drawLine(x+2,y+2,x+4,y+2); 
  sprite.drawLine(x+3,y+3,x+3,y+3); 
}

void drawTamaRow(uint8_t tamaLCD_y, uint8_t actualLCD_y, uint8_t thick) {
  uint8_t i;
  for (i = 0; i < LCD_WIDTH; i++) {
    uint8_t mask = 0b10000000;
    mask = mask >> (i % 8);
    if ((matrix_buffer[tamaLCD_y][i/8] & mask) != 0) {
      // Draw scaled pixel block (offsets removed since sprite is pushed at offset position)
      sprite.fillRect(i * DISPLAY_SCALE, 
                      actualLCD_y * DISPLAY_SCALE, 
                      DISPLAY_SCALE, thick * DISPLAY_SCALE);
    }
  }
}

void drawTamaSelection(uint8_t y) {
  uint8_t i;
  for(i=0;i<7;i++) {
    if (icon_buffer[i]) drawTriangle(i*16+5,y);
    // Draw icon bitmap scaled
    for(int by=0; by<9; by++) {
      for(int bx=0; bx<16; bx++) {
        uint8_t byteIdx = (i*18) + by*2 + (bx/8);
        uint8_t bitIdx = 7 - (bx % 8);
        if (pgm_read_byte(bitmaps + byteIdx) & (1 << bitIdx)) {
          sprite.fillRect((i*16+bx) * DISPLAY_SCALE,
                         (y+6+by) * DISPLAY_SCALE,
                         DISPLAY_SCALE, DISPLAY_SCALE);
        }
      }
    }
  }
  if (icon_buffer[7]) {
    drawTriangle(7*16+5,y);
    // Draw 8th icon
    for(int by=0; by<9; by++) {
      for(int bx=0; bx<16; bx++) {
        uint8_t byteIdx = (7*18) + by*2 + (bx/8);
        uint8_t bitIdx = 7 - (bx % 8);
        if (pgm_read_byte(bitmaps + byteIdx) & (1 << bitIdx)) {
          sprite.fillRect((7*16+bx) * DISPLAY_SCALE,
                         (y+6+by) * DISPLAY_SCALE,
                         DISPLAY_SCALE, DISPLAY_SCALE);
        }
      }
    }
  } 
}

void displayTama() {
  uint8_t j;
  // Clear the sprite with black background
  sprite.fillSprite(TFT_BLACK);
  
  // Draw Tamagotchi display area
  for (j = 0; j < LCD_HEIGHT; j++) {
    if(j!=5) drawTamaRow(j,j,2);
    if(j==5) {
       drawTamaRow(j,j,1);
       drawTamaRow(j,j+1,1);
    }
  }
  
  // Draw selection icons at bottom
  drawTamaSelection(0);
  
  // Push sprite to main display at centered position
  sprite.pushSprite(DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
}

#ifdef ENABLE_DUMP_STATE_TO_SERIAL_WHEN_START
void dumpStateToSerial() {
  uint16_t i, count=0;
  char tmp[10];
  cpu_get_state(&cpuState);
  u4_t *memTemp = cpuState.memory;
  uint8_t *cpuS = (uint8_t *)&cpuState;

  Serial.println("");
  Serial.println("static const uint8_t hardcodedState[] PROGMEM = {");
  for(i=0;i<sizeof(cpu_state_t);i++,count++) {
    sprintf(tmp, "0x%02X,", cpuS[i]);
    Serial.print(tmp);
    if ((count % 16)==15) Serial.println("");
  }
  for (i = 0; i < MEMORY_SIZE; i++,count++) {
    sprintf(tmp, "0x%02X,",memTemp[i]);
    Serial.print(tmp);
    if ((count % 16)==15) Serial.println("");
  }  
  Serial.println("};");
/*
  Serial.println("");
  Serial.println("static const uint8_t bitmaps[] PROGMEM = {");
  for(i=0;i<144;i++) {
    sprintf(tmp, "0x%02X,", bitmaps_raw[i]);
    Serial.print(tmp);
    if ((i % 18)==17) Serial.println("");
  }
  Serial.println("};");  */
} 
#endif

#ifdef ENABLE_LOAD_HARCODED_STATE_WHEN_START
void loadHardcodedState() {
  cpu_get_state(&cpuState);
  u4_t *memTemp = cpuState.memory;
  uint16_t i;
  uint8_t *cpuS = (uint8_t *)&cpuState;
  for(i=0;i<sizeof(cpu_state_t);i++) {
    cpuS[i]=pgm_read_byte_near(hardcodedState+i);
  }
  for(i=0;i<MEMORY_SIZE;i++) {
    memTemp[i]=pgm_read_byte_near(hardcodedState+ sizeof(cpu_state_t) + i);
  }
  cpuState.memory = memTemp;
  cpu_set_state(&cpuState);
  Serial.println("Hardcoded");
 }
#endif

#ifdef ENABLE_AUTO_SAVE_STATUS 
void saveStateToSPIFFS() {
  int i=0;
  File file = SPIFFS.open("/tamagotchi_state.bin", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open state file for writing");
    return;
  }
  
  cpu_get_state(&cpuState);
  
  // Write CPU state
  file.write((uint8_t*)&cpuState, sizeof(cpu_state_t));
  
  // Write memory
  for(i=0;i<MEMORY_SIZE;i++) {
    uint8_t memByte = cpuState.memory[i];
    file.write(&memByte, 1);
  }
  
  file.close();
  Serial.println("State saved to SPIFFS");
}
#endif

#ifdef ENABLE_LOAD_STATE_FROM_SPIFFS
void loadStateFromSPIFFS() {
  cpu_get_state(&cpuState);
  u4_t *memTemp = cpuState.memory;
  
  File file = SPIFFS.open("/tamagotchi_state.bin", FILE_READ);
  if (!file) {
    Serial.println("No saved state found in SPIFFS");
    return;
  }
  
  // Read CPU state
  file.read((uint8_t*)&cpuState, sizeof(cpu_state_t));
  cpu_set_state(&cpuState);
  
  // Read memory
  int i=0;
  for(i=0;i<MEMORY_SIZE;i++) {
    memTemp[i] = file.read();
  }
  
  file.close();
  Serial.println("State loaded from SPIFFS");
}
#endif


uint8_t reverseBits(uint8_t num) {
    uint8_t reverse_num = 0;
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if((num & (1 << i)))
           reverse_num |= 1 << ((8 - 1) - i);  
   }
   return reverse_num;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize M5Stack AtomS3 with M5Unified
  auto cfg = M5.config();
  M5.begin(cfg);
  
  // Initialize display
  M5.Display.begin();
  M5.Display.fillScreen(TFT_BLACK);
  
  // Initialize speaker
  M5.Speaker.begin();
  M5.Speaker.setVolume(64);  // Set initial volume
  
  // Initialize SPIFFS for persistent storage
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  // Initialize display sprite buffer - size matches Tamagotchi native resolution scaled
  // LCD_WIDTH and LCD_HEIGHT are from hw.h (32x16)
  sprite.createSprite(LCD_WIDTH * DISPLAY_SCALE, (LCD_HEIGHT + 10) * DISPLAY_SCALE);
  sprite.setColorDepth(1);  // 1-bit color depth for black/white
  sprite.setPaletteColor(0, TFT_BLACK);
  sprite.setPaletteColor(1, TFT_WHITE);
  
  // Clear screen to black
  M5.Display.fillScreen(TFT_BLACK);

  tamalib_register_hal(&hal);
  tamalib_set_framerate(TAMA_DISPLAY_FRAMERATE);
  tamalib_init(1000000);

#ifdef ENABLE_LOAD_STATE_FROM_SPIFFS 
  loadStateFromSPIFFS();
#endif  

#ifdef ENABLE_LOAD_HARCODED_STATE_WHEN_START
  loadHardcodedState();
#endif

/*
  int i;
  for(i=0;i<(18*8);i++) {
    bitmaps_raw[i]= reverseBits(bitmaps_raw[i]);
  }
*/

#ifdef ENABLE_DUMP_STATE_TO_SERIAL_WHEN_START
  dumpStateToSerial();
#endif 

}

void loop() {
  M5.update();  // Update M5Stack button states
  tamalib_mainloop_step_by_step();
#ifdef ENABLE_AUTO_SAVE_STATUS   
  if ((millis() - lastSaveTimestamp) > (AUTO_SAVE_MINUTES * 60 * 1000)) {
    lastSaveTimestamp = millis();
    saveStateToSPIFFS();
  }
#endif  
}
