/*
https://qiita.com/kobatan/items/40728fbb625057d9f42b
https://qiita.com/kobatan/items/253f1614a8653a1dcb1a
*/

/*
  Connections: 

  5V  <－－－＞ 5V
  GND <－－－＞ GND
  RX  <－－－＞ ELRS TX
  TX  <－－－＞ ELRS RX

  Both signal lines are 3.3V, so just connect them as they are.

  https://camo.qiitausercontent.com/bc1d135fc8b25d365a72593a9524bc0f3f4c10c9/68747470733a2f2f71696974612d696d6167652d73746f72652e73332e61702d6e6f727468656173742d312e616d617a6f6e6177732e636f6d2f302f323532303735382f66393961366138392d653838392d646334652d316239332d3065323937613334323965642e6a706567
*/

#include <Arduino.h>
#include <Adafruit_TinyUSB.h> 

// #define DEBUG

// USB HID report descriptor
// Specifies the structure of the gamepad data (for radio receiver 
// (16bit data x 8ch) + (1bit data x 8ch))
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
      HID_COLLECTION_END  

// CrossFire settings
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
  CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8,  // Incoming data comes in this
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
  CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,  // packed channel frame
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
    TUD_HID_REPORT_DESC_GAMEPAD_9()  // USB GamePad data structure
};

typedef struct gamepad_data {
  uint16_t ch[8];  // 16bit 8ch
  uint8_t sw;      // 1bit 8ch
} gp_t;

uint8_t rxbuf[CRSF_MAX_PACKET_LEN + 3];  // Raw data received from the CRSF Rx
uint8_t rxPos = 0;
static gamepad_data gp;  // Data sorted by channel
uint8_t frameSize = 0;
int datardyf = 0;  // data read to send to USB
uint32_t gaptime;  // for bus delimiter measurement

#if defined(DEBUG)
uint32_t time_m;   //  interval time (for debug)
void debug_out();
#endif

void crsf();
void crsfdecode();
void uart();

void setup()
{
  // USB HID Device Settings
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
  while (!USBDevice.mounted()) delay(1);  // wait until device mounted

  datardyf = 0;
  gaptime = 0;
  rxPos = 0;

  // For PC serial communication (to match receiver speed)
  Serial.begin(CRSF_BAUDRATE);
  // For CRSF communication (420kbps,8bitdata,nonParity,1stopbit)
  Serial1.begin(CRSF_BAUDRATE, SERIAL_8N1);
#if defined(DEBUG)
  time_m = micros();  // For interval measurement
#endif
}

void loop()
{
  // Remote wakeup
  if (USBDevice.suspended()) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    USBDevice.remoteWakeup();
  }
  crsf();          // Process CRSF
  uart();          // UART communication processing (for firmware rewriting)
  if (datardyf) {  // USB transmission when data is ready
    if (usb_hid.ready()) {
      // 17 = sizeof(gp) Directly specify sizeof() as a number since sizeof() size is strange in compile
      usb_hid.sendReport(0, &gp, 17);
#ifdef DEBUG
      debug_out();  // for debugging (check values on serial monitor)
#endif
    }
    datardyf = 0;  // Clear the flag after transmission
  }
}

//CRSF receive process
void crsf()
{
  uint8_t data;
  // byte received from CRSF
  if (Serial1.available()) {  // If there is incoming data on Serial1
    data = Serial1.read();    // 8-bit data read
    gaptime = micros();
    if (rxPos == 1) {
      frameSize = data;  // Second byte is the frame size
    }
    rxbuf[rxPos++] = data;  //  Store received data in buffer
    if (rxPos > 1 && rxPos >= frameSize + 2) {
      crsfdecode();  // Decode after receiving one frame
      rxPos = 0;
    }
  } else {
    // If no data comes in for more than 800us, judge as a break
    if (rxPos > 0 && micros() - gaptime > 800) {
      rxPos = 0;
    }
  }
}

// Decode 11-bit serial data received from CRSF to 16-bit data (further bottom up. 5-bit)
// sift)
void crsfdecode()
{
  if (rxbuf[0] == CRSF_ADDRESS_FLIGHT_CONTROLLER) {  // header check
    if (rxbuf[2] ==
        CRSF_FRAMETYPE_RC_CHANNELS_PACKED) {  // Decode if CH data
      gp.sw = 0;
      gp.ch[0] = ((rxbuf[3] | rxbuf[4] << 8) & 0x07ff) << 5;
      gp.ch[1] = ((rxbuf[4] >> 3 | rxbuf[5] << 5) & 0x07ff) << 5;
      gp.ch[2] = ((rxbuf[5] >> 6 | rxbuf[6] << 2 | rxbuf[7] << 10) & 0x07ff)
                 << 5;
      gp.ch[3] = ((rxbuf[7] >> 1 | rxbuf[8] << 7) & 0x07ff) << 5;
      if (((rxbuf[8] >> 4 | rxbuf[9] << 4) & 0x07ff) > 0x3ff)
        gp.sw |= 0x01;  //  AUX1 is binary data
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

      datardyf = 1;  // set data ready flag
    }
  }
}

// UART communication process (for firmware rewriting)
// Flashing Method in ExpressLRS Configurator should be [BetaflightPassthough], not [UART].
void uart()
{
  uint32_t t;

  // When data comes from the PC, it is determined to be in firmware update mode
  if (Serial.available()) {
    t = millis();
    do {
      while (Serial.available()) {     // When data comes in from the PC
        Serial1.write(Serial.read());  // Send data from PC to receiver
        t = millis();
      }
      while (Serial1.available()) {    // When data comes in from the receiver
        Serial.write(Serial1.read());  // Send receiver data to PC
        t = millis();
      }
    } while (millis() - t < 2000);  // When data stops coming in, it's over
  }
}

#if defined(DEBUG)
// Display received data on serial monitor (for debugging)
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
  Serial.print(micros() - time_m);  //Display interval time (us)
  Serial.println("us");
  time_m = micros();
}
#endif
