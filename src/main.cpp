/**
 * 2024/2/10  V1.1完成
*/
#include <Arduino.h>
#include <SD.h>
#include <M5ModuleRCA.h>
#include <M5Unified.h>

#include <M5StackUpdater.h>   //SDupdater使うとき M5unifedより後
#include <ArduinoJson.h>
#include <esp_log.h>

#include "jsonFile.h"
#include "porthub.h"

#include "drawDisp.h"

String version = "V1.1";      //プログラム書き換え確認用のため画面にバージョン表示

//#define CAPTURE             //CAPTUREする  しなときはコメントアウト
//#define CAPTUREOK           //CAPTUREずみ

#define JPG_WIDTH   640
#define JPG_HEIGHT  480
#define FRAMERATE_US 33333    //フレーム周期
#define TELEMORO_BPM 0        //テレモロ周期初期値 //120bpm:62500usec     

#define ID_FILE             "/CRT/CONFIG/myId.json"
#define CONFIG_SETLIST_FILE "/CRT/CONFIG/configSetlist.json"
#define SOURCE_FILE         "/CRT/SOURCELIST/source.json"
#define SETLIST_FILE_PATH   "/CRT/SETLIST/"
#define MP4_DATA_PATH       "/CRT/MP4/"
#define WAV_DATA_PATH       "/CRT/WAV/"
#define JPG_DATA_PATH       "/CRT/JPG/"

/* PortHubに接続した番号 */
#define FADER_EXPAND_PORT         4     //上下伸長用  
#define FADER_SPEED_PORT          3     //再生速度用
#define FADER_TELEMOROT           1     //テレモロ速度用
#define DUALBUTTON_SET_PORT       2     //2ボタン　セットリスト　進む、戻る
#define DUALBUTTON_TELEMORO_PORT  0     //2ボタン テレモロ、一時停止

/* 各表示モード用のタスクをqueueで操作 */
QueueHandle_t xQueueMP4SpeedRatio;         //再生速度
QueueHandle_t xQueueMP4ExpandRatio;        //縦倍率
QueueHandle_t xQueueMP4DrawCrtExpandRatio; //縦倍率  MP4CRTtask用
QueueHandle_t xQueueMP4Start;              //スタート命令    
QueueHandle_t xQueueMP4Stop;               //ストップ命令
QueueHandle_t xQueueMP4Colse;              //ファイルクローズ確認
QueueHandle_t xQueueMP4FrameWait;          //フレームレート調整待ち
QueueHandle_t xQueueMP4pause;              //一時停止命令
QueueHandle_t xQueueMP4telemoro;           //テレモロ周期

QueueHandle_t xQueueMP4DrawCrtStop;        //CRT表示用ルーチン設定  V1.2
QueueHandle_t xQueueMP4DrawCrtStopped;     //CRT表示用ストップ完了確認  V1.2
QueueHandle_t xQueueMP4DrawCrtCrtCanvasUsing;     //CRT表示用キャンバス使用中  V1.2

QueueHandle_t xQueueWavSpeedRatio;         //再生速度
QueueHandle_t xQueueWavStart;              //スタート命令
QueueHandle_t xQueueWavStop;               //ストップ命令
QueueHandle_t xQueueWavFileColse;          //wavFileColse確認

QueueHandle_t xQueueJpgStart;              //Start 命令
QueueHandle_t xQueueJpgStop;               //Stop 命令
QueueHandle_t xQueueJpgColse;              //ファイルクローズ確認

QueueHandle_t xQueueIniStart;              //Start 命令
QueueHandle_t xQueueIniStop;               //Stop 命令
QueueHandle_t xQueueIniColse;              //ファイルクローズ確認

/* タイマー割込み用 1フレーム周期 */
hw_timer_t * timer = NULL;                                  //ハードウエアタイマ
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;       //ミューテックス  変数に同時アクセスしないようにする
volatile SemaphoreHandle_t semaphore;                       //セマフォ　割込みのタイミング同期に使う
volatile int timerCounter = 0;                              //割込み回数
uint timerus = FRAMERATE_US;                                //フレーム周期

/* hub Z端末にスイッチとフェーダつなぐ */
PortHub porthub;
uint8_t i2cPorthubAddress = IIC_ADDR1;
uint8_t HUB_ADDR[6] = {HUB1_ADDR, HUB2_ADDR, HUB3_ADDR, HUB4_ADDR, HUB5_ADDR, HUB6_ADDR};
bool i2CPortHubExist;                                       //外部ボタンとフェーダー接続確認

/* jsonDocument セットリストとソースリストはファイル読み込んだ後も維持する
 *   確保するサイズ確認-> https://arduinojson.org/v6/assistant/#/step1
 *   setlistの数　4096では20個はダメ 
 *   setlistFileの数は6個程度
 */
StaticJsonDocument<8192> sourcelistJsonDoc;  //souceList jsondocumentファイル  全端末で必要　ソース番号をキーにしてファイル名・モード・パラメータを取り出す
StaticJsonDocument<8192> setlistJsonDoc;     //setList jsondocumentファイル 　親端末で子機端末向けのコマンドを生成する

/* SDから読み込んだパラメータ */
String myId;                    //自分のID    SDからmyId.jsonを読み込んで設定
String setlistFileName;         //使用するsetlistファイルをsetlistListに記載した複数から１つ選択して決定する
String sendSetlistCommand;      //子機へ送信するセットリストコマンド
String drawMode;                //mode "MP4" "WAV" "JPG"

int setlistNumber;              //setlistのset番号
int recievedSetlistNumber;      //子機が受け取ったセットリストの番号
int preRecievedSetlistNumber = -1;
int maxSetlistNumber;           //setlistのセット数
int sourceNumber;               //sourcelistのソース番号
int maxSourceNumber;            //ソースリストのソース数

/* 画像表示用 */
//portMUX_TYPE crtCanvasMux = portMUX_INITIALIZER_UNLOCKED;       //ミューテックス  変数に同時アクセスしないようにする
static M5Canvas crtCanvas;        //ブラウン管表示用のスプライト
static M5Canvas preLcdCanvas;     //LCD表示用のスプライト、タスク分離のためcrtCanvasと同じものを作った
static M5Canvas LcdCanvas;        //LCD用スプライト320x240は容量不足する

uint8_t buffer[crtImageHeight*2];  //1列分の画像データ　rgb565は1データあたり2バイト　960バイト

/* MP4再生用 */
int mp4Frame = 0;                 //表示フレームカウント　フレーム数から読取り位置計算する
float mp4Floatframe = 0.0f;       //表示フレーム速度可変のためfloatにする、実際は整数値をmp4Frameに渡す。
float playSpeedRatio = 1.0f;      //再生速度変更のためフレーム増分を小数にする 2.0fならOK。
                                  //大きくしすぎるとフレームレート下がる調整前に30fpsより速く回ること確認（SDの読み出し位置が遠くなると読むのが遅い？）
                                  //WAVのときはサンプリングレートを変えて再生速度を変える
float prePlaySpeedRatio = playSpeedRatio;                               
bool mp4Pause = false;            //MP4一時停止
float mp4YRatio = 1.0;            //表示可変縦倍率 rotationZoomで指定
float preMp4YRatio = mp4YRatio;

float playSpeedRatioZ = 1.0f;
float mp4YRatioZ = 1.0f;

/* テレモロ用タイマー */
hw_timer_t * timer1 = NULL;
portMUX_TYPE timer1Mux = portMUX_INITIALIZER_UNLOCKED;      //ミューテックス  変数に同時アクセスしないようにする   
volatile bool modeTelemolo = false;                         //テレモロ動作時の点滅表示用　表示非表示指定
int mp4TelemoroBpm = TELEMORO_BPM;                          //テレモロタイマー値　子機は0のときテレモロなしと判断する
int preMp4TelemoroBpm = mp4TelemoroBpm;
bool telemoloZ = false;                                     //親機のテレモロ動作指定
int telemoroBpmZ = 0;                                       //親機のテレモロbpm
/*テレモロ動作設定はTimer1で制御する
*  timerAlarmWrite(timer1, timerTelemoro, true);            //タイマ周期設定
*  timerAlarmEnable(timer1);                                //実際はコマンドでタイマースタートさせる     
*/

uint32_t fpsCount = 0;            //fps表示用
uint32_t fpsSec = 0;              //fps表示用

/* serialコマンド受信用 */
String serialRecieveString = "";  // a String to hold incoming data
bool stringComplete = false;      // whether the string is complete  LF受信したらTURE
bool commandReciebeEnable = true; //子機シリアル受信有効　マニュアル動作時は無効
      
void actionSourceChanged(int setlistNum, int sourceNum);  //souce番号変更時現在タスク停止し、モード設定して新タスク開始 
void serial2Event();                  //シリアル受信文字列確認
void checkButton();                   //子機ボタンの確認　ソースファイル変更と一時停止
void checkButtonZ();                  //親機ボタンの確認　セット変更と一時停止
void checkSerialCommand();            //シリアルからのコマンド受信と処理
void actionSetChangedZ();             //セット変更後の処理
void checkSetDualButton(int hubNumButton, int hubNumFader);   //親機の外付けボタンモジュール押したときの対応 進む・戻る
void checkTelemoroButton(int hubNumButton, int hubNumFader);  //親機の外付けボタンモジュール押したときの対応 停止・トレモロ
void setSpeedRatio(int hubNumFader);                          //Fader でフレームレート倍率設定　0.5~1.0~2.0　倍
void setExpandRatio(int hubNumFader);                         //Fader で画像縦倍率設定　0.5~1.0~2.0　倍
void setTelemoroSpeed(int hubNumFader);                       //Fader でテレモロタイマー値設定 30-120-184bpm

void setTelemoroTimer(int bpm);     //テレモロタイマーセット
void stopTelemoroZ();

void sendCurrentSetting();          //現在の設定情報送信

void taskStop(String mode);                         //タスク停止命令
void startNewTask(String mode, int sourceNumber);   //タスク開始命令

/* play mode別のtask */
void taskPlayWav(void* arg);
void taskDrawMp4(void* arg);
void taskDrawJpg(void* arg);
void taskDrawIni(void* arg);
void taskDrawCrt(void* arg);      //CRT表示用タスク トレモロ高速動作用にMP4タスクから独立させた(1フレーム内で点滅させるため)

/* frame timer interrupt */
void IRAM_ATTR onTimer() {                        //フレーム周期
  portENTER_CRITICAL_ISR(&timerMux);              //ミューテックスを利用して排他制御
  timerCounter ++;                                //カウンタは特に使っていない->定期送信に使用
  portEXIT_CRITICAL_ISR(&timerMux);
  xSemaphoreGiveFromISR(semaphore, NULL);         //セマフォを開放  1フレーム周期で動作させるためメインルーチンでセマフォ解放待ちさせる
}

/* telemoro timer interrupt */
void IRAM_ATTR onTimer1() {                       //テレモロ周期
  portENTER_CRITICAL_ISR(&timer1Mux);             //ミューテックスを利用して排他制御
  modeTelemolo = !modeTelemolo;
  portEXIT_CRITICAL_ISR(&timer1Mux);
}

void setup(void)
{
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;           // default=115200. if "Serial" is not needed, set it to 0.
  cfg.clear_display = true;               // default=true. clear the screen when begin.
  cfg.output_power  = true;               // default=true. use external port 5V output.  //falseにしても5VでてたBasic1.0
  cfg.external_speaker.module_rca = true; // default=false. use ModuleRCA AudioOutput
  cfg.external_display.module_rca = true; // default=true. use ModuleRCA VideoOutput

#if defined ( __M5GFX_M5MODULERCA__ ) // setting for Module RCA.
  cfg.module_rca.logical_width  = crtImageWidth;  //320:OK 480:表示OK,SD読めなくなる"card mount failed"　640:NG 
  cfg.module_rca.logical_height = crtImageHeight;
  cfg.module_rca.output_width   = crtImageWidth;
  cfg.module_rca.output_height  = crtImageHeight;
  cfg.module_rca.signal_type    = M5ModuleRCA::signal_type_t::NTSC_J;       //  NTSC / NTSC_J / PAL_M / PAL_N _Jの方が黒が黒い
  cfg.module_rca.use_psram      = M5ModuleRCA::use_psram_t::psram_no_use; // psram_no_use / psram_half_use
  cfg.module_rca.pin_dac        = GPIO_NUM_26;
  cfg.module_rca.output_level   = 128;
  //LCDはM5Display あるいは M5Displays(0)、CRTはM5Displays(1)
#endif
  cfg.external_speaker.module_rca = true;
  M5.begin(cfg);                                  // begin M5Unified.

  #ifdef COLOR_DEPTH16                          //16bitカラーが基本、8bitカラーにするとJPEG表示の分解能をあげられる
    M5.Display.setColorDepth(16);
    M5.Displays(1).setColorDepth(16);
  #else
    M5.Display.setColorDepth(8);
    M5.Displays(1).setColorDepth(8);
  #endif

  // 空きメモリ確認
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DMA):%d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DMA):%d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA) );

  porthub.begin();

  checkSDUpdater(
    SD,           // filesystem (default=SD)
    MENU_BIN,     // path to binary (default=/menu.bin, empty string=rollback only)
    0,            // wait delay, (default=0, will be forced to 2000 upon ESP.restart() )
    TFCARD_CS_PIN // usually default=4 but your mileage may vary
  );

  //実際に使用するのはSerial2, 開発用にSerial(USB)を使う
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // Init serial port 2.
  serialRecieveString.reserve(200);           // reserve 200 bytes for the inputString:
                                              //https://garretlab.web.fc2.com/arduino.cc/www/reference/ja/language/variables/data-types/stringobject/functions/reserve/

  if (!SD.begin()) {                          // Initialize the SD card.
    dispSdError();
    Serial.println("Card Mount Failed");
    while (1);
  }
  
  /* read myID */
  M5.Display.setBrightness(255);
  M5.Display.clear();
  M5.Display.setTextWrap(false);
  M5.Display.setRotation(2);        // 回転方向を 0～3 の4方向から設定します。(4～7を使用すると上下反転になります。)
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(5,5);
  myId = readMyId(ID_FILE);        //myId.jsonファイルを読み込んでIDを設定
  M5.Display.printf("myId is\n");
  
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  M5.Display.printf(" %s",myId.c_str());

  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

#ifdef CAPTUREOK
  saveToSD_24bit("/myIdZ.bmp");
#endif
 
  /* setlist.jsonの選択 端末ID:Zだけ */
  if (myId == "Z"){ 
    drawSetlistSelectbutton();
    DeserializationError error;
    do {
      setlistFileName = selectSetlistFile(CONFIG_SETLIST_FILE);  //セットリストファイル一覧からファイルを選択
      Serial.printf("setlist:%s selected\n",setlistFileName.c_str());
      error = readJsonFiletoDoc(SETLIST_FILE_PATH + setlistFileName, setlistJsonDoc);
      if (error){
        M5.Display.setCursor(0,120);
        M5.Display.setTextColor(RED, BLACK);
        M5.Display.setTextSize(2);
        M5.Display.printf("Setlist File Error!!,%s",setlistFileName.c_str());
#ifdef CAPTUREOK
  saveToSD_24bit("/setlistFileError.bmp");
  while(1);
#endif
        delay(1000);   //エラーの場合は再選択 json用エリアが小さいかも？
      }    
    } while (error);

#ifdef CAPTUREOK
  saveToSD_24bit("/selectSetlist.bmp");
#endif
    delay(1000);
    
    maxSetlistNumber = setlistJsonDoc["setting"].size() - 1;
    Serial.printf("setlist Maxnum:%d\n",maxSetlistNumber);
    setlistNumber = -1;      //セットリストの初期値は-1
  }

  /* soucelistの読み込み, souceNumber,mode 初期化 */
  DeserializationError error;
  error = readJsonFiletoDoc(SOURCE_FILE, sourcelistJsonDoc);
  if (error){
    M5.Display.setCursor(0,120);
    M5.Display.setTextColor(RED, BLACK);
    M5.Display.setTextSize(2);
    M5.Display.println("Sourcelist File Error!!");
#ifdef CAPTUREOK
  saveToSD_24bit("/sourcelistFileError.bmp");
#endif
    while(1);
  } else {
    maxSourceNumber = sourcelistJsonDoc["source"].size() - 1;
    Serial.printf("source Maxnum:%d\n",maxSourceNumber);
    sourceNumber = -1;                //初期値は-1（未決定)
    drawMode = "INI";                 //初期画面モード（テストチャート表示)
  } 

  if (myId == "Z"){
    Serial.printf("Checki2c\n");
    Wire.beginTransmission(i2cPorthubAddress);
    uint8_t i2cError = Wire.endTransmission();
    if (i2cError == 0) {
      i2CPortHubExist = true;
      Serial.printf("Found:0x%0X\n",i2cPorthubAddress); 
      porthub.hub_wire_setBrightness(HUB_ADDR[FADER_SPEED_PORT], 30);
      porthub.hub_wire_setBrightness(HUB_ADDR[FADER_EXPAND_PORT], 30);
      porthub.hub_wire_setBrightness(HUB_ADDR[FADER_TELEMOROT], 30);     
    } else {
      i2CPortHubExist = false;
      Serial.printf("Not Found:0x%0X\n",i2cPorthubAddress);
    }
  }

  if (myId != "Z"){
    /* create Sprite */
    crtCanvas.setColorDepth(16);
    LcdCanvas.createSprite(140, 105);
    crtCanvas.createSprite(1,480);
    preLcdCanvas.createSprite(1,480);
    Serial.printf("CRT x:%d,y:%d\r\n",cfg.module_rca.logical_width, cfg.module_rca.logical_height);
    Serial.printf("CRTCANVAS x:%d,y:%d\r\n",crtCanvas.width(), crtCanvas.height());

    /* Queue setting */
    xQueueWavSpeedRatio   = xQueueCreate(1, sizeof(float)); 
    xQueueWavStart        = xQueueCreate(1, sizeof(int)); 
    xQueueWavStop         = xQueueCreate(1, sizeof(int));
    xQueueWavFileColse    = xQueueCreate(1, sizeof(int)); 
    xQueueMP4SpeedRatio   = xQueueCreate(1, sizeof(float)); 
    xQueueMP4ExpandRatio  = xQueueCreate(1, sizeof(float));
    xQueueMP4DrawCrtExpandRatio = xQueueCreate(1, sizeof(float));
    xQueueMP4Start        = xQueueCreate(1, sizeof(int));
    xQueueMP4Stop         = xQueueCreate(1, sizeof(int)); 
    xQueueMP4Colse        = xQueueCreate(1, sizeof(int)); 
    xQueueMP4FrameWait    = xQueueCreate(1, sizeof(int)); 
    xQueueMP4pause        = xQueueCreate(1, sizeof(int));
    xQueueMP4telemoro     = xQueueCreate(1, sizeof(int));
    xQueueMP4DrawCrtStop  = xQueueCreate(1, sizeof(int));  
    xQueueMP4DrawCrtStopped = xQueueCreate(1, sizeof(int));   
    xQueueMP4DrawCrtCrtCanvasUsing = xQueueCreate(1, sizeof(int));
    xQueueJpgStart        = xQueueCreate(1, sizeof(int)); 
    xQueueJpgStop         = xQueueCreate(1, sizeof(int));
    xQueueJpgColse        = xQueueCreate(1, sizeof(int));
    xQueueIniStart        = xQueueCreate(1, sizeof(int)); 
    xQueueIniStop         = xQueueCreate(1, sizeof(int));
    xQueueIniColse        = xQueueCreate(1, sizeof(int));

    /* draw task start */
    xTaskCreatePinnedToCore(taskPlayWav, "taskPlayWav", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskDrawMp4, "taskDrawMp4", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskDrawJpg, "taskDrawJpg", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskDrawIni, "taskDrawIni", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskDrawCrt, "taskDrawCrt", 4096, NULL, 1, NULL, 1);      
  }

  /* timer setting */
  semaphore = xSemaphoreCreateBinary();           // バイナリセマフォ作成
  timer = timerBegin(0, 80, true);                // タイマ作成 80MHzを80分周
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, timerus, true);          //タイマ周期設定
  timerAlarmEnable(timer);

  /* telemoro timer setting */
  timer1 = timerBegin(1, 80, true);                // タイマ作成 80MHzを80分周
  timerAttachInterrupt(timer1, &onTimer1, true);
 // timerAlarmWrite(timer1, mp4TimerTelemoro, true);          //タイマ周期設定
 // timerAlarmEnable(timer1);            //実際はコマンドでタイマースタートさせる              
  
  /* display setting */
  M5.Display.clearDisplay();
  M5.Display.setRotation(2);
  dispMyId(myId);
  dispVersion(version);

  /* draw start 初期設定の表示 */
  if (myId == "Z"){                                           
    drawButtonZ();  
    dispSetNumberZ(setlistNumber, maxSetlistNumber);
    dispSetlistFileNameZ(setlistFileName);                     
  } else {
    recievedSetlistNumber = -1;
    dispRecievedSetNumber(recievedSetlistNumber);
    dispSourceNumber(sourceNumber);
    drawButton();
    startNewTask(drawMode, sourceNumber);
#ifdef CAPTUREOK
   saveToSD_24bit("/iniScreen.bmp");
#endif                        
  }

  // 空きメモリ確認
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DMA):%d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DMA):%d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA) );
}

void loop(void)
{
  if (myId == "Z"){
    uint32_t sendCuresettingTime = millis();
    while(1){
      //xSemaphoreTake(semaphore, portMAX_DELAY);                     //ボタンデータ取得速すぎるので、フレームレートごとに確認
      checkButtonZ();
      if (i2CPortHubExist){
        checkSetDualButton(DUALBUTTON_SET_PORT, FADER_EXPAND_PORT);
        checkTelemoroButton(DUALBUTTON_TELEMORO_PORT, FADER_TELEMOROT);
        setSpeedRatio(FADER_SPEED_PORT);
        setExpandRatio(FADER_EXPAND_PORT);
        setTelemoroSpeed(FADER_TELEMOROT);
      }
      if ((millis() - sendCuresettingTime) > 30){
        sendCurrentSetting();       //現在設定　定期送信
        sendCuresettingTime = millis();
      }
    }
  } else {
    while(1){
      checkButton();
      if (commandReciebeEnable) checkSerialCommand();
      xSemaphoreTake(semaphore, portMAX_DELAY);  //フレームレート調整　MP4taskでセマフォ取得しようとしたらエラーでたのでメインで取得後Queue送る
      if ((drawMode == "MP4") && !mp4Pause){
        int frame =1; 
        xQueueSend(xQueueMP4FrameWait, &frame, 0);
      }

      if ((drawMode == "MP4") && mp4Pause){     //PUASEしてるかどうかわからなかったのでCRTに表示するようにした
         M5.Displays(1).setFont(&fonts::Font0); //常時描画しているので点滅している
         M5.Displays(1).setTextColor(TFT_GREEN);
         M5.Displays(1).setCursor(10,50);
         M5.Displays(1).setTextSize(1,4);
         M5.Displays(1).printf("PAUSE");
      }
    }
  }
}


/**
 * @brief 親機から定期的に現在設定送信
 * @return none
 */
void sendCurrentSetting(){
  if ((timerCounter % 30) == 0){              //30フレームごとにソース番号コマンド送信する
    if(setlistNumber >= 0){                   //初期値-1のときは送らなくていい  
      actionSetChangedZ();                    //ソース受信ミス、ソース変更時の子機の謎のリセットかかってもソース番号読み込んで復帰するため
    }
  }

  if ((timerCounter % 30) == 5){             //テレモロ情報を送信する　5:送信タイミングをずらしている
    if (telemoloZ) {
      Serial2.printf("{\"COMMAND\":\"telemoro\",\"bpm\":\"%d\"}\n", telemoroBpmZ);
    } else {
      Serial2.printf("{\"COMMAND\":\"telemoro\",\"bpm\":\"%d\"}\n", 0);
    }
    delay(100); 
  }

  if ((timerCounter % 30) == 10){             //縦倍率情報を送信する
    Serial2.printf("{\"COMMAND\":\"expand\",\"ratio\":\"%0.2f\"}\n", mp4YRatioZ);
    delay(100);
  }

  if ((timerCounter % 30) == 15){             //再生速度情報を送信する
    Serial2.printf("{\"COMMAND\":\"speed\",\"ratio\":\"%0.2f\"}\n", playSpeedRatioZ);
    delay(100);
  }
}

/* 
 * 120bpm / 60sec = 2Hz  なぜか4倍数で表示するようなので8Hz
 * タイマー値　 30bpm 1000000usec/(30/60*4)/2=250,000
 * タイマー値　120bpm 1000000usec/(120/60*4)/2=62,500
 * タイマー値　184bpm 1000000usec/(184/60*4)/2=40,760
 * bpm = 1000000*60/(t*2*4)
 */

/**
 * @brief テレモロ動作 timer1設定
 * @param bpm トレモロ周期　0はテレモロオフ
 * @return none
 */
void setTelemoroTimer(int bpm){
  if(bpm){                                        //タイマ値0ならトレモロなし
      int timerCount = 125000*60/bpm;             //1000000/(t/60*4)/2;                  
      timerAlarmDisable(timer1);                  //タイマ止めなくても設定変更できるが一応止めてから更新
      timerAlarmWrite(timer1, timerCount, true);  //タイマ周期設定
      timerAlarmEnable(timer1);            
  } else {
      timerAlarmDisable(timer1);                  //タイマ停止
      portENTER_CRITICAL_ISR(&timer1Mux);         //ミューテックスを利用して排他制御
        modeTelemolo = false;                     //テレモロモードFlase-> テレモロなし常時表示にセットする
      portEXIT_CRITICAL_ISR(&timer1Mux);                    
  }
}

/**
 * @brief Faderを読み込んで、フレームレート倍率speed命令を出す
 * @param hubNumFader 接続先ボートハブ番号
 * @return none
 */
void setSpeedRatio(int hubNumFader){
  int faderValue;
  float rate;
  static float preRate = -1.00f;
  static int ledCount = 0;
  static int sendAfterCount = 100;
  faderValue = porthub.hub_a_read_value(HUB_ADDR[hubNumFader]);
  rate =(float) ((int)((0.5 * pow(2, ((float)faderValue/2048.0f))+0.005f)*100))/100.0f;
  rate = 1/rate;    //Fader上下反転
  if ((rate > 0.975f) && (rate < 1.025f)){
    rate = 1.00;
  }
  
  dispSpeedRatioZ(rate);

  if ((abs(rate-preRate) > 0.019f) && ((sendAfterCount > 1))){                   //送信頻度を下げる
    Serial2.printf("{\"COMMAND\":\"speed\",\"ratio\":\"%0.2f\"}\n", rate);
    delay(100);
    preRate = rate;
    sendAfterCount = 0;
    playSpeedRatioZ = rate;         //親機変数に保存（本当は関数で返したかったがとりあえず)
  } 

  sendAfterCount++;
  if(sendAfterCount>100) sendAfterCount=100;    //上限リミット

  ledCount++;                                   //FaderLED点滅用
  if (ledCount > (int)(33.3f / rate)){
    porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 0, 0, 0);
    ledCount = 0;
  }

  if (ledCount == (int)(16.6f / rate)) {
    porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 255, 0); 
  } 
}

/**
 * @brief Faderを読み込んで、画像縦倍率expand命令を出す
 * @param hubNumFader 接続先ボートハブ番号
 * @return none
 */
void setExpandRatio(int hubNumFader){
  int faderValue;
  float rate;
  static float preRate = -1.00f;
  static int sendAfterCount = 100;
  faderValue = porthub.hub_a_read_value(HUB_ADDR[hubNumFader]);
  rate =(float) ((int)((0.5 * pow(2, ((float)faderValue/2048.0f))+0.005f)*100))/100.0f;
  //スライダーコネクタ反対側が縮む方向　音が高い側
  //rate = 1/rate;    //Fader上下反転
  if ((rate > 0.965f) && (rate < 1.035f)){
    rate = 1.00;
  }
 
  if((abs(rate-preRate) > 0.009f) && ((sendAfterCount > 1))) {
    Serial2.printf("{\"COMMAND\":\"expand\",\"ratio\":\"%0.2f\"}\n", rate);
    delay(100);
    dispExpandRatioZ(rate);
    preRate = rate;
    sendAfterCount = 0;
    mp4YRatioZ = rate;        //親機変数に保存（本当は関数で返したかったがとりあえず)
  }
  sendAfterCount++;
  if(sendAfterCount>100) sendAfterCount=100;    //上限リミット
}

/**
 * @brief Faderを読み込んで、テレモロ動作命令を出す　30-120-184bpm
 * @param hubNumFader 接続先ボートハブ番号
 * @return none
 */
void setTelemoroSpeed(int hubNumFader){
  int faderValue;
  int bpm;
  static int preBpm = -10;
  static int sendAfterCount = 100;
  faderValue = porthub.hub_a_read_value(HUB_ADDR[hubNumFader]);

  //bpm値はFader値に対してリニアに設定する コネクタ反対側がトレモロ速い(高音側)
  if (faderValue <= 2048){
    bpm = map(faderValue, 2, 2048, 184, 120);
  } else {
    bpm = map(faderValue, 2048, 4090, 120, 30);
  }
  //中央の120bpmに固定しやすくする
  if ((bpm > 117) && (bpm < 122)){
    bpm = 120;
  } 

  if((abs(bpm-preBpm) > 1) && ((sendAfterCount > 2))) {
    if(telemoloZ){
      Serial2.printf("{\"COMMAND\":\"telemoro\",\"bpm\":\"%d\"}\n", bpm);    //{"COMMAND":"telemoro","bpm":"120"}
      delay(100);
    }
    preBpm = bpm;
    telemoroBpmZ = bpm;
    sendAfterCount = 0;
    dispTelemoroBpmZ(telemoloZ, telemoroBpmZ); 
  }
  sendAfterCount++;
  if(sendAfterCount>100) sendAfterCount=100;    //上限リミット
}

/**
 * @brief dualボタンの状態を読み込んで、セットとポーズ命令（Faderの色を変える)
 * @param hubNumButton ボタン接続先ボートハブ番号
 * @param hubNumFader LED接続先ボートハブ番号(Fader内LED)
 * @return none
 */
void checkSetDualButton(int hubNumButton, int hubNumFader){
  static int buttonBlueCount = 0; 
  static int buttonRedCount = 0;
  static int buttonBlueRedCount = 0;
  static bool buttonBlueRedPushed = false;

  if (!buttonBlueRedPushed){                                              //BLUE RED　両押しからはリリース後にカウント
    if (porthub.hub_d_read_value_A(HUB_ADDR[hubNumButton])==LOW){         //BLUE BUTTON PUSHED
        buttonBlueCount++;
        if (buttonBlueCount>100) buttonBlueCount=100;   //上限リミット
          if (buttonBlueCount == 2) {
            porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 0, 0, 255 );         //FADER LED BLUE
            setlistNumber++;
            actionSetChangedZ();
            mp4Pause = false;
            erasePause();
            if (telemoloZ) stopTelemoroZ();
            delay(100);
            while(porthub.hub_d_read_value_A(HUB_ADDR[hubNumButton])==LOW){};     
          }
    } else {
        buttonBlueCount = 0;
    }

    if (porthub.hub_d_read_value_B(HUB_ADDR[hubNumButton])==LOW){                    //RED BUTTON PUSHED
        buttonRedCount++;
        if (buttonRedCount>100) buttonRedCount=100;   //上限リミット
        if (buttonRedCount == 2){
          porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 0, 0 );         //FADER LED RED
          setlistNumber--;
          actionSetChangedZ();
          mp4Pause = false;
          erasePause();
          if (telemoloZ) stopTelemoroZ();
          delay(100);
          while(porthub.hub_d_read_value_B(HUB_ADDR[hubNumButton])==LOW){};        
        }
    } else {
        buttonRedCount = 0;
    }
  }
  
  if ((porthub.hub_d_read_value_A(HUB_ADDR[hubNumButton])==HIGH) && (porthub.hub_d_read_value_B(HUB_ADDR[hubNumButton])==HIGH)){
      porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 0, 255, 0);
      buttonBlueRedPushed = false;
  }
};

/**
 * @brief dualボタンの状態を読み込んで、テレモロとポーズ命令（Faderの色を変える)
 * @param hubNumButton ボタン接続先ボートハブ番号
 * @param hubNumFader LED接続先ボートハブ番号(Fader内LED)
 * @return none
 */
void checkTelemoroButton(int hubNumButton, int hubNumFader){
  static int buttonBlueCount = 0; 
  static int buttonRedCount = 0;
  static int buttonBlueRedCount = 0;
  static bool buttonBlueRedPushed = false;

  if (!buttonBlueRedPushed){                                                  //BLUE RED　両押しからはリリース後にカウント
    //トレモロ動作切り替え
    if (porthub.hub_d_read_value_A(HUB_ADDR[hubNumButton])==LOW){             //BLUE BUTTON PUSHED
        buttonBlueCount++;
        if (buttonBlueCount>100) buttonBlueCount=100;                         //上限リミット
        if (buttonBlueCount == 2) {
          porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 0, 0, 255 ); //FADER LED BLUE
          telemoloZ = !telemoloZ;
          dispTelemoroBpmZ(telemoloZ, telemoroBpmZ);
          if (telemoloZ){
            if(mp4Pause){             //ポーズ中にテレモロが来たらポーズ停止　コマンド送ってからテレモロコマンド
              mp4Pause = false;
              erasePause();
              Serial2.printf("{\"COMMAND\":\"resume\"}\n");                             //{"COMMAND":"resume"}
              delay(100);
            }
            porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 0, 255);
            dispTelemoroOnZ();
            Serial2.printf("{\"COMMAND\":\"telemoro\",\"bpm\":\"%d\"}\n", telemoroBpmZ);  //テレモロ開始命令　現在のbpm値送信
            delay(100);
          } else {
            porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 255, 0);
            stopTelemoroZ();
          }
        }
    } else {
        buttonBlueCount = 0;
    }
    //一時停止・再生切り替え
    if (porthub.hub_d_read_value_B(HUB_ADDR[hubNumButton])==LOW){                     //RED BUTTON PUSHED
        buttonRedCount++;
        if (buttonRedCount>100) buttonRedCount=100;                                   //上限リミット
        if (buttonRedCount == 2){
          porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 0, 0 );         //FADER LED RED
          mp4Pause = !mp4Pause;
          if (mp4Pause){
            if (telemoloZ){         //テレモロON中にpauseが来たらトレモロ停止　コマンド送ってからPuaseコマンド
              stopTelemoroZ();
            }
            porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 0, 255);
            dispPause();
            Serial2.printf("{\"COMMAND\":\"pause\"}\n");                              //{"COMMAND":"pause"}
            delay(100);
          } else {
            porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 255, 0);
            erasePause();
            Serial2.printf("{\"COMMAND\":\"resume\"}\n");                             //{"COMMAND":"resume"}
            delay(100);
          }
        }
    } else {
        buttonRedCount = 0;
    }
  }
  
  if ((porthub.hub_d_read_value_A(HUB_ADDR[hubNumButton])==HIGH) && (porthub.hub_d_read_value_B(HUB_ADDR[hubNumButton])==HIGH)){
      porthub.hub_wire_index_color(HUB_ADDR[hubNumFader], 1, 255, 192, 203);  //TFT_PINK
      buttonBlueRedPushed = false;
  }
};

/**
 * @brief テレモロ停止
 * @return none
 */
void stopTelemoroZ(){
  telemoloZ = false;    //テレモロ停止
  dispTelemoroOffZ();
  dispTelemoroBpmZ(telemoloZ, telemoroBpmZ); 
  Serial2.printf("{\"COMMAND\":\"telemoro\",\"bpm\":\"%d\"}\n", 0);             //テレモロ停止命令　bpmを0で送信
  delay(100);
}


/**
 * @brief serialからのコマンド受信して応答処理する
 * @return none
 */
void checkSerialCommand(){
  serial2Event();
  if (stringComplete) {
    Serial.print("r:" + serialRecieveString);
    recieveJson(serialRecieveString, myId, &recievedSetlistNumber, &sourceNumber, &mp4Pause, &playSpeedRatio, &mp4YRatio, &mp4TelemoroBpm);
    serialRecieveString = "";
    stringComplete = false;
  
    Serial.printf("set:%d, source:%d, pause:%d, speed:%f, ratio:%f, timer:%d\n",recievedSetlistNumber, sourceNumber, mp4Pause, playSpeedRatio, mp4YRatio, mp4TelemoroBpm);

    if (recievedSetlistNumber != preRecievedSetlistNumber){                  //setlist変更
      preRecievedSetlistNumber = recievedSetlistNumber;
      actionSourceChanged(recievedSetlistNumber, sourceNumber);
      mp4Pause = false;
    }   

    if (playSpeedRatio != prePlaySpeedRatio){                                //再生速度変更
      prePlaySpeedRatio = playSpeedRatio;
      if (drawMode == "MP4"){
        int ret = xQueueSend(xQueueMP4SpeedRatio, &playSpeedRatio, 0);       //MP4の再生タスクに送信
      }
      if (drawMode == "WAV"){
        int ret = xQueueSend(xQueueWavSpeedRatio, &playSpeedRatio, 0);       //wav再生タスクに送信
      }
    }

    if (mp4YRatio != preMp4YRatio){                                          //縦表示倍率変更
      if (drawMode == "MP4"){
        preMp4YRatio = mp4YRatio;
        int ret = xQueueSend(xQueueMP4ExpandRatio, &mp4YRatio, 0);           //MP4の再生タスクに送信
      }
    }

    if (drawMode == "MP4") {                                                //MP4 pause状態は受信の都度送信
        xQueueSend(xQueueMP4pause, &mp4Pause, 0);
    }

    if (mp4TelemoroBpm != preMp4TelemoroBpm){
      if (drawMode == "MP4"){
        preMp4TelemoroBpm = mp4TelemoroBpm;
        int ret = xQueueSend(xQueueMP4telemoro, &mp4TelemoroBpm, 0);      //MP4の再生タスクに送信
      }
    }
//    Serial.println("----");
  }
}

/**
 * @brief ボタン確認 ソースファイル変更と一時停止
 * @return none
 */
void checkButton(){
  M5.update();    //ボタン情報更新
  if (M5.BtnA.wasPressed()) {         //Aボタンでファイル変更 
    timerAlarmDisable(timer);
    sourceNumber--;
    if (sourceNumber < 0) { sourceNumber = maxSourceNumber; }
    mp4Pause = false;    
    actionSourceChanged(-1, sourceNumber);         
    timerAlarmEnable(timer);
    commandReciebeEnable = false;     //マニュアルでソース変更した場合、シリアルコマンド受信しない
    preRecievedSetlistNumber = -1;    //ソース変更後、コマンド受信有効にしたときにセットリスト変更できるよう現状セットリストをｰ1にしておく
  }
  else if (M5.BtnB.wasPressed()) {    //Bボタンでポーズ
    if (drawMode == "MP4") {
      mp4Pause = !mp4Pause;
      xQueueSend(xQueueMP4pause, &mp4Pause, 0);
    }
    commandReciebeEnable = true;
  }
  else if (M5.BtnC.wasPressed()) {    //Cボタンでファイル変更  
    timerAlarmDisable(timer);
    mp4Pause = false;  
    sourceNumber++;
    if (sourceNumber> maxSourceNumber) { sourceNumber = 0; }
    actionSourceChanged(-1, sourceNumber);      
    timerAlarmEnable(timer);
    commandReciebeEnable = false;
    preRecievedSetlistNumber = -1;
  }
}

/**
 * @brief ボタン確認 ソースファイル変更と一時停止
 * @return none
 */
void checkButtonZ(){
  M5.update();                        //ボタン情報更新
  if (M5.BtnA.wasPressed()) {         
    timerAlarmDisable(timer);
    setlistNumber--;
    actionSetChangedZ();
    mp4Pause = false;
    erasePause();
    if (telemoloZ) stopTelemoroZ();  
    timerAlarmEnable(timer);

  }
  else if (M5.BtnB.wasPressed()) {    //Bボタンでポーズ
      mp4Pause = !mp4Pause;   
      if (mp4Pause){
        if (telemoloZ) stopTelemoroZ();  
        dispPause();
        Serial2.printf("{\"COMMAND\":\"pause\"}\n");    //{"COMMAND":"pause"}
        delay(100);
#ifdef CAPTUREOK
  saveToSD_24bit("/zPause.bmp");
#endif    
      } else {
        erasePause();
        Serial2.printf("{\"COMMAND\":\"resume\"}\n");   //{"COMMAND":"resume"}
        delay(100);
#ifdef CAPTUREOK
  saveToSD_24bit("/zResume.bmp");
#endif
      }
  }  
  else if (M5.BtnC.wasPressed()) {     //Cボタンでファイル変更
    timerAlarmDisable(timer);
    setlistNumber++;
    actionSetChangedZ();
    mp4Pause = false;
    erasePause();
    if (telemoloZ) stopTelemoroZ();  
#ifdef CAPTUREOK
  saveToSD_24bit("/iniScreenZ.bmp");
#endif     
    timerAlarmEnable(timer);
  }
}

/**
 * @brief セット変更後の処理、セットリストコマンド送信
 * @return none
 */
void actionSetChangedZ(){
  if (setlistNumber > maxSetlistNumber) { setlistNumber = 0; }                  //セットリスト番号上下限処理
  if (setlistNumber < 0) { setlistNumber = maxSetlistNumber; }
  sendSetlistCommand = createSetlistCommand(setlistJsonDoc, setlistNumber);
  Serial2.printf("%s\n",sendSetlistCommand.c_str());
  delay(100);
  dispSetNumberZ(setlistNumber,maxSetlistNumber);
}

/**
 * @brief ソース変更後の処理、ソース種確認してタスク開始
 * @param setlistNum セットリスト番号　マニュアルのときはｰ1
 * @param sourceNum ソース番号からモード取得
 * @return none
 */
void actionSourceChanged(int setlistNum, int sourceNum){
  taskStop(drawMode);
  if (setlistNum > -1){                           //ボタン操作の場合はsetlistなしなので消去
    dispRecievedSetNumber(setlistNum);
    drawButton();                                 //操作ボタン再描画
  } else {
    eraseRecievedSetNumber();
    drawRecieveButton();                          //受信復帰ボタン表示
  #ifdef CAPTUREOK
    saveToSD_24bit("/RCVButton.bmp");
  #endif  
  }
  String mode = sourcelistJsonDoc["source"][sourceNum]["mode"];
  drawMode = mode;  
  startNewTask(drawMode, sourceNum); 
}

/**
 * @brief 現在のモードのタスク停止
 * @param mode  現在モード
 * @return none
 */
void taskStop(String mode){
  int repeat = 0;               //task内ループを抜けてファイルをクローズさせる
  int closedSourceNum;

  if (mode == "MP4"){
    int frame = 1;
    xQueueSend(xQueueMP4FrameWait, &frame, 0);                          //pauseでframe待ちしていたら解除
    int ret = xQueueSend(xQueueMP4Stop, &repeat, 0);
    xQueueReceive(xQueueMP4Colse, &closedSourceNum, portMAX_DELAY);     //file close完了待ち
  }
  else if (mode == "WAV"){                         
    int ret = xQueueSend(xQueueWavStop, &repeat, 0);
    xQueueReceive(xQueueWavFileColse, &closedSourceNum, portMAX_DELAY); //file close完了待ち
  }
  else if (mode == "JPG"){
    int ret = xQueueSend(xQueueJpgStop, &repeat, 0);
    xQueueReceive(xQueueJpgColse, &closedSourceNum, portMAX_DELAY);     //file close完了待ち
  }
  else if (mode == "INI"){
    int ret = xQueueSend(xQueueIniStop, &repeat, 0);
    xQueueReceive(xQueueIniColse, &closedSourceNum, portMAX_DELAY);     //file close完了待ち
  }
}

/**
 * @brief 次のモードのタスク処理開始
 * @param mode      指定のモード
 * @param souceNum  ソース番号
 * @return none
 */
void startNewTask(String mode, int sourceNum){
  dispMode(mode);                                                    //mode表示 すべてのタスク停止しているときはディスプレイ描いていい
  int ret;                                                    
  if (mode == "MP4"){           
    xQueueSend(xQueueMP4SpeedRatio, &playSpeedRatio, 0);             //queueで現在のSpeedRaitoを伝える(WAVと共通なので)
    xQueueSend(xQueueMP4ExpandRatio, &mp4YRatio, 0);
    xQueueSend(xQueueMP4telemoro, &mp4TelemoroBpm, 0);
    bool repeat = true;
    xQueueSend(xQueueMP4Stop, &repeat, 0);
    ret = xQueueSend(xQueueMP4Start, &sourceNum, 0);
  }
  else if (mode== "WAV"){   
    xQueueSend(xQueueWavSpeedRatio, &playSpeedRatio, 0);             //queueで現在のSpeedRaitoを伝える(MP4と共通なので)
    ret = xQueueSend(xQueueWavStart, &sourceNum, 0);
  }
  else if (mode == "JPG"){     
    ret = xQueueSend(xQueueJpgStart, &sourceNum, 0);
  }
  else if (mode == "INI"){
    ret = xQueueSend(xQueueIniStart, &sourceNum, 0);
  }
}


/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/

/**
 * @brief シリアル受信処理　LF来たら受信フラグセット
 * @return none
 */
void serial2Event() {
  while (Serial2.available()) {
    char inChar = (char)Serial2.read();        // get the new byte:
    if (inChar == '\n') {                     // if the incoming character is a newline, set a flag so the main loop can
      stringComplete = true;                  //https://ameblo.jp/montabross/entry-12701636753.html
    }
    serialRecieveString += inChar;            // add it to the inputString:
  }
}

/**
 * wavファイルヘッダ構造体
 */
struct __attribute__((packed)) wav_header_t
{
  char RIFF[4];
  uint32_t chunk_size;
  char WAVEfmt[8];
  uint32_t fmt_chunk_size;
  uint16_t audiofmt;
  uint16_t channel;
  uint32_t sample_rate;
  uint32_t byte_per_sec;
  uint16_t block_size;
  uint16_t bit_per_sample;
};

/**
 * wavファイルサブチャンク構造体
 */
struct __attribute__((packed)) sub_chunk_t
{
  char identifier[4];
  uint32_t chunk_size;
  uint8_t data[1];
};


/**
 * @brief 初期画面表示タスク
 */
void taskDrawIni(void* arg){
  while(1){
    int sNum;
    xQueueReceive(xQueueIniStart, &sNum, portMAX_DELAY);   //sourceNumberのキュー待ち
    LcdCanvas.clear(TFT_BLACK);                            //LCD表示エリアクリア
    float w = lcdCanvasWidth/8.0f;
    float h = lcdCanvasHeight/10.0f;    
    LcdCanvas.fillRect(0*w, 0, w+1, 6*h, TFT_WHITE);
    LcdCanvas.fillRect(1*w, 0, w+1, 6*h, TFT_YELLOW);
    LcdCanvas.fillRect(2*w, 0, w+1, 6*h, TFT_CYAN);
    LcdCanvas.fillRect(3*w, 0, w+1, 6*h, TFT_GREEN);
    LcdCanvas.fillRect(4*w, 0, w+1, 6*h, TFT_MAGENTA);
    LcdCanvas.fillRect(5*w, 0, w+1, 6*h, TFT_RED);
    LcdCanvas.fillRect(6*w, 0, w+1, 6*h, TFT_BLUE);
    LcdCanvas.fillRect(7*w, 0, w+1, 6*h, TFT_BLACK);
    for (int i=0; i<h; i++){
      LcdCanvas.drawGradientHLine(0, 6*h + i, lcdCanvasWidth, TFT_BLACK , TFT_WHITE);
      LcdCanvas.drawGradientHLine(0, 7*h + i, lcdCanvasWidth, TFT_RED   , TFT_GREEN);
      LcdCanvas.drawGradientHLine(0, 8*h + i, lcdCanvasWidth, TFT_GREEN , TFT_BLUE);
      LcdCanvas.drawGradientHLine(0, 9*h + i, lcdCanvasWidth, TFT_BLUE  , TFT_RED);
    }
    LcdCanvas.pushSprite(&M5.Display, 10, 120);
    LcdCanvas.pushRotateZoom(&M5.Displays(1), crtImageWidth/2, crtImageHeight/2, 0, (float)crtImageWidth/(float)lcdCanvasWidth,  (float)crtImageHeight/(float)lcdCanvasHeight);

    eraseMp4Info();
    eraseExpandRatio();
    eraseSpeedRatio();
    eraseTelemoroCount();
    drawViewFrame();
    bool repeat = true;                      //無限ループ
    while(repeat){
      xQueueReceive(xQueueIniStop, &repeat, 0);
      vTaskDelay(1);
    }
#ifdef CAPTUREOK
  saveToSD_24bit("/ini.bmp");
#endif
    xQueueSend(xQueueIniColse, &sNum, 0);  
  }
}

/**
 * @brief JPG表示タスク
 */
void taskDrawJpg(void* arg){
  while(1){
    int sNum;
    xQueueReceive(xQueueJpgStart, &sNum, portMAX_DELAY);   //sourceNumberのキュー待ち
    LcdCanvas.clear(TFT_BLACK);                            //LCD表示エリアクリア
    LcdCanvas.pushSprite(&M5.Display, 10, 120);
    dispSourceNumber(sNum);
    eraseMp4Info();
    eraseExpandRatio();
    eraseSpeedRatio();
    eraseTelemoroCount();
    String fileName = sourcelistJsonDoc["source"][sourceNumber]["file"];
    dispSourceFileName(fileName);
    drawViewFrame();
    M5.Displays(1).drawJpgFile(SD, JPG_DATA_PATH + fileName, 0, 0, crtImageWidth, crtImageHeight, 0, 0, (float)(crtImageWidth)/JPG_WIDTH, (float)(crtImageHeight)/JPG_HEIGHT); 
    LcdCanvas.drawJpgFile(SD, JPG_DATA_PATH + fileName, 0, 0, LcdCanvas.width(), LcdCanvas.height(), 0, 0, (float)LcdCanvas.width()/JPG_WIDTH, (float)LcdCanvas.height()/JPG_HEIGHT);
    LcdCanvas.pushSprite(&M5.Display, 10, 120);
    bool repeat = true;                                                         //無限ループ
    while(repeat){
      xQueueReceive(xQueueJpgStop, &repeat, 0);
      vTaskDelay(1);
    }
#ifdef CAPTUREOK
  saveToSD_24bit("/drawJPG.bmp");
#endif      
    xQueueSend(xQueueJpgColse, &sNum, 0);  
  }
}

/**
 * @brief CRT表示タスク
 */
void taskDrawCrt(void* arg){
  float mp4ExpandRatio;
  bool currentModeTelemoro = false;
  bool repeat = false;
  bool stopped = false;
  bool crtCanvasUsing = false;
  
  while(1){
    xQueueReceive(xQueueMP4DrawCrtExpandRatio, &mp4ExpandRatio, portMAX_DELAY);   //ExpandRatio待ち
    xQueueReceive(xQueueMP4DrawCrtStop, &repeat, portMAX_DELAY);                  //DrawCrtStop repetをtrueにすると表示ルーチンに入る

    while(repeat){
      stopped = false;
      xQueueReceive(xQueueMP4DrawCrtExpandRatio, &mp4ExpandRatio, 0);
      portENTER_CRITICAL(&timer1Mux);             //ミューテックスを利用して排他制御
        currentModeTelemoro = modeTelemolo;       //Timer1割込み処理でmodeTlemoro変更している
      portEXIT_CRITICAL(&timer1Mux);
      vTaskDelay(1);                              //WDTタイマーリセット　念のために入れておく
      
      if (currentModeTelemoro) {
        M5.Displays(1).clear(TFT_BLACK);
      } else {
        if(mp4ExpandRatio < 1.0f){                               //描画エリア外を黒で塗る
          M5.Displays(1).fillRect(0, 0, crtImageWidth, crtImageHeight/2*(1-mp4ExpandRatio), TFT_BLACK);
          M5.Displays(1).fillRect(0, crtImageHeight-crtImageHeight/2*(1-mp4ExpandRatio), crtImageWidth, crtImageHeight/2*(1-mp4ExpandRatio), TFT_BLACK);
        }
        crtCanvas.setBuffer(buffer, 1, crtImageHeight, 16); //spriteバッファにセット 1x480  バッファにセットして待つ
        crtCanvas.pushRotateZoom(&M5.Displays(1), crtImageWidth/2, crtImageHeight/2, 0, crtImageWidth, mp4ExpandRatio);   //crtに表示 Xの中央　横倍率は幅サイズ  
      }
      xQueueReceive(xQueueMP4DrawCrtStop, &repeat, 0);
      vTaskDelay(15);    //184bpm 1000000usec/(184/60*4)/2=40,760 最高速トレモロでも40msec周期なので15msecくらい待たせておく
    }
    stopped = true;
    xQueueSend(xQueueMP4DrawCrtStopped, &stopped, 0);
    vTaskDelay(1);
  }
}

/**
 * @brief MP4表示タスク
 */
void taskDrawMp4(void* arg){
  File mp4DrawFile;               //MP4用のデータファイル
  while(1){
    int mp4MaxFrame;              //mp4再生時の最終フレーム　（ファイルサイズより小さいフレームで繰り返したい時に使用、通常はデータサイズ)
    int sNum;
    float mp4PlaySpeedRatio;
    float mp4ExpandRatio;
    int taskMp4Pause = 0;
    int mp4TelemoroBpm;
    bool telemoroMark = false;    //テレモロ動作表示
    bool crtDrawRepeat = true;    //CRT描画ループ設定
    bool drawCrtStopped = false;
    bool repeat;
    bool crtCanvasUsing = false;

    xQueueReceive(xQueueMP4Stop, &repeat, portMAX_DELAY);
    xQueueReceive(xQueueMP4SpeedRatio, &mp4PlaySpeedRatio, portMAX_DELAY); 
    xQueueReceive(xQueueMP4ExpandRatio, &mp4ExpandRatio, portMAX_DELAY);
    xQueueReceive(xQueueMP4telemoro, &mp4TelemoroBpm, portMAX_DELAY);
    xQueueSend(xQueueMP4DrawCrtExpandRatio, &mp4ExpandRatio, 0);         //ExpandRatioをtaskDrawCrtに送信
    xQueueSend(xQueueMP4DrawCrtStop, &crtDrawRepeat, 0);                 //crtDrawルーチンスタート
    xQueueReceive(xQueueMP4Start, &sNum, portMAX_DELAY);                 //sourceNumberのキュー待ち
    dispSourceNumber(sNum);
    eraseMp4Info();
    dispExpandRatio(mp4ExpandRatio);
    dispSpeedRatio(mp4PlaySpeedRatio);
    dispTelemoroBpm(mp4TelemoroBpm);
    setTelemoroTimer(mp4TelemoroBpm);
    String sourceFile = sourcelistJsonDoc["source"][sNum]["file"];    //ソースファイル名
    dispSourceFileName(sourceFile);  
    int sourceframe = sourcelistJsonDoc["source"][sNum]["frame"];     //最大フレーム数
    drawViewFrame();
    mp4MaxFrame = sourceframe;
    mp4Frame = 0;
    mp4Floatframe = 0.0f;
    mp4DrawFile = SD.open(MP4_DATA_PATH + sourceFile);

    while(repeat){
      mp4DrawFile.seek(mp4Frame*crtImageHeight*2);
      mp4DrawFile.read(buffer, crtImageHeight*2);      //1画面分(480バイト×2)バッファに読み込む 読み込み速度のばらつきををここで吸収           
      preLcdCanvas.setBuffer(buffer, 1, crtImageHeight, 16);//spriteバッファにセット　crtCanvasと同じ内容　LCD表示用に使う 
      int frame=0;
      xQueueReceive(xQueueMP4FrameWait, &frame, portMAX_DELAY);   //Frame同期タイマー待ち
      /*割込みでsemaphor解放されるのを待つとリセットかかることがあるので、メインルーチンからQueue送ることにした*/    

      if(mp4ExpandRatio < 1.0f){                               //描画エリア外を黒で塗る
        LcdCanvas.clear(TFT_BLACK);
      }
      preLcdCanvas.pushRotateZoom(&LcdCanvas, LcdCanvas.width()/2, LcdCanvas.height()/2, 0, LcdCanvas.width(),(float)(LcdCanvas.height()/(float)preLcdCanvas.height()*mp4ExpandRatio));
      LcdCanvas.pushSprite(&M5.Display, 10, 120);
      
      mp4Floatframe += mp4PlaySpeedRatio;         //小数フレームカウント
      mp4Frame = (int)mp4Floatframe;              //整数フレームカウント
      if (mp4DrawFile.position() >= (crtImageHeight * mp4MaxFrame * 2)) {
          mp4Frame = 0;
          mp4Floatframe = 0.0f;
      }
      dispFrameCount(mp4Frame);
      
      fpsCount++;                              //Count Frame rate 
      if (fpsSec != millis() / 1000) {
        fpsSec = millis() / 1000;
        dispFps(fpsCount);
        fpsCount = 0;
      }

      xQueueReceive(xQueueMP4pause, &taskMp4Pause, 0);

      if (xQueueReceive(xQueueMP4SpeedRatio, &mp4PlaySpeedRatio, 0) == pdTRUE){
        dispSpeedRatio(mp4PlaySpeedRatio);
      }

      if (xQueueReceive(xQueueMP4ExpandRatio, &mp4ExpandRatio, 0) == pdTRUE){
        xQueueSend(xQueueMP4DrawCrtExpandRatio, &mp4ExpandRatio, 0);       //ExpandRatioをtaskDrawCrtに送信
        dispExpandRatio(mp4ExpandRatio);
      }

      if (xQueueReceive(xQueueMP4telemoro, &mp4TelemoroBpm, 0) == pdTRUE){
        setTelemoroTimer(mp4TelemoroBpm);                                 //コマンド受信したらタイマー設定と表示
        dispTelemoroBpm(mp4TelemoroBpm);
      }

      if ((mp4TelemoroBpm > 0) && ((mp4Frame % 8)==0)) {                             //テレモロ動作中表示
        int x = 100;
        int y = 250;
        M5.Display.setFont(&fonts::Font4);      //14x26
        M5.Display.setTextSize(1);
        M5.Display.setCursor(x+25, y);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        if (telemoroMark && (mp4TelemoroBpm > 0)){
          M5.Display.printf("T"); 
        } else {
          M5.Display.fillRect(125, 250, 14, 21, TFT_BLACK);
        }
        telemoroMark = !telemoroMark;
      } else {
        M5.Display.fillRect(125, 250, 14, 21, TFT_BLACK);
      }

      if (taskMp4Pause) dispPause();                              //Pause指令があった時にPause表示　それ以外は上書きするので消える

      xQueueReceive(xQueueMP4Stop, &repeat, 0);                   //無限再生ループ抜けるためのrepeat指令キュー確認
    }
    crtDrawRepeat = false;
    xQueueSend(xQueueMP4DrawCrtStop, &crtDrawRepeat, 0);                               //crtDrawルーチンストップ
    xQueueReceive(xQueueMP4DrawCrtStopped, &drawCrtStopped, portMAX_DELAY);            //Stop応答確認
    mp4DrawFile.close();
#ifdef CAPTUREOK
  saveToSD_24bit("/mp4.bmp");
#endif    
    xQueueSend(xQueueMP4Colse, &sNum, 0); 
    vTaskDelay(1);  
   }  
}


/* WAVファイル再生タスク
 *  https://github.com/m5stack/M5Unified/tree/master/examples/Advanced/Speaker_SD_wav_file
 *  https://twitter.com/lovyan03/status/1496749711255957507
 *  https://twitter.com/lovyan03/status/1672541086739480576
 */

/**
 * @brief WAVリピート再生タスク
 */
void taskPlayWav(void* arg){
  static constexpr const size_t buf_num = 3;          //バッファ数
  static constexpr const size_t buf_size = 1024;      //バッファサイズ
  static uint8_t wav_data[buf_num][buf_size];         //SDから読み込んだ再生用wavデータを入れる

  while (1) {
    int sNum;
    float wavPlaySpeedRatio;   
    xQueueReceive(xQueueWavSpeedRatio, &wavPlaySpeedRatio, portMAX_DELAY); 
    xQueueReceive(xQueueWavStart, &sNum, portMAX_DELAY);                 //sourceNumberのキュー待ち

    Serial.println("WAV START: OK");
    drawViewFrame();
    LcdCanvas.clear(TFT_BLACK);                                          //LCD表示エリアクリア
    LcdCanvas.pushSprite(&M5.Display, 10, 120);

    dispSourceNumber(sNum);
    dispSpeedRatio(wavPlaySpeedRatio);
    eraseMp4Info();
    eraseExpandRatio();
    eraseTelemoroCount();
    String sourceFile = sourcelistJsonDoc["source"][sNum]["file"];       //souceFile名を求める
    dispSourceFileName(sourceFile);   
    Serial.printf("wavTask openfile %s\n",sourceFile.c_str());
    int volume = sourcelistJsonDoc["source"][sNum]["volume"];            //volumeを求める
    
    M5.Display.setCursor(132, 92);                                       //Volome表示
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKCYAN, TFT_BLACK);
    M5.Display.printf("Vol:%03d", volume);

    M5.Display.setTextColor(TFT_DARKGREEN, TFT_BLACK);                  //WAV読み込み状況表示用
    M5.Display.setCursor(15,125);
    M5.Display.printf("Playing!\n");
    M5.Display.setCursor(75,175);
    M5.Display.printf("KByte");

    //CRTに情報表示
    M5.Displays(1).clear(TFT_BLACK);
    M5.Displays(1).setFont(&fonts::Font0);
    M5.Displays(1).setTextSize(1,4);
    M5.Displays(1).setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Displays(1).setCursor(5,80);
    M5.Displays(1).printf("Now Playing!\n\n");
    M5.Displays(1).setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Displays(1).setCursor(5,160);
    M5.Displays(1).printf("%s\n",sourceFile.c_str());
        
    Serial.printf("WAV OPENE:sourceNum %d\n",sNum);    
    auto file = SD.open(WAV_DATA_PATH + sourceFile);                        //ファイルオープン

    if (file){
      wav_header_t wav_header;
      file.read((uint8_t*)&wav_header, sizeof(wav_header_t));

      ESP_LOGD("wav", "RIFF           : %.4s" , wav_header.RIFF          );
      ESP_LOGD("wav", "chunk_size     : %d"   , wav_header.chunk_size    );
      ESP_LOGD("wav", "WAVEfmt        : %.8s" , wav_header.WAVEfmt       );
      ESP_LOGD("wav", "fmt_chunk_size : %d"   , wav_header.fmt_chunk_size);
      ESP_LOGD("wav", "audiofmt       : %d"   , wav_header.audiofmt      );
      ESP_LOGD("wav", "channel        : %d"   , wav_header.channel       );
      ESP_LOGD("wav", "sample_rate    : %d"   , wav_header.sample_rate   );
      ESP_LOGD("wav", "byte_per_sec   : %d"   , wav_header.byte_per_sec  );
      ESP_LOGD("wav", "block_size     : %d"   , wav_header.block_size    );
      ESP_LOGD("wav", "bit_per_sample : %d"   , wav_header.bit_per_sample);

      if (!( memcmp(wav_header.RIFF,    "RIFF",     4)
        || memcmp(wav_header.WAVEfmt, "WAVEfmt ", 8)
        || wav_header.audiofmt != 1
        || wav_header.bit_per_sample < 8
        || wav_header.bit_per_sample > 16
        || wav_header.channel == 0
        || wav_header.channel > 2
        )) {

        bool repeat = true;                                                         //無限再生ループ
        while(repeat){
          file.seek(offsetof(wav_header_t, audiofmt) + wav_header.fmt_chunk_size);
          sub_chunk_t sub_chunk;

          file.read((uint8_t*)&sub_chunk, 8);
          ESP_LOGD("wav", "sub id         : %.4s" , sub_chunk.identifier);
          ESP_LOGD("wav", "sub chunk_size : %d"   , sub_chunk.chunk_size);

          while(memcmp(sub_chunk.identifier, "data", 4))
          {
            if (!file.seek(sub_chunk.chunk_size, SeekMode::SeekCur)) { break; }
            file.read((uint8_t*)&sub_chunk, 8);
            ESP_LOGD("wav", "sub id         : %.4s" , sub_chunk.identifier);
            ESP_LOGD("wav", "sub chunk_size : %d"   , sub_chunk.chunk_size);
          }

          if (! memcmp(sub_chunk.identifier, "data", 4)) {      //memcmp:等しければ0 "data"位置の確認   
            int32_t data_len = sub_chunk.chunk_size;
            bool flg_16bit = (wav_header.bit_per_sample >> 4);
            uint32_t sampleRate = wav_header.sample_rate;       //再生速度可変用
            size_t idx = 0;
            size_t idxCount = 0;         
            M5.Speaker.setVolume(volume); 
            while (data_len > 0) {
              size_t len = data_len < buf_size ? data_len : buf_size;
              len = file.read(wav_data[idx], len);
              data_len -= len;

              if (flg_16bit) {
                M5.Speaker.playRaw((const int16_t*)wav_data[idx], len >> 1, sampleRate*wavPlaySpeedRatio, wav_header.channel > 1, 1, 0);
              } else {
                M5.Speaker.playRaw((const uint8_t*)wav_data[idx], len, sampleRate*wavPlaySpeedRatio, wav_header.channel > 1, 1, 0);
              }
              idx = idx < (buf_num - 1) ? idx + 1 : 0;
              idxCount++;
              
              M5.Display.setCursor(25,150);
              M5.Display.printf("%04d\n",idxCount);
              int progress = idxCount*buf_size*100/sub_chunk.chunk_size;
              if (progress >= 100){
                M5.Lcd.fillRect(15, 200, 130, 20, 0);
              } else {
              M5.Display.progressBar(15, 200, 130, 20, progress);
              }
              
              if (xQueueReceive(xQueueWavSpeedRatio, &wavPlaySpeedRatio, 0) == pdTRUE){
                dispSpeedRatio(wavPlaySpeedRatio);
              }

              if (xQueueReceive(xQueueWavStop, &repeat, 0) == pdTRUE){            //無限再生ループ抜けるためのrepeat指令キュー確認
                data_len = 0;                                                     //repeat=false, data_len=0でループ抜ける
              }  
            }
          }
        }
      }
      file.close();
#ifdef CAPTUREOK
  saveToSD_24bit("/playwav.bmp");
#endif  
    //  Serial.printf("WAV CLOSE:souceNum %d\n",sNum);   
      xQueueSend(xQueueWavFileColse, &sNum, 0);       
    }
    vTaskDelay(1);
  }  
}


/* 解像度変更　らびやんさんに聞く https://twitter.com/lovyan03/status/1672609800054800384 */
  // auto i = M5.getDisplayIndex(m5gfx::board_M5ModuleRCA);
  // if (i>=0){
  // auto &dsp = M5.Displays(i);
  // auto p = (m5gfx::Panel_CVBS*)(dsp.getPanel());
  // p->setResolution(120, 480);
  // dsp.setRotation(dsp.getRotation());
  // }