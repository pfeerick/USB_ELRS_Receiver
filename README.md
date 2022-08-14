USB CRSF/ELRS Receiver
======================

Use your ELRS transmitter as a joystick for your PC simulator using a 
microcontroller and ELRS receiver.


## Hardware Requirements
 - Microcontroller: 
   - [Seeed Studio XIAO SAMD21](https://www.seeedstudio.com/Seeeduino-XIAO-Arduino-Microcontroller-SAMD21-Cortex-M0+-p-4426.html)
   - [Seeed Studio XAIO RP2040](https://www.seeedstudio.com/XIAO-RP2040-v1-0-p-5026.html)
 - ELRS Receiver - e.g. [EP2](http://www.happymodel.cn/index.php/2021/04/10/happymodel-2-4g-expresslrs-elrs-nano-series-receiver-module-pp-rx-ep1-rx-ep2-rx) / 
                        [RP2](https://www.radiomasterrc.com/products/rp2-expresslrs-2-4ghz-nano-receiver)

Note: While the RP2040 is supported, I recommend the XIAO SAMD21, as RP2040 
platform support will take up some 2.5GB of space, so will take a while to 
download. I also ran into an issue on Windows 11 where the firmware could not 
be uploaded until using [Zadig](https://zadig.akeo.ie/) to install the 
`libusb-win32` driver for the `RP2 Boot2 (Interface 1)`.


## Device Connections: 

```
  XAIO   ---   ELRS
  =================
  5V  <－－－＞ 5V
  GND <－－－＞ GND
  RX  <－－－＞ TX
  TX  <－－－＞ RX
```
Both signal lines are 3.3V, just connect them as they are.

https://camo.qiitausercontent.com/bc1d135fc8b25d365a72593a9524bc0f3f4c10c9/68747470733a2f2f71696974612d696d6167652d73746f72652e73332e61702d6e6f727468656173742d312e616d617a6f6e6177732e636f6d2f302f323532303735382f66393961366138392d653838392d646334652d316239332d3065323937613334323965642e6a706567


## Acknowledgments:

Based on the project originally developed by kobatan:
https://qiita.com/kobatan/items/40728fbb625057d9f42b
https://qiita.com/kobatan/items/253f1614a8653a1dcb1a
