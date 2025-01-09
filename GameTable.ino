/*
  Game Table Control - NodeMCU with NeoPixel LEDs and mDNS
  Hostname: gametable.local

  Features:
  - Connects to stored WiFi networks
  - Provides a web interface to control LEDs, brightness, colors, animations
  - Uses mDNS to allow discovery via gametable.local
  - Displays an initial LED pattern, holds for 10 seconds, then displays the IP address via LEDs

  Libraries Required:
  - Adafruit NeoPixel
  - ESP8266WiFi
  - ESP8266WebServer
  - ESP8266mDNS
  - EEPROM

  Ensure you have the latest ESP8266 Arduino Core installed.
*/

#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h> // Include mDNS library
#include <EEPROM.h>

// =========================
// Global Constant Definitions
// =========================

const int MAX_WIFI_NETWORKS = 2;        // Maximum number of WiFi networks to store
const int MAX_PLAYERS = 8;              // Maximum number of players
const int SSID_MAX_LEN = 32;             // Maximum SSID length
const int PASS_MAX_LEN = 64;             // Maximum Password length

const int LED_PIN = 14;                  // GPIO pin connected to the NeoPixel strip (D5)
const int NUM_LEDS = 50;                 // Number of LEDs in the strip

// =========================
// Enumerations
// =========================

enum AnimationState { NONE, RAINBOW, THEATER_CHASE, BREATHING };

// =========================
// Global Variables
// =========================

int numPlayers = 1;                       // Default number of players
int brightnessLevel = 50;                 // Current brightness level (0-255)

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

ESP8266WebServer server(80);              // Create a web server object that listens on port 80

// WiFi Credentials Management
struct WiFiCredentials {
  char ssid[SSID_MAX_LEN];
  char password[PASS_MAX_LEN];
};

// Animation Control Variables
AnimationState currentAnimation = NONE;
uint32_t playerColors[MAX_PLAYERS] = {0}; // Colors for each player (10 players)
int activePlayer = -1;                      // Active player (1 to MAX_PLAYERS), -1 for none
int activePlayerStartLED = -1;              // Start LED index for active player
int activePlayerEndLED = -1;                // End LED index for active player
uint32_t theaterChaseColor = 0xFFFFFF;      // Default Theater Chase color (White)
uint32_t breathingColor = 0xFFFFFF;         // Default Breathing color (White)

// Fading Effect Variables
unsigned long lastFadeUpdate = 0;
const unsigned long fadeInterval = 100;     // in milliseconds
float fadeValue = 1.0;
bool fadingOut = false;

// =========================
// WiFi Credentials Management Functions
// =========================

// Save a network to EEPROM
bool saveNetwork(int index, const char* ssid, const char* password) {
  if(index >= MAX_WIFI_NETWORKS) return false;

  int addr = index * (SSID_MAX_LEN + PASS_MAX_LEN);

  // Clear previous data
  for(int i = 0; i < SSID_MAX_LEN + PASS_MAX_LEN; i++) {
    EEPROM.write(addr + i, 0);
  }

  // Write SSID
  for(int i = 0; i < SSID_MAX_LEN && ssid[i] != '\0'; i++) {
    EEPROM.write(addr + i, ssid[i]);
  }

  // Write Password
  for(int i = 0; i < PASS_MAX_LEN && password[i] != '\0'; i++) {
    EEPROM.write(addr + SSID_MAX_LEN + i, password[i]);
  }

  EEPROM.commit();
  Serial.println("[EEPROM] Saved Network " + String(index + 1) + ": " + String(ssid));
  return true;
}

// Load a network from EEPROM
WiFiCredentials loadNetwork(int index) {
  WiFiCredentials creds;
  memset(&creds, 0, sizeof(creds));

  if(index >= MAX_WIFI_NETWORKS) return creds;

  int addr = index * (SSID_MAX_LEN + PASS_MAX_LEN);

  // Read SSID
  for(int i = 0; i < SSID_MAX_LEN; i++) {
    creds.ssid[i] = char(EEPROM.read(addr + i));
    if(creds.ssid[i] == 0) break;
  }

  // Read Password
  for(int i = 0; i < PASS_MAX_LEN; i++) {
    creds.password[i] = char(EEPROM.read(addr + SSID_MAX_LEN + i));
    if(creds.password[i] == 0) break;
  }

  return creds;
}

// Delete a network
bool deleteNetwork(int index) {
  if(index >= MAX_WIFI_NETWORKS) return false;
  return saveNetwork(index, "", "");
}

// =========================
// EEPROM Setup Function
// =========================

void setupEEPROM() {
    EEPROM.begin(512); // Allocate 512 bytes for EEPROM
    Serial.println("[EEPROM] Initialized with 512 bytes.");
}

// =========================
// Connection Strategy
// =========================

bool connectToNetworks() {
  for(int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    WiFiCredentials creds = loadNetwork(i);
    if(strlen(creds.ssid) == 0) continue; // Skip empty entries

    Serial.print("[WiFi] Attempting to connect to: ");
    Serial.println(creds.ssid);
    WiFi.begin(creds.ssid, creds.password);

    // Wait for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 20 * 500ms = 10 seconds
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if(WiFi.status() == WL_CONNECTED){
      Serial.print("[WiFi] Connected to ");
      Serial.println(creds.ssid);
      return true;
    }
    else{
      Serial.print("[WiFi] Failed to connect to ");
      Serial.println(creds.ssid);
    }
  }
  return false;
}

// =========================
// LED Control Functions
// =========================

// Helper functions to extract color components
uint8_t getRed(uint32_t color) {
    return (color >> 16) & 0xFF;
}

uint8_t getGreen(uint32_t color) {
    return (color >> 8) & 0xFF;
}

uint8_t getBlue(uint32_t color) {
    return color & 0xFF;
}

// Update LED colors based on the number of players and their assigned colors
void updateLEDsBasedOnPlayers() {
    if (numPlayers > MAX_PLAYERS) numPlayers = MAX_PLAYERS;
    if (numPlayers < 1) numPlayers = 1;

    if(numPlayers ==1){
        // Set all LEDs to the single player's color
        for(int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, playerColors[0]); // Player 1
        }
        if(activePlayer ==1){
            activePlayerStartLED = 0;
            activePlayerEndLED = NUM_LEDS -1;
        }
        else{
            activePlayerStartLED = -1;
            activePlayerEndLED = -1;
        }
    }
    else{
        // Split LEDs among players with distinct colors
        int ledsPerPlayer = NUM_LEDS / numPlayers;
        int remainingLEDs = NUM_LEDS % numPlayers;

        int currentLED = 0;
        activePlayerStartLED = -1;
        activePlayerEndLED = -1;

        for(int i = 0; i < numPlayers; i++) {
            int ledsToSet = ledsPerPlayer + (i < remainingLEDs ? 1 : 0); // Distribute remaining LEDs
            for(int j = 0; j < ledsToSet; j++) {
                if(currentLED < NUM_LEDS){
                    strip.setPixelColor(currentLED, playerColors[i]);
                    currentLED++;
                }
            }

            // If this player is active, store their LED range
            if((i+1) == activePlayer){
                activePlayerStartLED = currentLED - ledsToSet;
                activePlayerEndLED = currentLED -1;
            }
        }

        // Turn off any remaining LEDs (if any)
        while(currentLED < NUM_LEDS){
            strip.setPixelColor(currentLED, strip.Color(0, 0, 0));
            currentLED++;
        }
    }

    strip.show();
    Serial.println("[LED] Updated LED colors based on number of players.");
}

// Rainbow Cycle Animation
void rainbowCycle() {
    static unsigned long lastRainbowUpdate = 0;
    static uint16_t currentHue = 0;

    unsigned long currentTime = millis();
    if (currentTime - lastRainbowUpdate >= 20) { // Adjust speed as needed
        lastRainbowUpdate = currentTime;

        for(int i = 0; i < NUM_LEDS; i++) {
            uint32_t color = strip.ColorHSV(((i * 65536L / NUM_LEDS) + currentHue) & 0xFFFF);
            strip.setPixelColor(i, color);
        }
        strip.show();

        currentHue += 256; // Adjust for speed and color change
    }
}

// Theater Chase Animation with Custom Color
void theaterChase(uint32_t color, int wait) {
    static unsigned long lastChaseUpdate = 0;
    static int chaseStep = 0;

    unsigned long currentTime = millis();
    if (currentTime - lastChaseUpdate >= wait) { // Adjust speed as needed
        lastChaseUpdate = currentTime;

        for(int i = 0; i < NUM_LEDS; i++) {
            if((i + chaseStep) % 3 == 0){
                strip.setPixelColor(i, color);
            }
            else{
                strip.setPixelColor(i, 0); // Off
            }
        }
        strip.show();

        chaseStep++;
        if(chaseStep >= 3){
            chaseStep = 0;
        }
    }
}

// Breathing Effect Animation with Custom Color
void breathingEffect(uint32_t color) {
    static unsigned long lastBreathUpdate = 0;
    static float breathPhase = 0.0;

    unsigned long currentTime = millis();
    if (currentTime - lastBreathUpdate >= 30) { // Adjust for smoothness
        lastBreathUpdate = currentTime;

        breathPhase += 0.05;
        if(breathPhase > 2.0 * PI){
            breathPhase -= 2.0 * PI;
        }

        // Calculate brightness scaling using sine wave
        float scale = (sin(breathPhase) + 1.0) / 2.0 * (1.0 - 0.3) + 0.3; // Scale between 0.3 and 1.0

        // Apply scaling to all LEDs with the selected breathing color
        for(int i = 0; i < NUM_LEDS; i++) {
            uint8_t r = getRed(color) * scale;
            uint8_t g = getGreen(color) * scale;
            uint8_t b = getBlue(color) * scale;
            strip.setPixelColor(i, strip.Color(r, g, b));
        }
        strip.show();
    }
}

// =========================
// Function to Display Initial LED Pattern
// =========================

void displayInitialPattern(){
    // Define the pattern: 'X' = On, 'o' = Off
    // Example pattern: XoXXXXXXXXXoXXXX (16 LEDs)
    String pattern = "XoXXXXXXXXXoXXXX";

    for(int i=0; i<NUM_LEDS; i++){
        if(i < pattern.length() && pattern[i] == 'X'){
            strip.setPixelColor(i, strip.Color(255, 255, 255)); // White
        }
        else{
            strip.setPixelColor(i, 0); // Off
        }
    }
    strip.show();
    Serial.println("[LED] Displayed initial pattern.");
}

// =========================
// Function to Display IP Address via LEDs
// =========================

void displayIPAddress(String ip) {
    Serial.println("[LED] Displaying IP Address via LEDs...");

    // Split IP into octets
    int firstDot = ip.indexOf('.');
    int secondDot = ip.indexOf('.', firstDot + 1);
    int thirdDot = ip.indexOf('.', secondDot + 1);

    String octet1 = ip.substring(0, firstDot);
    String octet2 = ip.substring(firstDot +1, secondDot);
    String octet3 = ip.substring(secondDot +1, thirdDot);
    String octet4 = ip.substring(thirdDot +1);

    // Function to flash a single digit
    auto flashDigit = [&](int num, uint32_t color) {
        if(num > 0 && num <= NUM_LEDS){
            // Turn on 'num' LEDs in the specified color
            for(int i = 0; i < num; i++){
                strip.setPixelColor(i, color);
            }
            strip.show();
            delay(1000); // Duration of flash

            // Turn off the LEDs
            for(int i = 0; i < num; i++){
                strip.setPixelColor(i, 0);
            }
            strip.show();
            delay(300); // Pause between flashes
        }
        else{
            // Handle '0' as no LEDs flashing (or implement a minimal indicator)
            delay(00); // Duration of flash (can be adjusted)
            delay(300); // Pause between flashes
        }
    };

    // Function to perform long flashes
    auto longFlashes = [&]() {
        for(int i =0; i <1; i++){
            // All LEDs on in White
            for(int j =0; j < NUM_LEDS; j++){
                strip.setPixelColor(j, strip.Color(2,20,255));
            }
            strip.show();
            delay(500); // Duration of long flash

            // All LEDs off
            for(int j =0; j < NUM_LEDS; j++){
                strip.setPixelColor(j, 0);
            }
            strip.show();
            delay(500); // Pause between flashes
        }
    };

    // Function to process an octet (flashing digits)
    auto processOctet = [&](String octet, bool isRed) {
        uint32_t color = isRed ? strip.Color(255, 0, 0) : strip.Color(255, 255, 255); // Red or White
        for(char c : octet){
            if(isDigit(c)){
                int num = c - '0';
                Serial.println("[LED] Flashing digit: " + String(num));
                flashDigit(num, color);
            }
            else{
                // Handle dot or any other non-digit characters
                delay(300); // Pause between octets
            }
        }
    };

    // Flash first 3 octets in Red
    Serial.println("[LED] Flashing first 3 octets in Red.");
    processOctet(octet1, true);
    delay(500); // Pause between octets
    processOctet(octet2, true);
    delay(500);
    processOctet(octet3, true);

    // Perform 5 long flashes in White
    Serial.println("[LED] Performing 5 long flashes.");
    longFlashes();

    // Flash the fourth octet in White
    Serial.println("[LED] Flashing fourth octet in White.");
    processOctet(octet4, false);
}

// =========================
// mDNS Setup Function with gametable.local
// =========================

void setupMDNS() {
    if (!MDNS.begin("gametable")) { // Set hostname to "gametable"
        Serial.println("[mDNS] Error setting up MDNS responder!");
        return;
    }
    Serial.println("[mDNS] mDNS responder started with hostname: gametable.local");
    // Register the HTTP service
    MDNS.addService("http", "tcp", 80);
}

// =========================
// Web Server Handlers
// =========================

// Root Page Handler
void handleRootPage() {
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Game Table Control</title>";
    // Add Google Fonts
    html += "<link href='https://fonts.googleapis.com/css?family=Roboto:400,700&display=swap' rel='stylesheet'>";
    // Add enhanced styling
    html += "<style>"
            "body { font-family: 'Roboto', sans-serif; background-color: #1e1e1e; color: #f0f0f0; margin: 0; padding: 0; }"
            "header { background-color: #333; padding: 20px; text-align: center; }"
            "header h1 { margin: 0; font-size: 2em; }"
            "main { padding: 20px; }"
            ".section { margin-bottom: 30px; }"
            ".section h2 { border-bottom: 2px solid #555; padding-bottom: 10px; }"
            ".controls { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }"
            ".controls button { padding: 15px 25px; font-size: 16px; cursor: pointer; border: none; border-radius: 8px; background-color: #4CAF50; color: white; transition: background-color 0.3s ease; }"
            ".controls button:hover { background-color: #45a049; }"
            ".slider-container { width: 100%; max-width: 400px; margin: 0 auto; }"
            "input[type=range] { width: 100%; }"
            ".label { font-size: 1.2em; margin-bottom: 10px; display: block; text-align: center; }"
            "select { padding: 10px; font-size: 16px; border-radius: 8px; border: 1px solid #ccc; width: 100%; max-width: 400px; margin: 0 auto; display: block; }"
            ".color-buttons { display: flex; flex-wrap: wrap; justify-content: center; gap: 10px; }"
            ".color-picker { display: flex; flex-direction: column; align-items: center; }"
            ".color-picker label { margin-bottom: 5px; }"
            ".animation-buttons { display: flex; flex-wrap: wrap; justify-content: center; gap: 10px; }"
            ".reset-button { background-color: #f44336; }"
            ".reset-button:hover { background-color: #da190b; }"
            "@media (max-width: 600px) {"
            "    .controls { flex-direction: column; align-items: center; }"
            "    .animation-buttons, .color-buttons { flex-direction: column; }"
            "}"
            "</style></head><body>";
    html += "<header><h1>🎮 Game Table Control 🎮</h1></header>";
    html += "<main>";

    // Number of Players Section
    html += "<div class='section'>";
    html += "<h2>Number of Players</h2>";
    html += "<div class='controls'>";
    html += "<button onclick=\"location.href='/set_players?action=inc'\">➕ Increase</button>";
    html += "<button onclick=\"location.href='/set_players?action=dec'\">➖ Decrease</button>";
    html += "</div>";
    html += "</div>";

    // Brightness Control Section
    html += "<div class='section'>";
    html += "<h2>Brightness Control</h2>";
    html += "<div class='slider-container'>";
    html += "<label for='brightness' class='label'>Brightness: " + String(brightnessLevel) + "</label>";
    html += "<input type='range' id='brightness' name='brightness' min='0' max='255' value='" + String(brightnessLevel) + "' onchange=\"updateBrightness(this.value)\">";
    html += "</div>";
    html += "</div>";

    // Active Player Selection Section
    html += "<div class='section'>";
    html += "<h2>Active Player</h2>";
    html += "<select id='activePlayer' name='activePlayer' onchange=\"setActivePlayer(this.value)\">";
    html += "<option value='0'>None</option>";

    for(int i = 1; i <= numPlayers && i <= MAX_PLAYERS; i++) { // Up to 10 players
        if(i == activePlayer){
            html += "<option value='" + String(i) + "' selected>Player " + String(i) + "</option>";
        }
        else{
            html += "<option value='" + String(i) + "'>Player " + String(i) + "</option>";
        }
    }

    html += "</select>";
    html += "</div>";

    // Player Colors Section
    html += "<div class='section'>";
    html += "<h2>Set Player Colors</h2>";
    html += "<form id='colorForm' action='/set_colors' method='POST'>";
    html += "<div class='color-buttons'>";
    for(int i = 1; i <= numPlayers && i <= MAX_PLAYERS; i++) { // Up to 10 players
        // Convert existing player color to HEX
        uint32_t color = playerColors[i-1];
        char hexColor[7];
        sprintf(hexColor, "%02X%02X%02X", getRed(color), getGreen(color), getBlue(color));

        html += "<div class='color-picker'>";
        html += "<label for='player" + String(i) + "_color'>Player " + String(i) + " Color:</label>";
        html += "<input type='color' id='player" + String(i) + "_color' name='player" + String(i) + "_color' value='#" + String(hexColor) + "'>";
        html += "</div>";
    }
    html += "</div>";
    html += "<div class='controls'>";
    html += "<button type='submit'>💾 Save Colors</button>";
    html += "</div>";
    html += "</form>";
    html += "</div>";

    // Animation Colors Section
    html += "<div class='section'>";
    html += "<h2>Set Animation Colors</h2>";
    html += "<form id='animationColorForm' action='/set_animation_colors' method='POST'>";
    html += "<div class='color-buttons'>";

    // Theater Chase Color Picker
    {
        // Convert existing animation color to HEX
        uint32_t color = theaterChaseColor;
        char hexColor[7];
        sprintf(hexColor, "%02X%02X%02X", getRed(color), getGreen(color), getBlue(color));

        html += "<div class='color-picker'>";
        html += "<label for='theaterChase_color'>Theater Chase Color:</label>";
        html += "<input type='color' id='theaterChase_color' name='theaterChase_color' value='#" + String(hexColor) + "'>";
        html += "</div>";
    }

    // Breathing Effect Color Picker
    {
        // Convert existing animation color to HEX
        uint32_t color = breathingColor;
        char hexColor[7];
        sprintf(hexColor, "%02X%02X%02X", getRed(color), getGreen(color), getBlue(color));

        html += "<div class='color-picker'>";
        html += "<label for='breathing_color'>Breathing Effect Color:</label>";
        html += "<input type='color' id='breathing_color' name='breathing_color' value='#" + String(hexColor) + "'>";
        html += "</div>";
    }

    html += "</div>";
    html += "<div class='controls'>";
    html += "<button type='submit'>💾 Save Animation Colors</button>";
    html += "</div>";
    html += "</form>";
    html += "</div>";

    // Animations Section
    html += "<div class='section'>";
    html += "<h2>Animations</h2>";
    html += "<div class='animation-buttons'>";
    html += "<button onclick=\"startAnimation('rainbow')\">🌈 Rainbow</button>";
    html += "<button onclick=\"startAnimation('theater_chase')\">🎬 Theater Chase</button>";
    html += "<button onclick=\"startAnimation('breathing')\">💨 Breathing</button>";
    html += "</div>";
    html += "</div>";

    // Reset All Button
    html += "<div class='section'>";
    html += "<div class='controls'>";
    html += "<button class='reset-button' onclick=\"location.href='/reset_all'\">🔄 Reset All</button>";
    html += "</div>";
    html += "</div>";

    // WiFi Configuration Section
    html += "<div class='section'>";
    html += "<h2>WiFi Configuration</h2>";
    html += "<form id='wifiConfigForm' action='/wifi_config' method='POST'>";
    html += "<div class='color-buttons'>"; // Reusing 'color-buttons' class for layout

    for(int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        WiFiCredentials creds = loadNetwork(i);
        String ssidValue = String(creds.ssid);
        String passValue = String(creds.password);

        html += "<div class='color-picker'>";
        html += "<label for='ssid" + String(i) + "'>SSID " + String(i + 1) + ":</label>";
        html += "<input type='text' id='ssid" + String(i) + "' name='ssid" + String(i) + "' value='" + ssidValue + "' required>";
        html += "<label for='password" + String(i) + "'>Password " + String(i + 1) + ":</label>";
        html += "<input type='password' id='password" + String(i) + "' name='password" + String(i) + "' value='" + passValue + "'>";
        html += "</div>";
    }

    html += "</div>";
    html += "<div class='controls'>";
    html += "<button type='submit'>💾 Save WiFi Settings</button>";
    html += "</div>";
    html += "</form>";
    html += "</div>";

    // JavaScript for dynamic updates
    html += "<script>"
            "function updateBrightness(val) {"
            "   document.querySelector('.label').innerText = 'Brightness: ' + val;"
            "   fetch('/set_brightness?value=' + val)"
            "   .then(response => {"
            "       console.log('Brightness set to ' + val);"
            "   });"
            "}"
            "function setActivePlayer(val) {"
            "   fetch('/set_active_player?player=' + val)"
            "   .then(response => {"
            "       console.log('Active player set to ' + val);"
            "   });"
            "}"
            "function startAnimation(animation) {"
            "   fetch('/start_animation?animation=' + animation)"
            "   .then(response => {"
            "       console.log('Started animation: ' + animation);"
            "   });"
            "}"
            "</script>";

    html += "</main></body></html>";

    server.send(200, "text/html", html);
    Serial.println("[Web Server] Served Root Page with WiFi Config.");
}

// Handler to set players (increase/decrease)
void handleSetPlayers() {
    if (server.hasArg("action")) {
        String action = server.arg("action");
        if (action == "inc") {
            if(numPlayers < MAX_PLAYERS){
                numPlayers++;
                Serial.println("[Web] Increased number of players to " + String(numPlayers));
            }
            else{
                Serial.println("[Web] Maximum number of players reached.");
            }
        } else if (action == "dec") {
            if(numPlayers > 1){
                numPlayers--;
                Serial.println("[Web] Decreased number of players to " + String(numPlayers));
            }
            else{
                Serial.println("[Web] Minimum number of players is 1.");
            }
        }
        updateLEDsBasedOnPlayers();

        // If activePlayer is now invalid, reset it
        if(activePlayer > numPlayers){
            activePlayer = -1;
            Serial.println("[Web] Active player reset to none as number of players decreased.");
        }
    }
    // After action, redirect back to root
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled set_players action and redirected to Root.");
}

// Handler to reset all LEDs to white
void handleResetAll() {
    // Reset active player
    activePlayer = -1;

    // Reset animation
    currentAnimation = NONE;

    // Set all LEDs to white
    for(int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255)); // White
    }
    strip.show();
    Serial.println("[Web] Reset all LEDs to white.");

    // Redirect back to root
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled reset_all action and redirected to Root.");
}

// Handler to set brightness
void handleSetBrightness() {
    if (server.hasArg("value")) {
        int newBrightness = server.arg("value").toInt();
        if (newBrightness < 0) newBrightness = 0;
        if (newBrightness > 255) newBrightness = 255;
        brightnessLevel = newBrightness;
        strip.setBrightness(brightnessLevel);
        strip.show();
        Serial.println("[Web] Brightness set to " + String(brightnessLevel));
    }
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled set_brightness action and redirected to Root.");
}

// Handler to set active player
void handleSetActivePlayer(){
    if(server.hasArg("player")){
        int player = server.arg("player").toInt();
        if(player >=1 && player <= numPlayers){
            activePlayer = player;
            Serial.println("[Web] Active player set to Player " + String(activePlayer));
        }
        else{
            activePlayer = -1;
            Serial.println("[Web] Active player set to None");
        }

        // Update LEDs based on players (to refresh activePlayerStartLED and activePlayerEndLED)
        updateLEDsBasedOnPlayers();
    }

    // After action, redirect back to root
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled set_active_player action and redirected to Root.");
}

// Handler to set colors for each player
void handleSetColors(){
    bool colorChanged = false;
    for(int i = 1; i <= numPlayers && i <= MAX_PLAYERS; i++) {
        String colorArg = "player" + String(i) + "_color";
        if(server.hasArg(colorArg)){
            String colorHex = server.arg(colorArg);
            // Convert HEX to RGB
            if(colorHex.length() == 7 && colorHex[0] == '#'){
                unsigned long hex = strtol(&colorHex[1], NULL, 16);
                uint8_t r = (hex >> 16) & 0xFF;
                uint8_t g = (hex >> 8) & 0xFF;
                uint8_t b = hex & 0xFF;
                playerColors[i-1] = strip.Color(r, g, b);
                Serial.println("[Web] Player " + String(i) + " color set to #" + colorHex.substring(1));
                colorChanged = true;
            }
        }
    }

    if(colorChanged){
        updateLEDsBasedOnPlayers();
    }

    // Redirect back to root
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled set_colors action and redirected to Root.");
}

// Handler to set colors for animations
void handleSetAnimationColors(){
    bool colorChanged = false;

    // Theater Chase Color
    if(server.hasArg("theaterChase_color")){
        String colorHex = server.arg("theaterChase_color");
        if(colorHex.length() == 7 && colorHex[0] == '#'){
            unsigned long hex = strtol(&colorHex[1], NULL, 16);
            uint8_t r = (hex >> 16) & 0xFF;
            uint8_t g = (hex >> 8) & 0xFF;
            uint8_t b = hex & 0xFF;
            theaterChaseColor = strip.Color(r, g, b);
            Serial.println("[Web] Theater Chase color set to #" + colorHex.substring(1));
            colorChanged = true;
        }
    }

    // Breathing Effect Color
    if(server.hasArg("breathing_color")){
        String colorHex = server.arg("breathing_color");
        if(colorHex.length() == 7 && colorHex[0] == '#'){
            unsigned long hex = strtol(&colorHex[1], NULL, 16);
            uint8_t r = (hex >> 16) & 0xFF;
            uint8_t g = (hex >> 8) & 0xFF;
            uint8_t b = hex & 0xFF;
            breathingColor = strip.Color(r, g, b);
            Serial.println("[Web] Breathing Effect color set to #" + colorHex.substring(1));
            colorChanged = true;
        }
    }

    if(colorChanged){
        // If the current animation uses these colors, update them
        if(currentAnimation == THEATER_CHASE){
            // No immediate action required; the theaterChase function uses the updated color
        }
        if(currentAnimation == BREATHING){
            // No immediate action required; the breathingEffect function uses the updated color
        }
    }

    // Redirect back to root
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled set_animation_colors action and redirected to Root.");
}

// Handler to start an animation
void handleStartAnimation(){
    if(server.hasArg("animation")){
        String animation = server.arg("animation");
        if(animation == "rainbow"){
            currentAnimation = RAINBOW;
            Serial.println("[Web] Started Rainbow animation.");
        }
        else if(animation == "theater_chase"){
            currentAnimation = THEATER_CHASE;
            Serial.println("[Web] Started Theater Chase animation.");
        }
        else if(animation == "breathing"){
            currentAnimation = BREATHING;
            Serial.println("[Web] Started Breathing animation.");
        }
        else{
            currentAnimation = NONE;
            Serial.println("[Web] Unknown animation type. Animation stopped.");
        }

        // Stop any ongoing animation or fading
        activePlayer = -1;
    }

    // Redirect back to root
    server.sendHeader("Location", "/");
    server.send(303);
    Serial.println("[Web Server] Handled start_animation action and redirected to Root.");
}

// Handler for WiFi Configuration Submission
void handleWiFiConfig(){
    Serial.println("[Web] Received WiFi Configuration Submission.");

    for(int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        String ssidArg = "ssid" + String(i);
        String passArg = "password" + String(i);

        if(server.hasArg(ssidArg) && server.hasArg(passArg)){
            String ssid = server.arg(ssidArg);
            String password = server.arg(passArg);

            if(ssid.length() > 0){
                saveNetwork(i, ssid.c_str(), password.c_str());
            }
            else{
                deleteNetwork(i);
            }
        }
    }

    String html = "<!DOCTYPE html><html><head><title>Saved</title></head><body>";
    html += "<h1>WiFi Settings Saved</h1>";
    html += "<p>The device will reboot and attempt to connect to the configured networks.</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
}

// Handler for undefined routes
void handleNotFound() {
    String message = "404 Not Found\n\n";
    message += "URI: " + server.uri();
    server.send(404, "text/plain", message);
    Serial.println("[Web Server] 404 Not Found for URI: " + server.uri());
}

// =========================
// Setup Function
// =========================

void setup() {
    // Initialize Serial Monitor for debugging
    Serial.begin(115200);
    Serial.println("\n[System] Initializing...");

    // Initialize EEPROM
    setupEEPROM();

    // Initialize NeoPixel strip
    strip.begin();
    strip.setBrightness(brightnessLevel);
    strip.show(); // Initialize all pixels to 'off'
    Serial.println("[LED] NeoPixel strip initialized.");

    // Initialize player colors with default values
    playerColors[0] = strip.Color(255, 0, 0);    // Player 1 - Red
    playerColors[1] = strip.Color(0, 255, 0);    // Player 2 - Green
    playerColors[2] = strip.Color(0, 0, 255);    // Player 3 - Blue
    playerColors[3] = strip.Color(255, 255, 0);  // Player 4 - Yellow
    playerColors[4] = strip.Color(255, 0, 255);  // Player 5 - Magenta
    playerColors[5] = strip.Color(0, 255, 255);  // Player 6 - Cyan
    playerColors[6] = strip.Color(255, 165, 0);  // Player 7 - Orange
    playerColors[7] = strip.Color(128, 0, 128);  // Player 8 - Purple
    playerColors[8] = strip.Color(0, 128, 128);  // Player 9 - Teal
    playerColors[9] = strip.Color(128, 128, 0);  // Player 10 - Olive

    // Attempt to connect to stored networks
    if(connectToNetworks()){
        Serial.println("[System] Connected to WiFi.");

        // Retrieve IP address
        IPAddress ip = WiFi.localIP();
        String ipAddress = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
        Serial.println("[WiFi] IP Address: " + ipAddress);

        // Start mDNS after successful WiFi connection
        setupMDNS();

        // Display initial pattern and IP Address via LEDs
        displayInitialPattern();
        delay(10000); // 10 seconds
        displayIPAddress(ipAddress);
    }
    else{
        Serial.println("[System] Unable to connect to any stored WiFi networks.");
        Serial.println("[System] Please configure WiFi from the web interface.");
    }

    // Define web server routes
    server.on("/", handleRootPage);
    server.on("/set_players", handleSetPlayers);
    server.on("/reset_all", handleResetAll);
    server.on("/set_brightness", handleSetBrightness);
    server.on("/set_active_player", handleSetActivePlayer);
    server.on("/set_colors", handleSetColors);
    server.on("/set_animation_colors", handleSetAnimationColors);
    server.on("/start_animation", handleStartAnimation);
    server.on("/wifi_config", HTTP_POST, handleWiFiConfig);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[Web Server] HTTP server started on port 80");

    // Initialize LEDs to white
    for(int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255)); // White
    }
    strip.show();
    Serial.println("[LED] All LEDs set to white.");
}

// =========================
// Loop Function
// =========================

void loop() {
    server.handleClient();

    // Handle ongoing animations
    if (currentAnimation != NONE) {
        switch (currentAnimation) {
            case RAINBOW:
                rainbowCycle();
                break;
            case THEATER_CHASE:
                theaterChase(theaterChaseColor, 50); // Customizable color
                break;
            case BREATHING:
                breathingEffect(breathingColor);    // Customizable color
                break;
            default:
                break;
        }
    }
    else if (activePlayer != -1) { // If an active player is set and no animation is running
        unsigned long currentTime = millis();

        if (currentTime - lastFadeUpdate >= fadeInterval) {
            lastFadeUpdate = currentTime;

            if (!fadingOut) {
                fadeValue -= 0.05;
                if (fadeValue <= 0.1) { // Minimum brightness scale
                    fadeValue = 0.1;
                    fadingOut = true;
                }
            }
            else {
                fadeValue += 0.05;
                if (fadeValue >= 1.0) { // Maximum brightness scale
                    fadeValue = 1.0;
                    fadingOut = false;
                }
            }

            // Update active player's LEDs with fading effect
            if (activePlayerStartLED != -1 && activePlayerEndLED != -1) {
                for(int i = activePlayerStartLED; i <= activePlayerEndLED && i < NUM_LEDS; i++) {
                    uint32_t baseColor = playerColors[activePlayer -1]; // players are 1-indexed

                    uint8_t r = getRed(baseColor) * fadeValue;
                    uint8_t g = getGreen(baseColor) * fadeValue;
                    uint8_t b = getBlue(baseColor) * fadeValue;

                    strip.setPixelColor(i, strip.Color(r, g, b));
                }
                strip.show();
            }
        }
    }

    // Small delay to prevent excessive CPU usage
    delay(1);
}
