/*
https://qiita.com/kobatan/items/40728fbb625057d9f42b
https://qiita.com/kobatan/items/253f1614a8653a1dcb1a
*/

/*

5V <－－－ ＞ 5V
GND <－－－ ＞ GND
RX <－－－ ＞ Be careful not to mistake
TX <－－－ ＞ RX ELRS communicates with Crossfire protocol,
              so signal inversion circuit like SBUS Wiring is easy without the need.
              Both signal lines are 3.3V, so just connect them as they are.
              The program can easily communicate with ELRS simply by setting the UART
              baud rate to 420,000. Also, just install Adafruit's TinyUSB library for
              Arduino in XIAO and it will be easily recognized as a gamepad, so the
               program is easy.
               (Once done ... I had a hard time investigating how it works)
               * Caution *　In the case of early XIAO, it is necessary to set the TinyUSB library version to 0.10.5 .
                The latest version gives a compile error. With XIAO RP2040, the latest version is OK.

https://camo.qiitausercontent.com/bc1d135fc8b25d365a72593a9524bc0f3f4c10c9/68747470733a2f2f71696974612d696d6167652d73746f72652e73332e61702d6e6f727468656173742d312e616d617a6f6e6177732e636f6d2f302f323532303735382f66393961366138392d653838392d646334652d316239332d3065323937613334323965642e6a706567
*/

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> /* Adafruit_TinyUSB ライブラリは * XIAO の場合 <Version 0.10.5> を使うこと。それ以上だとXIAOではコンパイルエラーが出ます。 * XIAO RP2040 は最新バージョンでOKです。 */

//#define DEBUG

// USB HID report descriptor
// ゲームパッドデータの構造を指定します （プロポ受信機用 (16bitデータ x 8ch) +
// (1bitデータ x 8ch）)
#define TUD_HID_REPORT_DESC_GAMEPAD_9(...)                                    \
  HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                     \
      HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD),                                   \
      HID_COLLECTION(HID_COLLECTION_APPLICATION), /* Report ID if any */      \
      __VA_ARGS__ HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                     \
      HID_USAGE(HID_USAGE_DESKTOP_X), HID_USAGE(HID_USAGE_DESKTOP_Y),         \
      HID_USAGE(HID_USAGE_DESKTOP_Z), HID_USAGE(HID_USAGE_DESKTOP_RX),        \
      HID_USAGE(HID_USAGE_DESKTOP_RY), HID_USAGE(HID_USAGE_DESKTOP_RZ),       \
      HID_USAGE(HID_USAGE_DESKTOP_SLIDER), HID_USAGE(HID_USAGE_DESKTOP_DIAL), \
      HID_LOGICAL_MIN(0), /* HID_LOGICAL_MAX ( 0x7ff ) ,*/                    \
      HID_LOGICAL_MAX_N(0xffff, 3), HID_REPORT_COUNT(8), HID_REPORT_SIZE(16), \
      HID_INPUT(HID_DATA | HID_VARIABLE |                                     \
                HID_ABSOLUTE), /* 8 bit Button Map */                         \
      HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), HID_USAGE_MIN(1),                \
      HID_USAGE_MAX(8), HID_LOGICAL_MIN(0), HID_LOGICAL_MAX(1),               \
      HID_REPORT_COUNT(8), HID_REPORT_SIZE(1),                                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                      \
      HID_COLLECTION_END  // CrossFire用
#define CRSF_BAUDRATE 420000
#define CRSF_MAX_PACKET_LEN 64
#define CRSF_NUM_CHANNELS 16

typedef enum {
  CRSF_ADDRESS_BROADCAST = 0x00,
  CRSF_ADDRESS_USB = 0x10,
  CRSF_ADDRESS_TBS_CORE_PNP_PRO = 0x80,
  CRSF_ADDRESS_RESERVED1 = 0x8A,
  CRSF_ADDRESS_CURRENT_SENSOR = 0xC0,
  CRSF_ADDRESS_GPS = 0xC2,
  CRSF_ADDRESS_TBS_BLACKBOX = 0xC4,
  CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8,  // 受信データはこれで来る
  CRSF_ADDRESS_RESERVED2 = 0xCA,
  CRSF_ADDRESS_RACE_TAG = 0xCC,
  CRSF_ADDRESS_RADIO_TRANSMITTER = 0xEA,
  CRSF_ADDRESS_CRSF_RECEIVER = 0xEC,
  CRSF_ADDRESS_CRSF_TRANSMITTER = 0xEE,
} crsf_addr_e;

typedef enum {
  CRSF_FRAMETYPE_GPS = 0x02,
  CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
  CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
  CRSF_FRAMETYPE_OPENTX_SYNC = 0x10,
  CRSF_FRAMETYPE_RADIO_ID = 0x3A,
  CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,  // チャンネルパックフレーム
  CRSF_FRAMETYPE_ATTITUDE = 0x1E,
  CRSF_FRAMETYPE_FLIGHT_MODE = 0x21,
  // Extended Header Frames, range: 0x28 to 0x96
  CRSF_FRAMETYPE_DEVICE_PING = 0x28,
  CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
  CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
  CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
  CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
  CRSF_FRAMETYPE_COMMAND = 0x32,
  // MSP commands
  CRSF_FRAMETYPE_MSP_REQ =  0x7A,   // response request using msp sequence as command
  CRSF_FRAMETYPE_MSP_RESP = 0x7B,   // reply with 58 byte chunked binary
  CRSF_FRAMETYPE_MSP_WRITE = 0x7C,  // write with 8 byte chunked binary (OpenTX outbound telemetry buffer limit)
} crsf_frame_type_e;

typedef struct crsf_header_s {
  uint8_t device_addr;  // from crsf_addr_e
  uint8_t frame_size;   // counts size after this byte, so it must be the payload size + 2 (type and crc)
  uint8_t type;         // from crsf_frame_type_e
  uint8_t data[0];
} crsf_header_t;

Adafruit_USBD_HID usb_hid;  // USB HID object
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD_9()  // USB GamePad のデータ構造を指定
};

typedef struct gamepad_data {
  uint16_t ch[8];  // 16bit 8ch
  uint8_t sw;      // 1bit 8ch
} gp_t;

uint8_t rxbuf[CRSF_MAX_PACKET_LEN + 3];  // 受信した生データ
uint8_t rxPos = 0;
static gamepad_data gp;  // CH毎に並び替えたデータ
uint8_t frameSize = 0;
int datardyf = 0;  // USBに送るデータが揃った。
uint32_t gaptime;  // bus 区切り測定用
uint32_t time_m;   // インターバル時間(debug用)

void crsf();
void crsfdecode();
void uart();
void debug_out();

void setup()
{
  // USB HID デバイス設定
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
  while (!USBDevice.mounted()) delay(1);  // wait until device mounted

  datardyf = 0;
  gaptime = 0;
  rxPos = 0;

  // PCシリアル通信用 (受信機の速度に合わせる)
  Serial.begin(CRSF_BAUDRATE);
  // CRSF通信用 (420kbps,8bitdata,nonParity,1stopbit)
  Serial1.begin(CRSF_BAUDRATE, SERIAL_8N1);
  time_m = micros();  // インターバル測定用
}

void loop()
{
  // Remote wakeup
  if (USBDevice.suspended()) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    USBDevice.remoteWakeup();
  }
  crsf();          // CRSF受信処理
  uart();          // UART通信処理(Firmware書き換え用)
  if (datardyf) {  // データが揃ったらUSB送信
    if (usb_hid.ready()) {
      // 17 = sizeof(gp) コンパイルでsizeof()のサイズが変なので直接数値で指定
      usb_hid.sendReport(0, &gp, 17);
#ifdef DEBUG
      debug_out();  // デバッグ用 (シリアルモニターで数値を確認)
#endif
    }
    datardyf = 0;  // データ揃ったよフラグをクリア
  }
}

// CRSF受信処理
void crsf()
{
  uint8_t data;
  // CRSFから1バイト受信
  if (Serial1.available()) {  // Serial1に受信データがあるなら
    data = Serial1.read();    // 8ビットデータ読込
    gaptime = micros();
    if (rxPos == 1) {
      frameSize = data;  // 2byte目はフレームサイズ
    }
    rxbuf[rxPos++] = data;  // 受信データをバッファに格納
    if (rxPos > 1 && rxPos >= frameSize + 2) {
      crsfdecode();  // １フレーム受信し終わったらデーコードする
      rxPos = 0;
    }
  } else {
    // 800us以上データが来なかったら区切りと判定
    if (rxPos > 0 && micros() - gaptime > 800) {
      rxPos = 0;
    }
  }
}

// CRSFから受信した11bitシリアルデータを16bitデータにデコード（さらに底上げ。5bit
// sift）
void crsfdecode()
{
  if (rxbuf[0] == CRSF_ADDRESS_FLIGHT_CONTROLLER) {  // ヘッダチェック
    if (rxbuf[2] ==
        CRSF_FRAMETYPE_RC_CHANNELS_PACKED) {  // CHデータならデコード
      gp.sw = 0;
      gp.ch[0] = ((rxbuf[3] | rxbuf[4] << 8) & 0x07ff) << 5;
      gp.ch[1] = ((rxbuf[4] >> 3 | rxbuf[5] << 5) & 0x07ff) << 5;
      gp.ch[2] = ((rxbuf[5] >> 6 | rxbuf[6] << 2 | rxbuf[7] << 10) & 0x07ff)
                 << 5;
      gp.ch[3] = ((rxbuf[7] >> 1 | rxbuf[8] << 7) & 0x07ff) << 5;
      if (((rxbuf[8] >> 4 | rxbuf[9] << 4) & 0x07ff) > 0x3ff)
        gp.sw |= 0x01;  // AUX1は2値データ
      gp.ch[4] = ((rxbuf[9] >> 7 | rxbuf[10] << 1 | rxbuf[11] << 9) & 0x07ff)
                 << 5;
      gp.ch[5] = ((rxbuf[11] >> 2 | rxbuf[12] << 6) & 0x07ff) << 5;
      gp.ch[6] = ((rxbuf[12] >> 5 | rxbuf[13] << 3) & 0x07ff) << 5;
      gp.ch[7] = ((rxbuf[14] | rxbuf[15] << 8) & 0x7ff) << 5;
      if (((rxbuf[15] >> 3 | rxbuf[16] << 5) & 0x07ff) > 0x3ff) gp.sw |= 0x02;
      if (((rxbuf[16] >> 6 | rxbuf[17] << 2 | rxbuf[18] << 10) & 0x07ff) >
          0x3ff)
        gp.sw |= 0x04;
      if (((rxbuf[18] >> 1 | rxbuf[19] << 7) & 0x07ff) > 0x3ff) gp.sw |= 0x08;
      if (((rxbuf[19] >> 4 | rxbuf[20] << 4) & 0x07ff) > 0x3ff) gp.sw |= 0x10;
      if (((rxbuf[20] >> 7 | rxbuf[21] << 1 | rxbuf[22] << 9) & 0x07ff) > 0x3ff)
        gp.sw |= 0x20;
      if (((rxbuf[22] >> 2 | rxbuf[23] << 6) & 0x07ff) > 0x3ff) gp.sw |= 0x40;
      if (((rxbuf[23] >> 5 | rxbuf[24] << 3) & 0x07ff) > 0x3ff) gp.sw |= 0x80;

      datardyf = 1;  // データ揃ったよフラグ
    }
  }
}

// UART通信処理( Firmware書き換え用 )
// ExpressLRS Configurator の Flashing Method は [UART]ではなく
// [BetaflightPassthough] にすること。
void uart()
{
  uint32_t t;

  // PCからデータが来たら、強制的に書き換えモードだと判断
  if (Serial.available()) {
    t = millis();
    do {
      while (Serial.available()) {     // PCからデータが来たら
        Serial1.write(Serial.read());  // PCからのデータを受信機に送る
        t = millis();
      }
      while (Serial1.available()) {    // 受信機からデータが来たら
        Serial.write(Serial1.read());  // 受信機のデータをPCに送る
        t = millis();
      }
    } while (millis() - t < 2000);  // データが来なくなったら終了
  }
}

// シリアルモニターに受信データを表示する（デバック用）
void debug_out()
{
  int i;
  Serial.print(rxbuf[0], HEX);  // device addr
  Serial.print(" ");
  Serial.print(rxbuf[1]);  // data size +1
  Serial.print(" ");
  Serial.print(rxbuf[2], HEX);  // type
  Serial.print(" ");
  for (i = 0; i < 8; i++) {
    Serial.print(gp.ch[i]);
    Serial.print(" ");
  }
  Serial.print(gp.sw, BIN);
  Serial.print(" ");
  Serial.print(micros() - time_m);  // インターバル時間(us)を表示
  Serial.println("us");
  time_m = micros();
}
