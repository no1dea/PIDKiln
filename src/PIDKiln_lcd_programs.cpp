#include "PIDKiln_lcd_internal.h"
#include <LittleFS.h>
#include <math.h>

void LCD_display_programs(){
uint8_t chh,start_pos=0,max_lines=0;
uint16_t y=1,x=2;       // if we get bigger screen :)
int sel;
char msg[MAX_CHARS_PL];

  LCD_State=SCR_PROGRAM_LIST;

  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  chh=u8g2.getMaxCharHeight();
  max_lines=floor((SCREEN_H-y*2)/chh);
  u8g2.setFontPosBottom();

  if((sel=Find_selected_program())<0) return;
  
  if(sel>=floor(max_lines/2)){
    start_pos=sel-floor(max_lines/2);
    max_lines+=start_pos;
    if(max_lines>Programs_DIR_size) max_lines=Programs_DIR_size;
  }
  DBG dbgLog(LOG_DEBUG,"[LCD] Start pos:%d, sel_prg:%d, max_lines:%d\n",start_pos,sel,max_lines);
  
  for(start_pos; start_pos<max_lines && start_pos<Programs_DIR_size; start_pos++){
    if(Programs_DIR[start_pos].filesize<999) snprintf(msg, sizeof(msg),"%-15.15s %3db",Programs_DIR[start_pos].filename,Programs_DIR[start_pos].filesize);
    else snprintf(msg, sizeof(msg),"%-15.15s %2dkb",Programs_DIR[start_pos].filename,(int)(Programs_DIR[start_pos].filesize/1024));
    DBG dbgLog(LOG_INFO,"[LCD] Program list:%s: sel:%d\n",msg,Programs_DIR[start_pos].sel);
    if(Programs_DIR[start_pos].sel){
      u8g2.setDrawColor(1);
      u8g2.drawBox(0,y,SCREEN_W,chh);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x,y+=chh,msg);
      u8g2.setDrawColor(1);
    }else u8g2.drawStr(x,y+=chh,msg);
  }

  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
  u8g2.sendBuffer();
}


// Display question if delete program
//
void LCD_Display_program_delete(int dir, boolean pressed){
uint16_t x,y,h;
uint8_t chh,half;
static boolean yes=false;

  LCD_State=SCR_PROGRAM_DELETE;
  
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  u8g2.setFontMode(0);
  chh=u8g2.getMaxCharHeight()+2;
  x=5; y=30; h=SCREEN_H-y-x;
  u8g2.drawFrame(x,y,SCREEN_W-2*x,h);
  u8g2.setDrawColor(0);
  x++;y++;
  u8g2.drawBox(x,y,SCREEN_W-2*x,h-2);
  half=floor((SCREEN_W-2*x-2)/2);
  y+=chh+1; x+=4;
  u8g2.setDrawColor(1);
    
  if(!dir && !pressed) yes=false; // reset to default
  if(pressed)
    if(yes){
      if(Erase_program_file()){  // removed
        DBG Serial.println(" Program removed!");
        u8g2.drawStr(x,y+2,"File deleted!");
        u8g2.sendBuffer();
        u8g2.clearBuffer();
      }
      Cleanup_program();
      Generate_INDEX();
      delay(2500);
      LCD_display_programs(); // get back to program listing
      return;
    }else{
      LCD_Display_program_summary();
      return;
    }
    
  if(dir>0) yes=true;
  else if(dir<0) yes=false;

  u8g2.drawStr(x,y,"Delete program?");
  y+=chh;
  // Draw yes/no boxes
  x+=2;
  if(!yes){ // no delete
    u8g2.drawBox(7,y-chh+1,half,h/2-3);
    u8g2.setDrawColor(0);
    u8g2.drawStr(x+5,y,"No!");
    u8g2.setDrawColor(1);
    u8g2.drawStr(x+half+5,y,"Yes");
  }else{    // delete
    u8g2.drawStr(x+5,y,"No!");
    u8g2.drawBox(7+half,y-chh+1,half,h/2-3);
    u8g2.setDrawColor(0);
    u8g2.drawStr(x+half+5,y,"Yes");
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}


// Display full program, step by step
//
void LCD_Display_program_full(int dir){
uint8_t x=4,y=3,chh,lines;
static uint8_t pos=0;
char msg[MAX_CHARS_PL];
  
  LCD_State=SCR_PROGRAM_FULL;
  if(dir==0) pos=0;
  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()-1;
  lines=floor((SCREEN_H-2)/chh)-1;
  
  u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);

  if(dir>0 && (pos+lines-1)<Program_size) pos++;
  else if(dir<0 && pos>0) pos--;
  snprintf(msg, sizeof(msg),"nr Temp  Time  Dwell");
  u8g2.drawBox(0,1, SCREEN_W, chh);
  u8g2.setDrawColor(0);
  y+=chh;
  u8g2.drawStr(x,y-2,msg);
  u8g2.setDrawColor(1);
  
  for(int a=pos;a<Program_size && (a-pos)<lines;a++){
    snprintf(msg, sizeof(msg),"%d t:%4d T:%3d D:%3d",a+1,Program[a].temp,Program[a].togo,Program[a].dwell);
    u8g2.drawStr(x,y+=chh,msg);
  }
  u8g2.sendBuffer();
}


// Display single program info and options
// Dir - if dial rotated, load_prg - 0 = yes (if first time showing program (from program list)), 1 = no (rotating encoder), 2 = encoder pressed
//
void LCD_Display_program_summary(int dir, byte load_prg){
static int prog_menu=0;
char file_path[32];
uint8_t x=2,y=1,chh,err=0;
uint16_t sel;
char msg[125],rest[125];  // this should be 5 lines with 125 chars..  it should be malloc but ehh

  LCD_State=SCR_PROGRAM_SHOW;
// If the button was pressed - redirect to other screen
  if(load_prg==2){
    if(prog_menu==P_EXIT){ // exit - unload program, go to menu
      Cleanup_program();
      LCD_display_programs();
      return;
    }else if(prog_menu==P_SHOW){
      LCD_Display_program_full();
      return;
    }else if(prog_menu==P_LOAD){
      Load_program_to_run();
      LCD_display_main_view();
      return;
    }
  }
  
  u8g2.setFont(FONT6);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight();
  
  sel=Find_selected_program();    // get selected program
  DBG dbgLog(LOG_DEBUG,"[LCD] Show single program (dir %d, load_prg %d): %s\n",dir,load_prg,Programs_DIR[sel].filename);
  
  snprintf(file_path, sizeof(file_path),"%s/%s",PRG_Directory,Programs_DIR[sel].filename);
  DBG dbgLog(LOG_DEBUG,"[LCD]\tprogram path: %s\n",file_path);
  if(LittleFS.exists(file_path)){
    u8g2.clearBuffer();
    
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
    u8g2.drawBox(0,0, SCREEN_W , chh+2);
    y+=chh;
    u8g2.setDrawColor(0);
    snprintf(msg, sizeof(msg),"Name: %s",Programs_DIR[sel].filename);
    return_LCD_string(msg,rest,-4);
    u8g2.drawStr(x,y,msg);
    u8g2.setDrawColor(1);

    y++; // add some space before description or error
       
    if(!load_prg)
      if(err=Load_program()){     // loading program - if >0 - failed with error - see description in PIDKiln.h
        u8g2.drawStr(x,y+=chh,"Program load failed!");
        snprintf(msg, sizeof(msg),"Error: %d",err);
        u8g2.drawStr(x,y+=chh,msg);
        u8g2.drawFrame(0,0,SCREEN_W,SCREEN_H);
        u8g2.sendBuffer();
        return;
      }else prog_menu=0;

    strcpy(msg,Program_desc.c_str());   // Show program description - 3 lines max
    err=0; // reusage
    while(return_LCD_string(msg,rest,-4) && (err++<3)){
      u8g2.drawStr(x,y+=chh,msg);
      strcpy(msg,rest);
    }
    u8g2.drawStr(x,y+=chh,msg);
    
    y+=2; // +small space
    uint16_t max_t=0,total_t=0;     // calculate max temp and total time
    for(int a=0;a<Program_size;a++){
      if(Program[a].temp>max_t) max_t=Program[a].temp;
      total_t+=Program[a].togo+Program[a].dwell;
      DBG dbgLog(LOG_DEBUG,"[LCD] PRG: %d/%d Temp: %dC Time:%dm Dwell:%dm\n",a,Program_size,Program[a].temp,Program[a].togo,Program[a].dwell);
    }
    
    y=SCREEN_H-chh-1;
    snprintf(msg, sizeof(msg),"Tmax:%dC",max_t);
    DrawMenuEl(msg,y,2,1,0);
    snprintf(msg, sizeof(msg),"Time:%uh %dm",total_t/60,total_t%60);
    DrawMenuEl(msg,y,2,2,0);
    
    DBG dbgLog(LOG_DEBUG,"[LCD] Creating program menu prog_menu:%d dir:%d\n",prog_menu,dir);
    prog_menu+=dir;
    if(prog_menu>=Prog_Menu_Size) prog_menu=Prog_Menu_Size-1;
    else if(prog_menu<0) prog_menu=0;

    y=SCREEN_H;
    strcpy(msg,Prog_Menu_Names[0]);
    DrawMenuEl(msg,y,4,1,(prog_menu==0));
    strcpy(msg,Prog_Menu_Names[1]);
    DrawMenuEl(msg,y,4,2,(prog_menu==1));
    strcpy(msg,Prog_Menu_Names[2]);
    DrawMenuEl(msg,y,4,3,(prog_menu==2));
    strcpy(msg,Prog_Menu_Names[3]);
    DrawMenuEl(msg,y,4,4,(prog_menu==3));
      
    // If user wants to delete program - ask about it
    if(load_prg==2 && prog_menu==P_DELETE) LCD_Display_program_delete();
    else u8g2.sendBuffer();
  }
}


// Set manual, quick program step
// Dir - if dial rotated; pos - 0 = yes (if first time showing program (from program list)), 1 = no (rotating encoder), 2 = encoder pressed
//
void LCD_Display_quick_program(int dir,byte pos){
uint8_t x=2,y=1,chh;
static uint8_t what=0,ok=0,xm,ym;
static uint16_t qp[3];
char msg[100];

  LCD_State=SCR_QUICK_PROGRAM;
  
  if(pos==0 && dir==0){ // if we are here for the first time (not turn)
    what=ok=0;
    xm=ym=0;
    if(kiln_temp>0) qp[0]=kiln_temp;// target temperature
    else qp[0]=20;
    qp[1]=10;           // time to go
    qp[2]=10;           // dwell time
  }

  // Some actions without displaying anything
  if(pos==2){
    if(what==12){   // if button pressed on cancel
      LCD_display_menu();
      return;
    }else if(what==11){  // load program end exit
      DBG dbgLog(LOG_INFO,"[LCD] Replacing current program in memory - start");
      // We need to define program here
      Initialize_program_to_run();  // clear current program
      snprintf(msg, sizeof(msg),"Manually created quick program.");
      DBG dbgLog(LOG_DEBUG,"[LCD] Replacing current program in memory:%d \n",strlen(msg));
      Program_run_desc=(char *)MALLOC((strlen(msg)+1)*sizeof(char));
      strcpy(Program_run_desc,msg);
      snprintf(msg, sizeof(msg),"QuickProgram");
      DBG dbgLog(LOG_DEBUG,"[LCD] Replacing current program in memory:%d \n",strlen(msg));
      Program_run_name=(char *)MALLOC((strlen(msg)+1)*sizeof(char));
      strcpy(Program_run_name,msg);
      Update_program_step(0, qp[0], qp[1], qp[2]);
      Program_run_state=PR_READY;
      LCD_display_main_view();
      return;
    }else if(what==10){
      what=0; // user pushed edit
      pos=0;
    }
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(FONT7);
  u8g2.setFontPosBottom();
  chh=u8g2.getMaxCharHeight()+1;

  // If this is just rotate
  if(what<4){ // temperature
    if(dir>0) qp[0]+=pow(10,what);
    else if(dir<0 && pow(10,what)<qp[0]) qp[0]-=pow(10,what);
  }
  else if(what<7){ // time
    if(dir>0) qp[1]+=pow(10,what-4);
    else if(dir<0 && pow(10,what-4)<qp[1]) qp[1]-=pow(10,what-4);
  }
  else if(what<10){ // dwell
    if(dir>0) qp[2]+=pow(10,what-7);
    else if(dir<0 && pow(10,what-7)<qp[2]) qp[2]-=pow(10,what-7);
  }
  else what+=dir; // rotate menu

  DBG dbgLog(LOG_DEBUG,"[LCD] Dir: %d What:%d Pos:%d\n",dir,what,pos);
  
  // If button pressed - cycle
  // 0-3 - temperature, 4-6 - time, 7-9 - dwell, 10 - cancel, 11 - load, 12 - back to edit
  if(pos==2) what++;
  if(what>12) what=0;
  
  // just in case = check values...
  if(qp[0]<1) qp[0]=1;
  else if(qp[0]>Prefs[PRF_MAX_TEMP].value.uint16) qp[0]=Prefs[PRF_MAX_TEMP].value.uint16;

  if(qp[1]<1) qp[1]=1;
  else if(qp[1]>999) qp[1]=999;

  if(qp[2]<1) qp[2]=1;
  else if(qp[2]>999) qp[2]=999;
  
  u8g2.drawFrame(0, 0, SCREEN_W, SCREEN_H);
  u8g2.drawBox(0, 0, SCREEN_W, chh+2);
  u8g2.setDrawColor(0);
  snprintf(msg, sizeof(msg),"Quick program");
  u8g2.drawStr(x,y+=chh,msg);
  u8g2.setDrawColor(1);
  y+=3;
  
  snprintf(msg, sizeof(msg),"Temperature");
  xm=u8g2.getStrWidth(msg);
  ym=y;
  u8g2.drawStr(x,y+=chh,msg);
  
  snprintf(msg, sizeof(msg)," Time to go");
  if(u8g2.getStrWidth(msg)>xm) xm=u8g2.getStrWidth(msg);
  u8g2.drawStr(x,y+=chh,msg);
  
  snprintf(msg, sizeof(msg)," Dwell time");
  if(u8g2.getStrWidth(msg)>xm) xm=u8g2.getStrWidth(msg);
  u8g2.drawStr(x,y+=chh,msg);

  DrawVline(xm+5,chh+1,SCREEN_H-2*chh-4);
  //u8g2.drawVLine(xm+5,chh+1,SCREEN_H-2*chh-4);
  xm+=10; y=ym;
  // Now we draw settable stuff
  snprintf(msg, sizeof(msg),"%04d C", qp[0]);
  if(what<4){ // if we are drawing detailed temperature
    u8g2.setCursor(xm, y+=chh);
    Draw_Marked_string(msg,3-what);
  }else u8g2.drawStr(xm,y+=chh,msg);

  snprintf(msg, sizeof(msg),"%03d min.", qp[1]);
  if(what>3 && what<7){ // if we are drawing detailed time
    u8g2.setCursor(xm, y+=chh);
    Draw_Marked_string(msg,6-what);    
  }else u8g2.drawStr(xm,y+=chh,msg);

  snprintf(msg, sizeof(msg),"%03d min.", qp[2]);
  if(what>6 && what<10){ // if we are drawing detailed time
    u8g2.setCursor(xm, y+=chh);
    Draw_Marked_string(msg,9-what);    
  }else u8g2.drawStr(xm,y+=chh,msg);

  y=SCREEN_H;
  snprintf(msg, sizeof(msg),"Edit");
  DrawMenuEl(msg,y,3,1,(what==10));
  snprintf(msg, sizeof(msg),"Use it");
  DrawMenuEl(msg,y,3,2,(what==11));
  snprintf(msg, sizeof(msg),"Cancel");
  DrawMenuEl(msg,y,3,3,(what==12));
  
  // and now submenu
  u8g2.sendBuffer();
}


// Display information screen
//
