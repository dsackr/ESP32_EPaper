#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include "epd7in3f.h"
#include "fonts.h"
#include "config.h"

Epd epd;
WebServer server(80);
UBYTE imageBuffer[EPD_WIDTH * EPD_HEIGHT / 2];

bool displayUpdatePending = false;

// Draw a character at x,y position
void drawChar(int x, int y, char c, sFONT* font, int color) {
  if (c < ' ' || c > '~') return;
  
  const unsigned char* charData = &font->table[(c - ' ') * font->Height * ((font->Width + 7) / 8)];
  
  for (int row = 0; row < font->Height; row++) {
    for (int col = 0; col < font->Width; col++) {
      int byteIndex = col / 8;
      int bitIndex = 7 - (col % 8);
      
      if (charData[row * ((font->Width + 7) / 8) + byteIndex] & (1 << bitIndex)) {
        int px = x + col;
        int py = y + row;
        
        if (px >= 0 && px < 800 && py >= 0 && py < 480) {
          int bufferIndex = py * 400 + px / 2;
          if (px % 2 == 0) {
            imageBuffer[bufferIndex] = (imageBuffer[bufferIndex] & 0x0F) | (color << 4);
          } else {
            imageBuffer[bufferIndex] = (imageBuffer[bufferIndex] & 0xF0) | color;
          }
        }
      }
    }
  }
}

void drawString(int x, int y, const char* str, sFONT* font, int color) {
  int cursorX = x;
  while (*str) {
    drawChar(cursorX, y, *str, font, color);
    cursorX += font->Width;
    str++;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32-C6 E-Paper Display Starting...");
  
  SPI.begin(19, -1, 21, -1);
  
  if (epd.Init() != 0) {
    Serial.println("Init failed!");
    while(1);
  }
  
  Serial.println("Display initialized!");
  
  epd.Clear(EPD_7IN3F_WHITE);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  String ipAddress = WiFi.localIP().toString();
  Serial.println("IP: " + ipAddress);
  
  // Clear buffer to white
  for(int i = 0; i < sizeof(imageBuffer); i++) {
    imageBuffer[i] = 0x11;
  }
  
  // Draw IP address info on display
  drawString(50, 180, "E-Paper Display Ready", &Font24, EPD_7IN3F_BLACK);
  drawString(50, 230, "Connect to:", &Font20, EPD_7IN3F_BLACK);
  drawString(50, 270, ipAddress.c_str(), &Font24, EPD_7IN3F_BLUE);
  
  epd.EPD_7IN3F_Display(imageBuffer);
  Serial.println("IP displayed on screen");
  
  // Get Pi Zero IP (assumes it's on same subnet, adjust if different)
  String piIP = WiFi.localIP().toString();
  piIP = piIP.substring(0, piIP.lastIndexOf('.') + 1) + "XXX";  // You'll need to set this manually
  
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<body>
  <h1>E-Paper Display Control</h1>
  <p>ESP32 IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</p>
  <button onclick="location.href='/white'">Clear White</button>
  <button onclick="location.href='/black'">Clear Black</button>
  <button onclick="location.href='/colors'">Show Color Test</button>
  <br><br>
  <h3>Upload Images</h3>
  <p><a href="http://YOUR_PI_IP:5002" target="_blank">Go to Image Upload Page (Pi Zero)</a></p>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
  });
  
  server.on("/white", []() { 
    epd.Clear(EPD_7IN3F_WHITE); 
    server.send(200, "text/plain", "OK"); 
  });
  
  server.on("/black", []() { 
    epd.Clear(EPD_7IN3F_BLACK); 
    server.send(200, "text/plain", "OK"); 
  });
  
  server.on("/colors", []() { 
    epd.EPD_7IN3F_Show7Block(); 
    server.send(200, "text/plain", "OK"); 
  });
  
  server.on("/display", HTTP_POST,
    []() {
      server.send(200, "text/plain", "OK");
    },
    []() {
      HTTPUpload& upload = server.upload();
      static size_t bytesReceived = 0;
      
      if (upload.status == UPLOAD_FILE_START) {
        bytesReceived = 0;
        Serial.println("=== Upload started ===");
      }
      else if (upload.status == UPLOAD_FILE_WRITE) {
        size_t remaining = sizeof(imageBuffer) - bytesReceived;
        size_t toWrite = min(upload.currentSize, remaining);
        
        memcpy(imageBuffer + bytesReceived, upload.buf, toWrite);
        bytesReceived += toWrite;
        
        if (bytesReceived % 20000 == 0) {
          Serial.printf("Progress: %d bytes\n", bytesReceived);
        }
      }
      else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("=== Upload complete: %d bytes ===\n", bytesReceived);
        
        if (bytesReceived == 192000) {
          displayUpdatePending = true;
        } else {
          Serial.printf("ERROR: Expected 192000, got %d\n", bytesReceived);
        }
      }
    }
  );
  
  server.begin();
  Serial.println("Ready!");
}

void loop() {
  server.handleClient();
  
  if (displayUpdatePending) {
    displayUpdatePending = false;
    Serial.println("Updating display...");
    epd.EPD_7IN3F_Display(imageBuffer);
    Serial.println("Done!");
  }
}