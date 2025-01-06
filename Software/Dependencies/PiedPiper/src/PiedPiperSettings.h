#ifndef PIEDPIPER_SETTINGS_h
#define PIEDPIPER_SETTINGS_h

#define SAMPLE_RATE 4096                ///< original sample rate of sampling input and output
#define WINDOW_SIZE 256                 ///< original window size to use for sampling

#define PLAYBACK_FILE_LENGTH 8          ///< maximum duration of playback file in seconds

#define PIN_AUD_OUT A0                  ///< audio input pin
#define PIN_AUD_IN A2                   ///< audio output pin
#define PIN_HYPNOS_3VR 5                ///< pin controlling hypnos 3V rail
#define PIN_HYPNOS_5VR 6                ///< pin controlling hypnos 5V rail
#define PIN_SD_CS 11                    ///< SD chip select pin
#define PIN_AMP_SD 9                    ///< amplifier shutdown pin
#define PIN_RTC_ALARM 12                ///< rtc alarm pin

#define DEFAULT_DS3231_ADDR 0x68        ///< default address of DS3213 rtc
#define DEFAULT_MCP465_ADDR 0x28        ///< default address of MCP465 digital potentiometer
#define DEFAULT_SHT31_ADDR 0x44         ///< default address of SHT31 temperature sensor

#define AUD_IN_DOWNSAMPLE_RATIO 2       ///< audio input downsampling ratio
#define AUD_OUT_UPSAMPLE_RATIO 8        ///< audio output upsampling ratio
#define SINC_FILTER_DOWNSAMPLE_ZERO_X 5 ///< number of zero crossings for audio input resampling filter
#define SINC_FILTER_UPSAMPLE_ZERO_X 5   ///< number of zero crossings for audio output resampling filter

#define ADC_RESOLUTION 12
#define DAC_RESOLUTION 12

#endif