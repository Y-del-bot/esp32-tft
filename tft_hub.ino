/*
  🎮 ESP32 TFT 大礼包 · 4 合 1
  ==============================
  硬件：ESP32 + 2.4" ILI9341 TFT（仅这两块！）
  接线：TFT VCC→3V3  GND→GND  SCK→GPIO18  MOSI→GPIO23
        TFT CS→GPIO5  DC→GPIO16  RST→GPIO14  BLK→3V3
  控制：GPIO0 BOOT 键
        · 短按 = 菜单下移 / 退出当前项目
        · 长按(>1s) = 选中项目
  菜单：
    1. 🐍 Snake        贪吃蛇（手机网页遥控）
    2. 📡 WiFi Radar   扫周围 WiFi 信号
    3. 🕐 Clock        NTP 联网时钟
    4. 💰 Price        BTC/ETH 实时价
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

/* ============ 引脚 ============ */
#define TFT_MOSI 23
#define TFT_SCK  18
#define TFT_CS    5
#define TFT_DC   16
#define TFT_RST  14
#define BTN      0   // BOOT 键
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

/* ============ WiFi ============ */
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

/* ============ 菜单 ============ */
enum { MENU, SNAKE, RADAR, CLOCK, PRICE };
int current = MENU;
int menuSel = 0;
const char* items[] = { "1. Snake", "2. WiFi Radar", "3. Clock", "4. Price" };
const int ITEM_CNT = 4;

/* ============ BOOT 键检测 ============ */
bool btnPressed() { return digitalRead(BTN) == LOW; }

/* 返回：0=没按, 1=短按抬起, 2=长按抬起 */
int checkBtn() {
  static bool wasPressed = false;
  static unsigned long pressStart = 0;
  bool now = btnPressed();
  if (!wasPressed && now) { pressStart = millis(); wasPressed = true; return 0; }
  if (wasPressed && !now) {
    wasPressed = false;
    return (millis() - pressStart >= 1000) ? 2 : 1;
  }
  return 0;
}

/* ============ 联网 ============ */
bool wifiOK = false;
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) { wifiOK = true; return; }
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.println("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  int dot = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300);
    tft.setCursor(10 + dot * 10, 90);
    tft.print(".");
    dot = (dot + 1) % 10;
  }
  wifiOK = (WiFi.status() == WL_CONNECTED);
  tft.setTextColor(wifiOK ? ILI9341_GREEN : ILI9341_RED);
  tft.setCursor(10, 120);
  tft.println(wifiOK ? WiFi.localIP().toString() : "FAIL — hotsp on?");
  delay(1200);
}

/* ============================================================ */
/*                     主菜单                                    */
/* ============================================================ */
void drawMenu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("ESP32 TFT Hub");
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.setTextColor(ILI9341_DARKGREY);
  tft.println("Short press BTN0 = down");
  tft.println("Long  press BTN0 = select");
  tft.println("Press during app = back");

  for (int i = 0; i < ITEM_CNT; i++) {
    int y = 95 + i * 32;
    if (i == menuSel) {
      tft.fillRect(0, y - 2, 320, 26, ILI9341_NAVY);
      tft.setTextColor(ILI9341_YELLOW);
      tft.setTextSize(2);
    } else {
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(2);
    }
    tft.setCursor(20, y);
    tft.println(items[i]);
  }
}

void runMenu() {
  drawMenu();
  while (true) {
    int b = checkBtn();
    if (b == 1) { menuSel = (menuSel + 1) % ITEM_CNT; drawMenu(); delay(150); }
    if (b == 2) {
      current = menuSel + 1;  // MENU=0, SNAKE=1...
      return;
    }
    delay(30);
  }
}

/* ============================================================ */
/*                     🐍 贪吃蛇                                 */
/* ============================================================ */
#define GRID_W 32
#define GRID_H 24
#define CELL   10
enum Dir { UP, DOWN, LEFT, RIGHT };
struct Point { int x, y; };
#define MAX_SNAKE 200
Point snake[MAX_SNAKE];
int snakeLen = 3;
Dir sdir = RIGHT;
Point food;
bool srun = true;
int sscore = 0;
unsigned long lastTick = 0;
WebServer* srv = nullptr;

const char SNAKE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>🐍 Snake</title>
<style>*{box-sizing:border-box}body{margin:0;background:#0d1b2a;color:#e0e1dd;font-family:-apple-system,sans-serif;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px}h1{margin:0;font-size:20px;color:#48cae4}.box{background:#1b263b;border-radius:12px;padding:18px 22px;box-shadow:0 4px 20px rgba(0,0,0,.3);text-align:center}.btn{width:64px;height:64px;border:none;border-radius:12px;background:#415a77;color:#fff;font-size:28px;font-weight:bold;touch-action:manipulation}.btn:active{background:#48cae4;color:#0d1b2a;transform:scale(.92)}.row{display:flex;gap:10px;justify-content:center}#score{font-size:32px;font-weight:bold;color:#ffd166}#msg{color:#ef476f;font-weight:bold;min-height:20px}#act{background:#06d6a0}#act:active{background:#0ae1aa;color:#0d1b2a}</style></head>
<body><div class="box"><h1>🐍 ESP32 Snake</h1><div id="score">Score: 0</div><div id="msg"></div></div>
<div class="box"><div class="row"><button class="btn" ontouchstart="send('UP')">▲</button></div>
<div class="row"><button class="btn" ontouchstart="send('LEFT')">◀</button><button class="btn" id="act" ontouchstart="send('TOGGLE')">⏯</button><button class="btn" ontouchstart="send('RIGHT')">▶</button></div>
<div class="row"><button class="btn" ontouchstart="send('DOWN')">▼</button></div>
<div class="row" style="margin-top:8px"><button class="btn" ontouchstart="send('RESET')" style="background:#ef476f;font-size:18px">RESET</button></div></div>
<script>async function send(cmd){try{await fetch('/?cmd='+cmd)}catch(e){}}async function tick(){try{let r=await fetch('/state');let d=await r.json();document.getElementById('score').textContent='Score: '+d.score;document.getElementById('msg').textContent=d.msg||''}catch(e){}}setInterval(tick,500);</script></body></html>)HTML";

void drawCell(int x, int y, uint16_t c) { tft.fillRect(x*CELL, y*CELL, CELL-1, CELL-1, c); }
void sDrawBorder() { tft.fillRect(0,0,320,240,ILI9341_BLACK); tft.drawRect(0,0,319,239,ILI9341_DARKGREEN); }
void sSpawnFood() { for(int i=0;i<200;i++){food.x=random(1,GRID_W-1);food.y=random(1,GRID_H-1);bool ok=true;for(int j=0;j<snakeLen;j++)if(snake[j].x==food.x&&snake[j].y==food.y){ok=false;break;}if(ok)return;} }
void sReset() { sDrawBorder(); snakeLen=3; snake[0]={GRID_W/2,GRID_H/2}; snake[1]={GRID_W/2-1,GRID_H/2}; snake[2]={GRID_W/2-2,GRID_H/2}; sdir=RIGHT; sscore=0; srun=true; lastTick=0; sSpawnFood(); }

void sStep() {
  if (!srun) return;
  Point head = snake[0];
  if (sdir==UP) head.y--; else if (sdir==DOWN) head.y++;
  else if (sdir==LEFT) head.x--; else head.x++;
  if (head.x<=0||head.x>=GRID_W-1||head.y<=0||head.y>=GRID_H-1){srun=false;return;}
  for(int i=0;i<snakeLen;i++) if(snake[i].x==head.x&&snake[i].y==head.y){srun=false;return;}
  bool ate = (head.x==food.x && head.y==food.y);
  Point tail = snake[snakeLen-1];
  drawCell(tail.x, tail.y, ILI9341_BLACK);
  for(int i=snakeLen-1;i>0;i--) snake[i]=snake[i-1];
  snake[0]=head;
  if(ate){snakeLen++;sscore+=10;sSpawnFood();}
  drawCell(head.x, head.y, ILI9341_CYAN);
  drawCell(food.x, food.y, ILI9341_RED);
}

String snakeJson() { String m = srun ? "" : "GAME OVER"; return "{\"score\":"+String(sscore)+",\"msg\":\""+m+"\"}"; }
void sHandle() {
  String cmd = srv->arg("cmd");
  if(cmd=="UP"&&sdir!=DOWN)sdir=UP;
  if(cmd=="DOWN"&&sdir!=UP)sdir=DOWN;
  if(cmd=="LEFT"&&sdir!=RIGHT)sdir=LEFT;
  if(cmd=="RIGHT"&&sdir!=LEFT)sdir=RIGHT;
  if(cmd=="TOGGLE")srun=!srun;
  if(cmd=="RESET")sReset();
  srv->send_P(200,"text/html",SNAKE_HTML);
}
void sState(){ srv->send(200,"application/json",snakeJson()); }

void runSnake() {
  connectWiFi();
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 215); tft.println("Long press BTN0 = back");
  sReset();
  srv = new WebServer(80);
  srv->on("/", sHandle); srv->on("/state", sState);
  srv->begin();
  unsigned long start = millis();
  while (true) {
    srv->handleClient();
    if (millis() - lastTick >= 120) { sStep(); lastTick = millis(); }
    int b = checkBtn();
    if (b == 2) { srv->stop(); delete srv; return; }
    delay(10);
  }
}

/* ============================================================ */
/*                     📡 WiFi 雷达                             */
/* ============================================================ */
void runRadar() {
  connectWiFi();
  while (true) {
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(2); tft.setCursor(10, 5); tft.println("WiFi Radar");
    tft.setTextSize(1); tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(10, 25); tft.println("Scanning...");
    int n = WiFi.scanNetworks();
    int rows = min(n, 10);
    int barY = 28;
    for (int i = 0; i < rows; i++) {
      int y = 32 + i * 18;
      tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 14) ssid = ssid.substring(0, 13) + ".";
      tft.setCursor(5, y); tft.print(ssid);
      int rssi = WiFi.RSSI(i);
      int pct = constrain(map(rssi, -95, -40, 0, 100), 0, 100);
      uint16_t col = pct > 60 ? ILI9341_GREEN : (pct > 30 ? ILI9341_YELLOW : ILI9341_RED);
      tft.fillRect(175, y + 2, (pct * 130) / 100, 10, col);
      tft.setTextColor(ILI9341_DARKGREY);
      tft.setCursor(308, y); tft.print((String)(int)pct);
    }
    tft.setTextColor(ILI9341_DARKGREY); tft.setCursor(10, 228);
    tft.print("Scanned "); tft.print(n); tft.print(" networks   BTN0=exit");
    WiFi.scanDelete();
    int b = checkBtn();
    if (b == 1 || b == 2) return;
    delay(5000);
  }
}

/* ============================================================ */
/*                     🕐 NTP 时钟                               */
/* ============================================================ */
#include <time.h>
const char* NTP_HOST = "pool.ntp.org";
const long  TZ_OFFSET = 8 * 3600;  // UTC+8 中国
const int   DST = 0;

void runClock() {
  connectWiFi();
  configTime(TZ_OFFSET, DST, NTP_HOST);
  tft.fillScreen(ILI9341_BLACK);
  while (true) {
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    char dateStr[32]; char timeStr[16];
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d  %s",
      t.tm_year+1900, t.tm_mon+1, t.tm_mday,
      (const char*[]){ "Sun","Mon","Tue","Wed","Thu","Fri","Sat" }[t.tm_wday]);
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);

    tft.fillRect(0, 0, 320, 240, ILI9341_BLACK);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    tft.setTextSize(1); tft.setCursor(10, 20); tft.println("ESP32 NTP Clock");
    tft.setTextColor(ILI9341_GREEN); tft.setTextSize(4);
    tft.setCursor(35, 70); tft.println(timeStr);
    tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
    tft.setCursor(60, 160); tft.println(dateStr);
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(10, 228); tft.println("BTN0=exit");

    int b = checkBtn();
    if (b == 1 || b == 2) return;
    delay(500);
  }
}

/* ============================================================ */
/*                     💰 币价看板                               */
/* ============================================================ */
void runPrice() {
  connectWiFi();
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2); tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(10, 10); tft.println("Coin Price");
  tft.setTextSize(1); tft.setTextColor(ILI9341_DARKGREY);
  tft.setCursor(10, 228); tft.println("BTN0=exit");

  unsigned long lastFetch = 0;
  char btcStr[24] = "..."; char ethStr[24] = "...";
  char btcChg[12] = "";   char ethChg[12] = "";
  int btcChgSign = 0, ethChgSign = 0;

  while (true) {
    if (WiFi.status() == WL_CONNECTED && millis() - lastFetch > 15000) {
      HTTPClient http;
      http.begin("http://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=usd&include_24hr_change=true");
      int code = http.GET();
      if (code == 200) {
        String payload = http.getString();
        // 简单解析，不引 ArduinoJson 省空间
        int bi = payload.indexOf("bitcoin");
        int ei = payload.indexOf("ethereum");
        if (bi >= 0) {
          int pi = payload.indexOf("usd", bi);
          int colon = payload.indexOf(":", pi);
          int comma = payload.indexOf(",", colon);
          String p = payload.substring(colon+1, comma);
          p.trim();
          float f = p.toFloat();
          snprintf(btcStr, sizeof(btcStr), "BTC $%.0f", f);
          // change
          int ci = payload.indexOf("24h_change", bi);
          int cc = payload.indexOf(":", ci);
          int ce = payload.indexOf(",", cc);
          String c = payload.substring(cc+1, ce);
          float cf = c.toFloat();
          btcChgSign = cf >= 0 ? 1 : -1;
          snprintf(btcChg, sizeof(btcChg), "%+.1f%%", cf);
        }
        if (ei >= 0) {
          int pi = payload.indexOf("usd", ei);
          int colon = payload.indexOf(":", pi);
          int comma = payload.indexOf(",", colon);
          String p = payload.substring(colon+1, comma);
          p.trim();
          float f = p.toFloat();
          snprintf(ethStr, sizeof(ethStr), "ETH $%.0f", f);
          int ci = payload.indexOf("24h_change", ei);
          int cc = payload.indexOf(":", ci);
          int ce = payload.indexOf(",", cc);
          String c = payload.substring(cc+1, ce);
          float cf = c.toFloat();
          ethChgSign = cf >= 0 ? 1 : -1;
          snprintf(ethChg, sizeof(ethChg), "%+.1f%%", cf);
        }
      }
      http.end();
      lastFetch = millis();
    }

    // 画
    tft.fillRect(0, 45, 320, 180, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK); tft.setTextSize(2);
    tft.setCursor(20, 60);  tft.println(btcStr);
    tft.setCursor(20, 140); tft.println(ethStr);
    tft.setTextSize(1);
    tft.setTextColor(btcChgSign >= 0 ? ILI9341_GREEN : ILI9341_RED);
    tft.setCursor(220, 70); tft.println(btcChg);
    tft.setTextColor(ethChgSign >= 0 ? ILI9341_GREEN : ILI9341_RED);
    tft.setCursor(220, 150); tft.println(ethChg);

    int b = checkBtn();
    if (b == 1 || b == 2) return;
    delay(1000);
  }
}

/* ============================================================ */
/*                     Setup / Loop                             */
/* ============================================================ */
void setup() {
  Serial.begin(115200); delay(100);
  pinMode(BTN, INPUT_PULLUP);
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
}

void loop() {
  switch (current) {
    case MENU:  runMenu();  break;
    case SNAKE: runSnake(); current = MENU; break;
    case RADAR: runRadar(); current = MENU; break;
    case CLOCK: runClock(); current = MENU; break;
    case PRICE: runPrice(); current = MENU; break;
  }
}
