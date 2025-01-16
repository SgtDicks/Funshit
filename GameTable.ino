#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h> // Optional but recommended

// =========================
// Configuration Parameters
// =========================

// LED configuration
#define LED_PIN        14         // GPIO pin connected to the NeoPixel strip (D5)
#define NUM_LEDS       16         // Number of LEDs in the strip (Adjust as needed)
#define DEFAULT_BRIGHTNESS 50     // Default Brightness (0-255)

// Initialize NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Constants
const int MAX_PLAYERS = 8;
int numPlayers = 1;                     // Default number of players
int brightness = DEFAULT_BRIGHTNESS;    // Current brightness level
int activePlayer = -1;                  // No active player initially

// WiFiManager instance
WiFiManager wifiManager;

// Create an instance of the web server
ESP8266WebServer server(80);

// =========================
// Function Prototypes
// =========================
void handleRoot();
void handleSetPlayers();
void handleResetAll();
void handleSetBrightness();
void handleSetActivePlayer();
void handleSetColors();
void handleSetAnimationColors(); // Newly added handler
void handleStartAnimation();
void handleNotFound();
void printMainMenu();
void printSetPlayers();
void updateLEDsBasedOnPlayers();
uint8_t getRed(uint32_t color);
uint8_t getGreen(uint32_t color);
uint8_t getBlue(uint32_t color);

// =========================
// Variables for LED Animations
// =========================
enum AnimationType {
    NONE,
    RAINBOW,
    THEATER_CHASE,
    BREATHING
};
AnimationType currentAnimation = NONE;

// Variables for Rainbow Animation
uint16_t rainbowHue = 0;

// Variables for Theater Chase Animation
int chaseStep = 0;
unsigned long lastChaseUpdate = 0;

// Variables for Breathing Effect
float breathPhase = 0.0;
unsigned long lastBreathUpdate = 0;

// Variables for Active Player Fading
float fadeValue = 1.0;
bool fadingOut = false;
unsigned long lastFadeUpdate = 0;
const long fadeInterval = 50;           // milliseconds between fade steps

// Define distinct colors for up to 8 players
uint32_t playerColors[MAX_PLAYERS] = {
    0xFF0000, // Player 1: Red
    0x00FF00, // Player 2: Green
    0x0000FF, // Player 3: Blue
    0xFFFF00, // Player 4: Yellow
    0xFF00FF, // Player 5: Magenta
    0x00FFFF, // Player 6: Cyan
    0xFFA500, // Player 7: Orange
    0x800080  // Player 8: Purple
};

// Animation Colors
uint32_t theaterChaseColor = strip.Color(127, 127, 127); // Default: White
uint32_t breathingColor = strip.Color(255, 255, 255);     // Default: White

// LED range for active player
int activePlayerStartLED = -1;
int activePlayerEndLED = -1;

// =========================
// Setup Function
// =========================
void setup() {
    // Initialize Serial Monitor for debugging
    Serial.begin(115200);
    Serial.println("\n[System] Initializing...");

    // Initialize NeoPixel strip
    strip.begin();
    strip.setBrightness(brightness);
    strip.show(); // Initialize all pixels to 'off'
    Serial.println("[LED] NeoPixel strip initialized.");

    // Initialize WiFiManager
    // Uncomment the following line to reset saved WiFi credentials (for debugging)
    // wifiManager.resetSettings();

    // Attempt to connect to WiFi; if not, start AP mode
    if(!wifiManager.autoConnect("Game_Table", "password")) {
        Serial.println("[WiFi] Failed to connect and hit timeout");
        // Reset and try again
        ESP.reset();
        delay(1000);
    }

    Serial.println("[WiFi] Connected to WiFi!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());

    // Configure web server routes
    server.on("/", handleRoot);
    server.on("/set_players", handleSetPlayers);
    server.on("/reset_all", handleResetAll);
    server.on("/set_brightness", handleSetBrightness);
    server.on("/set_active_player", handleSetActivePlayer);
    server.on("/set_colors", handleSetColors);
    server.on("/set_animation_colors", handleSetAnimationColors); // <-- Added
    server.on("/start_animation", handleStartAnimation);
    server.onNotFound(handleNotFound);
    Serial.println("[Web Server] Handlers configured.");

    // Start the web server
    server.begin();
    Serial.println("[Web Server] HTTP server started");

    // Display the main menu on Serial Monitor
    printMainMenu();
    Serial.println("[Menu] Main menu displayed.");

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
    // Handle web server requests
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

// =========================
// Web Server Handlers
// =========================

// Root handler: serves the main web page
void handleRoot() {
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Game Table Control</title>";
    // Add Google Fonts
    html += "<link href='https://fonts.googleapis.com/css?family=Roboto:400,700&display=swap' rel='stylesheet'>";
    // Add some enhanced styling
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
    html += "<label for='brightness' class='label'>Brightness: " + String(brightness) + "</label>";
    html += "<input type='range' id='brightness' name='brightness' min='0' max='255' value='" + String(brightness) + "' onchange=\"updateBrightness(this.value)\">";
    html += "</div>";
    html += "</div>";

    // Active Player Selection Section
    html += "<div class='section'>";
    html += "<h2>Active Player</h2>";
    html += "<select id='activePlayer' name='activePlayer' onchange=\"setActivePlayer(this.value)\">";
    html += "<option value='0'>None</option>";

    for(int i = 1; i <= numPlayers; i++) {
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
    for(int i = 1; i <= numPlayers; i++) {
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
    Serial.println("[Web Server] Served Root Page.");
}

// Handler to set players (increase/decrease)
void handleSetPlayers() {
    if (server.hasArg("action")) {
        String action = server.arg("action");
        if (action == "inc") {
            numPlayers++;
            if (numPlayers > MAX_PLAYERS) {
                numPlayers = MAX_PLAYERS;
            }
            Serial.println("[Web] Increased number of players to " + String(numPlayers));
        } else if (action == "dec") {
            numPlayers--;
            if (numPlayers < 1) {
                numPlayers = 1;
            }
            Serial.println("[Web] Decreased number of players to " + String(numPlayers));
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
        brightness = newBrightness;
        strip.setBrightness(brightness);
        strip.show();
        Serial.println("[Web] Brightness set to " + String(brightness));
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
    for(int i = 1; i <= numPlayers; i++) {
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

// Handler for undefined routes
void handleNotFound() {
    String message = "404 Not Found\n\n";
    message += "URI: " + server.uri();
    server.send(404, "text/plain", message);
    Serial.println("[Web Server] 404 Not Found for URI: " + server.uri());
}

// =========================
// Menu Display Functions (Serial)
// =========================

// Print the main menu to the Serial Monitor
void printMainMenu() {
    Serial.println("\n--- MAIN MENU ---");
    Serial.println("1) Set Players");
    Serial.println("2) Start Animation");
    Serial.println("3) Reset All");
    Serial.println("-----------------");
}

// Print the set players menu to the Serial Monitor
void printSetPlayers() {
    Serial.println("\n--- SET PLAYERS ---");
    Serial.print("Current number of players: ");
    Serial.println(numPlayers);
    Serial.println("Use the web interface to increase or decrease the number of players.");
    Serial.println("--------------------");
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

// =========================
// Animation Functions
// =========================

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
