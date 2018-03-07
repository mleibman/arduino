#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_DotStarMatrix.h>
#include <Adafruit_DotStar.h>
#include <Adafruit_SSD1306.h>

#define DATAPIN    27
#define CLOCKPIN   13

Adafruit_DotStarMatrix matrix = Adafruit_DotStarMatrix(
                                  12, 6, DATAPIN, CLOCKPIN,
                                  DS_MATRIX_BOTTOM     + DS_MATRIX_LEFT +
                                  DS_MATRIX_ROWS + DS_MATRIX_PROGRESSIVE,
                                  DOTSTAR_BGR);

#define OLED_RESET A4
Adafruit_SSD1306 oled(-1);

const uint16_t red = matrix.Color(255, 0, 0);
const uint16_t green = matrix.Color(0, 255, 0);
const uint16_t orange = matrix.Color(255, 125, 0);
const uint16_t MAX_INPUT = 4095;
const uint8_t sampleWindowMs = 20;

const uint8_t levelsCount = 16; // equal to the led matrix width
uint16_t levels[levelsCount];
uint8_t levelsIdx = 0;

uint16_t range_min = 0;
uint16_t range_max = 0;

void setup()
{
  Serial.begin(9600);

  matrix.begin();
  matrix.setBrightness(10);

  oled.begin();
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE, BLACK);
  oled.setCursor(0, 0);
  oled.println("Ready!");
  oled.println("Adjust sensitivity:");
  oled.fillRect(0, 48, 128, 8, WHITE);
  oled.display();

  // NOTE: This needs to be set after oled.begin(), otherwise it
  // will overwrite it with the default 100KHz speed!
  Wire.setClock(1000000L);
  
  for (int i = 0; i < levelsCount; i++) {
    levels[i] = 0;
  }
}


bool updateRange() {
  const uint16_t flux = 50;
  uint16_t min = analogRead(A1);
  uint16_t max = analogRead(A2);
  min = map(min, 0, MAX_INPUT, 0, MAX_INPUT / 2);
  max = map(max, 0, MAX_INPUT, MAX_INPUT / 2, MAX_INPUT);

  bool min_changed = abs(min - range_min) > flux;
  bool max_changed = abs(max - range_max) > flux;
  if (min_changed || max_changed) {
    if (min_changed) range_min = min;
    if (max_changed) range_max = max;
    drawSensitivityBar();
    return true;
  }
  return false;
}

void drawSensitivityBar() {
  unsigned long startMs = millis();

  oled.setCursor(0, 32);
  oled.fillRect(0, 32, 128, 8, BLACK);  // clear
  oled.print(String(range_min) + " - " + String(range_max));
  oled.fillRect(0, 48, 128, 8, WHITE);

  uint8_t from = map(range_min, 0, MAX_INPUT, 0, 126 / 2);
  oled.fillRect(
    1 + from,
    49,
    126 / 2 - from,
    6, BLACK);

  uint8_t to = map(range_max, MAX_INPUT / 2, MAX_INPUT, 126 / 2, 0);
  oled.fillRect(
    126 / 2,
    49,
    126 / 2 - to,
    6, BLACK);

  oled.display();

  unsigned long endMs = millis();
  Serial.println(endMs - startMs);
}




void loop()
{
  if (!updateRange()) {
    // Take a new sample only if the range wasn't updated.
    // Updating the OLED is very slow (~100ms), so if we end up doing it,
    // use the previous sampled value instead of running another one
    // taking up an extra 20ms.
    uint16_t peak = sampleAudio();

    levels[levelsIdx] = map(peak, range_min, range_max, 0, 6);
  } else {
    // Reuse the previous value.
    levels[levelsIdx] = levels[(levelsIdx + levelsCount - 1) % levelsCount];
  }

  // Shift the current index.
  levelsIdx = (levelsIdx + 1) % levelsCount;

  drawSoundLevelGraph();
}

uint16_t sampleAudio() {
  unsigned long startMillis = millis(); // Start of sample window
  uint16_t signalMax = 0;
  uint16_t signalMin = 0;
  uint16_t sample = 0;

  while (millis() - startMillis < sampleWindowMs)
  {
    sample = analogRead(A0);
    if (sample > signalMax)
    {
      signalMax = sample;  // save just the max levels
    }
    else if (sample < signalMin)
    {
      signalMin = sample;  // save just the min levels
    }
  }

  return (signalMax - signalMin);
}

void drawSoundLevelGraph() {
  matrix.startWrite();
  matrix.fillScreen(0);

  for (int i = 0; i < levelsCount; i++) {
    uint8_t idx = (levelsIdx + i) % levelsCount;

    for (int j = 0; j < levels[idx]; j++) {
      uint16_t color = red;
      if (j < 3) {
        color = green;
      } else if (j < 5) {
        color = orange;
      }
      matrix.drawPixel(i, 6 - j, color);
    }
  }

  matrix.endWrite();
  matrix.show();
}
