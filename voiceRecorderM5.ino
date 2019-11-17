/* this example has to be compiled with PSRAM enabled. */
#include <M5Stack.h>
#include <atomic>              /* https://baptiste-wicht.com/posts/2012/07/c11-concurrency-tutorial-part-4-atomic-type.html */

#define MICROPHONE             34
#define SPEAKER                25
#define BACKLIGHT              32
#define BUFFER_SIZE            4*1000*1000
#define SAMPLING_FREQUENCY     48000

#define LOWNOISE               true /* set to false to enable backlight dimming */

TFT_eSPI tft = TFT_eSPI();

// set log level to debug for output
void logMemory() {
  log_d("Used PSRAM: %d from: %d", ESP.getPsramSize() - ESP.getFreePsram(), ESP.getPsramSize() );
}

int8_t*                           sampleBuffer;
uint32_t                          allocatedSamples;
std::atomic<std::uint32_t>        currentSample{0};
static hw_timer_t *               sampleTimer{NULL}; /* only one timer is (re)used for both sampling and playback */
unsigned int                      sampling_period_us = round( 1000000 * ( 1.0 / SAMPLING_FREQUENCY ) );

static void IRAM_ATTR _sampleISR() {
  uint32_t pos = currentSample.load();
  if ( pos > BUFFER_SIZE - 1 ) {
    timerEnd( sampleTimer );
    sampleTimer = NULL;
    return;
  }
  sampleBuffer[pos] = analogRead( MICROPHONE ) >> 4;
  currentSample++;
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
  logMemory();

  tft.init();
  tft.setRotation( 1 );
  tft.fillScreen( TFT_BLACK );
  tft.setTextSize( 2 );
  tft.drawString( "M5-Stack voicerecorder", 10, 5, 2 );

  if ( LOWNOISE ) {
    pinMode( BACKLIGHT, OUTPUT );
    digitalWrite( BACKLIGHT, HIGH ); // This gives the least noise
  } else {
    ledcAttachPin( BACKLIGHT, 0);
    ledcSetup( 0, 1300, 16 );
    ledcWrite( 0, 0xFFFF / 16 );     // Dimming the BACKLIGHT will produce more base noise
  }

  if ( !ESP.getPsramSize() ) {
    tft.setCursor( 55, 40 );
    tft.print( "NO PSRAM FOUND!" );
    while ( 1 ) delay( 100 );
  }


  sampleBuffer = (int8_t*)ps_malloc( BUFFER_SIZE * sizeof( int8_t ) );

  tft.setCursor( 15, 40 );
  tft.printf( "%3.1fkHz %5.1fkB %6.2fs", SAMPLING_FREQUENCY / 1000.0, ( BUFFER_SIZE * sizeof( int8_t ) ) / 1000.0, BUFFER_SIZE / (float)SAMPLING_FREQUENCY );
  tft.drawString( "REC", 45, 200, 2 );
  tft.drawString( "PLAY", 130, 200, 2 );
  tft.drawString( "STOP", 220, 200, 2 );
}

void loop() {
  M5.update();
  if ( !sampleTimer && M5.BtnA.pressedFor( 5 ) ) startSampler();
  if ( !sampleTimer && M5.BtnB.pressedFor( 5 ) ) startPlayback();
  uint32_t pos = currentSample.load();
  tft.setCursor( 85, 100 );
  tft.printf( "%3i%% %6.2fs", map( pos, 0, BUFFER_SIZE - 1, 0, 100 ), pos / (float)SAMPLING_FREQUENCY );
  tft.setCursor( 60, 130 );
  tft.printf( " %7i/%7i", pos, BUFFER_SIZE );
  if ( sampleTimer && M5.BtnC.pressedFor( 2 ) ) {
    timerAlarmDisable( sampleTimer );
    timerEnd( sampleTimer );
    sampleTimer = NULL;
    dacWrite( SPEAKER, 0 );
  }
  delay(10);
}

void IRAM_ATTR _playThroughDAC_ISR() {
  uint32_t pos = currentSample.load();
  if ( pos > BUFFER_SIZE - 1 ) {
    timerEnd( sampleTimer );
    sampleTimer = NULL;
    dacWrite( SPEAKER, 0 );
    return;
  }
  dacWrite( SPEAKER, sampleBuffer[pos] );
  currentSample++;
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
