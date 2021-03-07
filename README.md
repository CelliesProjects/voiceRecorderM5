# voiceRecorderM5
A single channel voice recorder that uses the M5Stack microphone and DAC.

Instead of I2S, this sketch uses an ESP32 `hw_timer`, the built-in ADC/DAC and ISRs.<br>
One ISR to sample the mic through the ADC and a second ISR to stream samples to the DAC.

Sample rate is limited to about 22kHz and sound quality is crap.
