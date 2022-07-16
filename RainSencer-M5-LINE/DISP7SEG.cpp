#include "DISP7SEG.h"

static uint8_t oledAddr = 0x3C;     // SSD1306のI2Cアドレス
static uint8_t oldCommand = 0x80;   // SSD1306の1Byteコマンド
static uint8_t oldCommands = 0x00;  // SSD1306の複数Byteコマンド
static uint8_t oldData = 0x40;      // SSD1306のデータバイト
static uint8_t DISP7SEG_SSD1306_INIT[] = { // SSD1306の初期化コマンド群
  0xC8,         // C0/C8:上下の描画方向
  0xA1,         // A0/A1:左右の描画方向
  0x22,0x00,0x07,         // 20/21/22/23:アドレッシングモード
  0xA8,0x3F,    // 画面の解像度
  0xD3,0x00,    // 縦方向のオフセット
  0xDA,0x12,    // 描画タイプ
  0x40,         // 縦方向の描画開始位置
  0xA6,         // 表示方法 - A7にすると画面反転
  0x8D, 0x14    // OLEDパネルの供給電圧と思われる 14hに設定しないとダメらしい
};

uint8_t oledWidth = 128;  // OLEDの幅(ピクセル数)
uint8_t oledHeight = 64;  // OLEDの高さ(ピクセル数)
uint8_t oledPages = 8;    // OLEDのページ数(ピクセル数÷８)

// 初期化
void DISP7SEG::init() {
  // OLED設定コマンド送信
  oledSendCommands(sizeof(DISP7SEG_SSD1306_INIT), DISP7SEG_SSD1306_INIT) ;  // 初期化
  oledSendCommand(0xAF) ; // 画面表示
  cls() ;
  dispSwitch(true) ;
}

// 画面消去
void DISP7SEG::cls() {
  oledSendCommand(0xAE) ; // 画面表示

  for (uint8_t m = 0; m < oledPages; m++) {
    oledSendCommand3(0xB0 | (m & 0x07) , 0x10 , 0x00) ;
    Wire.beginTransmission(oledAddr);
    Wire.write(oldData);
    for (uint8_t x = 0; x < oledWidth; x++) {
      if (Wire.write(0) == 0) {
        Wire.endTransmission();
        Wire.beginTransmission(oledAddr);
        Wire.write(oldData);
        Wire.write(0);
      }
    }
    Wire.endTransmission();
  }

  oledSendCommand(0xAF) ; // 画面表示
}

// 表示日表示切替
void DISP7SEG::dispSwitch(bool sw) {
  if (sw) {
    oledSendCommand(0xAF) ; // 画面表示
  } else {
    oledSendCommand(0xAE) ; // 画面表示
  }
}

// 7セグメント調フォント表示
void DISP7SEG::disp7SEG(int offsetX , int offsetY , int num) {
  int width = 16 ;
  int heigh = 4 ;

  if (num < 16) {
    if (num < 0) { num = 10 ; }
    bool flag7[7] ;
    for (int i=0;i<7;i++) {
      flag7[i] = addMap[num][i] ; 
    }
    for (int i = 0; i < 16*4; i++) {
        bitMapBuff[i] = 0 ;
      if (flag7[0]) { bitMapBuff[i] |= SEGa[i] ; }  // a
      if (flag7[1]) { bitMapBuff[i] |= SEGb[i] ; }  // b
      if (flag7[2]) { bitMapBuff[i] |= SEGc[i] ; }  // c
      if (flag7[3]) { bitMapBuff[i] |= SEGd[i] ; }  // d
      if (flag7[4]) { bitMapBuff[i] |= SEGe[i] ; }  // e
      if (flag7[5]) { bitMapBuff[i] |= SEGf[i] ; }  // f
      if (flag7[6]) { bitMapBuff[i] |= SEGg[i] ; }  // g
    }
  } else
  if (num > 100) { // CLEAR
    width = num % 100 ;
    heigh = num / 100 ;
    memset(bitMapBuff,0,width*heigh) ;
  } else {
    uint8_t *bitMap ;
    switch(num) {
      case 20: bitMap = SEGdot ; break ;    // DOT
      case 21: bitMap = SEGg ; break ;      // -
      case 22: bitMap = base7SEG ; break ;  // Body
    }
    for (int i = 0; i < width*heigh; i++) {
      bitMapBuff[i] = bitMap[i] ; 
    } 
  }

  oledBitMap(offsetX,offsetY, width , heigh , bitMapBuff) ;
}

// OLEDに1バイトコマンドを送信
void DISP7SEG::oledSendCommand(uint8_t command) {
  Wire.beginTransmission(oledAddr);
  Wire.write(oldCommand);
  Wire.write(command);
  Wire.endTransmission();
}

// OLEDに2バイトコマンドを送信
void DISP7SEG::oledSendCommand2(uint8_t command1,uint8_t command2) {
  Wire.beginTransmission(oledAddr);
  Wire.write(oldCommands);
  Wire.write(command1);
  Wire.write(command2);
  Wire.endTransmission();
}

// OLEDに3バイトコマンドを送信
void DISP7SEG::oledSendCommand3(uint8_t command1,uint8_t command2,uint8_t command3) {
  Wire.beginTransmission(oledAddr);
  Wire.write(oldCommands);
  Wire.write(command1);
  Wire.write(command2);
  Wire.write(command3);
  Wire.endTransmission();
}

// OLEDに複数のコマンドを送信::Wireクラスの制限により最大32Byte
// 長さのチェックはしてないのでプログラマ責任で上手い事やってちょ
void DISP7SEG::oledSendCommands(int commandLen,uint8_t commands[]) {
  Wire.beginTransmission(oledAddr);
  Wire.write(oldCommands);
  for (int i = 0; i < commandLen; i++) {
    Wire.write(commands[i]);
  }
   Wire.endTransmission();
}

// BITマップを表示する::oledFillDataの可変データ型なので基本oledFillDataと同じ作り
void DISP7SEG::oledBitMap(int offsetX , int offsetY , int width,int hight, uint8_t bitMap[]) {
  int pos = 0 , pos2 = 0 ;
  for (int m = 0; m < hight; m++) {
    oledBitMap32(offsetX , (m*2) + offsetY , width , &bitMap[pos] , true) ;
    oledBitMap32(offsetX , (m*2+1) + offsetY , width , &bitMap[pos] , false) ;
    pos += width ;
  }
  oledSendCommand3(0xB0 , 0x10 , 0x00) ;
}

// 128x32ドットＯＬＥＤで偶数・奇数ラインにビットマップを書き込み
void DISP7SEG::oledBitMap32(int col , int row , int width , uint8_t *bitMap , bool even) {
  oledSendCommand3(0xB0 | (row & 0x07) , 0x10 | (col & 0xf0) >> 4 , col & 0x0F) ;
  Wire.beginTransmission(oledAddr);
  Wire.write(oldData);
  for (int x = 0; x < width; x++) {
    int8_t byteData = bitMap[x] ;
    if (even) {
      byteData = bitPattern[byteData & 0x0F] ;
    } else {
      byteData = bitPattern[(byteData & 0xF0)>>4] ;
    }
    if (Wire.write(byteData) == 0) {
      Wire.endTransmission();
      Wire.beginTransmission(oledAddr);
      Wire.write(oldData);
      Wire.write(byteData);
    }
  }
  Wire.endTransmission();
}
