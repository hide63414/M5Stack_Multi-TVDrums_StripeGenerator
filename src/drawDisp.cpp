
#include "drawDisp.h"

//親子共通
//===========================================================================
void dispVersion(String version){
  M5.Display.setCursor(180,300);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_BROWN, TFT_BLACK);
  M5.Display.printf(version.c_str());
}

void dispMyId(String id){
  M5.Display.setCursor(0,15);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(3.5);
  M5.Display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  M5.Display.printf("%s", id.c_str());
}

void drawButton(){
    M5.Display.drawRoundRect(170,120,60,20,4,TFT_WHITE);
    M5.Display.drawRoundRect(170,158,60,20,3,TFT_WHITE);
    M5.Display.drawRoundRect(170,196,60,20,2,TFT_WHITE);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    M5.Display.setCursor(190,122);
    M5.Display.print("UP");
    M5.Display.setCursor(180,160);
    M5.Display.print("PAUSE");
    M5.Display.setCursor(184,198);
    M5.Display.print("DOWN");
}

void drawRecieveButton(){
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(180,160);
    M5.Display.print(" RCV  ");
}


void dispPause(){
  M5.Display.setCursor(20,155);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1.5);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.print("PAUSE");
#ifdef CAPTUREOK
   saveToSD_24bit("/dispPause.bmp");
#endif  
}

void erasePause(){
  M5.Display.fillRect(20, 155, 126, 30, TFT_BLACK);
}

void dispSdError(){
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(5,5);
  M5.Display.println("Card Mount\n Failed");
}



//子機専用
//===========================================================================
void drawViewFrame(){
  M5.Display.drawRect(9, 119, lcdCanvasWidth+2, lcdCanvasHeight+2, TFT_DARKGRAY);
}


void dispSourceNumber(int fNum){
//  Serial.println("disp Source number");
//  Serial.printf("currentState set:%d, source:%d, pause:%d, speed:%f, ratio:%f, timer:%d\n",recievedSetlistNumber, sourceNumber, mp4Pause, playSpeedRatio, mp4YRatio, mp4TelemoroBpm);
  M5.Display.drawRoundRect(158, 8, 82, 78, 10, TFT_GREEN);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(168,0);
  M5.Display.printf("SRC");

  M5.Display.setFont(&fonts::Font7);  
  M5.Display.setTextSize(1);
  M5.Display.setCursor(168,30);
  M5.Display.printf("%02d",fNum);
}

void dispRecievedSetNumber(int setNum){
  M5.Display.drawRoundRect(66, 8, 82, 78, 10, TFT_CYAN);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(76,0);
  M5.Display.printf("SET");

  M5.Display.setFont(&fonts::Font7);      //32x48
  M5.Display.setTextSize(1);
  M5.Display.setCursor(76,30);
  M5.Display.printf("%02d",setNum);
}

void eraseRecievedSetNumber(){
  M5.Display.fillRect(76, 30, 64, 48, TFT_BLACK);
}

void dispSourceFileName(String fileName){
  M5.Display.setCursor(10,226);
  M5.Display.fillRect(0, 226, 240, 16, TFT_BLACK);      //erase filename
  M5.Display.setFont(&fonts::Font2);                    //16
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_SILVER, TFT_BLACK);
  M5.Display.printf("%s",fileName.c_str());
}

void eraseMp4Info(){
  M5.Display.fillRect(132, 92 , M5.Display.width()-132, 24, TFT_BLACK);
}

void dispFps(int count){
  M5.Display.setCursor(132, 92);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKCYAN, TFT_BLACK);
  M5.Display.printf("%02d", count);
  M5.Display.setTextSize(0.75, 0.75);
  M5.Display.printf("fps");
}

void dispFrameCount(int count){
  M5.Display.setCursor(195, 92);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_PINK, TFT_BLACK);
  M5.Display.printf("%03d", count);
}

void dispMode(String mode){
  M5.Display.setCursor(10, 92);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_DARKCYAN, TFT_BLACK);
  M5.Display.setTextSize(0.75,1);
  M5.Display.printf("MODE");
  M5.Display.setTextSize(1);  
  M5.Display.printf(":%s ",mode.c_str());
}

void dispWavDataPosition(int count){
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    M5.Display.setCursor(15,125);
    M5.Display.printf("Playing!\n");
    M5.Display.setCursor(25,150);
    M5.Display.printf("%04d\n",count);
    M5.Display.setCursor(75,175);
    M5.Display.printf("KByte");         //読み込みバッファ1Kbyte
}

void dispSpeedRatio(float ratio){
  int x = 10;
  int y = 250;
  M5.Display.setFont(&fonts::Font4);      //横14縦26
  M5.Display.setTextSize(1);
  M5.Display.setCursor(x+25, y);
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.printf("x%0.2f", ratio);
  M5.Display.fillTriangle(x,    y, x+10, y+10, x    , y+20, TFT_ORANGE);
  M5.Display.fillTriangle(x+10, y, x+20, y+10, x+10 , y+20, TFT_ORANGE);
}

void eraseSpeedRatio(){
  M5.Display.fillRect(10, 250, 100, 21, TFT_BLACK);
}

void dispExpandRatio(float ratio){
  int x = 10;
  int y = 280;
  M5.Display.setFont(&fonts::Font4);      //横14縦26
  M5.Display.setTextSize(1);
  M5.Display.setCursor(x+25, y);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.printf("x%0.2f", ratio); 
  M5.Display.fillTriangle(x, y+6, x+10, y, x+20, y+6, TFT_GREEN);
  M5.Display.fillRect(x+6, y+6, 8, 8, TFT_GREEN);
  M5.Display.fillTriangle(x, y+14, x+10, y+20, x+20, y+14, TFT_GREEN);
}

void eraseExpandRatio(){
  M5.Display.fillRect(10, 280, 100, 21, TFT_BLACK);
}

void dispTelemoroBpm(int bpm){
  int x = 120;
  int y = 250;
  M5.Display.setFont(&fonts::Font4);      //横14縦26
  M5.Display.setTextSize(1);
  M5.Display.setCursor(x+25, y);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.printf("%03dbpm", bpm);

#ifdef CAPTUREOK
  if(bpm){
    saveToSD_24bit("/telemoro.bmp");
  } else {
    saveToSD_24bit("/telemoro0.bmp");
  }
#endif
}

void eraseTelemoroCount(){
  M5.Display.fillRect(120, 250, 120, 26, TFT_BLACK);
}

//親機専用
//===========================================================================

void dispSetlistFileNameZ(String mode){
  M5.Display.setCursor(10, 92);
  M5.Display.fillRect(0, 92, 240, 16, TFT_BLACK);      //erase filename
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_SILVER, TFT_BLACK); 
  M5.Display.setTextSize(1);   
  M5.Display.printf("%s",mode.c_str());
}

void drawButtonZ(){
    drawButton();
    M5.Display.fillCircle(160, 128, 8, TFT_SKYBLUE);
    //M5.Display.fillCircle(160, 160, 6, TFT_SKYBLUE);    同時押しなし　V1.0->V1.1
    //M5.Display.fillCircle(160, 176, 6, TFT_RED);
    M5.Display.fillCircle(160, 204, 8, TFT_RED);
}

void drawSetlistSelectbutton(){
    M5.Display.drawRoundRect(170,20,60,20,4,TFT_WHITE);
    M5.Display.drawRoundRect(170,50,60,20,3,TFT_WHITE);
    M5.Display.drawRoundRect(170,80,60,20,2,TFT_WHITE);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);

    M5.Display.setCursor(190,22);
    M5.Display.print("UP");
    M5.Display.setCursor(178,52);
    M5.Display.print("SELECT");
    M5.Display.setCursor(184,82);
    M5.Display.print("DOWN");
}


void dispSetNumberZ(int setNum, int maxNum){
  M5.Display.drawRoundRect(66, 8, 170, 78, 10, TFT_CYAN);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(76,0);
  M5.Display.printf("SET");

  M5.Display.setFont(&fonts::Font7);      //32x48
  M5.Display.setTextSize(1);
  M5.Display.setCursor(76,30);
  M5.Display.printf("%02d  %02d",setNum, maxNum);

  M5.Display.setCursor(144,32);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(2);
  M5.Display.printf("/");
}

void dispSpeedRatioZ(float ratio){
  int x = 10;
  int y = 250;
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+25, y-24);
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.print("SPEED");

  M5.Display.setFont(&fonts::Font4);      //横14縦26
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+25, y);
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.printf("x%0.2f", ratio);
  M5.Display.fillTriangle(x,    y, x+10, y+10, x    , y+20, TFT_ORANGE);
  M5.Display.fillTriangle(x+10, y, x+20, y+10, x+10 , y+20, TFT_ORANGE);
}

void dispExpandRatioZ(float ratio){
  int x = 130;
  int y = 250;
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+25, y-24);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.print("EXPAND");

  M5.Display.setFont(&fonts::Font4);      //26
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+25, y);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.printf("x%0.2f", ratio); 
  M5.Display.fillTriangle(x, y+6, x+10, y, x+20, y+6, TFT_GREEN);
  M5.Display.fillRect(x+6, y+6, 8, 8, TFT_GREEN);
  M5.Display.fillTriangle(x, y+14, x+10, y+20, x+20, y+14, TFT_GREEN);
}

void dispTelemoroBpmZ(bool telemoloZ, int bpm){
  int x = 10;
  int y = 282;
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+0, y);
  M5.Display.setTextColor(TFT_PINK, TFT_BLACK);
  M5.Display.print("TELEMO");

  M5.Display.setFont(&fonts::Font4);      //26
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+65, y+8);
  if (telemoloZ){
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  } else {
    M5.Display.setTextColor(TFT_PINK, TFT_BLACK);
  }
  M5.Display.printf("%03d", bpm);
  M5.Display.setFont(&fonts::Font2); 
  M5.Display.printf(" bpm"); 
}

void dispTelemoroOnZ(){
  int x = 10;
  int y = 282;
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+28, y+18);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.print("ON  ");
#ifdef CAPTUREOK
  saveToSD_24bit("/zTelemoroON.bmp");
#endif 
}

void dispTelemoroOffZ(){
  int x = 10;
  int y = 282;
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(1.4);
  M5.Display.setCursor(x+28, y+18);
  M5.Display.setTextColor(TFT_PINK, TFT_BLACK);
  M5.Display.print("OFF");
#ifdef CAPTUREOK
  saveToSD_24bit("/zTelemoroOFF.bmp");
#endif 
}



bool saveToSD_24bit(String bmpFilename)
{
  bool result = false;
  File file = SD.open(bmpFilename, "w");
  if (file)
  {
    int width  = M5.Lcd.width();
    int height = M5.Lcd.height();

    int rowSize = (3 * width + 3) & ~ 3;

    lgfx::bitmap_header_t bmpheader;
    bmpheader.bfType = 0x4D42;
    bmpheader.bfSize = rowSize * height + sizeof(bmpheader);
    bmpheader.bfOffBits = sizeof(bmpheader);

    bmpheader.biSize = 40;
    bmpheader.biWidth = width;
    bmpheader.biHeight = height;
    bmpheader.biPlanes = 1;
    bmpheader.biBitCount = 24;
    bmpheader.biCompression = 0;

    file.write((std::uint8_t*)&bmpheader, sizeof(bmpheader));
    std::uint8_t buffer[rowSize];
    memset(&buffer[rowSize - 4], 0, 4);
    for (int y = M5.Lcd.height() - 1; y >= 0; y--)
    {
      M5.Lcd.readRect(0, y, M5.Lcd.width(), 1, (lgfx::rgb888_t*)buffer);
      file.write(buffer, rowSize);
    }
    file.close();
    result = true;
  }
  else
  {
    Serial.print("error:file open failure\n");
  }

  return result;
}