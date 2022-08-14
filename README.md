USB ELRS Receiver
=================

Use your ELRS transmitter with PC simulator.

## Hardware Requirements
 - Microcontroller: 
   - Seeed Studio XIAO SAMD21
   - Seeed Studio XAIO RP2040
 - ELRS Receiver (EP2/RP2 recommended)


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


## Acknoledgements:

Based on the project originally developed by kobatan:
https://qiita.com/kobatan/items/40728fbb625057d9f42b
https://qiita.com/kobatan/items/253f1614a8653a1dcb1a
