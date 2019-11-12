# voiceRecorderM5
A single channel voice recorder that uses the M5Stack Fire hw_timer, microphone and DAC.

Instead of I2S, this sketch uses an ESP32 `hw_timer` and ISR to sample the mic and stream samples to the DAC.
