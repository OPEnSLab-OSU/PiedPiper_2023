#include <arduinoFFTFloat.h> // Floating point version of ArduinoFFT 1.6.1, pulled on August 18, 2023
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <ArduCAM.h> // Version 1.0.0, pulled on August 18, 2023
#include "memorysaver.h" // part of ArduCAM library
#include "RTClib.h" // Version 2.1.1, pulled on August 18, 2023

/****************************************************/

// Audio sampling settings:
#define AUD_IN_SAMPLE_FREQ 4096 // Sampling frequency of incoming audio, must be at least twice the target frequency
#define AUD_OUT_SAMPLE_FREQ 4096
#define WIN_SIZE 256 // Size of window used when sampling incoming audio; must be a power of 2
#define AUD_OUT_INTERP_RATIO 8
#define AUD_IN_DOWNSAMPLE_RATIO 2 // sinc filter downsample ratio, ideally should be a power of 2
#define AUD_IN_DOWNSAMPLE_FILTER_ZERO_X 5 // downsample sinc filter number of zero crossing, more crossings will produce a cleaner result but will also use more processor time 
#define AUD_OUT_UPSAMPLE_RATIO 8 // sinc filter upsample ratio, ideally should be a power of 2
#define AUD_OUT_UPSAMPLE_FILTER_ZERO_X 5 // // upsample sinc filter number of zero crossings, more crossings will produce a cleaner result but will also use more processor time 
#define AUD_OUT_TIME 8
#define REC_TIME 8 // Number of seconds of audio to record when frequency test is positive
// The product of sampleFreq and recordTime must be an integer multiple of winSize.

#define ANALOG_RES 12 // Resolution (in bits) of audio input and output


// Detection algorithm settings

// Noise subtraction settings
#define ALPHA_TRIM_WINDOW 32
#define ALPHA_TRIM_THRESH 4.0

// Detection with cross correlation and template: CrossCorrelation()
#define CORRELATION_THRESH 0.5 // correlation threshold
#define TEMPLATE_LENGTH 13 // length of template in number of FFT windows
#define CORRELATION_FREQ_LOW 50
#define CORRELATION_FREQ_HIGH 110
#define TIME_AVG_WIN_COUNT 4 // Number of frequency windows used to average frequencies across time
#define FREQ_SMOOTHING 4

// Detection by using energy within a frequency range and time period: InsectDetection()
#define TGT_FREQ 70 // Primary (first harmonic) frequency of mating call to search for
#define FREQ_MARGIN 8  // Margin for error of target frequency
#define HARMONICS 1 // Number of harmonics to search for; looking for more than 3 is not recommended, because this can result in a high false-positive rate.
#define SIG_THRESH 400 // Threshhold for magnitude of target frequency peak to be considered a positive detection
#define EXP_SIGNAL_LEN 5 // Expected length of the mating call
#define EXP_DET_EFF 1.0 // Minimum expected efficiency by which the detection algorithm will detect target frequency peaks

#define SD_OPEN_ATTEMPT_COUNT 10
#define SD_OPEN_RETRY_DELAY_MS 100

//Hardware settings & information:
#define AUD_IN A2
#define AUD_OUT A0

#define HYPNOS_5VR 6
#define HYPNOS_3VR 5

#define SD_CS 11 // Chip select for SD
#define CAM_CS 10
#define AMP_SD 9

#define LOG_INT 3600000 // Miliseconds between status logs [3600000]
#define PLAYBACK_INT 60000 // Milliseconds between playback [900000]

#define CTRL_IMG_INT 3600000
#define IMG_TIME 300000
#define IMG_INT 30000

#define BEGIN_LOG_WAIT_TIME 10000 //3600000

#define SAVE_DETECTION_DELAY_TIME 1000

#define DEBUG 1

#define USE_CAMERA_MODULE 0

#define USE_DETECTION 1

static const int FFT_WIN_SIZE = WIN_SIZE / AUD_IN_DOWNSAMPLE_RATIO; // int(WIN_SIZE) >> int(log(int(AUD_IN_DOWNSAMPLE_RATIO)) / log(2)); // Size of window used when performing fourier transform of incoming audio; must be a power of 2
static const int FFT_SAMPLE_FREQ = AUD_IN_SAMPLE_FREQ / AUD_IN_DOWNSAMPLE_RATIO; // int(AUD_IN_SAMPLE_FREQ) >> int(log(int(AUD_IN_DOWNSAMPLE_RATIO)) / log(2));
static const float FREQ_RES = (float)FFT_SAMPLE_FREQ / FFT_WIN_SIZE;
static const float FREQ_WIDTH = (float)FFT_WIN_SIZE / FFT_SAMPLE_FREQ;

static const int FFT_WIN_SIZE_BY2 = FFT_WIN_SIZE / 2;

static const int FREQ_MARGIN_LOW = int(floor(CORRELATION_FREQ_LOW * FREQ_WIDTH));
static const int FREQ_MARGIN_HIGH = int(ceil(CORRELATION_FREQ_HIGH * FREQ_WIDTH));

    // Volatile audio input buffer (LINEAR)
volatile static short inputSampleBuffer[FFT_WIN_SIZE];
volatile static int inputSampleBufferIdx = 0;
static const int inputSampleDelayTime = 1000000 / AUD_IN_SAMPLE_FREQ;
const int analogRange = pow(2, ANALOG_RES);

static short outputSampleBuffer[AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME];
volatile static int outputSampleBufferIdx = 0;
volatile static int outputSampleInterpCount = 0;
//static const int outputSampleDelayTime = 1000000 / (AUD_OUT_SAMPLE_FREQ * AUD_OUT_INTERP_RATIO);
static const int outputSampleDelayTime = 1000000 / (AUD_OUT_SAMPLE_FREQ * AUD_OUT_UPSAMPLE_RATIO);
volatile static int interpCount = 0;
static volatile int playbackSampleCount = AUD_OUT_SAMPLE_FREQ * AUD_OUT_TIME;

static ArduCAM CameraModule(OV2640, CAM_CS);

static volatile int nextOutputSample = 0;
static volatile float interpCoeffA = 0;
static volatile float interpCoeffB = 0;
static volatile float interpCoeffC = 0;
static volatile float interpCoeffD = 0;

static const int sincTableSizeDown = (2 * AUD_IN_DOWNSAMPLE_FILTER_ZERO_X + 1) * AUD_IN_DOWNSAMPLE_RATIO - AUD_IN_DOWNSAMPLE_RATIO + 1;
static const int sincTableSizeUp = (2 * AUD_OUT_UPSAMPLE_FILTER_ZERO_X + 1) * AUD_OUT_UPSAMPLE_RATIO - AUD_OUT_UPSAMPLE_RATIO + 1;

// tables holding values corresponding to sinc filter for band limited upsampling/downsampling
static float sincFilterTableDownsample[sincTableSizeDown];
static float sincFilterTableUpsample[sincTableSizeUp];

    // circular input buffer for downsampling
static volatile short downsampleFilterInput[sincTableSizeDown];
static volatile int downsampleInputIdx = 0;
static volatile int downsampleInputCount = 0;

    // circular input buffer for upsampling
static volatile short upsampleFilterInput[sincTableSizeUp];
static volatile int upsampleInputIdx = 0;
static volatile int upsampleInputCount = 0;

class piedPiper {
  private:
    arduinoFFT FFT = arduinoFFT();  //object for FFT in frequency calcuation
    
    File data;

    char settingsFilename[32] = "settings.txt";
    char playbackFilename[32] = { 0 };
    char templateFilename[32] = { 0 };
    char operationTimesFilename[32] = { 0 };

    
    // This must be an integer multiple of the window size:
    static const int sampleCount = REC_TIME * FFT_SAMPLE_FREQ + FFT_WIN_SIZE * TIME_AVG_WIN_COUNT; // [Number of samples required to comprise small + large frequency arrays]
    static const int freqWinCount = REC_TIME * FFT_SAMPLE_FREQ / FFT_WIN_SIZE;

    // Samples array (CIRCULAR)
    short samples[sampleCount];
    int samplePtr = 0;

    // Time smoothing spectral buffer  (CIRCULAR ALONG TIME)
    float rawFreqs[TIME_AVG_WIN_COUNT][FFT_WIN_SIZE_BY2] = { 0 };
    int rawFreqsPtr = 0;

    // Spectral data (CIRCULAR ALONG TIME)
    short freqs[freqWinCount][FFT_WIN_SIZE_BY2] = { 0 };
    int freqsPtr = 0; // Position of "current" time in buffer (specifically, the time that will be written to next; the data at this location is the oldest)

    // template data
    short templateData[TEMPLATE_LENGTH][FFT_WIN_SIZE_BY2] = { 0 };
    // square root sum squared of template data (updates upon call to LoadTemplate())
    long templateSqrtSumSq = 0;
    float inverseTemplateSqrtSumSq = 1.0;

    // Scratchpad arrays used for calculating FFT's and frequency smoothing
    float vReal[FFT_WIN_SIZE];
    float vImag[FFT_WIN_SIZE];

    unsigned long lastLogTime = 0;
    unsigned long lastDetectionTime = 0;
    unsigned long lastImgTime = 0;
    unsigned long lastPlaybackTime = 0;

    int detectionNum = 0;
    int photoNum = 0;

    void ConfigurePins(void);
    
    int IterateCircularBufferPtr(int currentVal, int arrSize);

    // void StopAudio(void);
    void StartAudioOutput(void);
    // void StartAudioInput(void);
    
    void AlphaTrimming(int winSize, float threshold);
    void SmoothFreqs(int winSize);
    bool CheckFreqDomain(int t);
    
    void ResetSPI(void);
    bool BeginSD(void);
    bool OpenFile(char *fname);

    void(*ISRStopFn)(void);
    void(*ISRStartFn)(const unsigned long interval_us, void(*fnPtr)());
    uint8_t read_fifo_burst(ArduCAM CameraModule);

    static void RecordSample(void);
    static void OutputSample(void);
    static void OutputUpsampledSample(void);

    void calculateDownsampleSincFilterTable(void);
    void calculateUpsampleSincFilterTable(void);

    bool OpenFile(char *fname, uint8_t mode);

    bool LoadSettings();

    void ReadDetectionNumber(void);
    void ReadPhotoNum(void);

    
  public:
    piedPiper(void(*audStart)(const unsigned long interval_us, void(*fnPtr)()), void(*audStop)()) ;

    void init(void);
    bool InputSampleBufferFull(void);
    void ProcessData(void);
    bool InsectDetection(void);
    float CrossCorrelation(void);
    void Playback(void);
    //void LoadSound(void);
    void LogAlive(void);
    void LogAliveRTC(void);
    bool TakePhoto(int n);
    bool TakePhotoTTL();
    void SaveDetection(void);
    bool InitializeAudioStream(void);
    bool LoadSound(char *fname);
    bool LoadTemplate(char *fname);
    void ResetOperationIntervals(void);
    void ResetFrequencyBuffers(void);

    bool initializeRTC(void);

    void SetPhotoNum(int n);
    void SetDetectionNum(int n);

    void initializationFailFlash(void);

    RTC_DS3231 rtc;

    void StopAudio(void);
    void StartAudioInput(void);
    
    unsigned long GetLastPlaybackTime(void);
    unsigned long GetLastPhotoTime(void);
    unsigned long GetLastDetectionTime(void);
    unsigned long GetLastLogTime(void);
    int GetDetectionNum(void);
    int GetPhotoNum(void);
};
