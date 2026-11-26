#include "PIDKiln_lcd_internal.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <math.h>

#define LCD_RESET 4   // RST on LCD
#define LCD_CS 5      // RS on LCD
#define LCD_CLOCK 18  // E on LCD
#define LCD_DATA 23   // R/W on LCD

// LEGACY: SW-SPI alternate wiring (uses LCD_CLOCK/LCD_DATA) — not compiled; HW SPI is active.
//U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ LCD_CLOCK, /* data=*/ LCD_DATA, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, /* CS=*/ LCD_CS, /* reset=*/ LCD_RESET);


/*
** Helping functions
**
*/

boolean return_LCD_string(char* msg,char* rest, int mod, uint16_t screen_w){
uint16_t chh,lnw;
char out[MAX_CHARS_PL]; 

  chh=u8g2.getMaxCharHeight();
  //DBG Serial.printf("[LCD] Line cut: Got:%s\n",msg);
  lnw=floor((SCREEN_W+mod)/u8g2.getMaxCharWidth())-1; // max chars in line
  if(strlen(msg)<=lnw){
    rest[0]='\0';
    //DBG Serial.printf("[LCD] Line cut: line shorter then %d - skipping\n",lnw);
    return false;
  }
  strncpy(rest,msg+lnw+1,strlen(msg)-lnw);
  rest[strlen(msg)-lnw]='\0';
  msg[lnw+1]='\0';
  //DBG Serial.printf("[LCD] Line cut. Returning msg:'%s' and rest:'%s'\n",msg,rest);
  return true;
}


// Write short messages during starting
//
void load_msg(char msg[MAX_CHARS_PL]){
  u8g2.setDrawColor(0);
  u8g2.drawBox(3, 31, 121, 22);
  u8g2.setDrawColor(1);
  u8g2.drawStr(10,45,msg);
  u8g2.sendBuffer();
}


// Draws dotted vertical line
//
void DrawVline(uint16_t x,uint16_t y,uint16_t h){

  //DBG Serial.printf("[LCD] -> Draw V dots: x:%d\t y:%d\t h:%d\n",x,y,h);
  h+=y;
  for(uint16_t yy=y;yy<h;yy+=3) u8g2.drawPixel(x,yy);
}


// Draw string with one char inversed
// 
void Draw_Marked_string(char *str,uint8_t pos){
uint8_t a;
  u8g2.setFontMode(0);
  for(a=0;a<strlen(str);a++){
    if(a==pos){
      u8g2.setDrawColor(0);
      u8g2.print(str[a]);
      u8g2.setDrawColor(1);
    }else u8g2.print(str[a]);
  }
  u8g2.setFontMode(1);
}


// Draws one element of horizontal menu
// msg = text to draw, y - box start point (lower,left corner - like with text in u8g2), cnt - fraction (split to 2, 3 etc), el - which element of cnt is this, sel - should element be in-versed
void DrawMenuEl(char *msg, uint16_t y, uint8_t cnt, uint8_t el, boolean sel){
uint16_t chh,x_w=0,width=0,x=0,x_txt=0;
char rest[MAX_CHARS_PL];

  chh=u8g2.getMaxCharHeight()+2;
  if(cnt>1) x_w=floor((float)SCREEN_W/cnt);
  else x_w=SCREEN_W;
  el--;   // because we want to count 0.. not 1..
  x+=x_w*el;   // we start at 0 and el*button width
  
  if(cnt==el+1) x_w=SCREEN_W-x; // just make sure to draw last square to the end
  else x_w++; // some shitty voodoo
  
  return_LCD_string(msg,rest,2,x_w);  // cut text size if too long - this is for safety, should not happend
  width=u8g2.getStrWidth(msg);
  x_txt=x+(x_w-2-width)/2;            // find out how to put a text in the middle of the button

  u8g2.drawFrame(x,y-chh,x_w,chh);  // draw frame around text w+1 because start point is also counted to witdh
  DBG dbgLog(LOG_DEBUG,"[LCD] Width:%d cnt:%d el:%d x:%d x_txt:%d x_w:%d\n",SCREEN_W,cnt,el,x,x_txt,x_w);
  
  if(sel){
      u8g2.drawBox(x, y-chh, x_w, chh);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x_txt,y-1,msg);
      u8g2.setDrawColor(1);
  }else u8g2.drawStr(x_txt,y-1,msg);          // draw text - we should check it's size also..
}

/*
** Core/main LCD functions
**
*/

// Third main view - operations (start,pause,stop)
//
void LCD_display_mainv3(int dir, byte ctrl){
char msg[MAX_CHARS_PL];
uint16_t chh,chw,x=2,y=2;
static int what=0;

  if(!Program_run_size) return; //  don't go in if no program loaded
  LCD_State=SCR_MAIN_VIEW;
  LCD_Main=MAIN_VIEW3;  // just in case...

  if(dir==0 && ctrl==0) what=0; // initialize what
  
  what+=dir;    // rotate 1/-1
  
  if(what>4){  // we can rotate out of this screen
    LCD_Main=MAIN_VIEW1;
    LCD_display_main_view();
    return;
  }else if(what<0){
    LCD_Main=MAIN_VIEW2;
    LCD_display_main_view();
    return;
  }
  if(ctrl==2){
    switch(what){
      case 0:
        START_Program();
        break;
      case 1:
        if(Program_run_state==PR_PAUSED) RESUME_Program();
        else PAUSE_Program();
        break;
      case 3:
        END_Program();
        break;
      case 4:
        ABORT_Program(PR_ERR_USER_ABORT);
        break;
      default:
        break;
    }
    LCD_Main=MAIN_VIEW1;
    LCD_display_main_view();
    return;
  }
  
  u8g2.clearBuffer();
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()+1;
  chw=u8g2.getMaxCharWidth();

  y=SCREEN_H-chh;
  snprintf(msg, sizeof(msg),"Start");
  DrawMenuEl(msg,y,3,1,(what==0));
  if(Program_run_state==PR_PAUSED) snprintf(msg, sizeof(msg),"Resume");
  else snprintf(msg, sizeof(msg),"Pause");
  DrawMenuEl(msg,y,3,2,(what==1));
  snprintf(msg, sizeof(msg),"Return");
  DrawMenuEl(msg,y,3,3,(what==2));

  y=SCREEN_H;
  snprintf(msg, sizeof(msg),"Stop");
  DrawMenuEl(msg,y,2,1,(what==3));
  snprintf(msg, sizeof(msg),"Abort");
  DrawMenuEl(msg,y,2,2,(what==4));
  
  u8g2.sendBuffer();
}


// Second main view - program graph
//
void LCD_display_mainv2(){
uint16_t ttime=0,mxtemp=0,mxx=0,mxy=0,x,y,oldx,oldy,scx,scy,startx,starty;
char msg[MAX_CHARS_PL];

  if(!Program_run_size) return; //  don't go in if no program loaded
  LCD_State=SCR_MAIN_VIEW;
  LCD_Main=MAIN_VIEW2;  // just in case...

  u8g2.clearBuffer();
  u8g2.setFont(FONT4);
  u8g2.setFontPosBottom();
  for(uint8_t a=0; a<Program_run_size; a++){
    ttime+=Program_run[a].togo+Program_run[a].dwell;
    if(Program_run[a].temp>mxtemp) mxtemp=Program_run[a].temp;
  }

  mxx=SCREEN_W-2; // max X axis - time
  mxy=SCREEN_H-5; // max Y axis - temperature
  if(ttime) scx=(int)((mxx*100)/ttime);   // 1 minute is scx pixels * 100 on graph 
  if(mxtemp) scy=(int)((mxy*100)/mxtemp); // 1 celsius is scy pixel * 100

  DBG dbgLog(LOG_DEBUG,"[LCD] Graph. mxx:%d mxy:%d ttime:%d mxtemp:%d scx:%d scy:%d\n",mxx,mxy,ttime,mxtemp,scx,scy);

  // Draw axies
  u8g2.drawHLine(1,SCREEN_H-1,mxx);
  u8g2.drawVLine(1,SCREEN_H-mxy,mxy);
  u8g2.drawHLine(0,SCREEN_H-mxy,3);
  snprintf(msg, sizeof(msg),"%dC",mxtemp);
  u8g2.drawStr(4,9,msg);
  
  startx=1;
  starty=SCREEN_H-1;  // 0.0 on screen is in left, upper corner - reverse it
  
  oldx=startx;
  oldy=SCREEN_H-(Program_start_temp*scy)/100;  // we start from current temp
  
  for(uint8_t a=0; a<Program_run_size; a++){
    x=startx;y=starty;      // in case of any fuckup - jest go to start point
    y=starty-(int)(Program_run[a].temp*scy)/100;
    x=(int)(Program_run[a].togo*scx)/100+oldx;
    u8g2.drawLine(oldx,oldy,x,y);
    // DBG Serial.printf("[LCD].Drawing line: x0:%d \t y0:%d \tto\t x1:%d (%d) \t y1:%d (%d) \n",oldx,oldy,x,Program_run[a].togo,y,Program_run[a].temp);
    DrawVline(x,y,starty-y);
    oldx=x;
    oldy=y;
    if(Program_run[a].dwell){
      x=(int)(Program_run[a].dwell*scx)/100+oldx;
      u8g2.drawLine(oldx,oldy,x,y);
      //DBG Serial.printf("[LCD] ..Drawing line: x0:%d \t y0:%d \tto\t x1:%d (%d) \t y1:%d (%d)\n",oldx,oldy,x,Program_run[a].dwell,y,Program_run[a].temp);
      oldx=x;
    }
  }

  u8g2.sendBuffer();

  // If we are running - do the inverse part of graph
  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    int fullt,currt;
    float prop;
    fullt=(int)(Program_run_end-Program_run_start); // how long should program last
    currt=time(NULL)-Program_run_start;     // where are we now?
    prop=(float)currt/(float)fullt;   // current progress status
    u8g2.setDrawColor(2);
    DBG dbgLog(LOG_DEBUG,"[LCD] Redrawing box on graph width:%.2f fullt:%d currt:%d prop:%f\n",(float)((SCREEN_W-2)*prop),fullt,currt,prop);
    u8g2.drawBox(2,1,(int)((SCREEN_W-2)*prop),SCREEN_H-2);
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
  }
}



// Fist main view - basic running program information, status, time, start time, eta, temperatures
//
void LCD_display_mainv1(){
char msg[MAX_CHARS_PL];
uint16_t x=2,y=1;
uint8_t chh,chw,mch;
static uint8_t cnt=1;
struct tm timeinfo,*tmm;

  if(!Program_run_size) return; //  dont go in if no program loaded
  LCD_State=SCR_MAIN_VIEW;
  LCD_Main=MAIN_VIEW1;  // just in case...
  
  u8g2.clearBuffer();
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  strcmp(msg," ");
  chh=u8g2.getMaxCharHeight()+2;
  chw=u8g2.getMaxCharWidth();
  mch=floor(SCREEN_W-4/chw);  // how many chars per line...
  snprintf(msg, sizeof(msg),"%.*s",mch,Program_run_name);
  u8g2.drawBox(0,0, SCREEN_W, chh+1);
  u8g2.setDrawColor(0);
  y+=chh-1;
  u8g2.drawStr(x,y,msg);
  u8g2.setDrawColor(1);

  if(Program_run_state==PR_RUNNING){    // just some small animation for running program
    if(cnt==1) snprintf(msg, sizeof(msg),"%s",Prog_Run_Names[Program_run_state]);
    else if(cnt==2) snprintf(msg, sizeof(msg),"%s.",Prog_Run_Names[Program_run_state]);
    else if(cnt==3) snprintf(msg, sizeof(msg),"%s..",Prog_Run_Names[Program_run_state]);
    else if(cnt==4){
      snprintf(msg, sizeof(msg),"%s...",Prog_Run_Names[Program_run_state]);
      cnt=0;
    }
    cnt++;
  }else snprintf(msg, sizeof(msg),"%s",Prog_Run_Names[Program_run_state]);
  y+=chh;
  u8g2.drawStr(x+1,y,msg);

  // Program status & Clock
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W/2, chh+1);
  if(getLocalTime(&timeinfo)) strftime(msg, 20, "%T", &timeinfo);
  else snprintf(msg, sizeof(msg)," No time");
  u8g2.drawStr(SCREEN_W/2+3,y,msg);

  // Start time
  u8g2.setFont(FONT6);
  chh=u8g2.getMaxCharHeight()+2;
  chw=u8g2.getMaxCharWidth();
  
  y+=chh;
  u8g2.drawFrame(0,y-chh, chw*7, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  snprintf(msg, sizeof(msg),"Start");
  u8g2.drawStr(x,y,msg);
  if(Program_run_start){
    tmm=localtime(&Program_run_start);
    snprintf(msg, sizeof(msg),"%2d-%02d %d:%02d:%02d",(tmm->tm_mon+1),tmm->tm_mday,tmm->tm_hour,tmm->tm_min,tmm->tm_sec);
    u8g2.drawStr(chw*8+3,y-1,msg);
  }
  
  //ETA time
  y+=chh;
  u8g2.drawFrame(0,y-chh, chw*7, chh+1);
  u8g2.drawFrame(0,y-chh, SCREEN_W, chh+1);
  if(Program_run_state==PR_RUNNING || Program_run_state==PR_PAUSED){
    snprintf(msg, sizeof(msg),"  ETA");
    u8g2.drawStr(x,y,msg);
    tmm=localtime(&Program_run_end);
    snprintf(msg, sizeof(msg),"%2d-%02d %d:%02d:%02d",(tmm->tm_mon+1),tmm->tm_mday,tmm->tm_hour,tmm->tm_min,tmm->tm_sec);
    u8g2.drawStr(chw*8+3,y-1,msg);
  }else{
    snprintf(msg, sizeof(msg),"  End");
    u8g2.drawStr(x,y,msg);
    if(Program_run_end){
      tmm=localtime(&Program_run_end);
      snprintf(msg, sizeof(msg),"%2d-%02d %d:%02d:%02d",(tmm->tm_mon+1),tmm->tm_mday,tmm->tm_hour,tmm->tm_min,tmm->tm_sec);
      u8g2.drawStr(chw*8+3,y-1,msg);
    }
  }

  // Temperatures - current, target, environment
  snprintf(msg, sizeof(msg),"Ct:%4.0fC St:%4.0fC Amb:%2.0fC",kiln_temp,set_temp,int_temp);
  u8g2.drawStr(x,y+=chh,msg);

  // Print Step number/all steps and if it's Run od Dwell, print proportional heat time of SSR and case temperature
  //if(Program_run_step>-1) snprintf(msg, sizeof(msg),"Stp:%d/%d%c Ht:%3.0f%%  Cse:%2dC",Program_run_step+1,Program_run_size,(temp_incr!=0)?'r':'d',(pid_out/Prefs[PRF_PID_WINDOW].value.uint16)*100.0);
  if(Program_run_step>-1) snprintf(msg, sizeof(msg),"Stp:%d/%d%c Ht:%3.0f%% Cse:%.0fC",Program_run_step+1,Program_run_size,(temp_incr!=0)?'r':'d',PIDKiln_heat_percent(pid_out, Prefs[PRF_PID_WINDOW].value.uint16, PID_WINDOW_DIVIDER),case_temp);
  else snprintf(msg, sizeof(msg),"Stp:0/%d r/d Ht:0%% Cse:%.0fC",Program_run_size,case_temp);
  u8g2.drawStr(x,y+=chh-1,msg);
  
  u8g2.sendBuffer();
}


// Display main screens
//
void LCD_display_main_view(){
char sname[40];

  LCD_State=SCR_MAIN_VIEW;
  if(Program_run_size){
    if(LCD_Main==MAIN_VIEW1) LCD_display_mainv1();    // if any program loaded and view selected
    else if(LCD_Main==MAIN_VIEW2) LCD_display_mainv2();
    else if(LCD_Main==MAIN_VIEW3) LCD_display_mainv3();
  }else{
    u8g2.clearBuffer();          // clear the internal memory
    snprintf(sname, sizeof(sname),"Main screen %d",(int)LCD_Main);
    u8g2.setFont(FONT8);
    u8g2.drawStr(25,30,sname);
    u8g2.setFont(FONT7);
    snprintf(sname, sizeof(sname),"please load program");
    u8g2.drawStr(8,45,sname);
    u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
    u8g2.sendBuffer();
  }
}


// Display menu
//
void LCD_display_menu(){
char menu[MAX_CHARS_PL];
int m_startpos=LCD_Menu;
uint8_t chh,center=5;

  LCD_State=SCR_MENU;
  DBG dbgLog(LOG_DEBUG,"[LCD] Entering menu (%d) display: %s\n",LCD_Menu,Menu_Names[LCD_Menu]);
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT7);
  u8g2.setFontPosBaseline();
  chh=u8g2.getMaxCharHeight();
  center=floor((SCREEN_H-(chh+SCR_MENU_SPACE)*SCR_MENU_LINES)/2); // how much we have to move Y to be on the middle with all menu
  DBG dbgLog(LOG_DEBUG,"[LCD] In menu we can print %d lines, with %dpx space, and char height %d\n",SCR_MENU_LINES,SCR_MENU_SPACE,chh);
  
  if(LCD_Menu>SCR_MENU_MIDDLE) m_startpos=LCD_Menu-(SCR_MENU_MIDDLE-1); // if current menu pos > middle part of menu - start from LCD_Menu - SCR_MENU_MIDDLE-1
  else if(LCD_Menu<=SCR_MENU_MIDDLE) m_startpos=LCD_Menu-SCR_MENU_MIDDLE+1;  // if current menu pos < middle part - start
  DBG dbgLog(LOG_DEBUG,"[LCD] Start pos is %d, chosen position is %d, screen center is %d\n",m_startpos,LCD_Menu,center);
  
  for(int a=1; a<=SCR_MENU_LINES; a++){
    if(a==SCR_MENU_MIDDLE){   // reverse colors if we print middle part o menu
      u8g2.setDrawColor(1); /* color 1 for the box */
      DBG dbgLog(LOG_DEBUG,"[LCD] x0: %d, y0: %d, w: %d, h: %d\n",0, (a-1)*chh+SCR_MENU_SPACE+center, SCREEN_W , chh+SCR_MENU_SPACE);
      u8g2.drawBox(0, (a-1)*chh+SCR_MENU_SPACE+center+1, SCREEN_W , chh+SCR_MENU_SPACE);
      u8g2.setDrawColor(0);
    }
    if(m_startpos<0 || m_startpos>=M_END) u8g2.drawStr(15,(a*chh)+SCR_MENU_SPACE+center," ");  // just to add some top/bottom unselectable menu positions
    else{
      u8g2.drawStr(15,(a*chh)+SCR_MENU_SPACE+center,Menu_Names[m_startpos]);
    }
    u8g2.setDrawColor(1);
    m_startpos++;
  }
  
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display programs list
// action = direction of rotation
void LCD_Display_info(){
uint8_t chh,y=1,x=2;
char msg[MAX_CHARS_PL];
struct tm timeinfo;

  LCD_State=SCR_OTHER;
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(FONT6);
  y+=chh=u8g2.getMaxCharHeight();

  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  if(WiFi.getMode()==WIFI_OFF){
    snprintf(msg, sizeof(msg),"WiFi status: disabled");
    u8g2.drawStr(x,y,msg);
  }else if(WiFi.getMode()==WIFI_STA){
    snprintf(msg, sizeof(msg),"WiFi mode: client");
    u8g2.drawStr(x,y,msg);
    snprintf(msg, sizeof(msg),"WiFi ssid: %s",Prefs[PRF_WIFI_SSID].value.str);
    u8g2.drawStr(x,y+=chh,msg);
    if(WiFi.isConnected()) snprintf(msg, sizeof(msg),"WiFi status: connected");
    else snprintf(msg, sizeof(msg),"WiFi status: disconnected");
    u8g2.drawStr(x,y+=chh,msg);
  }else if(WiFi.getMode()==WIFI_AP){
    snprintf(msg, sizeof(msg),"WiFi mode: AP");
    u8g2.drawStr(x,y,msg);
  }

  if(WiFi.getMode()){
    IPAddress lips;      
    Return_Current_IP(lips);
    snprintf(msg, sizeof(msg),"WiFi IP: %s",lips.toString().c_str());
    u8g2.drawStr(x,y+=chh,msg);
  }

  snprintf(msg, sizeof(msg),"MAC: %s",WiFi.macAddress().c_str());
  DBG dbgLog(LOG_DEBUG,"[LCD] Printing MAC address: %s\n",msg);
  u8g2.drawStr(x,y+=chh,msg);
  
  snprintf(msg, sizeof(msg),"Max prg. size: %d",MAX_Prog_File_Size);
  u8g2.drawStr(x,y+=chh,msg);
  
  if(getLocalTime(&timeinfo)) strftime(msg, MAX_CHARS_PL, "%F %T", &timeinfo);
  else snprintf(msg, sizeof(msg),"Failed to acquire time");
  u8g2.drawStr(x,y+=chh,msg);

  u8g2.sendBuffer();          // transfer internal memory to the display 
}


// Display currently loaded preferences on LCD screen
//
void LCD_Display_prefs(int dir){
uint8_t x=4,y=3,chh,chw,lines;
static uint8_t pos=1;
char msg[MAX_CHARS_PL*2],rest[MAX_CHARS_PL];
  
  LCD_State=SCR_PREFERENCES;
  if(dir==0) pos=1;
  u8g2.clearBuffer();
  u8g2.setFont(FONT6);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()-1;
  lines=floor((SCREEN_H-2)/chh)-1;
  
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);

  if(dir>0 && (pos+lines-2)<PRF_end) pos++;
  else if(dir<0 && pos>1) pos--;
  snprintf(msg, sizeof(msg),"Prefs:%s",PREFS_FILE);  
  u8g2.drawBox(0,1, SCREEN_W, chh);
  u8g2.setDrawColor(0);
  y+=chh;
  u8g2.drawStr(x,y-2,msg);
  u8g2.setDrawColor(1);
  
  for(int a=pos;a<=PRF_end && (a-pos)<lines && y<SCREEN_H-5;a++){
    if(a>=PRF_end) snprintf(msg, sizeof(msg)," ");
    else if(Prefs[a].type==STRING) snprintf(msg, sizeof(msg),"%s = %s",PrefsName[a],Prefs[a].value.str ? Prefs[a].value.str : "");
    else if(Prefs[a].type==VFLOAT) snprintf(msg, sizeof(msg),"%s = %.2f",PrefsName[a],Prefs[a].value.vfloat);
    else if(Prefs[a].type==UINT8) snprintf(msg, sizeof(msg),"%s = %u",PrefsName[a],(unsigned)Prefs[a].value.uint8);
    else if(Prefs[a].type==UINT16) snprintf(msg, sizeof(msg),"%s = %u",PrefsName[a],(unsigned)Prefs[a].value.uint16);
    else if(Prefs[a].type==INT16) snprintf(msg, sizeof(msg),"%s = %d",PrefsName[a],(int)Prefs[a].value.int16);
    else if(Prefs[a].type==INT32) snprintf(msg, sizeof(msg),"%s = %ld",PrefsName[a],(long)Prefs[a].value.int32);
    else snprintf(msg, sizeof(msg),"%s = ?",PrefsName[a]);
    if(return_LCD_string(msg,rest,-4)){
      u8g2.drawStr(x,y+=chh,msg);
      y+=chh;
      if(y<SCREEN_H-4) u8g2.drawStr(x,y,rest);
    }else u8g2.drawStr(x,y+=chh,msg);
  }
  u8g2.sendBuffer();  
}


// Display about screen
//
void LCD_Display_about(){
  LCD_State=SCR_ABOUT;   // Update what are we showing on screen
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawStr(28,15,PVer);
  u8g2.drawStr(36,30,PDate);
  u8g2.setFont(FONT6);
  u8g2.drawStr(8,45,"Web page:");
  u8g2.drawStr(8,55,"adrian.siemieniak.net");
  u8g2.drawFrame(2,2,SCREEN_W-4,SCREEN_H-4);
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
}


// Restart the ESP device
//
static void LCD_Restart_ESP(){
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawStr(25,30,PVer);
  u8g2.setFont(FONT7);
  u8g2.drawStr(18,45,"is restarting...");
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
  delay(1000);
  Restart_ESP();
}


// Reconect to WiFi - if initial connection failed user may do it manually
//
void LCD_Reconect_WiFi(){
  char msg[MAX_CHARS_PL];

  LCD_State=SCR_OTHER;
  u8g2.clearBuffer();
  u8g2.setFont(FONT8);
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
  if(!Prefs[PRF_WIFI_MODE].value.uint8){
    strcpy(msg," WiFi is disabled!");
    load_msg(msg);
    return;
  }
  strcpy(msg,"Reconnecting WiFi");
  load_msg(msg);
  if(Setup_WiFi()){    // !!! Wifi connection FAILED
    DBG dbgLog(LOG_INFO,"[LCD] WiFi connection failed\n");
    strcpy(msg," WiFi con. failed ");
    load_msg(msg);
  }else{
    IPAddress lips;
      
    Return_Current_IP(lips);
    DBG Serial.println(lips); // Print ESP32 Local IP Address
    snprintf(msg, sizeof(msg)," IP: %s",lips.toString().c_str());
    load_msg(msg);
  }
}






/* 
** Setup LCD screen 
**
*/
void Setup_LCD(void) {
  
  u8g2.begin();
  u8g2.setBusClock(900000);   // without lowering the clock (default 1Mhz) LCD gets some glitches
  
  u8g2.clearBuffer();         // clear the internal memory
  u8g2.setFont(FONT8);
  u8g2.drawStr(27,30,PVer);
  u8g2.drawStr(38,45,"starting...");
  u8g2.drawFrame(2,2,123,59);
  u8g2.drawFrame(0,0,127,63);
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(500);
  
}
