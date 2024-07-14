/*

/CRT/CONFIG/          //各M5Stack毎に違う値を設定すること
  myId.json           //myId.jsonに、id用アルファベットを記述する
  myIdA.json          //テスト用　A,B,Z
  myIdB.json
  myIdZ.json
  configSetList.json  //親機"Z"だけ　使用するセットリスト設定ファイル名　記述する（複数作っておいて選択できるように)

/CRT/SETLIST/         //セットリスト　ID:A,B,C,D,E,F,G,Hに表示するソースの設定
  setList_1.json   <1024>
  setList_2.json   <1024>
  setList_3.json   <1024>

/CRT/JPG/             //jpgファイル　サイズは640x480にすること　CRTの表示は240x480になる
  KyoTansan.jpg
  RGB320x480.jpg
  RGB640x480.jpg
  Kyo640x480.jpg
  Kyo320x480.jpg

/CRT/MP4/BIN565_900data     //MP4から生成したdatファイル　RGB565形式(2byte)　1フレーム480ライン->980byte/frame をフレーム分繰り返し
                              すべてのM5STACKで共通データを持つこと
  arpeggioAhi_1minBIN565_900.dat
  arpeggioAlow_1minBIN565_900.dat

/CRT/WAV                    //WAVファイル　16bit/8bit　サンプリングレートはいくつでもOKだがデータ容量少ないほうがいいので16KHzがいい。
  koe_16bit_16KHz_mono.wav

/CRT/SOURCELIST/
  source.json   <4096>     //ソース番号とソースファイルを関連付けるファイル
*/

/*
コマンド データ受信したら”COMMAND”をキーにしてコマンド種類を確認する
"COMMAND":"set"     setListの指定
"COMMAND":"expand"  縦倍率指定
"COMMAND":"pause"   一時停止指定
"COMMAND":"resume"  一時停止解除
"COMMAND":"speed"   再生速度

 //{"COMMAND":"set","SET":1,"A":17,"B":18,"C":19,"D":20,"E":21,"F":22,"G":23}
 //{"COMMAND":"pause"}
 //{"COMMAND":"resume"}
 //{"COMMAND":"expand","ratio":"0.5"}  縦倍率指定
 //{"COMMAND":"speed","ratio":"0.5"}  再生速度比率指定

*/
#pragma once

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <M5Unified.h>


/**
 * @brief Jsonファイルを読みだしてJsondocにデシリアライズする
 * @param file    setlist用Jsonfile
 * @param doc     setlistJsondoc
 * @return　DeserializationError
 */
DeserializationError readJsonFiletoDoc(String file, JsonDocument& doc); 

/**
 * @brief Jsondocから指定行取り出す
 * @param doc     setlistJsondoc
 * @param listNum set Number
 * @return　String command ex:{"COMMAND":"set","A":17,"B":18,"C":19,"D":20,"E":21,"F":22,"G":23}
 */
String createSetlistCommand(JsonDocument& doc, int listNum);

/**
 * @brief 自分のIDをJson形式のファイルを開いて設定する
 * @param file   myId.json file
 * @return　String　ID　//myId: A, B, C, D, E, F, G, Z
 */
String readMyId(String file);

/**
 * @brief configSetlist.jsonファイルに記載の複数のセットリスト用jsonファイルから１つを選択する
 * @param input   configSetlist.json
 * @return String setlist.jsonファアイル名
 */
String selectSetlistFile(String file);

/**
 * @brief Json形式のコマンド判定してパラメータを書き換える
 * @param input   json command
 * @param myId    id myself
 * @param setlistNumber
 * @param sourceNumber      if command "set" , sourceNumber change
 * @param mp4Pause          mp4pause  ex:{"COMMAND":"pause"}
 * @param playSpeedRatio    
 * @param mp4YRatio
 * @param timerTelemoro     telemoro timer
 * @return OK 0, ERR -1
 */
int recieveJson(String input, String myId, int *setlistNumber, int *sourceNumber, bool *mp4Pause, float *playSpeedRatio, float *mp4YRatio, int *timerTelemoro);


  //memo
  //mode check
  // String mode2 = sourcelistJsonDoc["source"][sourceNumber]["mode"];
  // String sourceFile = sourcelistJsonDoc["source"][sourceNumber]["file"];
  // int frame = sourcelistJsonDoc["source"][sourceNumber]["frame"];
  // int volume = sourcelistJsonDoc["source"][sourceNumber]["volume"];
  // Serial.printf("mode:%s file:%s frame:%d volume:%d\n",mode.c_str(), sourceFile.c_str(),frame,volume);

  
  // /* setliset 送信コマンドテスト */
  // sendSetlistCommand = createSetlistCommand(setlistJsonDoc, setlistNumber);
  // /* setliset 受信テスト */
  // recieveJson(sendSetlistCommand, myId, &sourceNumber, &mp4Pause, &playSpeedRaito, &mp4YRatio);
  // Serial.printf("my source is %d\n",sourceNumber);    //setListコマンドから自分宛てのソース番号を抽出