/*
 * SSD1306 128x32に７セグメントっぽいフォントで表示
 * 
 */
#ifndef DISP7SEG_H
#define DISP7SEG_H
#include <arduino.h>
#include <Wire.h>
#include "bitmapdata.h"

class DISP7SEG
{
  private:
    uint8_t bitMapBuff[24*4];  // 7セグっぽいフォントの合成用バッファ
  
  public:
    void init(); // 初期化
    void cls(); // 画面クリア
    void dispSwitch(bool sw) ;
    void disp7SEG(int offsetX , int offsetY , int num); // 7セグメント調フォント表示

  protected:
    void oledBitMap(int offsetX , int offsetY , int width,int hight, uint8_t bitMap[]) ;
    void oledBitMap32(int col , int row , int width , uint8_t *bitMap , bool even) ;
    void oledSendCommand(uint8_t command); // OLEDに1バイトコマンドを送信
    void oledSendCommand2(uint8_t command1,uint8_t command2); // OLEDに2バイトコマンドを送信
    void oledSendCommand3(uint8_t command1,uint8_t command2,uint8_t command3); // OLEDに3バイトコマンドを送信
    void oledSendCommands(int commandLen,uint8_t commands[]); // OLEDに複数のコマンドを送信
} ;
#endif
