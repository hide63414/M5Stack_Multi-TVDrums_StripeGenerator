/**
 * LCD表示処理
*/

#include <M5Unified.h>
#include <SD.h>

//#define CAPTURE             //CAPTUREする  しなときはコメントアウト
//#define CAPTUREOK           //CAPTUREずみ

#define COLOR_DEPTH16

const int crtImageHeight =  480;   //縦解像度　横縞本数なので480本は変えない
#ifdef COLOR_DEPTH16
    const int crtImageWidth =   144;   //CRT16bit表示するために横144
#else
    const int crtImageWidth =   240;   //分解能優先(主にJPEG)8bit表示にする
#endif
const int lcdImageWidth =   320;   //LCD表示　幅　　固定
const int lcdImageHeight =  240;   //LCD表示　高さ　固定
const int lcdCanvasWidth =  140;   //LCDに表示するCRT画面用のスプライトサイズ　大きすぎるとフレームレート間に合わない
const int lcdCanvasHeight = 105;


//親子共通
//===========================================================================
/**
 * @brief バージョン表示
 * @param String バージョン番号
 */
void dispVersion(String version);

/**
 * @brief 自己ID表示表示
 * @param String 自己ID表示
 */
void dispMyId(String id);

/**
 * @brief ボタン操作表示
 */
void drawButton();

/**
 * @brief レシーブボタン表示
 */
void drawRecieveButton();

/**
 * @brief [PAUSE]表示
 */
void dispPause();

/**
 * @brief [PAUSE]消去
 */
void erasePause();

/**
 * @brief SDカードエラー表示
 */
void dispSdError();

//子機専用
//===========================================================================
/**
 * @brief LCD画面表示枠描画
 */  
void drawViewFrame();

/**
 * @brief ソースファイル番号表示
 * @param int ファイル番号
 */ 
void dispSourceNumber(int fNum);

/**
 * @brief ソースファイル名表示
 * @param String ファイル名
 */ 
void dispSourceFileName(String fileName);

/**
 * @brief 子機の受信したセット番号を表示
 * @param int セット番号
 */
void dispRecievedSetNumber(int setNum);

/**
 * @brief 子機のセット番号を消去
 */
void eraseRecievedSetNumber();

/**
 * @brief サンプルレート、フレーム位置　表示消す
 */  
void eraseMp4Info();

/**
 * @brief フレームレート表示
 * @param int フレームレート
 */  
void dispFps(int count);

/**
 * @brief フレーム位置表示
 * @param int フレーム位置
 */  
void dispFrameCount(int count);

/**
 * @brief 現在モード表示
 * @param String モード名 
 */
void dispMode(String mode);

/**
 * @brief WAVデータ再生中の読み込みバイト数表示
 */
void dispWavDataPosition(int count);

/**
 * @brief 子機速度設定表示
 * @param float 速度倍率
 */
void dispSpeedRatio(float ratio);

/**
 * @brief 子機速度設定消去
 */
void eraseSpeedRatio();

/**
 * @brief 子機縦倍率設定表示
 * @param float 表示倍率
 */
void dispExpandRatio(float ratio);

/**
 * @brief 子機縦倍率表示消去
 */
void eraseExpandRatio();

/**
 * @brief 子機テレモロbpm表示
 * @param int bpm値
 */  
void dispTelemoroBpm(int bpm);      //テレモロbpm値表示

/**
 * @brief 子機テレモロ値消去
 */ 
void eraseTelemoroCount();

//親機専用
//===========================================================================
/**
 * @brief ボタン操作表示　親青赤ボタン説明付き　
 */
void drawButtonZ();

/**
 * @brief セットリストファイル名表示
 * @param String ファイル名 
 */
void dispSetlistFileNameZ(String mode);

/**
 * @brief setlist file選択用ボタン表示
 */
void drawSetlistSelectbutton();

/**
 * @brief セット番号をLCD表示
 * @param int セット番号
 * @param int セット総数
 */
void dispSetNumberZ(int setNum, int maxNum);

/**
 * @brief 親機速度設定表示
 * @param float 速度倍率
 */
void dispSpeedRatioZ(float ratio);

/**
 * @brief 親機縦倍率設定表示
 * @param float 表示倍率
 */
void dispExpandRatioZ(float ratio);

/**
 * @brief 親機テレモロbpm表示
 * @param bool テレモロON/OFF
 * @param int bpm値
 */
void dispTelemoroBpmZ(bool telemoloZ, int bpm);

/**
 * @brief 親機テレモロON表示
 */
void dispTelemoroOnZ();

/**
 * @brief 親機テレモロOFF表示
 */
void dispTelemoroOffZ();

/**
 * @brief 画面キャプチャ
 * @param bmpFilename ファイル名
 * @return true:成功
 */
bool saveToSD_24bit(String bmpFilename);
//キャプチャ用 lovyanGFX->Example-
//https://github.com/lovyan03/LovyanGFX/tree/master/examples/Standard/SaveBMP