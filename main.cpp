#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Output pin to drive HIGH when a client connects (GPIO5 == D1 on many boards)
const uint8_t OUTPUT_PIN = 5; // D1

// Open AP settings
const char* AP_SSID = "ESP8266-Open";

// New: D2 control and web server
const uint8_t PIN_D2 = 4; // D2 (GPIO4)
const uint8_t PIN_D3 = 0; // D3 (GPIO0)
ESP8266WebServer server(80);
bool d2State = false;
bool d3State = false;

void setD2(bool on) { d2State = on; digitalWrite(PIN_D2, on ? HIGH : LOW); }
void setD3(bool on) { d3State = on; digitalWrite(PIN_D3, on ? HIGH : LOW); }

// HTTP handlers (forward declarations)
void handleRoot();
void handleSet();
void handleState();
void handleSetD3();
void handleStateD3();

// Track connected stations and update OUTPUT_PIN immediately on events
volatile uint8_t stationCount = 0;
unsigned long lastResync = 0;
static inline void updateConnPin() {
  digitalWrite(OUTPUT_PIN, stationCount > 0 ? HIGH : LOW);
}
// AP station event callbacks
void onStaConnected(const WiFiEventSoftAPModeStationConnected& /*evt*/) {
  stationCount++;
  updateConnPin();
}
void onStaDisconnected(const WiFiEventSoftAPModeStationDisconnected& /*evt*/) {
  if (stationCount) stationCount--;
  updateConnPin();
}
static inline void resyncStationCount() {
  uint8_t n = WiFi.softAPgetStationNum();
  if (n != stationCount) {
    stationCount = n;
    updateConnPin();
  }
}

void setup() {
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);
  pinMode(PIN_D2, OUTPUT);
  pinMode(PIN_D3, OUTPUT);
  setD2(false);
  setD3(false);

  // Start open Wi-Fi AP (no password)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID); // open network by default when no password is supplied

  // Register AP station connect/disconnect events
  WiFi.onSoftAPModeStationConnected(onStaConnected);
  WiFi.onSoftAPModeStationDisconnected(onStaDisconnected);
  stationCount = WiFi.softAPgetStationNum();
  updateConnPin();

  // Web server routes
  server.on("/", handleRoot);
  server.on("/api/set", HTTP_GET, handleSet);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/setD3", HTTP_GET, handleSetD3);
  server.on("/api/stateD3", HTTP_GET, handleStateD3);
  server.begin();
}

void loop() {
  // Handle web requests
  server.handleClient();

  // Periodic resync in case an event was missed
  const unsigned long now = millis();
  if (now - lastResync >= 1000) {
    lastResync = now;
    resyncStationCount();
  }

  // Remove per-iteration polling and blocking delay; keep WiFi stack responsive
  yield();
}

// -------- Web UI handlers --------
void handleRoot() {
  String html = F(
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>HTBLuVA SmartBox LEDs</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;min-height:100vh;background:#f8fafc;display:flex;align-items:center;justify-content:center;padding:1.5rem}"
    ".container{width:100%;max-width:500px}"
    ".card{background:#ffffff;border-radius:16px;padding:2.5rem;box-shadow:0 1px 3px rgba(0,0,0,0.1),0 1px 2px rgba(0,0,0,0.06);border:1px solid #e2e8f0}"
    ".header{text-align:center;margin-bottom:2.5rem;padding-bottom:2rem;border-bottom:2px solid #f1f5f9}"
    ".logo{display:inline-block;margin-bottom:1rem}"
    "h1{font-size:1.875rem;color:#1e293b;font-weight:700;margin-bottom:0.5rem;letter-spacing:-0.025em}"
    ".subtitle{color:#64748b;font-size:0.875rem;font-weight:600;text-transform:uppercase;letter-spacing:0.1em}"
    ".control-panel{display:grid;gap:1.25rem}"
    ".led-control{background:#ffffff;border-radius:12px;padding:2rem;border:2px solid #e2e8f0;transition:all 0.2s ease}"
    ".led-control:hover{border-color:#cbd5e1;box-shadow:0 4px 6px -1px rgba(0,0,0,0.1),0 2px 4px -1px rgba(0,0,0,0.06)}"
    ".led-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:1.5rem}"
    ".led-title{font-size:0.875rem;font-weight:700;color:#475569;display:flex;align-items:center;gap:0.75rem;text-transform:uppercase;letter-spacing:0.08em}"
    ".led-icon{width:12px;height:12px;border-radius:50%;transition:all 0.3s ease;border:2px solid}"
    ".led-icon.off{background:#e2e8f0;border-color:#cbd5e1}"
    ".led-icon.on-blue{background:#3b82f6;border-color:#2563eb;box-shadow:0 0 0 4px rgba(59,130,246,0.2)}"
    ".led-icon.on-yellow{background:#f59e0b;border-color:#d97706;box-shadow:0 0 0 4px rgba(245,158,11,0.2)}"
    ".status-badge{padding:0.375rem 0.875rem;border-radius:6px;font-size:0.75rem;font-weight:700;letter-spacing:0.05em;text-transform:uppercase;border:2px solid}"
    ".status-on{background:#dcfce7;color:#15803d;border-color:#86efac}"
    ".status-off{background:#fee2e2;color:#991b1b;border-color:#fca5a5}"
    ".toggle-container{display:flex;justify-content:center}"
    ".toggle-switch{position:relative;width:68px;height:34px;cursor:pointer;-webkit-tap-highlight-color:transparent}"
    ".toggle-input{opacity:0;width:0;height:0;position:absolute}"
    ".toggle-slider{position:absolute;top:0;left:0;right:0;bottom:0;background:#cbd5e1;border-radius:34px;transition:all 0.3s ease;border:2px solid #94a3b8}"
    ".toggle-slider:before{content:'';position:absolute;height:26px;width:26px;left:2px;bottom:2px;background:#ffffff;border-radius:50%;transition:all 0.3s ease;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
    ".toggle-input:checked+.toggle-slider{background:#3b82f6;border-color:#2563eb}"
    ".toggle-input.yellow:checked+.toggle-slider{background:#f59e0b;border-color:#d97706}"
    ".toggle-input:checked+.toggle-slider:before{transform:translateX(34px)}"
    ".toggle-switch:active .toggle-slider:before{width:32px}"
    ".footer{text-align:center;margin-top:2rem;padding-top:2rem;border-top:2px solid #f1f5f9}"
    ".footer-info{display:inline-flex;align-items:center;gap:0.625rem;color:#64748b;font-size:0.8125rem;background:#f8fafc;padding:0.625rem 1.125rem;border-radius:8px;border:1px solid #e2e8f0;font-weight:600}"
    ".wifi-icon{width:18px;height:18px;fill:#64748b}"
    "@media(max-width:480px){.card{padding:2rem}h1{font-size:1.5rem}.led-control{padding:1.5rem}}"
    "</style></head><body>"
    "<div class='container'><div class='card'>"
    "<div class='header'>"
    "<div class='logo'>"
    "<svg width='120' height='68' viewBox='0 -127.316 932.138 535.961'>"
    "<g><g>"
    "<path d='M416.947,118.676h14.975v37.952h35.499v-37.952h14.975v90.362h-14.975v-38.469h-35.499v38.469h-14.975V118.676z'/>"
    "<path d='M519.96,132.617h-23.754v-13.941h62.48v13.941h-23.754v76.421H519.96V132.617z'/>"
    "<path d='M572.755,118.676h32.273c9.811,0,18.072,2.71,23.107,7.745c4,4.002,6.066,9.036,6.066,15.232v0.387c0,11.103-6.455,16.911-13.297,20.268c9.941,3.485,17.17,9.424,17.17,21.558v0.517c0,15.878-12.91,24.656-32.145,24.656h-33.176V118.676z M619.227,144.105c0-7.616-5.551-12.005-15.232-12.005h-16.523v25.043h15.619c9.812,0,16.137-4.389,16.137-12.779V144.105z M605.673,169.795h-18.201v25.818h18.848c10.326,0,16.781-4.519,16.781-12.78v-0.259C623.1,174.7,617.034,169.795,605.673,169.795z'/>"
    "<path d='M653.048,118.676h14.975v76.55h38.727v13.812h-53.701V118.676z'/>"
    "<path d='M718.237,186.447V140.62h14.586v41.696c0,9.553,4.52,14.846,12.135,14.846c7.617,0,12.91-5.68,12.91-15.104V140.62h14.715v68.418h-14.715v-9.939c-3.873,6.066-9.295,11.23-18.59,11.23C725.981,210.329,718.237,201.164,718.237,186.447z'/>"
    "<path d='M781.36,118.676h16.266l22.203,68.288l22.203-68.288h15.748l-31.238,90.879h-13.812L781.36,118.676z'/>"
    "<path d='M884.503,118.159h14.459l33.176,90.879h-15.621l-7.486-21.3h-35.113l-7.486,21.3h-15.104L884.503,118.159z M904.384,174.442l-12.91-36.274l-12.908,36.274H904.384z'/>"
    "</g><g>"
    "<path d='M413.905,275.385l4.587-5.702c3.407,3.081,7.209,5.44,12.059,5.44c4.392,0,7.209-2.426,7.209-5.833v-0.065c0-2.884-1.507-4.784-8.782-7.472c-8.716-3.276-13.238-6.488-13.238-13.828v-0.131c0-7.537,6.029-12.846,14.549-12.846c5.309,0,10.225,1.704,14.418,5.374l-4.324,5.833c-3.146-2.556-6.619-4.325-10.289-4.325c-4.129,0-6.686,2.294-6.686,5.309v0.065c0,3.211,1.704,4.915,9.503,7.93c8.521,3.277,12.452,6.816,12.452,13.501v0.131c0,8.061-6.291,13.238-14.877,13.238C424.587,282.004,418.885,279.973,413.905,275.385z'/>"
    "<path d='M451.782,271.452v-0.327c0-7.472,4.98-11.142,12.322-11.142c3.275,0,5.635,0.59,7.994,1.377v-1.442c0-4.653-2.752-7.078-7.471-7.078c-3.342,0-6.096,1.049-8.389,2.098l-2.098-5.898c3.342-1.639,6.816-2.753,11.469-2.753c4.523,0,7.996,1.245,10.225,3.539c2.293,2.294,3.473,5.702,3.473,10.027v21.562h-7.273v-4.26c-2.033,2.949-5.113,4.915-9.635,4.915C456.632,282.069,451.782,278.203,451.782,271.452z M472.165,269.749v-3.474c-1.705-0.721-3.934-1.245-6.424-1.245c-4.26,0-6.75,2.162-6.75,5.832v0.132c0,3.473,2.426,5.439,5.637,5.505C468.887,276.564,472.165,273.681,472.165,269.749z'/>"
    "<path d='M488.876,234.097h7.404v47.317h-7.404V234.097z'/>"
    "<path d='M504.733,276.302l16.908-23.462h-16.318v-6.16h25.363v5.112l-16.908,23.462h16.842v6.16h-25.887V276.302z'/>"
    "<path d='M546.02,276.171v5.243h-7.404v-47.317h7.404v18.153c2.164-3.342,5.178-6.226,10.158-6.226c6.947,0,13.502,5.833,13.502,17.563v0.918c0,11.665-6.488,17.563-13.502,17.563C551.134,282.069,548.118,279.251,546.02,276.171z M562.208,264.375v-0.59c0-6.947-3.604-11.207-8.061-11.207s-8.258,4.325-8.258,11.141v0.656c0,6.815,3.801,11.141,8.258,11.141C558.669,275.516,562.208,271.321,562.208,264.375z'/>"
    "<path d='M577.477,269.945V246.68h7.406v21.168c0,4.85,2.293,7.537,6.16,7.537s6.553-2.884,6.553-7.668V246.68h7.471v34.734h-7.471v-5.046c-1.965,3.08-4.719,5.701-9.438,5.701C581.409,282.069,577.477,277.416,577.477,269.945z'/>"
    "<path d='M614.635,246.68h7.404v6.815c1.836-4.521,5.178-7.602,10.422-7.34v8.062h-0.328c-6.029,0-10.094,3.932-10.094,11.861v15.336h-7.404V246.68z'/>"
    "<path d='M638.489,288.164l2.426-5.832c3.014,1.835,6.684,2.948,10.42,2.948c6.096,0,9.438-3.015,9.438-9.175v-3.342c-2.426,3.407-5.439,6.095-10.486,6.095c-6.947,0-13.238-5.571-13.238-16.253v-0.263c0-10.748,6.357-16.318,13.238-16.318c5.111,0,8.191,2.753,10.42,5.833v-5.178h7.406v29.099c0,5.177-1.377,8.979-3.932,11.534c-2.754,2.818-7.014,4.129-12.518,4.129C646.878,291.441,642.487,290.327,638.489,288.164z M660.837,262.539v-0.131c0-6.095-3.865-9.896-8.322-9.896c-4.521,0-7.996,3.735-7.996,9.896v0.065c0,6.161,3.539,9.962,7.996,9.962S660.837,268.569,660.837,262.539z'/>"
    "</g><g>"
    "<rect x='72.553' fill='#FCBF00' width='63.657' height='63.658'/>"
    "<rect x='72.553' y='72.554' fill='#E68B00' width='63.657' height='63.66'/>"
    "<rect x='0.001' y='145.111' fill='#DB3F29' width='63.655' height='63.66'/>"
    "<rect x='72.553' y='217.668' fill='#9560A4' width='63.661' height='63.66'/>"
    "<rect x='145.11' y='145.111' fill='#D24592' width='63.659' height='63.66'/>"
    "<rect x='217.668' y='217.672' fill='#4F8FCC' width='63.657' height='63.658'/>"
    "<rect x='290.228' y='145.111' fill='#009D60' width='63.655' height='63.66'/>"
    "<rect x='217.668' y='72.554' fill='#AEBE38' width='63.657' height='63.66'/>"
    "<rect x='0.001' fill='#FCBF00' width='63.655' height='63.658'/>"
    "</g></g></svg>"
    "</div>"
    "<h1>SmartBox LEDs</h1>"
    "<div class='subtitle'>HTBLuVA Salzburg</div>"
    "</div>"
    "<div class='control-panel'>"
    "<div class='led-control'>"
    "<div class='led-header'>"
    "<div class='led-title'><span class='led-icon ");
  html += d2State ? "on-blue" : "off";
  html += F("' id='led2'></span>LED D2</div>"
    "<span class='status-badge ");
  html += d2State ? "status-on" : "status-off";
  html += F("' id='status'>");
  html += d2State ? "ON" : "OFF";
  html += F("</span></div>"
    "<div class='toggle-container'>"
    "<label class='toggle-switch'>"
    "<input type='checkbox' class='toggle-input' id='toggle' ");
  html += d2State ? "checked" : "";
  html += F(" onclick='toggle()'>"
    "<span class='toggle-slider'></span>"
    "</label></div></div>"
    "<div class='led-control'>"
    "<div class='led-header'>"
    "<div class='led-title'><span class='led-icon ");
  html += d3State ? "on-yellow" : "off";
  html += F("' id='led3'></span>LED D3</div>"
    "<span class='status-badge ");
  html += d3State ? "status-on" : "status-off";
  html += F("' id='statusD3'>");
  html += d3State ? "ON" : "OFF";
  html += F("</span></div>"
    "<div class='toggle-container'>"
    "<label class='toggle-switch'>"
    "<input type='checkbox' class='toggle-input yellow' id='toggleD3' ");
  html += d3State ? "checked" : "";
  html += F(" onclick='toggleD3()'>"
    "<span class='toggle-slider'></span>"
    "</label></div></div></div>"
    "<div class='footer'>"
    "<div class='footer-info'>"
    "<svg class='wifi-icon' viewBox='0 0 24 24' fill='#718096'>"
    "<path d='M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z'/>"
    "</svg>");
  html += AP_SSID;
  html += F("</div></div></div></div>"
    "<script>"
    "function updateUI(isD2,state){"
    "const statusEl=document.getElementById(isD2?'status':'statusD3');"
    "const ledEl=document.getElementById(isD2?'led2':'led3');"
    "const toggleEl=document.getElementById(isD2?'toggle':'toggleD3');"
    "statusEl.textContent=state?'ON':'OFF';"
    "statusEl.className='status-badge '+(state?'status-on':'status-off');"
    "ledEl.className='led-icon '+(state?(isD2?'on-blue':'on-yellow'):'off');"
    "toggleEl.checked=state;}"
    "async function refresh(){"
    "try{const r=await fetch('/api/state');const j=await r.json();updateUI(true,j.on);}catch(e){}}"
    "async function refreshD3(){"
    "try{const r=await fetch('/api/stateD3');const j=await r.json();updateUI(false,j.on);}catch(e){}}"
    "async function toggle(){"
    "const want=!document.getElementById('toggle').checked;"
    "document.getElementById('toggle').checked=!want;"
    "try{const r=await fetch('/api/set?on='+(want?0:1));const j=await r.json();updateUI(true,j.on);}catch(e){refresh();}}"
    "async function toggleD3(){"
    "const want=!document.getElementById('toggleD3').checked;"
    "document.getElementById('toggleD3').checked=!want;"
    "try{const r=await fetch('/api/setD3?on='+(want?0:1));const j=await r.json();updateUI(false,j.on);}catch(e){refreshD3();}}"
    "setInterval(()=>{refresh();refreshD3();},3000);"
    "</script></body></html>"
  );
  server.send(200, "text/html", html);
}

void handleSet() {
  if (!server.hasArg("on")) {
    server.send(400, "text/plain", "Missing 'on' query param");
    return;
  }
  const String v = server.arg("on");
  const bool on = (v == "1" || v == "true" || v == "on");
  setD2(on);
  // Add no-store to avoid any caching delays
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "application/json", String("{\"on\":") + (d2State ? "true" : "false") + "}");
}

void handleState() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "application/json", String("{\"on\":") + (d2State ? "true" : "false") + "}");
}

void handleSetD3() {
  if (!server.hasArg("on")) {
    server.send(400, "text/plain", "Missing 'on' query param");
    return;
  }
  const String v = server.arg("on");
  const bool on = (v == "1" || v == "true" || v == "on");
  setD3(on);
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "application/json", String("{\"on\":") + (d3State ? "true" : "false") + "}");
}

void handleStateD3() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "application/json", String("{\"on\":") + (d3State ? "true" : "false") + "}");
}