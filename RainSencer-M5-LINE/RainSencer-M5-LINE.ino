/**
 * 静電容量式レインセンサー for M5Stamp
 * 雨が降ったらＬＩＮＥに通知
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define DEBUG true  // true:OLED SSD1307 表示

#if DEBUG
#include "DISP7SEG.h"
DISP7SEG disp ;
#endif

#define PULSE_PIN 18 
#define DIGITAL_READ_PIN 19
#define ANALOG_READ_PIN 25
#define LED_RAIN 26  // 雨LED

const double P = 4095.0;
const int    EP = P * 0.6322;   // 63.22%の電圧のAD読み取り値
const int    BorderRain = 620 ; // 雨降りと判定する境界値
const int    BorderSunny = 590; // 雨上がりと判定する境界値

// 移動平均用
const int avrNum = 10 ; // 移動平均のサンプル数
int avrPos = 0 ;
long avr[10] ;

// 雨降りフラグ
bool flagRain = false ;

// ----- NetWork Data ---
const char* ssid     = "****";  // Wi-Fi SSID
const char* password = "****";  // Wi-Fi Password
const char* lineHost = "notify-api.line.me";  // LINE Notify API URL
const char* lineToken = "****"; // LINE Notify API Token

// ------------------
// ----- OLED表示 ---
// ------------------
void dispCapacity(int T) {
#if DEBUG
  int  dispSeg[9] ;
  if (T > 0) {
    // --- T 表示
    for (int i=0;i<9;i++) {
      dispSeg[i] = 416 ;  // 空白で埋める
    }
    long range = T ;
    for (int i=7;i>=0;i--) {
      dispSeg[i] = range % 10 ;
      range /= 10 ;
      if (range == 0) break ;
    }
  } else {
    // --- 初期画面 8888888 表示
    for (int i=0;i<9;i++) {
      dispSeg[i] = 22 ;
    }
    disp.cls() ;
  }

  // --- 表示
  int x = 0 ;
  for (int i=0;i<8;i++) {
    disp.disp7SEG(x,0,dispSeg[i]) ;
    x += (dispSeg[i] == 20) ? 8 : 16 ; // DOTだけ幅を狭める
  } 
#endif
}

// ----------------------
// ----- ERROR NO表示 ---
// ----------------------
void dispError(int No) {
#if DEBUG
  int  dispSeg[9] ;

  // --- 容量値表示
  for (int i=0;i<9;i++) {
    dispSeg[i] = 416 ;  // 空白で埋める
  }
  dispSeg[0] = 15 ; // E
  dispSeg[1] = 21 ; // -
  long range = No ;
  for (int i=3;i>=0;i--) {
    dispSeg[2+i] = range % 10 ;
    range /= 10 ;
    if (range == 0) break ;
  }

  // --- 表示
  int x = 0 ;
  for (int i=0;i<8;i++) {
    disp.disp7SEG(x,0,dispSeg[i]) ;
    x += (dispSeg[i] == 20) ? 8 : 16 ; // DOTだけ幅を狭める
  } 
#endif
}

// ----------------------
// ----- LINE に通知 -----
// ----------------------
// mode - 0 : 雨が降ってきた
// mode - 1 : 雨が上がった
bool sendLINE(int mode) {
  // Wi-Fi接続
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Line APIに接続
  WiFiClientSecure client;
  client.setInsecure( ) ;
  if (!client.connect(lineHost, 443)) {
    dispError(1) ; // Connection failed
    return false ;
  }
 
  // リクエストを送信
  String message = (mode == 0) ? "雨が降ってきた" : "雨が上がった" ;
  String query = String("message=") + message;
  String request = String("") +
               "POST /api/notify HTTP/1.1\r\n" +
               "Host: " + lineHost + "\r\n" +
               "Authorization: Bearer " + lineToken + "\r\n" +
               "Content-Length: " + String(query.length()) +  "\r\n" + 
               "Content-Type: application/x-www-form-urlencoded\r\n\r\n" +
                query + "\r\n";
  client.print(request);

  // 受信終了まで待つ 
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  WiFi.disconnect(true,false) ;
  delay(2000) ;
  return true ;
}

// ----------------------
// ----- 静電容量計測 -----
// ----------------------
long getCapacitance() {
  // ---  放電 --
  pinMode(ANALOG_READ_PIN, INPUT);

  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);
  pinMode(DIGITAL_READ_PIN, OUTPUT);
  digitalWrite(DIGITAL_READ_PIN, LOW);
  delay(1000); 
  pinMode(DIGITAL_READ_PIN, INPUT);
  delay(20);

  // --- 充電時間T計測
  digitalWrite(PULSE_PIN, HIGH);
  unsigned long startTime = micros();
  while (analogRead(ANALOG_READ_PIN) < EP) ;
  long T = micros() - startTime;

  return T ;
}

// ----------------------
// ----- セットアップ -----
// ----------------------
void setup() {
#if DEBUG
  Wire.begin();
  disp.init() ;
  dispCapacity(0) ;
  delay(1000) ;
#endif

  pinMode(LED_RAIN, OUTPUT);
  digitalWrite(LED_RAIN, LOW);

  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);

  pinMode(ANALOG_READ_PIN, INPUT);

  // 移動平均の初期設定(誤動作防止用)
  for (int i=0;i<avrNum;i++) {
    avr[i] = getCapacitance() ;
  }
}

// -----------------
// ------ LOOP -----
// -----------------
void loop() {
  // --- 時定数Ｔ計測
  long T = getCapacitance();
  avr[avrPos] = T ;
  avrPos = (avrPos + 1) % avrNum ;
  // --- 移動平均算出
  long avrT = 0 ;
  for (int i=0;i<avrNum;i++) {
    avrT += avr[i] ;
  }
  avrT /= avrNum ;

  // --- 雨降り判定 -----
  //  avrT > BorderRain で雨降りと判定
  //     ↑↓この間はグレーソーン、降り始め or 雨上がり
  //  avrT < BorderSunny で晴と判定

  if (avrT < BorderSunny) {
    // 雨LED消灯
    digitalWrite(LED_RAIN, LOW);
  }
  if (avrT >= BorderSunny) {
    // 雨LED点灯
    digitalWrite(LED_RAIN, HIGH);
  }
  if (avrT > BorderRain && flagRain == false) {
    // 雨が降ってきた
    flagRain = true ;
    sendLINE(0) ;
  }
  if (avrT < BorderSunny && flagRain == true) {
    // 雨が上がった
    flagRain = false ;
    sendLINE(1) ;
  }

  // --- 表示 ---
  dispCapacity(avrT) ;
}
