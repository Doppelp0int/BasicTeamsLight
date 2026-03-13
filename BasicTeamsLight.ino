#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <vector>

#define LED_PIN 5
#define NUM_LEDS 12

CRGB leds[NUM_LEDS];
CRGB currentColor = CRGB::Black;

Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

String wifiSSID = "";
String wifiPASS = "";
bool fallbackActive = false;
int ledBrightness = 128; // default 50%
String currentStatus = "offline";

// ---------- LED functions ----------
void showColor(CRGB color) {
    FastLED.setBrightness(ledBrightness);
    for(int i=0; i<NUM_LEDS; i++) leds[i] = color;
    FastLED.show();
    currentColor = color;
}

CRGB getStatusColor(String s) {
    if(s=="av") return CRGB::Green;
    if(s=="in_call") return CRGB::Red;
    if(s=="away") return CRGB(255, 255, 0);
    if(s=="offline") return CRGB::Black;
    return CRGB::Black;
}

// ---------- Non-blocking LED animation ----------
unsigned long lastLedUpdate = 0;
int ledPos = 0;
int pulseBrightness = 0;
int pulseDelta = 5;

void ledLoop() {
    unsigned long now = millis();
    if(fallbackActive) {
        if(now - lastLedUpdate > 20) {
            pulseBrightness += pulseDelta;
            if(pulseBrightness >= 255 || pulseBrightness <= 0) pulseDelta = -pulseDelta;
            FastLED.setBrightness(ledBrightness);
            for(int i=0; i<NUM_LEDS; i++)
                leds[i] = CRGB(pulseBrightness, pulseBrightness, pulseBrightness);
            FastLED.show();
            lastLedUpdate = now;
        }
    } else if(WiFi.status() != WL_CONNECTED) {
        if(now - lastLedUpdate > 120) {
            FastLED.setBrightness(ledBrightness);
            for(int i=0; i<NUM_LEDS; i++) leds[i] = CRGB::Black;
            leds[ledPos] = CRGB::Blue;
            FastLED.show();
            ledPos++;
            if(ledPos >= NUM_LEDS) ledPos = 0;
            lastLedUpdate = now;
        }
    }
}

// ---------- Webserver ----------
void handleStatus() {
    String s = server.arg("s");
    currentStatus = s;
    for(int i=0; i<NUM_LEDS; i++) leds[i] = getStatusColor(s);
    showColor(getStatusColor(s));
    server.send(200,"text/plain","ok");
}

void handlePath() {
    String p = server.uri();
    if(p.length() > 1) p = p.substring(1);
    currentStatus = p;
    for(int i=0; i<NUM_LEDS; i++) leds[i] = getStatusColor(p);
    showColor(getStatusColor(p));
    server.send(200,"text/plain","ok");
}

void handleGetStatus() {
    server.send(200,"text/plain",currentStatus);
}

void handleWiFiSave() {
    if(server.hasArg("ssid") && server.hasArg("pass")) {
        preferences.begin("wifi", false);
        preferences.putString("ssid", server.arg("ssid"));
        preferences.putString("pass", server.arg("pass"));
        preferences.end();
        server.send(200,"text/plain","Saved. Restarting...");
        delay(1000);
        ESP.restart();
    }
}

void handleBrightness() {
    if(server.hasArg("b")) {
        int b = server.arg("b").toInt();
        if(b < 0) b = 0;
        if(b > 255) b = 255;
        ledBrightness = b;
        preferences.begin("wifi", false);
        preferences.putInt("brightness", ledBrightness);
        preferences.end();
        showColor(currentColor);
        server.send(200,"text/plain","Brightness set");
    } else {
        server.send(200,"text/plain",String(ledBrightness));
    }
}

// Redirect unknown paths to /
void handleNotFound() {
    server.sendHeader("Location","/");
    server.send(302,"text/plain","");
}

// ---------- WiFi dropdown with signal strength ----------
String getWiFiOptions() {
    String options = "";
    std::vector<String> addedSSIDs;

    if(wifiSSID != "") {
        options += "<option value='" + wifiSSID + "' selected>" + wifiSSID + "</option>\n";
        addedSSIDs.push_back(wifiSSID);
    }

    int n = WiFi.scanNetworks();
    for(int i=0; i<n; i++) {
        String ssid = WiFi.SSID(i);
        bool exists = false;
        for(auto &s : addedSSIDs) if(s == ssid) exists = true;
        if(!exists) {
            addedSSIDs.push_back(ssid);
            int rssi = WiFi.RSSI(i);
            int strengthPercent = map(rssi, -100, -50, 0, 100);
            if(strengthPercent < 0) strengthPercent = 0;
            if(strengthPercent > 100) strengthPercent = 100;
            options += "<option value='" + ssid + "'>" + ssid + " (" + String(strengthPercent) + "%)</option>\n";
        }
    }

    return options;
}

// ---------- Web UI ----------
void handleRoot() {
    String options = getWiFiOptions();

    String page = "<!DOCTYPE html><html><head><title>ESP Teams Status</title><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;background:#111;color:white;text-align:center;padding:30px;}"
                  "button{padding:15px 30px;font-size:18px;margin:10px;border:none;border-radius:10px;cursor:pointer;}"
                  ".av{background:#00c853;}.call{background:#d50000;}.away{background:#ffd600;color:black;}.off{background:#444;}"
                  "input,select{padding:8px;margin:5px;}"
                  "#statusCircle{width:20px;height:20px;border-radius:50%;margin:15px auto;background:#111;}"
                  "</style></head><body>"
                  "<h2>Status</h2>"
                  "<button class='av' onclick=\"set('av'); loadStatus();\">Available</button>"
                  "<button class='call' onclick=\"set('in_call'); loadStatus();\">In Call</button>"
                  "<button class='away' onclick=\"set('away'); loadStatus();\">Away</button>"
                  "<button class='off' onclick=\"set('offline'); loadStatus();\">Offline</button>"
                  "<div id='statusCircle'></div>"
                  "<h2>WiFi</h2>"
                  "SSID:<br><select id='ssid'>" + options + "</select><br>"
                  "Password:<br><input id='pass'><br>"
                  "<button onclick='save()'>Save</button>"
                  "<h2>Brightness</h2>"
                  "<input type='range' id='brightness' min='0' max='255' value='" + String(ledBrightness) + "' oninput='updateBrightness(this.value)'><br>"
                  "<span id='bval'>" + String(ledBrightness*100/255) + " %</span>"
                  "<script>"
                  "function set(s){fetch('/status?s='+s);}"
                  "function save(){let ssid=document.getElementById('ssid').value;"
                  "let pass=document.getElementById('pass').value;"
                  "fetch('/wifi_save?ssid='+ssid+'&pass='+pass).then(r=>r.text()).then(t=>alert(t));}"
                  "function updateBrightness(b){"
                  "document.getElementById('bval').innerText = Math.round(b/255*100)+' %';"
                  "fetch('/brightness?b='+b);}"
                  "function loadStatus(){"
                  "fetch('/get_status').then(r=>r.text()).then(s=>{"
                  "let colorMap={'av':'#00c853','in_call':'#d50000','away':'#ffd600','offline':'#444'};"
                  "document.getElementById('statusCircle').style.background = colorMap[s] || '#111';"
                  "});}"
                  "setInterval(loadStatus,2000);"
                  "window.onload = loadStatus;"
                  "</script></body></html>";

    server.send(200,"text/html",page);
}

// ---------- Setup ----------
void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812,LED_PIN,GRB>(leds,NUM_LEDS);

    preferences.begin("wifi",true);
    wifiSSID = preferences.getString("ssid","");
    wifiPASS = preferences.getString("pass","");
    ledBrightness = preferences.getInt("brightness",128);
    preferences.end();

    WiFi.mode(WIFI_STA);
    if(wifiSSID!="") {
        WiFi.begin(wifiSSID.c_str(),wifiPASS.c_str());
        unsigned long start=millis();
        while(WiFi.status()!=WL_CONNECTED && millis()-start<15000) { 
            ledLoop(); 
            delay(1); 
        }
    }

    if(WiFi.status()==WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.println(WiFi.localIP());
        for(int i=0;i<NUM_LEDS;i++) leds[i]=CRGB::Black;
        FastLED.show();
        fallbackActive=false;
    } else {
        Serial.println("No WiFi! Starting fallback AP");
        WiFi.disconnect(true,true);
        delay(200);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("BasicTeamsLamp","12345678");
        IPAddress apIP = WiFi.softAPIP();
        Serial.print("AP started, IP: ");
        Serial.println(apIP);
        dnsServer.start(DNS_PORT,"*",apIP);
        fallbackActive=true;
    }

    // Server routes
    server.on("/",handleRoot);
    server.on("/status",handleStatus);
    server.on("/av",handlePath);
    server.on("/in_call",handlePath);
    server.on("/away",handlePath);
    server.on("/offline",handlePath);
    server.on("/wifi_save",handleWiFiSave);
    server.on("/brightness",handleBrightness);
    server.on("/get_status",handleGetStatus);
    server.onNotFound(handleNotFound);

    server.begin();
}

// ---------- Loop ----------
void loop() {
    server.handleClient();
    if(fallbackActive) dnsServer.processNextRequest();
    ledLoop();
}