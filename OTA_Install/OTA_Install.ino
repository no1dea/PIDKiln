/*
** PIDKiln OTA Install — LEGACY / LAST-RESORT ONLY
**
** Preferred install path for v1.6+: PlatformIO
**   pio run -e esp32-wrover -t upload
**   pio run -e esp32-wrover -t uploadfs
** Or use the device web UI "Flash firmware" page with a local .bin.
**
** This sketch downloads LittleFS + firmware over cleartext HTTP from a
** hard-coded host — keep it only for brick recovery on air-gapped setups.
**
** Note: httpUpdate.updateSpiffs() still targets the filesystem partition
** (Arduino U_SPIFFS cmd) even when the image content is LittleFS.
*/


static char* WiFi_SSID="";
static char* WiFi_PASS="";
//           EDIT THIS ^^^^^^^^^^^^^^



#define BIN_LITTLEFS "http://adrian.siemieniak.net/PIDKiln/PIDKiln.littlefs.bin"
//#define BIN_LITTLEFS "https://raw.githubusercontent.com/Saur0o0n/pidkiln/master/OTA_Install/PIDKiln.littlefs.bin"
#define BIN_SKETCH "http://adrian.siemieniak.net/PIDKiln/PIDKiln.ino.esp32.bin"
//#define BIN_SKETCH "https://raw.githubusercontent.com/Saur0o0n/pidkiln/master/OTA_Install/PIDKiln.ino.esp32.bin"

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include <HTTPUpdate.h>

WiFiMulti WiFiMulti;

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  Serial.println();
  
  if(strlen(WiFi_SSID)<1 || strlen(WiFi_PASS)<1){
    Serial.println("You must provide WiFi SSID and Password to use OTA Install.");
    Serial.println(" Make sure this WiFi has direct, Internet access!");
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WiFi_SSID, WiFi_PASS);

}

void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    Serial.println("Update LittleFS...");

    WiFiClient client;

    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    //httpUpdate.setLedPin(LED_BUILTIN, LOW);

    // updateSpiffs = filesystem partition update (image may be LittleFS-formatted)
    t_httpUpdate_return ret = httpUpdate.updateSpiffs(client, BIN_LITTLEFS);
    if (ret == HTTP_UPDATE_OK) {
      Serial.println("Update sketch...");
      ret = httpUpdate.update(client, BIN_SKETCH);

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          break;
      }
    }
  }
}
