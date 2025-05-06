#include <TFT_eSPI.h>
TFT_eSPI tft;

// 游戏配置
#define GRID_SIZE 16       // 网格单位（像素）
#define GRID_WIDTH 20      // 横向格子数（320/16）
#define GRID_HEIGHT 15     // 纵向格子数（240/16）
#define INIT_LENGTH 3      // 初始蛇长
#define NORMAL_SPEED 150   // 正常移动间隔（ms）
#define BOOST_SPEED 75     // 加速移动间隔（ms）

// 游戏状态枚举
enum GameState {
  MENU,
  PLAYING,
  PAUSED,
  GAME_OVER
};

// 蛇结构体
struct Snake {
  int x[GRID_WIDTH * GRID_HEIGHT]; // 身体X坐标
  int y[GRID_WIDTH * GRID_HEIGHT]; // 身体Y坐标
  uint8_t dir;    // 当前方向：0-上 1-下 2-左 3-右
  uint8_t length; // 当前长度
};

// 全局变量
Snake snake;
GameState gameState = MENU;
int foodX, foodY;
uint16_t score = 0;
unsigned long lastUpdate = 0;
bool speedBoost = false;

void setup() {
  Serial.begin(115200);
  
  // 初始化屏幕
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  
  // 初始化输入引脚
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  
  showMainMenu();
}

void loop() {
  handleSystemInput(); // 处理系统按键
  
  switch(gameState) {
    case MENU:
      break;
      
    case PLAYING:
      handleGameplay();
      break;
      
    case PAUSED:
      drawPauseScreen();
      break;
      
    case GAME_OVER:
      handleGameOver();
      break;
  }
}

/*********************
 * 核心游戏逻辑函数 *
 *********************/

// 初始化新游戏
void startNewGame() {
  // 初始化蛇
  snake.dir = 3; // 初始向右
  snake.length = INIT_LENGTH;
  for(int i=0; i<snake.length; i++) {
    snake.x[i] = 4 - i;
    snake.y[i] = 7;
  }
  
  generateFood();
  score = 0;
  gameState = PLAYING;
  tft.fillScreen(TFT_BLACK);
}

// 生成新食物
void generateFood() {
  bool valid;
  do {
    valid = true;
    foodX = random(GRID_WIDTH);
    foodY = random(GRID_HEIGHT);
    
    // 检查食物不与蛇身重叠
    for(int i=0; i<snake.length; i++) {
      if(foodX == snake.x[i] && foodY == snake.y[i]) {
        valid = false;
        break;
      }
    }
  } while(!valid);
}

// 更新游戏状态
void updateGame() {
  // 移动身体
  for(int i=snake.length-1; i>0; i--) {
    snake.x[i] = snake.x[i-1];
    snake.y[i] = snake.y[i-1];
  }
  
  // 移动头部
  switch(snake.dir) {
    case 0: snake.y[0]--; break; // 上
    case 1: snake.y[0]++; break; // 下
    case 2: snake.x[0]--; break; // 左
    case 3: snake.x[0]++; break; // 右
  }
  
  // 处理边界穿越
  snake.x[0] = (snake.x[0] + GRID_WIDTH) % GRID_WIDTH;
  snake.y[0] = (snake.y[0] + GRID_HEIGHT) % GRID_HEIGHT;
  
  checkCollisions();
  checkFoodConsumption();
}

/*********************
 * 碰撞检测函数 *
 *********************/
void checkCollisions() {
  // 自碰检测
  for(int i=1; i<snake.length; i++) {
    if(snake.x[0] == snake.x[i] && snake.y[0] == snake.y[i]) {
      gameOver();
      return;
    }
  }
}

// 食物检测
void checkFoodConsumption() {
  if(snake.x[0] == foodX && snake.y[0] == foodY) {
    snake.length++;
    score += 10;
    generateFood();
  }
}

/*********************
 * 输入处理函数 *
 *********************/
void handleSystemInput() {
  // 处理加速键（实时检测）
  speedBoost = (digitalRead(WIO_5S_PRESS) == LOW);
  
  // 处理功能按键（带防抖）
  if(debouncedPress(WIO_KEY_A)) handleMenuButton();
  if(debouncedPress(WIO_KEY_B)) handleStartButton();
  if(debouncedPress(WIO_KEY_C)) handlePauseButton();
  
  // 处理方向输入
  if(gameState == PLAYING) {
    handleDirectionInput();
  }
}

// 方向输入处理
void handleDirectionInput() {
  if(digitalRead(WIO_5S_UP) == LOW && snake.dir != 1) {
    snake.dir = 0;
  } else if(digitalRead(WIO_5S_DOWN) == LOW && snake.dir != 0) {
    snake.dir = 1;
  } else if(digitalRead(WIO_5S_LEFT) == LOW && snake.dir != 3) {
    snake.dir = 2;
  } else if(digitalRead(WIO_5S_RIGHT) == LOW && snake.dir != 2) {
    snake.dir = 3;
  }
}

/*********************
 * 按钮处理函数 *
 *********************/
bool debouncedPress(uint8_t pin) {
  static uint32_t lastPressTime = 0;
  if(digitalRead(pin) == LOW && millis() - lastPressTime > 200) {
    lastPressTime = millis();
    while(digitalRead(pin) == LOW); // 等待释放
    return true;
  }
  return false;
}

void handleMenuButton() {
  if(gameState != MENU) {
    gameState = MENU;
    showMainMenu();
  }
}

void handleStartButton() {
  if(gameState == MENU || gameState == GAME_OVER) {
    startNewGame();
  }
}

void handlePauseButton() {
  if(gameState == PLAYING) {
    gameState = PAUSED;
  } else if(gameState == PAUSED) {
    gameState = PLAYING;
    tft.fillScreen(TFT_BLACK);
  }
}

/*********************
 * 图形渲染函数 *
 *********************/
void drawGame() {
  tft.fillScreen(TFT_BLACK);
  
  // 绘制食物
  tft.fillRect(foodX*GRID_SIZE, foodY*GRID_SIZE, 
              GRID_SIZE-1, GRID_SIZE-1, TFT_RED);
  
  // 绘制蛇身
  for(int i=0; i<snake.length; i++) {
    uint16_t color = (i == 0) ? TFT_GREEN : TFT_DARKGREEN;
    tft.fillRect(snake.x[i]*GRID_SIZE, snake.y[i]*GRID_SIZE,
                GRID_SIZE-1, GRID_SIZE-1, color);
  }
  
  // 绘制分数
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 10);
  tft.printf("Score: %d", score);
}

/*********************
 * 游戏状态函数 *
 *********************/
void gameOver() {
  gameState = GAME_OVER;
  tft.fillScreen(TFT_MAROON);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("GAME OVER", 160, 80, 2);
  tft.drawCentreString("Score: "+String(score), 160, 120, 2);
  tft.drawCentreString("B:Restart  A:Menu", 160, 160, 1);
}

void drawPauseScreen() {
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("PAUSED", 160, 100, 2);
}

void showMainMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.drawCentreString("SNAKE GAME", 160, 80, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Control:", 160, 120, 1);
  tft.drawCentreString("5-Way: Move", 160, 140, 1);
  tft.drawCentreString("Press: Speed Up", 160, 160, 1);
  tft.drawCentreString("A:Menu  B:Start  C:Pause", 160, 180, 1);
}

void handleGameOver() {
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawCentreString("GAME OVER", 160, 80, 2);
  tft.drawCentreString("Score: "+String(score), 160, 120, 2);
  tft.drawCentreString("Press to restart", 160, 160, 1);
  
  while (digitalRead(WIO_5S_PRESS) == HIGH); // 等待按下
  startNewGame();
}

/*********************
 * 游戏主循环函数 *
 *********************/
void handleGameplay() {
  if(millis() - lastUpdate > (speedBoost ? BOOST_SPEED : NORMAL_SPEED)) {
    updateGame();
    drawGame();
    lastUpdate = millis();
  }
}