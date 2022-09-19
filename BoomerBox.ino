\#include <VS1053.h>
#include <SPI.h>

#include <Adafruit_GFX.h>



#include <WiFi.h>
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4

// Default volume
#define DEFAULT_VOLUME  80


// LCD constants
char ESC = 0xFE; // Used to issue command
const char CLS = 0x51;
const char CURSOR = 0x45;
const char LINE1_POS = 0x00;
const char LINE2_POS = 0x40;
const char LINE3_POS = 0x14;
const char LINE4_POS = 0x54;
const String NOT_FOUND = "n/a";

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
WiFiClient client;

// WiFi settings example, substitute your own
const char *ssid = "MyCharterWiFi49-2G";
const char *password = "LazarusRises971";

// Button
//const int buttonPin = 25;     // the number of the pushbutton pin
//int buttonState = 0;         // variable for reading the pushbutton status
//int lastState = LOW;


// The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
const int BUFFER_SIZE = 64;
uint8_t mp3buff[BUFFER_SIZE];

// *** VOLUME  ***
const int VOLUME_KNOB_INPUT = 34; // Must use ADC1, NOT ADC2 when using WiFi on ESP32
bool soundCalibrated = false;
// Check volume knob value ever 1/2 second when calibrating
int SOUND_CALIBRATE_INTERVAL = 500;
// Check volume 20 times to get high and low values
int NUM_SOUND_CALIBRATIONS = 20;
// For my esp32 dev kit, verified using one-time calibrateSound()
int knobHigh = 4095;
int knobLow = 0; 
int lastVolumeKnobInput;nput;
const int KNOB_THRESHOLD = 100; // Must change by 100 before changing volume
const int CHECK_MILLIS = 1000; // Check if volume has changed every 1 second, be patient!
const int SAMPLE_CHECKS = 10; // Don't rely on a single reading
const int SAMPLE_PAUSE = 10; // Small delay between measuring
int lastVolCheckMillis = 0;
int lastStationCheckMillis = 0;
bool firstRun = true;
// Some stations
const int NUM_STATIONS = 9;
//const char VOL_CHAR = '*';

// These are stations that were working for me in January 2020 - Tom Dison
const char *station[NUM_STATIONS] = {"UK Oldies", "Magic Old", "1FM Blues", "SomaFM", "Hotmix Rk",
  "Blues Cove","Rad Volta", "Lf Praise","DJ Trance"};
const char *host[NUM_STATIONS] = {"149.255.59.162","airspectrum.cdnstream1.com","sc2b-sjc.1.fm", "ice1.somafm.com", 
  "streamingads.hotmixradio.fm", "radio.streemlion.com","air.radiolla.com","radio.cgbc.org",
  "globaldjbroadcast.cc"};
const char *path[NUM_STATIONS] = {"/1","/1261_192","/","/u80s-128-mp3", "/hotmixradio-rock-128.mp3",
  "/stream","/volta.32k.mp3","/3.mp3","/world96k"};
int   port[NUM_STATIONS] = {8062,8000,8030,80,80,2070,80,8000,8000};
int curStation = 0;
int curVolume = DEFAULT_VOLUME;
int curDisplayVolume = DEFAULT_VOLUME;
const int SCREEN_ROWS = 4;
const int SCREEN_COLS = 20;
const int SCREEN_CHARS = 80;

char getLinePos(int line_num) {
  switch(line_num) {
    case 1:
      return LINE1_POS;
    case 2:
      return LINE2_POS;
    case 3:
      return LINE3_POS;
    case 4:
      return LINE4_POS;
    default:
      return LINE1_POS; // Default to line 1
  }
}
void clearScreen() {
  Serial2.write(ESC);
  Serial2.write(CLS);  
  delay(10);
}

// Lines are 1 based
void printScreen(const char* line, bool cls = true, int whichLine = 1) {
  // Clear screen
  if(cls) {
    clearScreen();
  }

  Serial2.write(ESC);
  Serial2.write(CURSOR);
  Serial2.write(getLinePos(whichLine));
  delay(10);
  Serial2.print(line);
}

void setupScreen() {
  // Initialize serial
  Serial.println("Setting up screen.");
  
  // Setup LCD
    // Initialize LCD module
  Serial2.write(ESC);
  Serial2.write(0x41);
  Serial2.write(ESC);
  Serial2.write(0x51);
  
  // Set Contrast
  Serial2.write(ESC);
  Serial2.write(0x52);
  Serial2.write(40);
  
  // Set Backlight
  Serial2.write(ESC);
  Serial2.write(0x53);
  Serial2.write(8);

  Serial2.write(ESC);
  Serial2.write(CLS);
  
  Serial2.print("NKC Electronics");
  
  // Set cursor line 2, column 0
  Serial1.write(ESC);
  Serial1.write(CURSOR);
  Serial1.write(LINE2_POS);
  
  Serial2.print("20x4 Serial LCD");
  
  delay(1000);

}

// Only used to verify high and low values
void calibrateSound() {
  int readVal = 0;
  
  for(int i = 0; i < NUM_SOUND_CALIBRATIONS; i++) {
    readVal = analogRead(VOLUME_KNOB_INPUT);

    Serial.println("Cal readVal = " + String(readVal));
  
    if(readVal > knobHigh) {
      knobHigh = readVal;
    }

    if(readVal < knobLow) {
      knobLow = readVal;
    }

    delay(SOUND_CALIBRATE_INTERVAL);
  }

  Serial.println("Sound high: " + String(knobHigh));
  Serial.println("Sound low: " + String(knobLow));
  lastVolumeKnobInput = readVal;
}

int calculateSoundVolume() {
  int readVolume = analogRead(VOLUME_KNOB_INPUT);

  Serial.println("readVolume = " + String(readVolume));

  if(soundHigh == soundLow) {
    return DEFAULT_VOLUME;
  }
  float calcVolume = ((1.0 * readVolume - soundLow) / (1.0 * soundHigh - soundLow)) * 100.0;

  if(calcVolume > 100) {
    Serial.println("Needs volume calibration. Volume > 100: " + String(calcVolume));
    return 100;
  }

  if(calcVolume < 0) {
    Serial.println("Needs volume calibration. Volume < 0 : " + String(calcVolume));
    return 0;
  }

  curDisplayVolume = int(calcVolume);
  
  // Volumes below 50 don't do anything on my system
  // I am going to map between 50 - 100, especially with logrithmic knob

  //Convert back to fraction
  calcVolume = calcVolume / 100;

  calcVolume = 50 + (calcVolume * 50);
  
  return int(calcVolume);
}



void updateScreen() {
  printScreen("#" + String(curStation) + " - " + String(station[curStation]),
  "Volume:"), String(curDisplayVolume).c_str(), null);
}

void connectToStation() {
  Serial.print("connecting to station: ");
  Serial.println("#" + String(curStation) + " - " + String(station[curStation]));

  if (!client.connect(host[curStation], port[curStation])) {
      Serial.println("Connection failed");
      return;
  }

  Serial.print("Requesting stream: ");
  Serial.println(path[curStation]);

  client.print(String("GET ") + path[curStation] + " HTTP/1.1\r\n" +
               "Host: " + host[curStation] + "\r\n" +
               "Connection: close\r\n\r\n");
  
  updateScreen();
}

void setup() {
    Serial.begin(115200);
    //calibrateSound();
    setupScreen();
    pinMode(VOLUME_KNOB_INPUT, INPUT_PULLUP);
    //pinMode(STATION_KNOB_INPUT, INPUT_PULLUP);  
    // Wait for VS1053 and PAM8403 to power up
    // otherwise the system might not start up correctly
    delay(1000);

    // This can be set in the IDE no need for ext library
    // system_update_cpu_freq(160);

    Serial.println("\n\nSimple Radio Node WiFi Radio");

    SPI.begin();
    delay(1000); // For MP3 Player
    Serial.println("SPI Started");
    player.begin();

    Serial.println("Player started");
    
    player.switchToMp3Mode();

    Serial.println("Switched to MP3 Mode");

    if(!soundCalibrated) {
      //calibrateSound();
    }

    curVolume = calculateSoundVolume();
    Serial.println("Calculated volume = " + String(curVolume) + "%");
    player.setVolume(curVolume);

    Serial.println("Volume is set");

    Serial.print("Connecting to SSID ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    curStation = calculateStation();
    connectToStation();
}

void checkSound() {
  int curMillis = millis();

  if(curMillis < lastVolCheckMillis // millis rolled over
      || (curMillis - lastVolCheckMillis) > CHECK_MILLIS) {

    int totalSampled = 0;
    for(int v = 0; v < SAMPLE_CHECKS; v++) {
      totalSampled += analogRead(VOLUME_KNOB_INPUT);
      delay(SAMPLE_PAUSE);
    }

    int curVol = totalSampled / SAMPLE_CHECKS;
    
    lastVolCheckMillis = curMillis;
    
    if(abs(curVol - lastVolumeKnobInput) >= KNOB_THRESHOLD) {
      curVolume = calculateSoundVolume();
      Serial.println("Calculated volume = " + String(curVolume) + "%");
      player.setVolume(curVolume);
      updateScreen();
      lastVolumeKnobInput = curVol;
    }
  }
}

void loop() {
  if(!firstRun) {
    checkSound();
    //checkStation();
  } else {
    firstRun = false;
  }
  
  if (!client.connected()) {
    connectToStation();
  }

  if (client.available() > 0) {
      // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
      uint8_t bytesread = client.read(mp3buff, BUFFER_SIZE);
      player.playChunk(mp3buff, bytesread);
  }
}
