#include <Adafruit_VS1053.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h> 


// Default volume
#define DEFAULT_VOLUME  100

// OLOED screen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VS1053_RESET   -1     // VS1053 reset pin (not used!)


#define VS_MOSI 35
#define VS_MISO 37
#define VS_SCK 36
// Feather ESP8266
#if defined(ESP8266)
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32) && !defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_NRF52832_FEATHER )
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin

// Feather M4, M0, 328, ESP32S2, nRF52840 or 32u4
#else
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

#endif


//Adafruit_VS1053 player(VS_MOSI, VS_MISO,  VS_SCK, 
// VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ);

  Adafruit_VS1053 player(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ);


WiFiClient client;

// WiFi settings example, substitute your own
const char *ssid = "MyCharterWiFi49-2G";
const char *password = "LazarusRises971";

// Button
const int buttonPin = 25;     // the number of the pushbutton pin
int buttonState = 0;         // variable for reading the pushbutton status
int lastState = LOW;

// The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
const int BUFFER_SIZE = 32;
PROGMEM uint8_t mp3buff[BUFFER_SIZE];

// *** VOLUME  ***
const int VOLUME_KNOB_INPUT = 34; // Must use ADC1, NOT ADC2 when using WiFi on ESP32
bool soundCalibrated = false;
// Check volume knob value ever 1/2 second when calibrating
int SOUND_CALIBRATE_INTERVAL = 500;
// Check volume 20 times to get high and low values
int NUM_SOUND_CALIBRATIONS = 20;
// For my esp32 dev kit, verified using one-time calibrateSound()
int soundHigh = 4095;
int soundLow = 0; 
int lastVolumeKnobInput;
const int VOL_KNOB_THRESHOLD = 100; // Must change by 100 before changing volume
const int VOL_CHECK_MILLIS = 1000; // Check if volume has changed every 1 second, be patient!
const int VOL_SAMPLE_CHECKS = 10; // Don't rely on a single reading
const int VOL_SAMPLE_PAUSE = 10; // Small delay between measuring
int lastVolCheckMillis = 0;
bool firstRun = true;
// Some stations
const int NUM_STATIONS = 9;
const char VOL_CHAR = '*';

// These are stations that were working for me in January 2020 - Tom Dison
const char *station[NUM_STATIONS] = {"AnonRadio", "Magic Old", "1FM Blues", "SomaFM", "Hotmix Rk",
  "Blues Cove","Rad Volta", "Lf Praise","DJ Trance"};
const char *host[NUM_STATIONS] = {"anonradio.net","airspectrum.cdnstream1.com","sc2b-sjc.1.fm", "ice1.somafm.com", 
  "streamingads.hotmixradio.fm", "radio.streemlion.com","air.radiolla.com","radio.cgbc.org",
  "globaldjbroadcast.cc"};
const char *path[NUM_STATIONS] = {"/anonradio","/1261_192","/","/u80s-128-mp3", "/hotmixradio-rock-128.mp3",
  "/stream","/volta.32k.mp3","/3.mp3","/world96k"};
int   port[NUM_STATIONS] = {8000,8000,8030,80,80,2070,80,8000,8000};
int curStation = 1;
int curVolume = DEFAULT_VOLUME;
int curDisplayVolume = DEFAULT_VOLUME;

const char* getVolumeChars() {
  String ret = "";
  int numChars = curDisplayVolume / 8;
  for(int i = 0; i < numChars; i++) {
    ret += VOL_CHAR;
  }

  return ret.c_str();
}


void setupScreen() {
  // Initialize serial
  Serial.println("Setting up screen.");
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds
 // Clear the buffer
  display.clearDisplay();
  
}

// Only used to verify high and low values
void calibrateSound() {
  int readVal = 0;
  
  for(int i = 0; i < NUM_SOUND_CALIBRATIONS; i++) {
    readVal = analogRead(VOLUME_KNOB_INPUT);

    Serial.println("Cal readVal = " + String(readVal));
  
    if(readVal > soundHigh) {
      soundHigh = readVal;
    }

    if(readVal < soundLow) {
      soundLow = readVal;
    }

    delay(SOUND_CALIBRATE_INTERVAL);
  }

  Serial.println("Sound high: " + String(soundHigh));
  Serial.println("Sound low: " + String(soundLow));
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
  display.clearDisplay();
  //

  Serial.print("Station:");
  Serial.println(station[curStation]);
  // Print stationstation[curStation], 
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor (1,1);
  display.println(station[curStation]);
  //display.println("");
  
  //display.setTextSize(1);
  // Draw volume bar
  int barLength = (curDisplayVolume / 100.0) * display.width();
  display.fillRect(0, 20, barLength, 12, SSD1306_WHITE);
  display.display();
}

void connectToStation() {
  Serial.print("connecting to station: ");
  Serial.println(station[curStation]);

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

void onButtonClick() {
  curStation++;

  if(NUM_STATIONS == curStation) {
    curStation = 0;
  }

  connectToStation();
}

void setup() {
    Serial.begin(115200);
    //calibrateSound();

    pinMode(VOLUME_KNOB_INPUT, INPUT_PULLUP);
  
    pinMode(buttonPin, INPUT);
    lastState = digitalRead(buttonPin);    
    // Wait for VS1053 and PAM8403 to power up
    // otherwise the system might not start up correctly
    delay(1000);

    // This can be set in the IDE no need for ext library
    // system_update_cpu_freq(160);

    Serial.println("\n\nSimple Radio Node WiFi Radio");

    setupScreen();

    SPI.begin();

    Serial.println("SPI Started");
    player.begin();

    Serial.println("Player started");
    

    if(!soundCalibrated) {
      //calibrateSound();
    }

    curVolume = DEFAULT_VOLUME; //                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                calculateSoundVolume();
    Serial.println("Calculated volume = " + String(curVolume) + "%");
    player.setVolume(100 - curVolume, 100 - curVolume);

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

    connectToStation();
}

void checkSound() {
  int curMillis = millis();

  if(curMillis < lastVolCheckMillis // millis rolled over
      || (curMillis - lastVolCheckMillis) > VOL_CHECK_MILLIS) {

    int totalSampled = 0;
    for(int v = 0; v < VOL_SAMPLE_CHECKS; v++) {
      totalSampled += analogRead(VOLUME_KNOB_INPUT);
      delay(VOL_SAMPLE_PAUSE);
    }

    int curVol = totalSampled / VOL_SAMPLE_CHECKS;
    
    lastVolCheckMillis = curMillis;
    
    if(abs(curVol - lastVolumeKnobInput) >= VOL_KNOB_THRESHOLD) {
      curVolume = calculateSoundVolume();
      Serial.println("Calculated volume = " + String(curVolume) + "%");
      player.setVolume(100 - curVolume, 100 - curVolume);
      updateScreen();
      lastVolumeKnobInput = curVol;
    }
  }
}

void loop() {
  if(!firstRun) {
    //checkSound();
  } else {
    firstRun = false;
  }

   // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) {
    if(lastState == LOW) {
      Serial.print("Changing station");
      onButtonClick();
      delay(100); // for bounce of button
    }
  } 

  lastState = buttonState;
  
  if (!client.connected()) {
    connectToStation();
  }

  if(player.readyForData()) {
    if (client.available() > 0) {
        // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
        uint8_t bytesread = client.read(mp3buff, BUFFER_SIZE);
        player.playData(mp3buff ,bytesread);
    }
  }
}
