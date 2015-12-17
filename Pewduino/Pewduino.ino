#include <SPI.h>
#include <Adafruit_WS2801.h>
#include <IRLib.h>
#include <VirtualWire.h>
#include <U8glib.h>

#define PIN_IR_RECEIVER     2
#define PIN_IR_TRANSMITTER  3 //actually you can't set this pin, it is per default pin three
#define PIN_RF_RECEIVER     4
#define PIN_RF_TRANSMITTER  5
#define PIN_WS2801_DATA     6
#define PIN_WS2801_CLOCK    7
#define PIN_LASER_POINTER   8
#define PIN_SPEAKER         9

#define PIN_TRIGGER        A0
#define PIN_RELOAD         A1
#define PIN_MARKER_CHANGE  A2
#define PIN_TEAM_CHANGE    A3
#define PIN_START          A6
#define PIN_SHIELD         A7

#define LIGHT_STRIKE_DATA_LENGTH    32
#define LIGHT_STRIKE_RAW_LENGTH     66
#define LIGHT_STRIKE_HEADER_MARK  6750
#define LIGHT_STRIKE_HEADER_SPACE    0
#define LIGHT_STRIKE_MARK_ONE      900
#define LIGHT_STRIKE_MARK_ZERO     900
#define LIGHT_STRIKE_SPACE_ONE    3700
#define LIGHT_STRIKE_SPACE_ZERO    900
#define LIGHT_STRIKE_KHZ            38
#define LIGHT_STRIKE_USE_STOP     true
#define LIGHT_STRIKE_MAX_EXTENT      0

#define MARKER_COUNT  16
#define START_MARKER   0
#define TEAM_COUNT     4
#define START_TEAM     1
#define LIGHT_UP_LASERPOINTER 5
#define LIGHT_UP_LED 1
#define LIGHT_UP_LED_HIT 3000
#define WS2801_STRIP_COUNT 5

#define MAX_ENERGY   120

#define WHITE  0xFFFFFF
#define BLACK  0x000000

#define BLUE   0x0000FF
#define RED    0xFF0000
#define YELLOW 0xFFFF00
#define GREEN  0x00FF00

//as we are using the internal pull up resistors, a key press is a low signal and a release key is a high signal, i will define these compiler variables to make the code easier to read
#define KEY_PRESSED LOW
#define KEY_RELEASED HIGH

IRrecv receiver(PIN_IR_RECEIVER);
IRsend transmitter;
IRdecodeBase decoder;
U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NONE);  
Adafruit_WS2801 strip = Adafruit_WS2801(WS2801_STRIP_COUNT, PIN_WS2801_DATA, PIN_WS2801_CLOCK);

struct Teams {
  unsigned int code;
  String name;
  uint32_t color;
};

const Teams teams[] = {
  //HEXCode, DisplayName, Color
  { 0x0700, "Blue", BLUE },
  { 0x0400, "Red", RED },
  { 0x0500, "Yellow", YELLOW },
  { 0x0600, "Green", GREEN },
};

struct Markers {
  unsigned int code;
  char type;
  boolean enabled;
  String name;
  int damage;
  int charges;
  int bounceDelay;
  int reloadTime;
};

const Markers markers[] = {
  //HexCode, Pistol/Rifle, Enabled Flag, DisplayName, -Damage/+Heal, Ammo, Button Debouce (ms), Reload Time (ms)
  { 0x0102, 'P', true,  "Laserstrike",    -10, 12, 1000, 1000 },
  { 0x0202, 'P', true,  "Stealthstrike",  -10, 12, 100, 1000 },
  { 0x0303, 'P', true,  "Pulsestrike",    -15, 10, 100, 50 },
  { 0x0406, 'P', true,  "Sonicstrike",    -40, 4, 100, 1000 },
  { 0x0502, 'R', true,  "Laserstrike",    -10, 12, 100, 1000 },
  { 0x0602, 'R', true,  "Stealthstrike",  -10, 12, 100, 1000 },
  { 0x0703, 'R', true,  "Pulsestrike",    -15, 10, 100, 1000 },
  { 0x0806, 'R', true,  "Railstrike",     -30, 6, 100, 1000 },
  { 0x0908, 'R', true,  "Sonicstrike",    -40, 4, 100, 1000 },
  { 0x0E18, 'S', false, "Bomb",          -120, 1, 100, 1000 },
  { 0x0A0C, 'S', false, "Optic",          -60, 2, 100, 1000 },
  { 0x0F08, 'S', false, "Sentry",         -40, 3, 100, 1000 },
  { 0x0B12, 'S', false, "Refractor",      -60, 2, 100, 1000 },
  { 0x0C03, 'S', false, "Auto Strike",    -15, 8, 100, 1000 },
  { 0x0D06, 'S', false, "Launcher",       -30, 4, 100, 1000 },
  { 0x0800, 'S', false, "Medic",          +40, 3, 100, 1000 },
};

unsigned int currentTeam = START_TEAM;
unsigned int currentMarker = START_MARKER;
unsigned int currentEnergy = MAX_ENERGY;
unsigned int currentCharge = markers[START_MARKER].charges;

boolean updateDisplay = true;

unsigned long lastHit = 0;
unsigned long msSinceLastHit = 0;
unsigned long msSinceLastShot = 0;
unsigned long msSinceLastTick = 0;

byte reloadState = KEY_RELEASED;
byte triggerState = KEY_RELEASED;
byte markerChangeState = KEY_RELEASED;
byte teamChangeState = KEY_RELEASED;

byte lastTriggerState = KEY_RELEASED;

void setup() {
  pinMode(PIN_TRIGGER, INPUT_PULLUP);
  pinMode(PIN_RELOAD, INPUT_PULLUP);
  pinMode(PIN_SHIELD, INPUT_PULLUP);
  pinMode(PIN_MARKER_CHANGE, INPUT_PULLUP);
  pinMode(PIN_TEAM_CHANGE, INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);

  pinMode(PIN_LASER_POINTER, OUTPUT);

  receiver.enableIRIn();
  Serial.begin(9600);
  strip.begin();
  strip.show();
  start();
}

void setLEDColor(uint32_t c) {
  int i;
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

void start() {
  setTeam(START_TEAM);
  setMarker(START_MARKER);
  refreshDisplayValues();
}

void refreshDisplayValues() {
  if (updateDisplay == true) {
    updateDisplay = false;

    u8g.firstPage();
    do {
      u8g.setFont(u8g_font_6x10);
      u8g.setPrintPos(0, 10);
      u8g.print(teams[currentTeam].name);

      String temp = markers[currentMarker].name;
      u8g.setFont(u8g_font_5x7);
      u8g.drawStr(6, 25, F("Charges"));
      u8g.drawStr(78, 25, F("Energy"));

      u8g.setPrintPos((128 - ((temp.length() * 6) + 1)), 10);
      u8g.print(temp);
      u8g.drawStr(34, 62, F("Hit by "));
      u8g.setFont(u8g_font_9x15);
      u8g.drawStr(18, 38, "/");
      u8g.setPrintPos(0, 38);
      u8g.print(currentCharge);
      u8g.setPrintPos(27, 38);
      u8g.print(markers[currentMarker].charges);


      u8g.setPrintPos(60, 38);
      u8g.print(currentEnergy);
      u8g.setPrintPos(87, 38);
      u8g.print("/120"); //MAX_ENERGY
    } while (u8g.nextPage());
  }
}


void loop() {
  //Serial.println(millis()-msSinceLastTick);
  msSinceLastTick = millis();
 
  if (currentEnergy > 0) {
    if (receiver.GetResults(&decoder)) {
      decoder.decodeGeneric(LIGHT_STRIKE_RAW_LENGTH, 
                            LIGHT_STRIKE_HEADER_MARK, 
                            LIGHT_STRIKE_HEADER_SPACE, 
                            LIGHT_STRIKE_MARK_ONE, 
                            LIGHT_STRIKE_MARK_ZERO, 
                            LIGHT_STRIKE_SPACE_ONE, 
                            LIGHT_STRIKE_SPACE_ZERO);
      lastHit = decoder.value;
      receiver.resume();
    }

    triggerState = digitalRead(PIN_TRIGGER);
    reloadState = digitalRead(PIN_RELOAD);
    markerChangeState = digitalRead(PIN_MARKER_CHANGE);
    teamChangeState = digitalRead(PIN_TEAM_CHANGE);

    if (lastHit != 0) {
      if (getTeamCodeFromHit(lastHit) != teams[currentTeam].code) {
        setLEDColor(getTeamColorByCode(getTeamCodeFromHit(lastHit)));
        currentEnergy = currentEnergy + getMarkerDamageByCode(getMarkerCodeFromHit(lastHit));
        lastHit = 0;
        msSinceLastHit = millis();
        
        updateDisplay = true;
      }
    }
    if (triggerState == KEY_PRESSED) {
      if (lastTriggerState == KEY_RELEASED) {
        shot(teams[currentTeam].code, markers[currentMarker].code);
      } else {
        if (millis() > (msSinceLastShot + markers[currentMarker].bounceDelay)) {
          shot(teams[currentTeam].code, markers[currentMarker].code);
        }
      }
    }
    if (reloadState == KEY_PRESSED) {
      reload();
    }
    if (markerChangeState == KEY_PRESSED) {
      changeMarker();
    }
    if (teamChangeState == KEY_PRESSED) {
      changeTeam();
    }
  }
  lastTriggerState = triggerState;
  refreshDisplayValues();
  refreshLights();
}

void refreshLights() {
  if (millis() > (msSinceLastHit + LIGHT_UP_LED_HIT)) {
    setLEDColor(teams[currentTeam].color);
    if (millis() > (msSinceLastShot + LIGHT_UP_LED)) {
      setLEDColor(teams[currentTeam].color);
    }
  }
  if (millis() > (msSinceLastShot + LIGHT_UP_LASERPOINTER)) {
    digitalWrite(PIN_LASER_POINTER, LOW);
  }
}

void shot(long teamCode, int markerCode) {
  if (currentCharge > 0) {
    transmitter.sendGeneric(teamCode + markerCode,
                            LIGHT_STRIKE_DATA_LENGTH,
                            LIGHT_STRIKE_HEADER_MARK,
                            LIGHT_STRIKE_HEADER_SPACE,
                            LIGHT_STRIKE_MARK_ONE,
                            LIGHT_STRIKE_MARK_ZERO,
                            LIGHT_STRIKE_SPACE_ONE,
                            LIGHT_STRIKE_SPACE_ZERO,
                            LIGHT_STRIKE_KHZ,
                            LIGHT_STRIKE_USE_STOP,
                            LIGHT_STRIKE_MAX_EXTENT);
    receiver.resume();
    currentCharge--;
    updateDisplay = true;
    digitalWrite(PIN_LASER_POINTER, HIGH);
    setLEDColor(WHITE);
    msSinceLastShot = millis();
  }
}


void reload() {
  currentCharge = markers[currentMarker].charges;
  updateDisplay = true;
}

void respawn() {
  currentEnergy = MAX_ENERGY;
  start();
}

void setTeam(int team) {
  currentTeam = team;
  setLEDColor(teams[team].color);
  updateDisplay = true;
}

void changeTeam() {
  if (currentTeam + 1 == TEAM_COUNT) {
    setTeam(0);
  } else {
    setTeam(currentTeam + 1);
  }
}

void setMarker(int marker) {
  currentMarker = marker;
  reload();
  updateDisplay = true;
}

void changeMarker() {
  if (currentMarker + 1 == MARKER_COUNT) {
    setMarker(0);
  } else {
    setMarker(currentMarker + 1);
  }
}

String getTeamNameByCode(long code) {
  for (byte i = 0; i < TEAM_COUNT; i++) {
    if (code == teams[i].code) {
      return teams[i].name;
    }
  }
  return "";
}

uint32_t getTeamColorByCode(long code) {
  for (byte i = 0; i < TEAM_COUNT; i++) {
    if (code == teams[i].code) {
      return teams[i].color;
    }
  }
  return 0xFFFFFF;
}


int getMarkerDamageByCode(unsigned int code) {
  for (int i = 0; i < MARKER_COUNT; i++) {
    if (code == markers[i].code) {
      return markers[i].damage;
    }
  }
  return 0;
}

String getMarkerNameByCode(unsigned int code) {
  for (int i = 0; i < MARKER_COUNT; i++) {
    if (code == markers[i].code) {
      return markers[i].name;
    }
  }
  return "";
}

unsigned int getTeamCodeFromHit(long shotValue) {
  return shotValue >> 16;
}

unsigned int getMarkerCodeFromHit(long shotValue) {
  return shotValue;
}


