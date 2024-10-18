#include "PiedPiper.h"

// calculating sinc table sizes
static const int sincTableSizeDown = (2 * SINC_FILTER_DOWNSAMPLE_ZERO_X + 1) * AUD_IN_DOWNSAMPLE_RATIO - AUD_IN_DOWNSAMPLE_RATIO + 1;
static const int sincTableSizeUp = (2 * SINC_FILTER_UPSAMPLE_ZERO_X + 1) * AUD_OUT_UPSAMPLE_RATIO - AUD_OUT_UPSAMPLE_RATIO + 1;

// tables holding values corresponding to sinc filter for band limited upsampling/downsampling
static float sincFilterTableDownsample[sincTableSizeDown];
static float sincFilterTableUpsample[sincTableSizeUp];

// circular input buffer for downsampling
static volatile short downsampleFilterInput[sincTableSizeDown];
static volatile uint16_t downsampleInputIdx = 0;
static volatile uint16_t downsampleInputCount = 0;

// circular input buffer for upsampling
static volatile short upsampleFilterInput[sincTableSizeUp];
static volatile uint16_t upsampleInputIdx = 0;
static volatile uint16_t upsampleInputCount = 0;

void PiedPiperBase::RecordSample(void) {

}

void PiedPiperBase::OutputSample(void) {

}