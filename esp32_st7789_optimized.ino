#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>  // or Adafruit_ST7735.h
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include "image_data.h" // Your owned bitmap

#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }

// Pin definitions for ST7789
#define TFT_CS     5  // Chip_Select pin
#define TFT_RST   17  // Reset pin
#define TFT_DC    16  // Data / Command pin
#define TFT_MOSI  19  // MOSI (Master Out Slave In) pin
#define TFT_SCLK  18  // SCLK (SPI Clock) pin
#define TFT_BLK   -1  // Back Light pin (-1 for not use)

// Setup display size
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Initialize the ST7789 display
SPIClass spi = SPIClass(VSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&spi, TFT_CS, TFT_DC, /*TFT_MOSI, TFT_SCLK,*/ TFT_RST);   // Do not specified MOSI and SCLK here, due to library will run as software SPI
// Adafruit_ST7735 tft = Adafruit_ST7735(&spi, TFT_CS, TFT_DC, /*TFT_MOSI, TFT_SCLK,*/ TFT_RST);  // For ST7735

// Wi-Fi credentials (notice: esp32 only supports 802.11a/b/g/n at 2.4GHz)
const char* ssid = "your_ssid";
const char* password = "your_password";

// NTP Client settings
const long utcOffsetInSeconds = 28800;  // Adjust this according to your time zone, 28800s for UTC+8. (Asia/Taipei)

// Setup client for getting time from ntp server
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Function to draw the clock face
void drawClockFace(int centerX, int centerY, int radius) {
  // Draw the clock face
  tft.drawCircle(centerX, centerY, radius, ST77XX_WHITE);
  
  // Draw the hour markers
  for (int i = 0; i < 12; i++) {
    float angle = (i * 30 - 90) * DEG_TO_RAD;
    int x1 = centerX + cos(angle) * (radius - 10);
    int y1 = centerY + sin(angle) * (radius - 10);
    int x2 = centerX + cos(angle) * radius;
    int y2 = centerY + sin(angle) * radius;
    tft.drawLine(x1, y1, x2, y2, ST77XX_WHITE);
  }
}

// Function to restore previous clock hand with image data; Reference function 'Adafruit_GFX::writeLine()'
void restorePrevHand(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {

  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx = x1 - x0;
  int16_t dy = abs(y1 - y0);
  int16_t err = dx / 2;
  int16_t ystep = (y0 < y1) ? 1 : -1;
  int16_t _x0 = x0, _y0 = y0;

  tft.startWrite();
  for (; _x0 <= x1; _x0++) {
    if (steep)
      tft.writePixel(_y0, _x0, image_data[TFT_WIDTH * (_x0 - 1) + _y0]);
    else
      tft.writePixel(_x0, _y0, image_data[TFT_WIDTH * (_y0 - 1) + _x0]);
    err -= dy;
    if (err < 0) {
      _y0 += ystep;
      err += dx;
    }
  }
  tft.endWrite();
}

// The parameters for clock hands
int hrX, hrY, minX, minY, secX, secY;
const int halfWidth = TFT_WIDTH / 2;
const int halfHeight = TFT_HEIGHT / 2;
const int center = (halfWidth <= halfHeight) ? halfWidth : halfHeight;

// default display direction, you can modify these value from 0 to 3
int rotation = 2;

void setup() {

  // Specfied MOSI and SCLK pins
  spi.begin(TFT_SCLK, -1, TFT_MOSI, -1);

  // Initialize boot button as input
  pinMode(0, INPUT);

  // Initialize back light control if specified at defination
  if (TFT_BLK != -1) {
    pinMode(TFT_BLK, OUTPUT);
    digitalWrite(TFT_BLK, 1);
  }
    
  // Initialize the display (ST7789)
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  
  // Initialize the display (ST7735)
  /*
  tft.initR(INITR_BLACKTAB);
  tft.setAddrWWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
  */

  // Default display direction
  tft.setRotation(rotation);

  // Fill display with color black
  tft.fillScreen(ST77XX_BLACK);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  tft.setTextSize(2);               // You may have to reconfig textsize if the display size not match 240x240
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.println("Connecting to WiFi:");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(0, 20);
  tft.println(ssid);
  delay(2000);
  
  // Waiting for Wi-Fi connected
  while (WiFi.status() != WL_CONNECTED) delay(500);

  // Show IP address if Wi-Fi connected
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.println("WiFi connected.");
  tft.setCursor(0, 20);
  tft.println("IP:");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(40, 20);
  tft.println(WiFi.localIP());
  delay(2000);

  // Fill display with custom bitmap
  tft.drawRGBBitmap(0, 0, image_data, TFT_WIDTH, TFT_HEIGHT);

  // Initialize the NTPClient to get time
  timeClient.begin();

  // Initialize hour, minute and second hand
  hrX = minX = secX = halfWidth;
  hrY = minY = secY = halfHeight;

  // Draw the static parts of the clock face
  drawClockFace(halfWidth, halfHeight, center - 20);
}

void loop() {
  // Update the NTP client to get the latest time
  timeClient.update();

  // Get the current time
  unsigned long epochTime = timeClient.getEpochTime();

  // Convert the epoch time to a time structure
  setTime(epochTime);

  // Get the current hour, minute, and second
  int hr = hour();
  int min = minute();
  int sec = second();

  // Clear the previous clock hands by restoring the background pixels
  restorePrevHand(halfWidth, halfHeight, hrX, hrY);
  restorePrevHand(halfWidth, halfHeight, minX, minY);
  restorePrevHand(halfWidth, halfHeight, secX, secY);

  // Setup display direction by pressing boot button (gpio 0)
  if (!digitalRead(0)) {
    tft.setRotation(rotation = (rotation == 3) ? 0 : rotation + 1);
    tft.drawRGBBitmap(0, 0, image_data, TFT_WIDTH, TFT_HEIGHT);
    drawClockFace(halfWidth, halfHeight, center - 20);
  }
    
  // Draw the hour hand
  float hrAngle = (((hr % 12) + min / 60.0) * 30 - 90) * DEG_TO_RAD;
  hrX = halfWidth + cos(hrAngle) * (center - 50);
  hrY = halfHeight + sin(hrAngle) * (center - 50);
  tft.drawLine(halfWidth, halfHeight, hrX, hrY, ST77XX_WHITE);

  // Draw the minute hand
  float minAngle = ((min + sec / 60.0) * 6 - 90) * DEG_TO_RAD;
  minX = halfWidth + cos(minAngle) * (center - 40);
  minY = halfHeight + sin(minAngle) * (center - 40);
  tft.drawLine(halfWidth, halfHeight, minX, minY, ST77XX_WHITE);

  // Draw the second hand
  float secAngle = (sec * 6 - 90) * DEG_TO_RAD;
  secX = halfWidth + cos(secAngle) * (center - 30);
  secY = halfHeight + sin(secAngle) * (center - 30);
  tft.drawLine(halfWidth, halfHeight, secX, secY, ST77XX_RED);

  delay(250);
}