#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Storage.h"
#include "Game.h"

#include <math.h>

#define GAME_W        320
#define GAME_H        170
#define HUD_H          16  // Score line at the top
#define COLS           10
#define ROWS            6
#define BRICK_W        30
#define BRICK_H         9
#define BRICK_GAP       2
#define BRICKS_TOP     24
#define ANTENNA_Y     158  // Top of the antenna bar
#define ANTENNA_H       4
#define WAVE_SIZE       4
#define FRAME_MS       33  // About 30 frames per second
#define STEP_MS         4  // Physics step
#define LIVES           3
#define PAD_STEP        9  // Antenna travel per encoder step (pixels)

enum GameState { GAME_READY, GAME_PLAY, GAME_PAUSE, GAME_CLEARED, GAME_OVER };

// Ionosphere layers, top to bottom: hits needed, points, color
static const struct { const char *name; uint8_t hits; uint8_t points; uint16_t color; } layers[ROWS] =
{
  { "F2", 3, 50, 0xF8C3 },
  { "F2", 3, 50, 0xF8C3 },
  { "F1", 2, 30, 0xFC40 },
  { "E",  1, 20, 0xFEA0 },
  { "E",  1, 20, 0xFEA0 },
  { "D",  1, 10, 0x2F85 },
};

static GameState state = GAME_READY;
static uint8_t bricks[ROWS][COLS];   // Hits left
static int bricksLeft = 0;
static uint8_t level = 1;
static uint8_t lives = LIVES;
static uint32_t score = 0;
static uint32_t best = 0;
static bool newBest = false;
static float padX, waveX, waveY, waveVX, waveVY;
static uint32_t lastStep = 0, lastFrame = 0, stateTime = 0;

static int padWidth()  { return(max(30, 48 - 3 * (level - 1))); }
static float speed()   { return(min(300.0f, 150.0f + 18.0f * (level - 1))); }

static void loadBest()
{
  prefs.begin("game", true, STORAGE_PARTITION);
  best = prefs.getUInt("Best", 0);
  prefs.end();
}

static void saveBest()
{
  prefs.begin("game", false, STORAGE_PARTITION);
  prefs.putUInt("Best", best);
  prefs.end();
}

static void buildLevel()
{
  bricksLeft = 0;
  for(int r=0 ; r<ROWS ; r++)
    for(int c=0 ; c<COLS ; c++)
    {
      // Each level opens the layers in its own pattern
      bool on;
      switch(level % 4)
      {
        case 1:  on = true; break;
        case 2:  on = (r + c) % 2 == 0 || r == ROWS - 1; break;
        case 3:  on = abs(c * 2 - (COLS - 1)) <= (ROWS - r) * 2 + 1; break;
        default: on = c % 3 != 1 || r < 2; break;
      }
      bricks[r][c] = on? layers[r].hits : 0;
      if(on) bricksLeft++;
    }
}

static void readyWave()
{
  state = GAME_READY;
  waveX = padX - WAVE_SIZE / 2;
  waveY = ANTENNA_Y - WAVE_SIZE - 1;
  waveVX = waveVY = 0;
}

void gameStart()
{
  loadBest();
  level = 1;
  lives = LIVES;
  score = 0;
  newBest = false;
  padX = GAME_W / 2;
  buildLevel();
  readyWave();
  lastStep = lastFrame = millis();
  currentCmd = CMD_GAME;
}

void gameStop()
{
  if(newBest) saveBest();
  newBest = false;
  currentCmd = CMD_NONE;
}

void gameRotate(int16_t enc)
{
  if(state == GAME_PAUSE || state == GAME_OVER) return;
  int half = padWidth() / 2;
  padX = constrain(padX + enc * PAD_STEP, (float)half, (float)(GAME_W - half));
  if(state == GAME_READY) waveX = padX - WAVE_SIZE / 2;
}

static void launch()
{
  // Off at a slight angle, to the side the antenna last moved to
  float angle = (padX < GAME_W / 2? -1 : 1) * 0.35f;
  waveVX = speed() * sinf(angle);
  waveVY = -speed() * cosf(angle);
  state = GAME_PLAY;
}

void gameClick()
{
  switch(state)
  {
    case GAME_READY:   launch(); break;
    case GAME_PLAY:    state = GAME_PAUSE; break;
    case GAME_PAUSE:   state = GAME_PLAY; lastStep = millis(); break;
    case GAME_OVER:    gameStart(); break;
    case GAME_CLEARED: break;
  }
}

static void bounceOffAntenna()
{
  // Where it lands on the antenna sets the angle, up to 60 degrees
  float half = padWidth() / 2.0f;
  float hit = constrain((waveX + WAVE_SIZE / 2 - padX) / half, -1.0f, 1.0f);
  float angle = hit * 1.05f;
  waveVX = speed() * sinf(angle);
  waveVY = -speed() * cosf(angle);
  waveY = ANTENNA_Y - WAVE_SIZE;
}

static bool hitBrick(float oldX, float oldY)
{
  // Corners of the wave against the brick grid
  for(int corner=0 ; corner<4 ; corner++)
  {
    float px = waveX + (corner & 1? WAVE_SIZE - 1 : 0);
    float py = waveY + (corner & 2? WAVE_SIZE - 1 : 0);
    int c = (int)((px - 1) / (BRICK_W + BRICK_GAP));
    int r = (int)((py - BRICKS_TOP) / (BRICK_H + BRICK_GAP));
    if(py < BRICKS_TOP || r < 0 || r >= ROWS || c < 0 || c >= COLS || !bricks[r][c]) continue;

    float bx = 1 + c * (BRICK_W + BRICK_GAP);
    // Coming from the side or from above or below
    bool fromSide = oldX + WAVE_SIZE <= bx || oldX >= bx + BRICK_W;
    if(fromSide) { waveVX = -waveVX; waveX = oldX; }
    else         { waveVY = -waveVY; waveY = oldY; }

    if(!--bricks[r][c])
    {
      score += layers[r].points * level;
      bricksLeft--;
    }
    else score += 2;
    return(true);
  }
  return(false);
}

static void step(float dt)
{
  float oldX = waveX, oldY = waveY;
  waveX += waveVX * dt;
  waveY += waveVY * dt;

  if(waveX < 0) { waveX = 0; waveVX = fabsf(waveVX); }
  if(waveX > GAME_W - WAVE_SIZE) { waveX = GAME_W - WAVE_SIZE; waveVX = -fabsf(waveVX); }
  if(waveY < HUD_H) { waveY = HUD_H; waveVY = fabsf(waveVY); }

  hitBrick(oldX, oldY);

  // The antenna
  float half = padWidth() / 2.0f;
  if(waveVY > 0 && waveY + WAVE_SIZE >= ANTENNA_Y && oldY + WAVE_SIZE <= ANTENNA_Y + ANTENNA_H &&
     waveX + WAVE_SIZE >= padX - half && waveX <= padX + half)
    bounceOffAntenna();

  // Never let it run almost flat
  if(fabsf(waveVY) < speed() * 0.3f)
  {
    waveVY = waveVY < 0? -speed() * 0.3f : speed() * 0.3f;
    waveVX = (waveVX < 0? -1 : 1) * sqrtf(speed() * speed() - waveVY * waveVY);
  }

  if(waveY > GAME_H)
  {
    // Signal lost
    if(!--lives)
    {
      state = GAME_OVER;
      if(score > best) { best = score; newBest = true; saveBest(); }
    }
    else readyWave();
    stateTime = millis();
  }
  else if(!bricksLeft)
  {
    state = GAME_CLEARED;
    stateTime = millis();
  }
}

bool gameTick()
{
  if(currentCmd != CMD_GAME) return(false);
  uint32_t now = millis();

  if(state == GAME_PLAY)
  {
    // Fixed steps, so that the speed does not depend on the loop
    int steps = 0;
    while(now - lastStep >= STEP_MS && steps++ < 50 && state == GAME_PLAY)
    {
      step(STEP_MS / 1000.0f);
      lastStep += STEP_MS;
    }
    if(now - lastStep > 200) lastStep = now;
  }
  else lastStep = now;

  if(state == GAME_CLEARED && now - stateTime > 1500)
  {
    level++;
    lives = min(lives + 1, LIVES + 2);
    buildLevel();
    padX = GAME_W / 2;
    readyWave();
  }

  if(now - lastFrame < FRAME_MS) return(false);
  lastFrame = now;
  return(true);
}

static void drawCentered(const char *text, int y, uint16_t color)
{
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(color);
  spr.drawString(text, GAME_W / 2, y, FONT_SMALL);
}

void gameDraw()
{
  char text[40];

  // Night sky
  spr.fillSprite(0x0841);
  for(int i=0 ; i<24 ; i++)
    spr.drawPixel((i * 97 + 13) % GAME_W, HUD_H + (i * 53 + 7) % (ANTENNA_Y - HUD_H), 0x8410);

  // Score line
  spr.fillRect(0, 0, GAME_W, HUD_H - 2, 0x0000);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(0xFEA0);
  spr.drawString("SKYWAVE", 4, 0, FONT_SMALL);
  for(int i=0 ; i<lives ; i++)
  {
    // A small antenna per life left
    int x = 72 + i * 10;
    spr.drawFastVLine(x + 3, 3, 9, 0xBDF7);
    spr.drawFastHLine(x, 4, 7, 0xBDF7);
    spr.drawFastHLine(x + 1, 7, 5, 0xBDF7);
  }
  snprintf(text, sizeof(text), "Level %u", level);
  drawCentered(text, 7, 0xBDF7);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(0xFFFF);
  snprintf(text, sizeof(text), "%lu", (unsigned long)score);
  spr.drawString(text, GAME_W - 4, 0, FONT_SMALL);

  // Ionosphere
  for(int r=0 ; r<ROWS ; r++)
    for(int c=0 ; c<COLS ; c++)
    {
      if(!bricks[r][c]) continue;
      int x = 1 + c * (BRICK_W + BRICK_GAP), y = BRICKS_TOP + r * (BRICK_H + BRICK_GAP);
      uint16_t color = layers[r].color;
      // Worn layers fade
      if(bricks[r][c] < layers[r].hits) color = (color >> 1) & 0x7BEF;
      spr.fillRect(x, y, BRICK_W, BRICK_H, color);
      spr.drawFastHLine(x, y, BRICK_W, 0xFFFF & (color | 0x4208));
    }

  // Antenna, with its elements
  int half = padWidth() / 2, px = (int)padX;
  spr.fillRect(px - half, ANTENNA_Y, half * 2, ANTENNA_H, 0xBDF7);
  for(int x = px - half + 3 ; x < px + half - 2 ; x += 6)
    spr.drawFastVLine(x, ANTENNA_Y + ANTENNA_H, 4, 0x7BEF);
  spr.drawFastVLine(px, ANTENNA_Y + ANTENNA_H, GAME_H - ANTENNA_Y - ANTENNA_H, 0x7BEF);

  // The wave
  if(state != GAME_OVER)
  {
    spr.fillRect((int)waveX, (int)waveY, WAVE_SIZE, WAVE_SIZE, 0xFFFF);
    spr.drawPixel((int)(waveX - waveVX * 0.02f) + 1, (int)(waveY - waveVY * 0.02f) + 1, 0x7BEF);
  }

  switch(state)
  {
    case GAME_READY:
      drawCentered("Click to transmit", 110, 0xFFFF);
      drawCentered("Rotate to move the antenna, hold to leave", 128, 0xBDF7);
      break;
    case GAME_PAUSE:
      drawCentered("Paused", 110, 0xFFFF);
      drawCentered("Click to go on, hold to leave", 128, 0xBDF7);
      break;
    case GAME_CLEARED:
      snprintf(text, sizeof(text), "Level %u cleared!", level);
      drawCentered(text, 110, 0xFEA0);
      break;
    case GAME_OVER:
      drawCentered("Signal lost", 96, 0xF8C3);
      snprintf(text, sizeof(text), newBest? "New best: %lu" : "Score %lu, best %lu",
               (unsigned long)score, (unsigned long)best);
      drawCentered(text, 114, 0xFFFF);
      drawCentered("Click to play again, hold to leave", 132, 0xBDF7);
      break;
    default:
      break;
  }

  spr.pushSprite(0, 0);
}
