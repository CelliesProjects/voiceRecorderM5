# voiceRecorderM5
A single channel voice recorder that uses the `M5Stack Go` microphone and the esp32 internal ADC/DAC.

Instead of I2S, this sketch uses an ESP32 `hw_timer`, the built-in ADC/DAC and ISRs.<br>
One ISR to sample the mic through the ADC and a second ISR to stream samples to the DAC.

Because of the used 'technology', sample rate is limited to about 22kHz and sound quality is crap.

Compiles with esp32 core version 1.0.6. See https://github.com/CelliesProjects/voiceRecorderM5/issues/1 if you get the following error while compiling:

`'ADC1_GPIO34_CHANNEL' was not declared in this scope`


