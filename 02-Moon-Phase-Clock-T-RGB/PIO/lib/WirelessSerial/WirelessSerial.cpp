#include "WirelessSerial.h"

static const char* index_html = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <title>T-RGB Monitor 📟</title>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
  <style>
    :root {
      --bg: #1e1e2e; --surface: #11111b; --header: #181825;
      --text: #cdd6f4; --subtext: #a6adc8;
      --blue: #89b4fa; --lavender: #b4befe; --yellow: #f9e2af;
      --peach: #fab387; --red: #f38ba8; --green: #a6e3a1;
      --mauve: #cba6f7; --sky: #89dceb;
    }
    body { background: var(--bg); color: var(--text); font-family: -apple-system, system-ui, sans-serif; margin: 0; padding: 0; height: 100vh; display: flex; flex-direction: column; overflow: hidden; }
    header { background: var(--header); padding: 12px 16px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #313244; box-shadow: 0 2px 10px rgba(0,0,0,0.3); z-index: 10; }
    .title { font-weight: 600; font-size: 1.1rem; display: flex; align-items: center; gap: 10px; }
    .status { font-size: 0.8rem; padding: 4px 8px; border-radius: 20px; background: #313244; color: var(--subtext); }
    .status.online { color: var(--green); background: rgba(166, 227, 161, 0.1); }
    main { flex: 1; padding: 12px; display: flex; flex-direction: column; position: relative; overflow: hidden; }
    #console-container { flex: 1; background: var(--surface); border-radius: 12px; border: 1px solid #313244; position: relative; display: flex; flex-direction: column; overflow: hidden; box-shadow: inset 0 2px 15px rgba(0,0,0,0.5); }
    #console { flex: 1; overflow-y: auto; padding: 15px; font-family: 'JetBrains Mono', monospace; font-size: 13px; line-height: 1.5; white-space: pre-wrap; word-break: break-all; -webkit-overflow-scrolling: touch; }
    .controls { position: absolute; top: 12px; right: 12px; display: flex; gap: 8px; z-index: 5; }
    .btn { background: rgba(49, 50, 68, 0.8); color: var(--text); border: 1px solid #45475a; width: 36px; height: 36px; border-radius: 10px; display: flex; align-items: center; justify-content: center; cursor: pointer; backdrop-filter: blur(4px); transition: all 0.2s; -webkit-tap-highlight-color: transparent; }
    .btn:active { transform: scale(0.9); background: var(--blue); color: var(--surface); }
    .btn.paused { color: var(--red); border-color: var(--red); }
    .msg { margin: 1px 0; border-left: 2px solid transparent; padding-left: 8px; }
    .tag-batt { color: var(--peach); font-weight: bold; }
    .tag-moon { color: var(--mauve); }
    .tag-wifi, .tag-boot { color: var(--sky); font-weight: bold; }
    .tag-ntp { color: var(--yellow); }
    .tag-error { color: var(--red); background: rgba(243, 139, 168, 0.1); border-left-color: var(--red); }
    .tag-success { color: var(--green); }
    .info { color: var(--blue); font-style: italic; }
    #console::-webkit-scrollbar { width: 4px; }
    #console::-webkit-scrollbar-thumb { background: #45475a; border-radius: 10px; }
    @media (max-width: 600px) {
      header { padding: 10px; }
      #console { font-size: 12px; padding: 10px; }
      .btn { width: 32px; height: 32px; }
    }
  </style>
</head>
<body>
  <header>
    <div class="title"><i class="fas fa-terminal" style="color:var(--blue)"></i> T-RGB Monitor</div>
    <div id="status-badge" class="status">Connecting...</div>
  </header>
  <main>
    <div id="console-container">
      <div class="controls">
        <div id="pause-btn" class="btn" title="Autoscroll Pause"><i class="fas fa-pause"></i></div>
        <div id="clear-btn" class="btn" title="Clear Console"><i class="fas fa-trash-can"></i></div>
      </div>
      <div id="console"></div>
    </div>
  </main>
  <script>
    var gateway = `ws://${window.location.hostname}:81/`, websocket, autoscroll = true, maxLines = 500;
    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = (e) => { 
          document.getElementById('status-badge').innerText = 'Online';
          document.getElementById('status-badge').className = 'status online';
          appendLog("System connected", "info"); 
      };
      websocket.onclose = (e) => { 
          document.getElementById('status-badge').innerText = 'Offline';
          document.getElementById('status-badge').className = 'status';
          setTimeout(initWebSocket, 2000); 
      };
      websocket.onmessage = (e) => { appendLog(e.data); };
    }
    function appendLog(msg, forceCl = "") {
      const c = document.getElementById('console'), d = document.createElement('div');
      d.className = "msg " + forceCl;
      let h = msg;
      if (!forceCl) {
        h = h.replace(/\[BATT\]/g, '<span class="tag-batt">[BATT]</span>');
        h = h.replace(/\[Moon\]/g, '<span class="tag-moon">[Moon]</span>');
        h = h.replace(/\[WiFi\]/g, '<span class="tag-wifi">[WiFi]</span>');
        h = h.replace(/\[BOOT\]/g, '<span class="tag-boot">[BOOT]</span>');
        h = h.replace(/\[NTP\]/g, '<span class="tag-ntp">[NTP]</span>');
        if (msg.toLowerCase().includes("error") || msg.toLowerCase().includes("fail")) d.classList.add("tag-error");
        else if (msg.toLowerCase().includes("succ") || msg.toLowerCase().includes("erfolg")) d.classList.add("tag-success");
      }
      d.innerHTML = h;
      c.appendChild(d);
      if (autoscroll) c.scrollTop = c.scrollHeight;
      if (c.childNodes.length > maxLines) c.removeChild(c.firstChild);
    }
    document.getElementById('clear-btn').onclick = () => { document.getElementById('console').innerHTML = ''; };
    document.getElementById('pause-btn').onclick = function() {
      autoscroll = !autoscroll;
      this.classList.toggle('paused');
      this.innerHTML = autoscroll ? '<i class="fas fa-pause"></i>' : '<i class="fas fa-play"></i>';
    };
    window.onload = initWebSocket;
  </script>
</body>
</html>
)rawliteral";

WirelessSerialClass::WirelessSerialClass() : server(80), webSocket(81) {
    wsMutex = xSemaphoreCreateRecursiveMutex();
}

void WirelessSerialClass::begin(const char* mdnsName) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
        if (type == WStype_CONNECTED) {
            if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
                this->webSocket.sendTXT(num, "### BOOT REPLAY START ###");
                for (const String &log : this->logHistory) {
                    this->webSocket.sendTXT(num, log.c_str());
                }
                this->webSocket.sendTXT(num, "### END REPLAY / LIVE START ###");
                xSemaphoreGiveRecursive(this->wsMutex);
            }
        }
    });

    if (MDNS.begin(mdnsName)) {
        MDNS.addService("http", "tcp", 80);
    }
}

void WirelessSerialClass::addToHistory(const String &s) {
    this->logHistory.push_back(s);
    if (this->logHistory.size() > MAX_HISTORY) {
        this->logHistory.erase(this->logHistory.begin());
    }
}

void WirelessSerialClass::update() {
    if (xSemaphoreTakeRecursive(wsMutex, 10)) {
        webSocket.loop();
        xSemaphoreGiveRecursive(wsMutex);
    }
}

size_t WirelessSerialClass::printf(const char * format, ...) {
    char loc_buf[256];
    va_list arg;
    va_start(arg, format);
    int len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(arg);
    
    Serial.print(loc_buf);
    if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
        this->addToHistory(String(loc_buf));
        this->webSocket.broadcastTXT(loc_buf);
        xSemaphoreGiveRecursive(this->wsMutex);
    }
    return len;
}

size_t WirelessSerialClass::println(const String &s) {
    Serial.println(s);
    if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
        this->addToHistory(s);
        this->webSocket.broadcastTXT(s.c_str());
        xSemaphoreGiveRecursive(this->wsMutex);
    }
    return s.length();
}

size_t WirelessSerialClass::println(const char *s) {
    Serial.println(s);
    if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
        this->addToHistory(String(s));
        this->webSocket.broadcastTXT(s);
        xSemaphoreGiveRecursive(this->wsMutex);
    }
    return strlen(s);
}

size_t WirelessSerialClass::println() {
    Serial.println();
    if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
        this->addToHistory(String(""));
        this->webSocket.broadcastTXT("");
        xSemaphoreGiveRecursive(this->wsMutex);
    }
    return 1;
}

size_t WirelessSerialClass::print(const String &s) {
    Serial.print(s);
    if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
        if (!this->logHistory.empty()) {
            this->logHistory.back() += s;
        } else {
            this->addToHistory(s);
        }
        this->webSocket.broadcastTXT(s.c_str());
        xSemaphoreGiveRecursive(this->wsMutex);
    }
    return s.length();
}

size_t WirelessSerialClass::print(const char *s) {
    Serial.print(s);
    if (xSemaphoreTakeRecursive(this->wsMutex, portMAX_DELAY)) {
        if (!this->logHistory.empty()) {
            this->logHistory.back() += String(s);
        } else {
            this->addToHistory(String(s));
        }
        this->webSocket.broadcastTXT(s);
        xSemaphoreGiveRecursive(this->wsMutex);
    }
    return strlen(s);
}

WirelessSerialClass WirelessSerial;
