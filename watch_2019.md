# 1.Pinout table

|    Peripherals    |  2019/TOUCH  |
| :---------------: | :----------: |
|        Core       | ESP32-DOWDQ6 |
|       Flash       |     16MB     |
|       SPRAM       |      8MB     |
|    TOUCH Driver   |    FT6236    |
|     TFT Driver    |    ST7789    |
|      TFT Size     | 240x240/1.54 |
|      TFT RST      |      N/A     |
|      TFT MISO     |      N/A     |
|      TFT MOSI     |      19      |
|      TFT SCLK     |      18      |
|       TFT DC      |      27      |
|       TFT CS      |       5      |
|   TFT BackLight   |      12      |
|   SENSOR SDA\[3]  |      21      |
|   SENSOR SCL\[3]  |      22      |
|     FT6236 SDA    |      23      |
|     FT6236 SCL    |      32      |
|       MOTOR       |    N/A\[1]   |
|  BMA423 Interrupt |      39      |
|  FT6236 Interrupt |      38      |
| PCF8563 Interrupt |      37      |
|       BUTTON      |      36      |
|  AXP202 Interrupt |      35      |
|      I2S BCK      |    N/A\[1]   |
|       I2S WS      |    N/A\[1]   |
|      I2S DOUT     |    N/A\[1]   |
|      IR Send      |      N/A     |
|      SD MISO      |      N/A     |
|      SD MOSI      |      N/A     |
|      SD SCLK      |      N/A     |
|       SD CS       |      N/A     |

\[1]. According to different base plates

\[2]. Capacitive touch button

\[3]. SENSOR SDA and SCL share PCF8563, BMA423, AXP202,T-Block without BMA423, use MPU6050

## 2. AXP202 Power domain

| channel |       Explanation      |
| :-----: | :--------------------: |
|   DC2   |         NO USE         |
|   DC3   |   ESP32(Can't close)   |
|   LDO1  |      Can't control     |
|   LDO2  |        Backlight       |
|   LDO3  | Backplane power supply |
|   LDO4  | S76/78G Backplane only |

## 3. Bottom plate Pinout

**Standard**

| ESP32 Core | GPIO33 |  GPIO25 | GPIO21 | GPIO22 |
| :--------: | :----: | :-----: | :----: | :----: |
|  Standard  |  Motor | Speaker |   SDA  |   SCL  |

- Onboard SD card slot
- The motor/speaker power domain is LDO3

**MPR121**

| ESP32 Core | GPIO21 | GPIO22 |
| :--------: | :----: | :----: |
|   MPR121   |   SDA  |   SCL  |

- Onboard SD card slot
- The sensor power domain is LDO3

**GPS**

| ESP32 Core | GPIO33 | GPIO34 |
| :--------: | :----: | :----: |
|     gps    |   TX   |   RX   |

- Onboard SD card slot
- The gps power domain is LDO3

## 3.datasheet

- [Esp32](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [AXP202](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/AXP202)
- [BMA423](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/BMA423)
- [ST7789](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/blob/master/DISPLAY/ST7789V.pdf)
- [PCF8563](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/RTC)
- [FT6236](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/blob/master/TOUCHSCREEN/FT6236-FT6336-FT6436L-FT6436_Datasheet.pdf)
- [MPU6050](https://github.com/Xinyuan-LilyGO/LilyGo-HAL/tree/master/MPU6050)

<br />

