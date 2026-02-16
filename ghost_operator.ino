/*
 * BLE Keyboard/Mouse Device - "Ghost Operator"
 * ENCODER MENU + FLASH STORAGE
 * For Seeed XIAO nRF52840
 * 
 * MODES (cycle with function button short press):
 *   NORMAL    - Encoder selects keystroke
 *   KEY MIN   - Encoder adjusts key interval minimum
 *   KEY MAX   - Encoder adjusts key interval maximum
 *   MOUSE JIG - Encoder adjusts mouse jiggle duration
 *   MOUSE IDLE- Encoder adjusts mouse idle duration
 * 
 * PIN ASSIGNMENTS:
 * D0 (P0.02) - Encoder A (digital interrupt)
 * D1 (P0.03) - Encoder B (digital interrupt)
 * D2 (P0.28) - Encoder Button (toggle key/mouse)
 * D3 (P0.29) - Function Button (mode cycle / long=sleep)
 * D4 (P0.04) - SDA (OLED)
 * D5 (P0.05) - SCL (OLED)
 * D6 (P0.30) - Activity LED (optional)
 * 
 * CONTROLS:
 * - Encoder rotation: Select key (NORMAL) or adjust value (settings)
 * - Encoder button: Toggle keys ON/OFF (key modes) or mouse ON/OFF (mouse modes)
 * - Function button short: Cycle to next mode
 * - Function button long (3s): Enter deep sleep
 */

#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <nrf_soc.h>
#include <nrf_power.h>

using namespace Adafruit_LittleFS_Namespace;

// ============================================================================
// VERSION & CONFIG
// ============================================================================
#define VERSION "1.0.0"
#define DEVICE_NAME "GhostOperator"
#define SETTINGS_FILE "/settings.dat"
#define SETTINGS_MAGIC 0x4A494747  // "JIGG"

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define PIN_ENCODER_A    0   // D0 - Encoder A (interrupt)
#define PIN_ENCODER_B    1   // D1 - Encoder B (interrupt)
#define PIN_ENCODER_BTN  2   // D2 - Encoder pushbutton
#define PIN_FUNC_BTN     3   // D3 - Function button
#define PIN_SDA          4   // D4 - I2C SDA
#define PIN_SCL          5   // D5 - I2C SCL
// Activity LED - use built-in or D6
#ifndef PIN_LED
#define PIN_LED          6   // D6 - Activity LED
#endif

// Internal battery pin - use board defaults if available
#ifndef PIN_VBAT
#define PIN_VBAT         32
#endif
#define PIN_VBAT_ENABLE  14

// nRF52840 GPIO for wake
#define PIN_FUNC_BTN_NRF  29

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
#define VALUE_MIN_MS          500UL       // 0.5 seconds
#define VALUE_MAX_MS          30000UL     // 30 seconds
#define VALUE_STEP_MS         500UL       // 0.5 second increments
#define RANDOMNESS_PERCENT    20          // ±20% for mouse only
#define MIN_CLAMP_MS          500UL

#define MOUSE_MOVE_STEP_MS    20
#define DISPLAY_UPDATE_MS     100         // Faster for smooth countdown
#define BATTERY_READ_MS       60000UL
#define LONG_PRESS_MS         3000
#define SLEEP_DISPLAY_MS      2000
#define MODE_TIMEOUT_MS       10000       // Return to NORMAL after 10s inactivity

// Battery calibration (3.0V internal reference)
#define VBAT_MV_PER_LSB   (3000.0F / 4096.0F)
#define VBAT_DIVIDER      (1510.0F / 510.0F)
#define VBAT_MIN_MV       3200
#define VBAT_MAX_MV       4200

// ============================================================================
// UI MODES
// ============================================================================
enum UIMode {
  MODE_NORMAL,
  MODE_KEY_MIN,
  MODE_KEY_MAX,
  MODE_MOUSE_JIG,
  MODE_MOUSE_IDLE,
  MODE_COUNT
};

const char* MODE_NAMES[] = {
  "NORMAL",
  "KEY MIN",
  "KEY MAX",
  "MOUSE JIG",
  "MOUSE IDLE"
};

// ============================================================================
// SETTINGS STRUCTURE (saved to flash)
// ============================================================================
struct Settings {
  uint32_t magic;
  uint32_t keyIntervalMin;
  uint32_t keyIntervalMax;
  uint32_t mouseJiggleDuration;
  uint32_t mouseIdleDuration;
  uint8_t selectedKeyIndex;
  uint8_t checksum;
};

Settings settings;

// Default values
void loadDefaults() {
  settings.magic = SETTINGS_MAGIC;
  settings.keyIntervalMin = 2000;      // 2 seconds
  settings.keyIntervalMax = 6500;      // 6.5 seconds
  settings.mouseJiggleDuration = 15000; // 15 seconds
  settings.mouseIdleDuration = 30000;   // 30 seconds
  settings.selectedKeyIndex = 0;
}

uint8_t calcChecksum() {
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)&settings;
  for (size_t i = 0; i < sizeof(Settings) - 1; i++) {
    sum ^= p[i];
  }
  return sum;
}

// ============================================================================
// KEY DEFINITIONS
// ============================================================================
struct KeyDef {
  uint8_t keycode;
  const char* name;
  bool isModifier;
};

const KeyDef AVAILABLE_KEYS[] = {
  { HID_KEY_F15,          "F15",    false },
  { HID_KEY_F14,          "F14",    false },
  { HID_KEY_F13,          "F13",    false },
  { HID_KEY_SCROLL_LOCK,  "ScrLk",  false },
  { HID_KEY_PAUSE,        "Pause",  false },
  { HID_KEY_NUM_LOCK,     "NumLk",  false },
  { HID_KEY_SHIFT_LEFT,   "LShift", true  },
  { HID_KEY_CONTROL_LEFT, "LCtrl",  true  },
  { HID_KEY_ALT_LEFT,     "LAlt",   true  },
  { 0x00,                 "NONE",   false },
};

const int NUM_KEYS = sizeof(AVAILABLE_KEYS) / sizeof(AVAILABLE_KEYS[0]);

// ============================================================================
// MOUSE DIRECTIONS
// ============================================================================
const int8_t MOUSE_DIRS[][2] = {
  { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
  { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
};
const int NUM_DIRS = 8;

// ============================================================================
// GLOBALS
// ============================================================================

// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false;

// File system
File settingsFile(InternalFS);

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;

// State
volatile int encoderPos = 0;
int lastEncoderPos = 0;
bool deviceConnected = false;
bool keyEnabled = true;
bool mouseEnabled = true;

// UI Mode
UIMode currentMode = MODE_NORMAL;
unsigned long lastModeActivity = 0;

// Timing
unsigned long startTime = 0;
unsigned long lastKeyTime = 0;
unsigned long lastMouseStateChange = 0;
unsigned long lastMouseStep = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBatteryRead = 0;

// Current targets (with randomness applied for mouse)
unsigned long currentKeyInterval = 4000;
unsigned long currentMouseJiggle = 15000;
unsigned long currentMouseIdle = 30000;

// Mouse state machine
enum MouseState { MOUSE_IDLE, MOUSE_JIGGLING };
MouseState mouseState = MOUSE_IDLE;
int8_t currentMouseDx = 0;
int8_t currentMouseDy = 0;
int32_t mouseNetX = 0;
int32_t mouseNetY = 0;

// Battery
int batteryPercent = 100;
float batteryVoltage = 4.2;

// Button state
unsigned long funcBtnPressStart = 0;
bool funcBtnWasPressed = false;
bool sleepPending = false;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

unsigned long applyRandomness(unsigned long baseValue) {
  long variation = (long)(baseValue * RANDOMNESS_PERCENT / 100);
  long result = (long)baseValue + random(-variation, variation + 1);
  if (result < (long)MIN_CLAMP_MS) result = MIN_CLAMP_MS;
  return (unsigned long)result;
}

String formatDuration(unsigned long ms) {
  float sec = ms / 1000.0;
  if (sec < 10) {
    return String(sec, 1) + "s";
  } else {
    return String((int)sec) + "s";
  }
}

String formatUptime(unsigned long ms) {
  unsigned long secs = ms / 1000;
  unsigned long mins = secs / 60;
  unsigned long hrs = mins / 60;
  
  secs %= 60;
  mins %= 60;
  
  char buf[12];
  sprintf(buf, "%02lu:%02lu:%02lu", hrs, mins, secs);
  return String(buf);
}

// ============================================================================
// FLASH STORAGE
// ============================================================================

void saveSettings() {
  settings.checksum = calcChecksum();
  
  if (InternalFS.exists(SETTINGS_FILE)) {
    InternalFS.remove(SETTINGS_FILE);
  }
  
  settingsFile.open(SETTINGS_FILE, FILE_O_WRITE);
  if (settingsFile) {
    settingsFile.write((uint8_t*)&settings, sizeof(Settings));
    settingsFile.close();
    Serial.println("Settings saved to flash");
  } else {
    Serial.println("Failed to save settings");
  }
}

void loadSettings() {
  InternalFS.begin();
  
  if (InternalFS.exists(SETTINGS_FILE)) {
    settingsFile.open(SETTINGS_FILE, FILE_O_READ);
    if (settingsFile) {
      settingsFile.read((uint8_t*)&settings, sizeof(Settings));
      settingsFile.close();
      
      // Validate
      if (settings.magic == SETTINGS_MAGIC && settings.checksum == calcChecksum()) {
        Serial.println("Settings loaded from flash");
        
        // Bounds check
        if (settings.keyIntervalMin < VALUE_MIN_MS) settings.keyIntervalMin = VALUE_MIN_MS;
        if (settings.keyIntervalMax > VALUE_MAX_MS) settings.keyIntervalMax = VALUE_MAX_MS;
        if (settings.mouseJiggleDuration < VALUE_MIN_MS) settings.mouseJiggleDuration = VALUE_MIN_MS;
        if (settings.mouseIdleDuration < VALUE_MIN_MS) settings.mouseIdleDuration = VALUE_MIN_MS;
        if (settings.selectedKeyIndex >= NUM_KEYS) settings.selectedKeyIndex = 0;
        
        return;
      } else {
        Serial.println("Settings corrupted, using defaults");
      }
    }
  } else {
    Serial.println("No settings file, using defaults");
  }
  
  loadDefaults();
}

// ============================================================================
// INTERRUPT HANDLER
// ============================================================================

void encoderISR() {
  static uint8_t prevState = 0;
  uint8_t state = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
  static const int8_t transitions[] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
  int8_t delta = transitions[(prevState << 2) | state];
  encoderPos += delta;
  prevState = state;
}

// ============================================================================
// BLE CALLBACKS
// ============================================================================

void connect_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  char central_name[32] = {0};
  conn->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to: ");
  Serial.println(central_name);
  deviceConnected = true;
  conn->requestConnectionParameter(12);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  Serial.print("Disconnected, reason: 0x");
  Serial.println(reason, HEX);
  deviceConnected = false;
}

// ============================================================================
// SLEEP
// ============================================================================

void enterDeepSleep() {
  Serial.println("\n*** ENTERING DEEP SLEEP ***");
  Serial.flush();
  
  // Save settings before sleep
  saveSettings();
  
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.setCursor(30, 20);
    display.println("SLEEPING...");
    display.setCursor(20, 38);
    display.println("Press btn to wake");
    display.display();
    delay(SLEEP_DISPLAY_MS);
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  
  digitalWrite(PIN_LED, LOW);
  Bluefruit.Advertising.stop();
  NRF_UARTE0->ENABLE = 0;
  NRF_TWIM0->ENABLE = 0;
  
  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A));
  detachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B));
  
  nrf_gpio_cfg_input(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_sense_set(PIN_FUNC_BTN_NRF, NRF_GPIO_PIN_SENSE_LOW);
  
  delay(100);
  while (digitalRead(PIN_FUNC_BTN) == LOW) delay(10);
  delay(100);
  
  sd_power_system_off();
  while(1) { }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  
  uint32_t t = millis();
  while (!Serial && (millis() - t < 2000)) delay(10);
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║   GHOST OPERATOR - BLE Jiggler            ║");
  Serial.println("║   Version " VERSION " - Encoder Menu + Flash     ║");
  Serial.println("║   TARS: Humor 80% | Honesty 100%          ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.println();
  
  loadSettings();
  setupPins();
  setupDisplay();
  setupBLE();
  readBattery();
  
  // Seed RNG
  uint32_t seed = analogRead(A0) ^ (millis() << 16) ^ NRF_FICR->DEVICEADDR[0];
  randomSeed(seed);
  
  scheduleNextKey();
  scheduleNextMouseState();
  
  startTime = millis();
  lastModeActivity = millis();
  
  Serial.println("Setup complete.");
  Serial.println("Short press func btn = cycle modes");
  Serial.println("Long press func btn = sleep");
}

void setupPins() {
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
  pinMode(PIN_FUNC_BTN, INPUT_PULLUP);
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoderISR, CHANGE);
  
  NRF_POWER->DCDCEN = 1;
  
  Serial.println("[OK] Pins configured");
}

void setupDisplay() {
  Wire.begin();
  Wire.setClock(400000);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[WARN] Display not found");
    displayInitialized = false;
    return;
  }
  
  displayInitialized = true;
  Serial.println("[OK] Display initialized");
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.println("GHOST OPERATOR");
  display.setCursor(48, 28);
  display.print("v");
  display.println(VERSION);
  display.setCursor(22, 44);
  display.println("Encoder + Flash");
  display.display();
  delay(1500);
}

void setupBLE() {
  Serial.println("[...] Initializing Bluetooth...");
  
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);
  
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  
  bledis.setManufacturer("TARS Industries");
  bledis.setModel("Ghost Operator v1.0.0");
  bledis.setSoftwareRev(VERSION);
  bledis.begin();
  
  blehid.begin();
  startAdvertising();
  
  Serial.println("[OK] BLE initialized");
}

void startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();
  
  if (sleepPending) {
    enterDeepSleep();
  }
  
  handleSerialCommands();
  handleEncoder();
  handleButtons();
  
  // Auto-return to NORMAL mode after timeout
  if (currentMode != MODE_NORMAL && (now - lastModeActivity > MODE_TIMEOUT_MS)) {
    currentMode = MODE_NORMAL;
    saveSettings();  // Save when leaving settings
  }
  
  // Battery monitoring
  if (now - lastBatteryRead >= BATTERY_READ_MS) {
    readBattery();
    lastBatteryRead = now;
  }
  
  // Jiggler logic runs in background regardless of mode
  if (deviceConnected) {
    // Keystroke logic
    if (keyEnabled && AVAILABLE_KEYS[settings.selectedKeyIndex].keycode != 0) {
      if (now - lastKeyTime >= currentKeyInterval) {
        sendKeystroke();
        lastKeyTime = now;
        scheduleNextKey();
      }
    }
    
    // Mouse logic
    if (mouseEnabled) {
      handleMouseStateMachine(now);
    }
  }
  
  // Display update
  if (displayInitialized) {
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
      updateDisplay();
      lastDisplayUpdate = now;
    }
  }
  
  // LED blink
  static unsigned long lastBlink = 0;
  if (deviceConnected && (now - lastBlink >= 1000)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  } else if (!deviceConnected && (now - lastBlink >= 200)) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    lastBlink = now;
  }
  
  delay(5);
}

// ============================================================================
// SCHEDULING
// ============================================================================

void scheduleNextKey() {
  // Random between min and max (no additional randomness)
  if (settings.keyIntervalMax > settings.keyIntervalMin) {
    currentKeyInterval = settings.keyIntervalMin + 
      random(settings.keyIntervalMax - settings.keyIntervalMin + 1);
  } else {
    currentKeyInterval = settings.keyIntervalMin;
  }
}

void scheduleNextMouseState() {
  if (mouseState == MOUSE_IDLE) {
    currentMouseJiggle = applyRandomness(settings.mouseJiggleDuration);
  } else {
    currentMouseIdle = applyRandomness(settings.mouseIdleDuration);
  }
}

// ============================================================================
// MOUSE STATE MACHINE
// ============================================================================

void handleMouseStateMachine(unsigned long now) {
  unsigned long elapsed = now - lastMouseStateChange;
  
  switch (mouseState) {
    case MOUSE_IDLE:
      if (elapsed >= currentMouseIdle) {
        mouseState = MOUSE_JIGGLING;
        lastMouseStateChange = now;
        lastMouseStep = now;
        mouseNetX = 0;
        mouseNetY = 0;
        pickNewDirection();
        scheduleNextMouseState();
      }
      break;
      
    case MOUSE_JIGGLING:
      if (elapsed >= currentMouseJiggle) {
        returnMouseToOrigin();
        mouseState = MOUSE_IDLE;
        lastMouseStateChange = now;
        scheduleNextMouseState();
      } else {
        if (now - lastMouseStep >= MOUSE_MOVE_STEP_MS) {
          if (random(100) < 15) pickNewDirection();
          blehid.mouseMove(currentMouseDx, currentMouseDy);
          mouseNetX += currentMouseDx;
          mouseNetY += currentMouseDy;
          lastMouseStep = now;
        }
      }
      break;
  }
}

void pickNewDirection() {
  int dir = random(NUM_DIRS);
  currentMouseDx = MOUSE_DIRS[dir][0];
  currentMouseDy = MOUSE_DIRS[dir][1];
}

void returnMouseToOrigin() {
  while (abs(mouseNetX) > 0 || abs(mouseNetY) > 0) {
    int8_t dx = 0, dy = 0;
    if (mouseNetX > 0) { dx = -min((int32_t)5, mouseNetX); mouseNetX += dx; }
    else if (mouseNetX < 0) { dx = min((int32_t)5, -mouseNetX); mouseNetX += dx; }
    if (mouseNetY > 0) { dy = -min((int32_t)5, mouseNetY); mouseNetY += dy; }
    else if (mouseNetY < 0) { dy = min((int32_t)5, -mouseNetY); mouseNetY += dy; }
    if (dx != 0 || dy != 0) {
      blehid.mouseMove(dx, dy);
      delay(5);
    } else break;
  }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void handleEncoder() {
  int change = encoderPos - lastEncoderPos;
  
  if (abs(change) >= 4) {  // Full detent
    int direction = (change > 0) ? 1 : -1;
    lastEncoderPos = encoderPos;
    lastModeActivity = millis();
    
    switch (currentMode) {
      case MODE_NORMAL:
        // Select key
        settings.selectedKeyIndex = (settings.selectedKeyIndex + direction + NUM_KEYS) % NUM_KEYS;
        break;
        
      case MODE_KEY_MIN:
        adjustValue(&settings.keyIntervalMin, direction);
        // Keep min <= max
        if (settings.keyIntervalMin > settings.keyIntervalMax) {
          settings.keyIntervalMax = settings.keyIntervalMin;
        }
        break;
        
      case MODE_KEY_MAX:
        adjustValue(&settings.keyIntervalMax, direction);
        // Keep max >= min
        if (settings.keyIntervalMax < settings.keyIntervalMin) {
          settings.keyIntervalMin = settings.keyIntervalMax;
        }
        break;
        
      case MODE_MOUSE_JIG:
        adjustValue(&settings.mouseJiggleDuration, direction);
        break;
        
      case MODE_MOUSE_IDLE:
        adjustValue(&settings.mouseIdleDuration, direction);
        break;
        
      default:
        break;
    }
  }
}

void adjustValue(uint32_t* value, int direction) {
  long newVal = (long)*value + (direction * (long)VALUE_STEP_MS);
  if (newVal < (long)VALUE_MIN_MS) newVal = VALUE_MIN_MS;
  if (newVal > (long)VALUE_MAX_MS) newVal = VALUE_MAX_MS;
  *value = (uint32_t)newVal;
}

void handleButtons() {
  static bool lastEncBtn = HIGH;
  static unsigned long lastEncPress = 0;
  const unsigned long DEBOUNCE = 200;
  
  unsigned long now = millis();
  bool encBtn = digitalRead(PIN_ENCODER_BTN);
  bool funcBtn = digitalRead(PIN_FUNC_BTN);
  
  // Encoder button - toggle keys or mouse depending on mode
  if (encBtn == LOW && lastEncBtn == HIGH && (now - lastEncPress > DEBOUNCE)) {
    lastEncPress = now;
    lastModeActivity = now;
    
    if (currentMode == MODE_NORMAL || currentMode == MODE_KEY_MIN || currentMode == MODE_KEY_MAX) {
      keyEnabled = !keyEnabled;
      Serial.print("Keys: ");
      Serial.println(keyEnabled ? "ON" : "OFF");
    } else {
      mouseEnabled = !mouseEnabled;
      Serial.print("Mouse: ");
      Serial.println(mouseEnabled ? "ON" : "OFF");
    }
  }
  lastEncBtn = encBtn;
  
  // Function button - short = next mode, long = sleep
  if (funcBtn == LOW) {
    if (!funcBtnWasPressed) {
      funcBtnPressStart = now;
      funcBtnWasPressed = true;
    } else {
      if (now - funcBtnPressStart >= LONG_PRESS_MS) {
        sleepPending = true;
        funcBtnWasPressed = false;
      }
    }
  } else {
    if (funcBtnWasPressed) {
      unsigned long holdTime = now - funcBtnPressStart;
      if (holdTime < LONG_PRESS_MS && holdTime > 50) {
        // Short press - cycle mode
        if (currentMode != MODE_NORMAL) {
          saveSettings();  // Save when leaving a settings mode
        }
        currentMode = (UIMode)((currentMode + 1) % MODE_COUNT);
        lastModeActivity = now;
        Serial.print("Mode: ");
        Serial.println(MODE_NAMES[currentMode]);
      }
      funcBtnWasPressed = false;
    }
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'h':
        Serial.println("\n=== Commands ===");
        Serial.println("s - Status");
        Serial.println("z - Sleep");
        Serial.println("d - Dump settings");
        break;
      case 's':
        printStatus();
        break;
      case 'z':
        sleepPending = true;
        break;
      case 'd':
        Serial.println("\n=== Settings ===");
        Serial.print("Key MIN: "); Serial.println(settings.keyIntervalMin);
        Serial.print("Key MAX: "); Serial.println(settings.keyIntervalMax);
        Serial.print("Mouse Jig: "); Serial.println(settings.mouseJiggleDuration);
        Serial.print("Mouse Idle: "); Serial.println(settings.mouseIdleDuration);
        Serial.print("Key Index: "); Serial.println(settings.selectedKeyIndex);
        break;
    }
  }
}

void printStatus() {
  Serial.println("\n=== Status ===");
  Serial.print("Mode: "); Serial.println(MODE_NAMES[currentMode]);
  Serial.print("Connected: "); Serial.println(deviceConnected ? "YES" : "NO");
  Serial.print("Key: "); Serial.print(AVAILABLE_KEYS[settings.selectedKeyIndex].name);
  Serial.print(" ("); Serial.print(keyEnabled ? "ON" : "OFF"); Serial.println(")");
  Serial.print("Mouse: "); Serial.println(mouseEnabled ? "ON" : "OFF");
  Serial.print("Mouse state: "); Serial.println(mouseState == MOUSE_JIGGLING ? "JIG" : "IDLE");
  Serial.print("Battery: "); Serial.print(batteryPercent); Serial.println("%");
}

// ============================================================================
// HID
// ============================================================================

void sendKeystroke() {
  const KeyDef& key = AVAILABLE_KEYS[settings.selectedKeyIndex];
  if (key.keycode == 0) return;
  
  if (key.isModifier) {
    blehid.keyPress(key.keycode);
    delay(30);
    blehid.keyRelease();
  } else {
    blehid.keyPress(key.keycode);
    delay(50);
    blehid.keyRelease();
  }
}

// ============================================================================
// BATTERY
// ============================================================================

void readBattery() {
  digitalWrite(PIN_VBAT_ENABLE, HIGH);
  delay(1);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_VBAT);
    delay(1);
  }
  
  digitalWrite(PIN_VBAT_ENABLE, LOW);
  analogReference(AR_DEFAULT);
  analogReadResolution(10);
  
  float mv = (sum / 8) * VBAT_MV_PER_LSB * VBAT_DIVIDER;
  batteryVoltage = mv / 1000.0;
  batteryPercent = constrain((int)(((mv - VBAT_MIN_MV) / (VBAT_MAX_MV - VBAT_MIN_MV)) * 100), 0, 100);
}

// ============================================================================
// DISPLAY
// ============================================================================

void updateDisplay() {
  if (!displayInitialized) return;
  
  display.clearDisplay();
  
  if (currentMode == MODE_NORMAL) {
    drawNormalMode();
  } else {
    drawSettingsMode();
  }
  
  display.display();
}

void drawNormalMode() {
  unsigned long now = millis();
  
  // === Header ===
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("GHOST ");
  if (deviceConnected) {
    display.print("[LINK]");
  } else {
    static bool blink = false;
    blink = !blink;
    display.print(blink ? "[SCAN]" : "[    ]");
  }
  
  // Battery percentage (right aligned)
  String batStr = String(batteryPercent) + "%";
  int batX = 128 - (batStr.length() * 6);
  display.setCursor(batX, 0);
  display.print(batStr);
  
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  
  // === Key section ===
  display.setCursor(0, 12);
  display.print("KEY:");
  display.print(AVAILABLE_KEYS[settings.selectedKeyIndex].name);
  display.print(" ");
  display.print(formatDuration(settings.keyIntervalMin));
  display.print("-");
  display.print(formatDuration(settings.keyIntervalMax));
  
  // ON/OFF right aligned
  display.setCursor(110, 12);
  display.print(keyEnabled ? "ON" : "--");
  
  // Key progress bar (countdown)
  unsigned long keyElapsed = now - lastKeyTime;
  int keyRemaining = max(0L, (long)currentKeyInterval - (long)keyElapsed);
  int keyProgress = 0;
  if (currentKeyInterval > 0) {
    keyProgress = map(keyRemaining, 0, currentKeyInterval, 0, 100);
  }
  
  display.drawRect(0, 21, 100, 7, SSD1306_WHITE);
  int keyBarWidth = map(keyProgress, 0, 100, 0, 98);
  if (keyBarWidth > 0) {
    display.fillRect(1, 22, keyBarWidth, 5, SSD1306_WHITE);
  }
  
  // Time remaining
  String keyTimeStr = formatDuration(keyRemaining);
  display.setCursor(102, 21);
  display.print(keyTimeStr);
  
  display.drawFastHLine(0, 29, 128, SSD1306_WHITE);
  
  // === Mouse section ===
  display.setCursor(0, 32);
  display.print("MOUSE ");
  
  // State indicator
  if (mouseState == MOUSE_JIGGLING) {
    display.print("[JIG]");
  } else {
    display.print("[---]");
  }
  
  display.print(" ");
  display.print(formatDuration(settings.mouseJiggleDuration));
  display.print("/");
  display.print(formatDuration(settings.mouseIdleDuration));
  
  // ON/OFF
  display.setCursor(110, 32);
  display.print(mouseEnabled ? "ON" : "--");
  
  // Mouse progress bar
  unsigned long mouseElapsed = now - lastMouseStateChange;
  unsigned long mouseDuration = (mouseState == MOUSE_JIGGLING) ? currentMouseJiggle : currentMouseIdle;
  int mouseRemaining = max(0L, (long)mouseDuration - (long)mouseElapsed);
  int mouseProgress = 0;
  if (mouseDuration > 0) {
    mouseProgress = map(mouseRemaining, 0, mouseDuration, 0, 100);
  }
  
  display.drawRect(0, 41, 100, 7, SSD1306_WHITE);
  int mouseBarWidth = map(mouseProgress, 0, 100, 0, 98);
  if (mouseBarWidth > 0) {
    display.fillRect(1, 42, mouseBarWidth, 5, SSD1306_WHITE);
  }
  
  // Time remaining
  String mouseTimeStr = formatDuration(mouseRemaining);
  display.setCursor(102, 41);
  display.print(mouseTimeStr);
  
  display.drawFastHLine(0, 50, 128, SSD1306_WHITE);
  
  // === Uptime ===
  display.setCursor(0, 54);
  display.print("Up: ");
  display.print(formatUptime(now - startTime));
  
  // Activity spinner
  static uint8_t frame = 0;
  frame = (frame + 1) % 4;
  const char* spinner = "|/-\\";
  if (deviceConnected) {
    display.setCursor(122, 54);
    display.print(spinner[frame]);
  }
}

void drawSettingsMode() {
  // === Header ===
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MODE: ");
  display.print(MODE_NAMES[currentMode]);
  
  // Show what encoder button toggles
  display.setCursor(100, 0);
  if (currentMode == MODE_KEY_MIN || currentMode == MODE_KEY_MAX) {
    display.print(keyEnabled ? "[K]" : "[k]");
  } else {
    display.print(mouseEnabled ? "[M]" : "[m]");
  }
  
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  
  // === Value display ===
  uint32_t value = 0;
  switch (currentMode) {
    case MODE_KEY_MIN:   value = settings.keyIntervalMin; break;
    case MODE_KEY_MAX:   value = settings.keyIntervalMax; break;
    case MODE_MOUSE_JIG: value = settings.mouseJiggleDuration; break;
    case MODE_MOUSE_IDLE: value = settings.mouseIdleDuration; break;
    default: break;
  }
  
  // Big centered value
  display.setTextSize(2);
  String valStr = ">>> " + formatDuration(value) + " <<<";
  int valWidth = valStr.length() * 12;
  int valX = (128 - valWidth) / 2;
  display.setCursor(max(0, valX), 20);
  display.print(valStr);
  
  // Progress bar showing position in range
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("0.5s");
  display.setCursor(104, 40);
  display.print("30s");
  
  int barProgress = map(value, VALUE_MIN_MS, VALUE_MAX_MS, 0, 100);
  display.drawRect(0, 48, 128, 7, SSD1306_WHITE);
  int barWidth = map(barProgress, 0, 100, 0, 126);
  if (barWidth > 0) {
    display.fillRect(1, 49, barWidth, 5, SSD1306_WHITE);
  }
  
  // Instructions
  display.setCursor(0, 57);
  display.print("Turn encoder to adjust");
}
