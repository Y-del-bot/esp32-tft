# ESP32 TFT Hub · 4合1 迷你大礼包

基于 ESP32 + 2.4" ILI9341 TFT 彩屏的单机 4 合 1 趣味项目集合。

**只需要两块硬件：ESP32 开发板 + ILI9341 TFT 彩屏**（连面包板和电阻都不用！）

---

## ✨ 包含项目

| # | 项目 | 说明 |
|---|---|---|
| 1 | **Snake (贪吃蛇)** | 手机网页遥控，WiFi 同网段即可操作 |
| 2 | **WiFi Radar** | 扫描周围 2.4GHz WiFi 信号强度，彩色条形图显示 |
| 3 | **NTP Clock** | 联网自动校时，大字显示日期时间（UTC+8 中国时区） |
| 4 | **Coin Price** | CoinGecko API 抓 BTC/ETH 实时价 + 24h 涨跌 |

---

## 🔌 硬件接线

**TFT 排针从下往上数（只接下面 8 根，上面触摸/SD 全部留空）：**

| TFT 排针脚 | TFT 丝印 | → | ESP32 丝印 | ESP32 位置 |
|---|---|---|---|---|
| 14 (最底) | UCC | → | 3V3 | 下排最左 |
| 13 | GND | → | GND | 下排 |
| 12 | RESET | → | 014 | **上排** |
| 11 | CS | → | D5 | 下排 |
| 10 | DC | → | RX2 | 下排 (GPIO16) |
| 9 | SDI(MOSI) | → | D23 | 下排最右 |
| 8 | SCK | → | D18 | 下排 |
| 7 | LED | → | 3V3 | 同 UCC |

> **ESP32 引脚命名说明**：GPIO16 在上排印为 `RX2`，GPIO17 印为 `TX2`。
> **UCC 和 LED 都接 3V3**：可通过面包板同一列共享，或两根线拧一起插。

### 一句话口诀
```
LED→3V3  SCK→D18  SDI→D23  DC→RX2  CS→D5  RESET→014  GND→GND  UCC→3V3
```

---

## 🧩 所需库（Arduino Library Manager 一键搜）

| 库 | 版本 |
|---|---|
| Adafruit GFX Library | 最新 |
| Adafruit ILI9341 | 最新 |
| WiFi / WebServer / HTTPClient | ESP32 Core 自带 |

---

## 🚀 使用

1. 改 WiFi 参数：
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI_SSID";
   const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
   ```

2. Arduino IDE 打开 `tft_hub.ino` → 选板 **ESP32 Dev Module** → 烧录

3. 接线 → 插 USB → TFT 亮菜单

4. **GPIO0 BOOT 键控制**：
   - **短按** → 菜单项下移 / 项目内退出
   - **长按 (>1秒)** → 选中当前项目

5. 贪吃蛇：Serial Monitor 看 ESP32 IP → 手机浏览器打开 `http://那个IP`

---

## 📁 引脚定义一览

```cpp
#define TFT_MOSI 23
#define TFT_SCK  18
#define TFT_CS    5
#define TFT_DC   16   // ESP32 丝印 RX2
#define TFT_RST  14   // ESP32 丝印 014
#define BTN       0   // BOOT 键
```

---

## 📜 License

MIT — 随便玩，随便改。
