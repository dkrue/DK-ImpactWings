//#include <Arduino.h>
#include <FastLED.h>
#include <Conceptinetics.h> // DMX

FASTLED_USING_NAMESPACE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_SQUARES 10
#define LED_SQUARE_SIZE 16
#define NUM_LEDS    NUM_SQUARES*LED_SQUARE_SIZE
CRGB leds[NUM_LEDS];

// Define LED regions by module
CRGBSet ledsraw(leds, NUM_LEDS);
struct CRGB * ledsquares[NUM_SQUARES];

const int8_t PROGMEM bitmap_shape[3][12] = {
  {0,1,2,3,4,11,12,13,14,15,8,7}, // outer square, perimeter order
  {5,6,9,10,-1,-1,-1,-1,-1,-1,-1,-1,}, // inner square
  {0,3,5,6,9,10,12,15,-1,-1,-1,-1} // full X
};
const uint8_t PROGMEM bitmap_tricorner[4][6] = {
  {0,1,7,2,6,8},
  {3,2,4,1,5,11},
  {15,14,8,7,9,13},
  {12,13,11,14,10,4}
};
const uint8_t PROGMEM bitmap_dice[6][6] = {
  {1,5,11,0,0,0},//reverse 3
  {4,6,0,0,0,0},//2
  {3,5,9,0,0,0},//3
  {1,3,9,11,0,0},//4
  {1,3,9,11,5,0},//5
  {1,2,3,9,10,11}//6  
};
const uint8_t PROGMEM bitmap_linear[4][4] = {
  {3,4,11,12},
  {2,5,10,13},
  {1,6,9,14},
  {0,7,8,15}
};

#define DMX_STROBE_BRIGHTNESS 1
#define DMX_STROBE_SPEED 2
#define DMX_BURN_BRIGHTNESS 3
#define DMX_AMBIENT_BRIGHTNESS 4
#define DMX_FADE_TIME 5
#define DMX_MASTER_SPEED 6
#define DMX_HUE_SCROLL_SPEED 7
#define DMX_SAT_SCROLL_SPEED 8

#define DMX_SLAVE_CHANNELS 16
#define RXEN_PIN 0
DMX_Slave dmx_slave ( DMX_SLAVE_CHANNELS , RXEN_PIN );
const int ledPin = 13;

void setup() {
  delay(2000); // 2 second delay for recovery

  randomSeed(analogRead(A3));
  random16_add_entropy(random16());
  
  FastLED.addLeds<LED_TYPE,14,COLOR_ORDER>(leds, 0, NUM_LEDS/2).setCorrection(Tungsten40W); //or "Candle"
  FastLED.addLeds<LED_TYPE,15,COLOR_ORDER>(leds, NUM_LEDS/2, NUM_LEDS).setCorrection(Tungsten40W);

  // Populate LED regions dynamically
  for(uint8_t i=0; i < NUM_SQUARES; i++) {
    ledsquares[i] = ledsraw(i*LED_SQUARE_SIZE, ((i+1)*LED_SQUARE_SIZE)-1);
  }

  // Attach callback to serial interrupt
  dmx_slave.onReceiveComplete ( OnDMXFrameReceiveComplete );
  
  // Set led pin as output pin
  pinMode(ledPin, OUTPUT);  

  // Read DMX device address from 9bit DIP switch (starting at digital pin 2)
  uint16_t dmxAddress = 0;
  for(uint8_t i=0; i<9; i++) {
    pinMode(i+2, INPUT_PULLUP);
    dmxAddress |= (digitalRead(i+2) == LOW) << i;
  }
  
  dmxAddress = dmxAddress > 0 ? dmxAddress : 1;
  dmx_slave.setStartAddress(dmxAddress);
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gStrobePatterns = { allStrobe, allStrobePattern, chaseStrobe, 
  roundRobinStrobe, randomRobinStrobe};
SimplePatternList gAmbientPatterns = { rainFall, tricornerFlip, edgeChase, sinelonCenter, sinelonDual, darkNeighbor, 
  waterFall, symmetricShapes, diceRoll, linesAndSquares};

uint8_t gCurrentStrobePattern = 4; // Index number of which pattern is current
uint8_t gHue = random8(255), gHue2 = random8(255), gSat = 0, gSat2 = 0, currentHue = 0, currentSat = 0;
uint8_t gCurrentAmbientPattern = 0, gCurrentFadeScene = 2;

uint32_t currentMillis = 0, prevMillis = 0;
byte refreshRate = 80; // 80fps: strobe up past visible refresh rate
uint32_t strobeActivationTime, prevStrobeTime, prevFadeTime, prevSceneTime, prevHueScrollTime;
uint8_t strobeIndex, strobeState, strobeCount, keyframe, pixelframe;
bool strobeActive, dmxEnabled;
uint8_t dmxBrightness = 95, dmxSpeed = 50, dmxFadeTime = 70, dmxStrobeBrightness = 65, dmxStrobeSpeed, dmxBurnBrightness, dmxBurnBrightnessCurrentVal, dmxHueScrollSpeed = 30;
  
void loop()
{
  currentMillis = millis();

  // Update neopixels at a constant frame rate (FPS = refreshRate)
  if(currentMillis - prevMillis > 1000 / refreshRate) {
    prevMillis = currentMillis;

    dmx_slave.disable(); // Turn off dmx serial interrupts while drawing animation frame
    dmxEnabled = false;

    if(random8(2) == 0) {
      currentHue = gHue;
      currentSat = gSat;
    } else {
      currentHue = gHue2;
      currentSat = gSat2;
    }
    
    // Main pixel drawing:
    if(strobeActive) {
      fadeToBlackBy(leds, NUM_LEDS, 25); // (some strobe patterns have soft fade when active)
      gStrobePatterns[gCurrentStrobePattern]();
      dmxBurnBrightnessCurrentVal = dmxStrobeBrightness < dmxBurnBrightnessCurrentVal ? dmxStrobeBrightness : dmxBurnBrightnessCurrentVal;
    } else {
      if(dmxBurnBrightnessCurrentVal > 1 && dmxBurnBrightnessCurrentVal > dmxBurnBrightness)
        dmxBurnBrightnessCurrentVal-=2;
      if(dmxBurnBrightnessCurrentVal < 250 && dmxBurnBrightnessCurrentVal < dmxBurnBrightness && dmxBurnBrightness > 20)
        dmxBurnBrightnessCurrentVal+=4;        
      if(dmxBurnBrightnessCurrentVal > 20) { 
        fill_solid( leds, NUM_LEDS, CHSV(30, 170, dmxBurnBrightnessCurrentVal)); // HSV value of warm white DK
      } else {
        gAmbientPatterns[gCurrentAmbientPattern]();
      }
    }

    FastLED.show();  

    if(currentMillis - prevFadeTime > dmxFadeTime) {
      prevFadeTime = currentMillis;
      globalFadeToBlack();
    }

    if(currentMillis - prevHueScrollTime > 255-dmxHueScrollSpeed && dmxHueScrollSpeed > 10) {
      prevHueScrollTime = currentMillis;
      gHue++;
      if(random8(2) == 0) gHue2++;
      if(dmxHueScrollSpeed < 30) gHue2 = gHue;
    }    

    EVERY_N_MILLISECONDS( 300 ) { gSat > 200 ? gSat++ : gSat+=3; } 
    EVERY_N_MILLISECONDS( 500 ) { gSat2 > 100 ? gSat2++ : gSat2+=2; }  
    EVERY_N_MILLISECONDS( 500 ) { nextStrobePattern(); } 
    EVERY_N_SECONDS( 25 ) { gCurrentFadeScene++; } 

    if(currentMillis - prevSceneTime > (uint32_t)(260u-dmxSpeed)*500u) { 
      prevSceneTime = currentMillis;
      nextAmbientPattern();
    }    
    //EVERY_N_SECONDS( 42 ) { nextAmbientPattern(); } 

    if(dmxStrobeBrightness > 0 && dmxStrobeSpeed > 0) {
      strobeActive = true;
      strobeActivationTime = currentMillis;
    }

    // Turn off strobe after loss of touch / inactivity
    if(strobeActive == true) {
      if(currentMillis - strobeActivationTime > 100) {
        strobeActive = false;
        strobeState = 0;
        strobeCount = 0;
        fill_solid(leds, NUM_LEDS, CRGB::Black);
      }
    }
  } 
  else 
  {
    // In between animation frames sip on incoming dmx data
    if(!dmxEnabled) {
      dmx_slave.enable();  
      dmxEnabled = true;  
    }
  }
}

void globalFadeToBlack() {
  switch(gCurrentFadeScene % 2) {
    case 0:
      fadeToBlackBy(leds + random8(NUM_SQUARES)*LED_SQUARE_SIZE, LED_SQUARE_SIZE, 10); // square fade
      break;
    case 1:
      //fadeToBlackBy(leds + random8(NUM_LEDS/4)*4, 4, 30); // line fade
      fadeToBlackBy(leds + random8(2)*(NUM_LEDS/2), NUM_LEDS/2, 2); // left/right strip fade
      break;
  }
}

/***** AMBIENT SCENES *****/
// Idea: Plain color squares to color the room! -Easy.
// Idea: AmbientRevealMode = Quick cascade several turn on in a row then pause
// Idea: Scene: color poses - entire square fading color-white-color
void rainFall() {
  keyframe++;

  if(keyframe > (255-dmxSpeed)>>5) // 30fps
  {
    uint8_t i = strobeIndex%NUM_SQUARES; // clear old raindrop
    uint8_t pixel = pgm_read_dword(&bitmap_linear[pixelframe%4][0]); //outer line
    leds[(i*LED_SQUARE_SIZE) + pixel] = CRGB::Black;
    pixel = pgm_read_dword(&bitmap_linear[pixelframe%4][3]); //outer line
    leds[(i*LED_SQUARE_SIZE) + pixel] = CRGB::Black; 

    keyframe = 0;
    pixelframe++; 

    if(pixelframe%4 == 0) strobeIndex--;
    if(random8(50)==0) strobeIndex+=5; //random square skip

    i = strobeIndex%NUM_SQUARES;
    pixel = pgm_read_dword(&bitmap_linear[pixelframe%4][0]); //outer line
    leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(0, 0, dmxStrobeBrightness/(random8(2)+1));
    pixel = pgm_read_dword(&bitmap_linear[pixelframe%4][3]); //outer line
    leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(0, 0, dmxStrobeBrightness/(random8(2)+1));

    uint8_t hue = random(30);
    pixel = pgm_read_dword(&bitmap_linear[pixelframe%4][1]); //inner line
    leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(gHue + hue, 255, dmxBrightness/2);   
    pixel = pgm_read_dword(&bitmap_linear[pixelframe%4][2]); //inner line
    leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(gHue + hue, 255, dmxBrightness/2);            
  }   
}

void edgeChase() {
  //if(random8(200) <= dmxSpeed/2)
  keyframe++;
  //if(keyframe/NUM_SQUARES > NUM_SQUARES) keyframe = 0;

  if(keyframe > (255-dmxSpeed)>>5) // 30fps
  {
    keyframe = 0;
    pixelframe++; 

    for(uint8_t i=0; i < NUM_SQUARES; i++) {
      int8_t pixel = pgm_read_dword(&bitmap_shape[0][pixelframe%12]); //outer square
      leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(0, 0, dmxStrobeBrightness);
      pixel = pgm_read_dword(&bitmap_shape[0][(pixelframe-1)%12]); //outer square
      leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(0, 0, 50); 

      pixel = pgm_read_dword(&bitmap_shape[1][(pixelframe)%4]); //inner square
      leds[(i*LED_SQUARE_SIZE) + pixel] += CHSV(gHue, 255, dmxBrightness/3);       
    }
  }   
}

void symmetricShapes() {
  if(random16(1200) <= dmxSpeed) { 
    uint8_t pos = random8(NUM_SQUARES);
    uint8_t mirror = (pos >= NUM_SQUARES/2 ? -1 : 1) * (NUM_SQUARES/2)*LED_SQUARE_SIZE;
    uint8_t roll = random8(3);
    uint8_t hue = random(50);

    for(uint8_t i=0; i<12; i++) {
      int8_t pixel = pgm_read_dword(&bitmap_shape[roll][i]);
      if(pixel > -1) {
        leds[(pos*LED_SQUARE_SIZE) + pixel] = CHSV(currentHue + hue + random8(10), random8(55)+200, random8(dmxBrightness/2)+100);
        leds[(pos*LED_SQUARE_SIZE) + pixel + mirror] = CHSV(currentHue + hue + random8(10), random8(55)+200, random8(dmxBrightness/2)+100);
      }
    }
  }
}

void tricornerFlip() {
  keyframe++;

  if(keyframe > (255-dmxSpeed))
  {
    /*int8_t pixel = pgm_read_dword(&bitmap_tricorner[pixelframe%4][0]); // dim out old tri point
    for(uint8_t i=0; i<NUM_SQUARES; i++) {
      leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(0, 0, 30);
    }*/

    keyframe = 0;
    pixelframe += random8(2)+1; //once per few seconds

    /*int8_t pixel = pgm_read_dword(&bitmap_tricorner[pixelframe%4][0]); // highlight new tri point direction
    for(uint8_t i=0; i<NUM_SQUARES; i++) {
      leds[(i*LED_SQUARE_SIZE) + pixel] = CHSV(0, 0, dmxStrobeBrightness);
    }*/
  }

  if(random16(1200) <= dmxSpeed) { // display colored triangles
    uint8_t pos = random8(NUM_SQUARES);
    uint8_t hue = random(20);

    for(uint8_t i=0; i<6; i++) {
      int8_t pixel = pgm_read_dword(&bitmap_tricorner[pixelframe%4][i]);
      leds[(pos*LED_SQUARE_SIZE) + pixel] = CHSV(currentHue + hue, 255, dmxBrightness);
    }
  }
}

void diceRoll() {
  if(random16(1200) <= dmxSpeed) { 
    uint8_t pos = random8(NUM_SQUARES);
    uint8_t mirror = (pos >= NUM_SQUARES/2 ? -1 : 1) * (NUM_SQUARES/2)*LED_SQUARE_SIZE;
    uint8_t sub = random8(2)*4;
    uint8_t roll = random8(6);

    for(uint8_t i=0; i<6; i++) {
      uint8_t pixel = pgm_read_dword(&bitmap_dice[roll][i]);
      if(pixel > 0) {
        leds[(pos*LED_SQUARE_SIZE)+sub + pixel] = CHSV(currentHue, 255, dmxBrightness);
        leds[(pos*LED_SQUARE_SIZE)+sub + pixel + mirror] = CHSV(currentHue, 255, dmxBrightness);
      }
    }
  }
}

void linesAndSquares() {
    if(random16(1200) <= dmxSpeed) {
      if(random8(15) == 0) {
        //Entire square
        fill_solid(ledsquares[keyframe % ARRAY_SIZE(ledsquares)], LED_SQUARE_SIZE, CHSV(currentHue, currentSat, random8(dmxBrightness)));//+rnd(90)40));
      } else {
        // One line of square
        uint8_t pos = random8(NUM_LEDS/4);
        uint8_t val = random(dmxBrightness)+30;
        leds[pos*4  ] = CHSV( currentHue, currentSat, val);
        leds[pos*4+1] = CHSV( currentHue, currentSat, val);
        leds[pos*4+2] = CHSV( currentHue, currentSat, val);
        leds[pos*4+3] = CHSV( currentHue, currentSat, val);
      }
      keyframe++;
    }
}

void waterFall() {
  if(random8(200) <= dmxSpeed) {  
    uint8_t pos = random8(keyframe % (NUM_LEDS/2))+1;
    uint8_t val = random(dmxBrightness/4) + dmxBrightness/3;
    leds[NUM_LEDS - pos] = CHSV(currentHue, currentSat, val);
    leds[NUM_LEDS/2 - pos] = CHSV(currentHue, currentSat, val);
    keyframe++;
  }
}

void darkNeighbor() {
  if(random8(200) <= dmxSpeed) {
    uint8_t pos = random8(NUM_LEDS/2)+1;
    uint8_t val = random(dmxBrightness/3) + dmxBrightness/3;

    leds[NUM_LEDS - pos] = CHSV(currentHue, 160 - random(val/4), val); //brightpoint
    leds[NUM_LEDS - pos+1] -= 90-dmxFadeTime;//= CRGB::Black; //darkneighbor
    leds[NUM_LEDS - pos-1] -= 90-dmxFadeTime;// //darkneighbor

    leds[NUM_LEDS/2 - pos] = CHSV(currentHue, 160 - random(val/4), val); //brightpoint
    leds[NUM_LEDS/2 - pos+1] -= 90-dmxFadeTime; //darkneighbor
    leds[NUM_LEDS/2 - pos-1] -= 90-dmxFadeTime; //darkneighbor
  }
}

void sinelonDual()
{
  int pos = beatsin16( (dmxSpeed/10)+1, 0, (NUM_LEDS/2)-1 );
  leds[pos] += CHSV( currentHue, 255, dmxBrightness/3);
  leds[pos + (NUM_LEDS/2)] += CHSV( currentHue, 255, dmxBrightness/3);
}

void sinelonCenter()
{
  int pos = beatsin16( (dmxSpeed/12)+1, 0, (LED_SQUARE_SIZE*3)-1 );
  leds[pos] += CHSV( currentHue, currentSat, dmxBrightness/5);
  leds[pos + (NUM_LEDS/2)] += CHSV( currentHue, currentSat, dmxBrightness/5);
  leds[NUM_LEDS - pos] += CHSV( currentHue, currentSat, dmxBrightness/5);
  leds[(NUM_LEDS/2) - pos] += CHSV( currentHue, currentSat, dmxBrightness/5);  
}

/***** STROBE SCENES *****/
// Simultaneous strobe across all modules 
void allStrobe() {
  fill_solid( leds, NUM_LEDS, CHSV(gHue, 0, strobeState));
  flipStrobeState();
}

// Strobe 3 times then blackout 3 times
void allStrobePattern() {
  fill_solid( leds, NUM_LEDS, CHSV(gHue, 0, strobeState * (strobeIndex%2)));
  if(flipStrobeState()) {
    if(strobeCount++ > 3) {
      strobeIndex++;
      strobeCount = 0;
    }
  }
}

// Strobe once and move to next module, leaving current module to fade out
void chaseStrobe() {
  fill_solid(ledsquares[strobeIndex % ARRAY_SIZE(ledsquares)], LED_SQUARE_SIZE, CHSV(gHue, 0, strobeState));
  if(flipStrobeState()) {
    strobeIndex++;
  }
}

// Blink each strobe a few times before advancing
void roundRobinStrobe() {
  fill_solid(ledsquares[strobeIndex % ARRAY_SIZE(ledsquares)], LED_SQUARE_SIZE, CHSV(gHue, 0, strobeState));
  if(flipStrobeState()) {
    if(strobeCount++ > 4) {
      strobeIndex++; // Move to next module, leaving current module to fade out
      strobeCount = 0;
    }
  }
}

// Strobe once and move to random module
void randomRobinStrobe() {
  uint8_t oldStrobeIndex = strobeIndex;
  if(flipStrobeState()) {
    strobeIndex+=random8(ARRAY_SIZE(ledsquares)); 
  }
  fill_solid(ledsquares[oldStrobeIndex % ARRAY_SIZE(ledsquares)], LED_SQUARE_SIZE, CHSV(gHue, 0, strobeState));
}



//--------- Utility functions ----------------

// Returns true when strobe has been flipped off
bool flipStrobeState() {
  if(currentMillis - prevStrobeTime > dmxStrobeSpeed) {
    strobeState = strobeState == 0 ? dmxStrobeBrightness : 0; 
    prevStrobeTime = currentMillis;
    return strobeState == 0; 
  }  
  return false;
}

void nextAmbientPattern()
{
  gCurrentAmbientPattern = random8(ARRAY_SIZE(gAmbientPatterns));
  dmxBurnBrightnessCurrentVal = dmxBrightness; // burn flash on scene change
}

void nextStrobePattern()
{
  if(strobeActive == false) {
    if(currentMillis - strobeActivationTime > 400) {
      gCurrentStrobePattern = random8(ARRAY_SIZE(gStrobePatterns));
    }
  }
}

void OnDMXFrameReceiveComplete(unsigned short nrChannels) {
  if (nrChannels == DMX_SLAVE_CHANNELS)
  {
    uint8_t val = dmx_slave.getChannelValue(DMX_STROBE_BRIGHTNESS);

    // Onboard led debug - show channel 1 active
    if (val > 0 ) 
      digitalWrite ( ledPin, HIGH );
    else
      digitalWrite ( ledPin, LOW );

  
    dmxStrobeBrightness = val > 20 ? (val < 160 ? val+20 : 180) : 0;

    val = dmx_slave.getChannelValue(DMX_STROBE_SPEED);
    dmxStrobeSpeed =  val > 20 ? map(val, 20, 255, 90, 10) : 0; //10-90ms strobe

    dmxBurnBrightness = dmx_slave.getChannelValue(DMX_BURN_BRIGHTNESS);
    dmxBrightness = dmx_slave.getChannelValue(DMX_AMBIENT_BRIGHTNESS);
    
    val = dmx_slave.getChannelValue(DMX_FADE_TIME);
    dmxFadeTime = val > 3 ? val / 5 : 3;// 3ms minimum

    dmxSpeed = dmx_slave.getChannelValue(DMX_MASTER_SPEED);
    dmxHueScrollSpeed = dmx_slave.getChannelValue(DMX_HUE_SCROLL_SPEED);

  }
}