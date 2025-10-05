#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include "epd7in3f.h"
#include "config.h"

Epd epd;
WebServer server(80);
UBYTE imageBuffer[EPD_WIDTH * EPD_HEIGHT / 2];

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

  Serial.printf("Buffer size: %d bytes\n", sizeof(imageBuffer));
  Serial.printf("Expected: %d bytes\n", EPD_WIDTH * EPD_HEIGHT / 2);
  Serial.printf("Width: %d, Height: %d\n", EPD_WIDTH, EPD_HEIGHT);

  epd.Clear(EPD_7IN3F_WHITE);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("IP: " + WiFi.localIP().toString());
  
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<body>
  <h1>E-Paper Display</h1>
  <button onclick="location.href='/white'">White</button>
  <button onclick="location.href='/black'">Black</button>
  <button onclick="location.href='/colors'">7 Colors</button>
  <button onclick="location.href='/colortest'">Color Test</button>
  <button onclick="location.href='/scancolors'">Scan All Colors</button>
  <br><br>
  <input type="file" id="imageFile" accept=".bin">
  <button onclick="uploadRaw()">Upload RAW Image</button>
  <p id="status"></p>
  
  <script>
    async function uploadRaw() {
      const file = document.getElementById('imageFile').files[0];
      if (!file) { alert('Select a .bin file'); return; }
      
      document.getElementById('status').textContent = 'Uploading...';
      
      const data = await file.arrayBuffer();
      const response = await fetch('/display', {
        method: 'POST',
        body: data
      });
      document.getElementById('status').textContent = await response.text();
    }
  </script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
  });
  
  server.on("/white", []() { epd.Clear(EPD_7IN3F_WHITE); server.send(200, "text/plain", "OK"); });
  server.on("/black", []() { epd.Clear(EPD_7IN3F_BLACK); server.send(200, "text/plain", "OK"); });
  server.on("/colors", []() { epd.EPD_7IN3F_Show7Block(); server.send(200, "text/plain", "OK"); });
  server.on("/colortest", []() {
  Serial.println("Testing horizontal stripes...");
  
  int stripeHeight = EPD_HEIGHT / 7;  // ~68 pixels per stripe
  
  for(int i = 0; i < sizeof(imageBuffer); i++) {
    int byteRow = i / (EPD_WIDTH / 2);  // Which row this byte is on
    int stripe = byteRow / stripeHeight;
    if(stripe > 6) stripe = 6;
    
    imageBuffer[i] = (stripe << 4) | stripe;
  }
  
  epd.EPD_7IN3F_Display(imageBuffer);
  server.send(200, "text/plain", "Horizontal test done");
});


  server.on("/display", HTTP_POST, 
  []() {
    // Send response IMMEDIATELY, before display updates
    server.send(200, "text/plain", "Received");
  }, 
  []() {
    HTTPUpload& upload = server.upload();
    static size_t bytesReceived = 0;
    
    if (upload.status == UPLOAD_FILE_START) {
      bytesReceived = 0;
    } 
    else if (upload.status == UPLOAD_FILE_WRITE) {
      memcpy(imageBuffer + bytesReceived, upload.buf, upload.currentSize);
      bytesReceived += upload.currentSize;
    } 
    else if (upload.status == UPLOAD_FILE_END) {
      Serial.printf("Got %d bytes, updating display...\n", bytesReceived);
      if (bytesReceived == 192000) {
        epd.EPD_7IN3F_Display(imageBuffer);
        Serial.println("Display updated!");
      }
    }
  }
);
  
  server.on("/testcolor", []() {
  if (server.hasArg("code")) {
    int code = server.arg("code").toInt();
    Serial.printf("Testing color code: 0x%X\n", code);
    epd.Clear(code);
    server.send(200, "text/plain", "Showing code " + String(code));
  } else {
    server.send(400, "text/plain", "Add ?code=0 through ?code=6");
  }
});

server.on("/scancolors", []() {
  Serial.println("Scanning color codes 0-15...");
  
  for(int code = 0; code <= 15; code++) {
    Serial.printf("Testing code %d\n", code);
    epd.Clear(code);
    delay(3000);  // 3 seconds per color
  }
  
  server.send(200, "text/plain", "Scan complete");
});

  server.begin();
  Serial.println("Ready!");
}

void loop() {
  server.handleClient();
}
