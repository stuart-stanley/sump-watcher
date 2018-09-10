
/*
 * sump/list system monitor.
 * 
 * uses an SCT013-30/1 to detect when it's one/off
 * uses a 1.8" adafruit ST7735 display/inputs (not used yet)
 */
#include <SPI.h>
#include <SD.h>
#include <Adafruit_seesaw.h>
#include <Adafruit_TFTShield18.h>

#include <Adafruit_ST77xx.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST7735.h>

#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>


// TFT display and SD card will share the hardware SPI interface.
// Hardware SPI pins are specific to the Arduino board type and
// cannot be remapped to alternate pins.  For Arduino Uno,
// Duemilanove, etc., pin 11 = MOSI, pin 12 = MISO, pin 13 = SCK.
#define SD_CS    4  // Chip select line for SD card on Shield
#define TFT_CS  10  // Chip select line for TFT display on Shield
#define TFT_DC   8  // Data/command line for TFT on Shield
#define TFT_RST  -1  // Reset line for TFT is handled by seesaw!

// ss is the seesaw logic (talking over SPI lib)
Adafruit_TFTShield18 ss;


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// more local-ish tuning...
#define PRIMARY_BACKGROUND ST77XX_BLACK
#define PRIMARY_TEXT_COLOR ST77XX_WHITE
#define PRIMARY_TEXT_SIZE 1
#define FONT_HEIGHT 8
#define FONT_WIDTH 5

// And now the current-detector
#include "EmonLib.h"             // Include Emon Library
#define EMON_APIN 1              // analog1 adc
#define EMON_CALIBRATION 30      // Fixed because the 30A/1v has its own resistor
#define EMON_IRMS_SAMPLES 1480   // from demo code
#define EMON_LOCAL_VOLTAGE 120.0 // only really used for printing
EnergyMonitor emon1;             // Create an instance

#define EMON_RED_LED_PIN       6 // soldered on dev-shield
#define EMON_GRN_LED_PIN       5 // same!

double zero_cutoff = 1.00;          // todo: detect thoi
void setup() {
  Serial.begin(9600);
  Serial.println("Setting up status LEDs");
  pinMode(EMON_GRN_LED_PIN, OUTPUT);
  pinMode(EMON_RED_LED_PIN, OUTPUT);
  Serial.println("Setting up view panel");
  panel_init();
  Serial.println("Setting up EMON");
  emon1.current(EMON_APIN, EMON_CALIBRATION);
}

int cnt = 0;
void loop() {
  double Irms = doCurrentSample();
  
  Serial.print(Irms*120.0);         // Apparent power
  Serial.print(" ");
  Serial.println(Irms);          // Irms
  doRunningStatus(Irms);
  if ( Irms > zero_cutoff ) {
    digitalWrite(EMON_RED_LED_PIN, HIGH);
  } else {
    digitalWrite(EMON_RED_LED_PIN, LOW);    
  }
  delay(500);
}

void doRunningStatus(double current_current) {
  static boolean first_time = true;
  static boolean was_running = false;
  static uint16_t row = getRowFromLast(1);

  boolean running;

  if (current_current > zero_cutoff) {
    running = true;
  } else {
    running = false;
  }

  if (first_time) {
    printStatus("Running: ", row);
    tft.print("no");
    first_time = false;
  }

  if (was_running != running) {
    if (running) {
      if (!was_running) {
          printStatus("Running: ", row);
          bmpPoop(0, 0);
          tft.print("yes");
          was_running = true;
      }
    } else {
      if (was_running) {
        printStatus("Running: ", row);
        // blackout the poop. todo: relate to poop-window!
        tft.fillRect(0, 0, 128, 128, PRIMARY_BACKGROUND);
        tft.print("no");
        was_running = false;
      }
    }
  }
}

/*
 * panel_init()
 * 
 * Do basic setup of the display panel along
 * with checks.
 */
void panel_init() {

  // start by disabling both SD and TFT
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Start seesaw helper chip
  if (!ss.begin()){
    Serial.println("seesaw could not be initialized!");
    while(1);
  }
  Serial.println("seesaw started");
  Serial.print("Version: "); Serial.println(ss.getVersion(), HEX);

  // Start set the backlight off
  ss.setBacklight(TFTSHIELD_BACKLIGHT_OFF);
  // Reset the TFT
  ss.tftReset();

  // Initialize 1.8" TFT
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab

  Serial.println("TFT OK!");
  tft.fillScreen(ST77XX_CYAN);

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
  } else {
    Serial.println("OK!");
    // File root = SD.open("/");
    // printDirectory(root, 0);
    // root.close();
    bmpPoop(0, 0);
  }
  delay(500);
  // Set backlight on fully
  ss.setBacklight(TFTSHIELD_BACKLIGHT_ON);
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab

  tft.fillScreen(PRIMARY_BACKGROUND);
  tft.setTextWrap(false);
  tft.setTextColor(PRIMARY_TEXT_COLOR, PRIMARY_BACKGROUND);
  tft.setTextSize(PRIMARY_TEXT_SIZE);
}

double doCurrentSample() {
  digitalWrite(EMON_GRN_LED_PIN, HIGH);
  // printSamplingHeader();
  // tft.print("yes");  // 'yes' looks dorky. Trust the LED is enough...
  double Irms = emon1.calcIrms(1480);  // Calculate Irms only
  digitalWrite(EMON_GRN_LED_PIN, LOW);
  printSamplingHeader();
  tft.print(Irms);
  return Irms;
}

uint16_t getRowFromLast(uint16_t row_from_last) {
  uint16_t row_sub_value;
  
  row_from_last += 1;  // adjust for 0 offset
  row_sub_value = FONT_HEIGHT * row_from_last;
  if (row_sub_value > tft.height()) {
    row_sub_value = tft.height();
  }
  return (tft.height() - row_sub_value) / FONT_HEIGHT;
}

void printSamplingHeader() {
  static uint16_t row = getRowFromLast(0);

  printStatus("Sampling: ", row);
}

uint16_t cursorToX(uint16_t x0) {
  uint16_t pixel_x0 = FONT_WIDTH * x0;
  
  // if beyond end-of-display, bring back
  if (pixel_x0 >= (tft.width() - FONT_WIDTH)) {
    pixel_x0 = tft.width() - FONT_WIDTH;
  }
  return pixel_x0;
}

uint16_t cursorToY(uint16_t y0) {
  uint16_t pixel_y0 = FONT_HEIGHT * y0;

  // if beyond end-of-display, bring back
  if (pixel_y0 >= (tft.height() - FONT_HEIGHT)) {
    pixel_y0 = tft.height() - FONT_HEIGHT;
  }
  return pixel_y0;
}

void setTextCursor(uint16_t x0, uint16_t y0) {
  uint16_t pixel_x0;
  uint16_t pixel_y0;

  pixel_x0 = cursorToX(x0);
  pixel_y0 = cursorToY(y0);
  tft.setCursor(pixel_x0, pixel_y0);
}

void printStatus(const String &mtext, uint16_t row) {
  int16_t save_x, save_y;
  setTextCursor(0, row);
  tft.print(mtext);
  // save where we are...
  save_x = tft.getCursorX();
  save_y = tft.getCursorY();
  tft.print("                             "); // todo... maybe a bit more efficient here :)
  tft.setCursor(save_x, save_y);
}


// snagged from "shieldtest" from adafruit

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpPoop(uint8_t x, uint16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.println(F("Loading image 'poop.bmp'"));

  // Open requested file on SD card
  bmpFile = SD.open("poop.bmp");
  if (!bmpFile) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.startWrite();
        tft.setAddrWindow(x, y, w, h);
        tft.endWrite();

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
        tft.endWrite();
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}


// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}



