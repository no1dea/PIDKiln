#include <Update.h>
#include <LittleFS.h>
#include "PIDKiln_http_internal.h"
#if PK_FEATURE_LOCAL_UI
#include <U8g2lib.h>
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
#endif

char *screenshot;
#if PK_FEATURE_LOCAL_UI
void out(const char *s){strcat(screenshot,s);}
void do_screenshot(AsyncWebServerRequest *request){
  if (!_webAuth(request))
    return;

  screenshot=(char *)MALLOC(SCREEN_W*SCREEN_H*2*sizeof(char)+1);
  if(screenshot==NULL){
    DBG dbgLog(LOG_ERR,"[HTTP] Failed to allocate memory for screenshot");
    request->send(500);
    return;
  }
  *screenshot='\0';

  AsyncResponseStream *response = request->beginResponseStream("image/x-portable-bitmap");
  response->addHeader("Server","ESP Async Web Server");
  response->addHeader("Content-Disposition","attachment; filename=\"PIDKiln_screenshot.pbm\"");

  u8g2_WriteBufferPBM2(u8g2.getU8g2(),out);
  response->println(screenshot);
  free(screenshot); screenshot=NULL;
  
  request->send(response);
}
#endif


// Firmware-only upload/update (from https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino).
// LittleFS images are not updated over HTTP — use PlatformIO uploadfs.
//
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  (void)filename;
  if (!index){
    DBG dbgLog(LOG_INFO,"[HTTP] Beginning firmware update\n");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Please wait while the device reboots...");
    response->addHeader("Refresh", "20; url=/about.html");
    response->addHeader("Connection", "close");
    request->send(response);
    delay(1000);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      DBG Serial.println("[HTTP] Update complete");
      DBG Serial.flush();
      Restart_ESP();
    }
  }
}
