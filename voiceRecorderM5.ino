//#include <TFT_eSPI.h>
#include <M5Stack.h>
#include <atomic>

#define MICROPHONE             34
#define SPEAKER                25
#define BACKLIGHT              32

#define BUFFER_SIZE            ESP.getPsramSize() / 3
#define SAMPLING_FREQUENCY     22050

#define LOWNOISE               true /* set to false to enable backlight dimming */

TFT_eSPI tft = TFT_eSPI();

// set log level to debug for output
void logMemory() {
  log_d("Used PSRAM: %d from: %d", ESP.getPsramSize() - ESP.getFreePsram(), ESP.getPsramSize() );
}

// https://baptiste-wicht.com/posts/2012/07/c11-concurrency-tutorial-part-4-atomic-type.html

int16_t* sampleBuffer;
std::atomic<std::uint32_t>  currentSample{0};
static hw_timer_t * sampleTimer = NULL;

unsigned int sampling_period_us = round( 1000000 * ( 1.0 / SAMPLING_FREQUENCY ) );

//this ISR should run on core 0
static void IRAM_ATTR _sampleISR() {
  sampleBuffer[currentSample.load()] = analogRead( MICROPHONE );
  ++currentSample;
  if ( currentSample.load() == BUFFER_SIZE ) {
    timerEnd( sampleTimer );
    sampleTimer = NULL;
  }
}

bool startSampler() {
  if ( NULL != sampleTimer ) return false;
  delay(170); /* to not record the click from releasing the button */
  currentSample.store( 0, std::memory_order_relaxed );
  sampleTimer = timerBegin( 0, 80, true );
  timerAttachInterrupt( sampleTimer, &_sampleISR, true );
  timerAlarmWrite( sampleTimer, sampling_period_us, true );
  timerAlarmEnable( sampleTimer );
  return true;
}

void setup() {
  pinMode( MICROPHONE, INPUT );
  pinMode( SPEAKER, OUTPUT );
  sampleBuffer = (int16_t*)ps_malloc( BUFFER_SIZE * sizeof( int16_t ) );
  logMemory();
  tft.init();
  tft.setRotation( 1 );
  tft.fillScreen( TFT_BLACK );
  tft.setTextSize( 2 );
  tft.drawString( "M5-Stack voicerecorder", 10, 5, 2 );
  tft.setCursor( 25, 40 );
  tft.printf( "%3.1fkHz %3.1f kB %3.1fs", SAMPLING_FREQUENCY / 1000.0, ( BUFFER_SIZE * sizeof( int16_t ) ) / 1000.0, (float)BUFFER_SIZE / SAMPLING_FREQUENCY );
  tft.drawString( "REC", 45, 200, 2 );
  tft.drawString( "PLAY", 130, 200, 2 );
  tft.drawString( "STOP", 220, 200, 2 );

  if ( LOWNOISE ) {
    pinMode( BACKLIGHT, OUTPUT );
    digitalWrite( BACKLIGHT, HIGH ); // This gives the least noise
  } else {
    ledcAttachPin( BACKLIGHT, 0);
    ledcSetup( 0, 1300, 16 );
    ledcWrite( 0, 0xFFFF / 16 ); // Dimming the BACKLIGHT will produce more base noise
  }  
}

void loop() {
  M5.update();
  if ( !sampleTimer && M5.BtnA.pressedFor( 10 ) ) startSampler();
  if ( !sampleTimer && M5.BtnB.pressedFor( 10 ) ) startPlayback();
  uint32_t pos = currentSample.load();
  tft.setCursor( 30, 100 );
  tft.printf( "%3i%% %7i/%7i", map( pos, 0, BUFFER_SIZE - 1, 0, 100 ), pos, BUFFER_SIZE );
  if ( sampleTimer && M5.BtnC.pressedFor( 2 ) ) {
    timerAlarmDisable( sampleTimer );
    timerEnd( sampleTimer );
    sampleTimer = NULL;
    dacWrite( SPEAKER, 0 );
  }
  delay(10);
}

void IRAM_ATTR _playThroughDAC_ISR() {
  dacWrite( SPEAKER, map( sampleBuffer[currentSample.load()], 0, 2048, 0, 128 ) );
  ++currentSample;
  if ( currentSample.load() == BUFFER_SIZE ) {
    timerEnd( sampleTimer );
    sampleTimer = NULL;
    dacWrite( SPEAKER, 0 );
  }
}

bool startPlayback() {
  if ( NULL != sampleTimer ) return false;
  currentSample.store( 0, std::memory_order_relaxed );
  sampleTimer = timerBegin( 0, 80, true );
  timerAttachInterrupt( sampleTimer, &_playThroughDAC_ISR, true );
  timerAlarmWrite( sampleTimer, sampling_period_us, true );
  timerAlarmEnable( sampleTimer );
  return true;
}