#include "jsonFile.h"

DeserializationError readJsonFiletoDoc(String file, JsonDocument& doc){
  File setlistFile = SD.open(file);
  DeserializationError error = deserializeJson(doc, setlistFile);  //ファイル(Stream)からデシリアライズ
  setlistFile.close();
  if (error) {
    Serial.print("readJsonFile failed: ");
    Serial.println(error.c_str());
  }
  return error;
}

String createSetlistCommand(JsonDocument& doc, int listNum){
  String setlistCommand = doc["setting"][listNum];
  return setlistCommand;
}

String readMyId(String file){
  StaticJsonDocument<48> doc;
  File idFile = SD.open(file);
  DeserializationError error = deserializeJson(doc, idFile);
  idFile.close();

  if (error) {
    Serial.print("readMyId failed: ");
    Serial.println(error.c_str());
    return "";
  } else {
    String readData = doc["crtid"];
    return readData;
  }    
}

String selectSetlistFile(String file){
  StaticJsonDocument<256> doc;
  File setListConfigFile = SD.open(file);
  DeserializationError error = deserializeJson(doc, setListConfigFile);
  setListConfigFile.close();

  if (error) {
    Serial.print("selectSetlistFile failed: ");
    Serial.println(error.c_str());
    return "";
  } else {
    int setlistFileNum = doc["setList"].size();
    int selectFileNum = 0;
    int startFileNum = 0;
    String filename[setlistFileNum];

    for (int i=0; i<setlistFileNum; i++){
      String fname = doc["setList"][i];
      filename[i] = fname;
    }

    M5.Display.setTextWrap(false);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0,120);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.printf("Select Setlist File!!");

    bool fileSelected = false;
    while(!fileSelected){
      M5.Display.setCursor(0,160);
      //M5.Display.printf("%d/%d\n",selectFileNum+1, setlistFileNum, filename[selectFileNum].c_str());

      if (selectFileNum > 4){
        startFileNum = selectFileNum - 4;
      } else {
        startFileNum = 0;
      } 
      
      for (int i=startFileNum; i<startFileNum+5; i++){
        if (i == selectFileNum){
            M5.Display.setTextColor(TFT_BLACK ,TFT_WHITE);
        }else{
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        M5.Display.printf("%s      \n",filename[i].c_str());
      }
      
      M5.update();
      if(M5.BtnC.wasPressed()){
        selectFileNum--;
        if (selectFileNum < 0) selectFileNum = 0;         
      }
      else if (M5.BtnA.wasPressed()){
        selectFileNum++;
        if (selectFileNum > setlistFileNum-1) selectFileNum = setlistFileNum-1;
      }
      else if (M5.BtnB.wasPressed()){
        fileSelected = true;
      }
    }    
    return filename[selectFileNum];
  } 
}

int recieveJson(String input, String myId, int *setlistNumber, int *sourceNumber, bool *mp4Pause, float *playSpeedRatio, float *mp4YRatio, int *timerTelemoro){
 //{"COMMAND":"set","A":17,"B":18,"C":19,"D":20,"E":21,"F":22,"G":23}
 //{"COMMAND":"pause"}
 //{"COMMAND":"resume"}
 //{"COMMAND":"expand","ratio":"0.5"}  縦倍率指定
 //{"COMMAND":"speed","ratio":"0.5"}  再生速度比率指定
 //{"COMMAND":"telemoro","bpm":"120"} テレモロタイマー値bpm
  StaticJsonDocument<192> doc;
  DeserializationError error = deserializeJson(doc, input);
  if (error) {
    Serial.print("recieveJson failed: ");
    Serial.println(error.c_str());
    return -1;
  }

  if (doc["COMMAND"] == "set"){ 
    int num = doc[myId];           //myId: A, B, C, D, E, F, G
    int set = doc["SET"];
  //  Serial.printf("recieved set:%d\n",set);
    *sourceNumber = num;
    *setlistNumber = set;    
    return 0;
  }

  if (doc["COMMAND"] == "pause"){ 
    *mp4Pause = true;
  //  Serial.printf("recieved pause:%d\n",mp4Pause); 
    return 0;
  }
  
  if (doc["COMMAND"] == "resume"){ 
    *mp4Pause = false;
  //  Serial.printf("recieved pause:%d\n",mp4Pause); 
    return 0;
  }

  if (doc["COMMAND"] == "speed"){
    float r = doc["ratio"];
  //  Serial.printf("recieved speed:%f\n",r);  
    *playSpeedRatio = r;
    return 0;
  }

  if (doc["COMMAND"] == "expand"){
    float r = doc["ratio"]; 
  //  Serial.printf("recieved expand:%f\n",r);  
    *mp4YRatio = r;
    return 0;
  }

  if (doc["COMMAND"] == "telemoro"){
    uint32_t t = doc["bpm"]; 
  //  Serial.printf("recieved telemolo command:%f\n",t);  
    *timerTelemoro = t;
    return 0;
  }

  return -1;
}

