#include <TFT_eSPI.h>
#include "LIS3DHTR.h"

TFT_eSPI tft;
LIS3DHTR<TwoWire> lis;

// 游戏参数
#define PLATFORM_SIZE 180
#define BALL_RADIUS 8
#define GRAVITY 0.6
#define FRICTION 0.92
#define MAX_SPEED 4.0
#define EDGE_BUFFER 3  // 边缘缓冲像素

// 颜色定义
#define BG_COLOR TFT_BLACK
#define PLAT_COLOR TFT_DARKGREEN
#define BALL_COLOR TFT_YELLOW
#define TEXT_COLOR TFT_WHITE

// 游戏状态枚举
enum GameState {
  STATE_MENU,
  STATE_PLAYING,
  STATE_PAUSED,
  STATE_GAMEOVER
};

// 全局变量
GameState gameState = STATE_MENU;
float ballX, ballY, speedX, speedY;
uint32_t gameStartTime, survivalTime;
uint16_t highScore = 0;

void setup() {
  Serial.begin(115200);
  
  // 初始化硬件
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  
  // 初始化屏幕
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(BG_COLOR);
  
  // 初始化加速度计
  lis.begin(Wire1);
  lis.setOutputDataRate(LIS3DHTR_DATARATE_50HZ);
  lis.setFullScaleRange(LIS3DHTR_RANGE_2G);
  
  showMainMenu();
}

void loop() {
  handleInput();
  
  switch(gameState){
    case STATE_PLAYING:
      updateGame();
      break;
      
    case STATE_PAUSED:
    case STATE_MENU:
    case STATE_GAMEOVER:
    default:
      delay(50); // 降低非游戏状态时的功耗
      break;
  }
}

void handleInput() {
  // 按键A - 开始/继续游戏
  if(debounce(WIO_KEY_A)){
    if(gameState == STATE_MENU || gameState == STATE_GAMEOVER){
      startNewGame();
    }
    else if(gameState == STATE_PAUSED){
      resumeGame();
    }
  }
  
  // 按键B - 暂停/恢复
  if(debounce(WIO_KEY_B)){
    if(gameState == STATE_PLAYING){
      pauseGame();
    }
    else if(gameState == STATE_PAUSED){
      resumeGame();
    }
  }
  
  // 按键C - 重新开始
  if(debounce(WIO_KEY_C)){
    if(gameState == STATE_PLAYING || gameState == STATE_PAUSED){
      showMainMenu();
    }
  }
}

bool debounce(uint8_t pin){
  static uint32_t lastPress[3] = {0};
  uint8_t index = pin - WIO_KEY_A;
  
  if(digitalRead(pin) == LOW && (millis() - lastPress[index]) > 200){
    lastPress[index] = millis();
    while(digitalRead(pin) == LOW); // 等待释放
    return true;
  }
  return false;
}

void startNewGame(){
  // 初始化游戏参数
  ballX = tft.width()/2;
  ballY = tft.height()/2;
  speedX = speedY = 0;
  gameStartTime = millis();
  
  // 绘制游戏界面
  tft.fillScreen(BG_COLOR);
  drawPlatform();
  updateTimer();
  gameState = STATE_PLAYING;
}

void pauseGame(){
  gameState = STATE_PAUSED;
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextSize(3);
  tft.drawCentreString("PAUSED", 160, 100, 2);
}

void resumeGame(){
  tft.fillRect(100, 80, 120, 180, BG_COLOR); // 清除暂停文字

// 绘制游戏界面
  tft.fillScreen(BG_COLOR);
  drawPlatform();
  updateTimer();

  gameState = STATE_PLAYING;
}

void updateGame(){
  static uint32_t lastUpdate = 0;
  if(millis() - lastUpdate < 33) return; // 约30fps
  lastUpdate = millis();
  
  // 读取加速度计数据
  float ay = lis.getAccelerationX() * GRAVITY;
  float ax = -lis.getAccelerationY() * GRAVITY;
  
  // 更新物理状态
  speedX = constrain(speedX + ax, -MAX_SPEED, MAX_SPEED);
  speedY = constrain(speedY + ay, -MAX_SPEED, MAX_SPEED);
  speedX *= FRICTION;
  speedY *= FRICTION;
  
  // 计算新位置
  float newX = ballX + speedX;
  float newY = ballY + speedY;
  
  // 边界碰撞检测
  if(abs(newX - 160) > (PLATFORM_SIZE/2 - BALL_RADIUS + EDGE_BUFFER)){
    speedX *= -0.6;
    newX = constrain(newX, 
                   160 - PLATFORM_SIZE/2 + BALL_RADIUS - EDGE_BUFFER,
                   160 + PLATFORM_SIZE/2 - BALL_RADIUS + EDGE_BUFFER);
  }
  if(abs(newY - 120) > (PLATFORM_SIZE/2 - BALL_RADIUS + EDGE_BUFFER)){
    speedY *= -0.6;
    newY = constrain(newY,
                   120 - PLATFORM_SIZE/2 + BALL_RADIUS - EDGE_BUFFER,
                   120 + PLATFORM_SIZE/2 - BALL_RADIUS + EDGE_BUFFER);
  }
  
  // 更新显示
  drawBall(ballX, ballY, BG_COLOR); // 擦除旧位置
  ballX = newX;
  ballY = newY;
  drawBall(ballX, ballY, BALL_COLOR);
  
  // 更新计时
  if(millis() - gameStartTime > 1000){
    survivalTime = (millis() - gameStartTime)/1000;
    updateTimer();
  }
  
  // 坠落检测
  if(checkGameOver()){
    gameOver();
  }
}

bool checkGameOver(){
  float dx = abs(ballX - 160);
  float dy = abs(ballY - 120);
  return sqrt(dx*dx + dy*dy) > (PLATFORM_SIZE/2 - BALL_RADIUS/2);
}

void gameOver(){
  gameState = STATE_GAMEOVER;
  if(survivalTime > highScore) highScore = survivalTime;
  
  tft.fillScreen(TFT_MAROON);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);
  tft.drawCentreString("GAME OVER", 160, 80, 2);
  tft.drawCentreString("Time: "+String(survivalTime), 160, 120, 2);
  tft.drawCentreString("Best: "+String(highScore), 160, 160, 2);
  tft.drawCentreString("A:Retry C:Menu", 160, 200, 1);
}

void drawPlatform(){
  tft.fillRoundRect(160-PLATFORM_SIZE/2, 120-PLATFORM_SIZE/2, 
                   PLATFORM_SIZE, PLATFORM_SIZE, 10, PLAT_COLOR);
}

void drawBall(int x, int y, uint16_t color){
  tft.fillCircle(x, y, BALL_RADIUS, color);
}

void updateTimer(){
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("Time: ");
  tft.print(survivalTime);
  tft.print("s  Best: ");
  tft.print(highScore);
  tft.print("s ");
}

void showMainMenu(){
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);
  tft.drawCentreString("Balance Ball", 160, 80, 2);
  tft.setTextSize(1);
  tft.drawCentreString("Use accelerometer to balance", 160, 120, 2);
  tft.drawCentreString("A:Start  B:Pause  C:Menu", 160, 160, 2);
  tft.drawCentreString("High Score: "+String(highScore)+"s", 160, 200, 2);
  gameState = STATE_MENU;
}