#include "predictor.h"

#define STRONG_NOT_TAKEN 0
#define WEAK_NOT_TAKEN 1
#define WEAK_TAKEN 2
#define STRONG_TAKEN 3


/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

static UINT32 TwoBitSatPHT[4096];

UINT32 GetPHTIndex(UINT32 PC)
{
    // mask the 12 least significant bits to get the index
    UINT32 index = PC & 0x00000fff;
    
    return index;
}

void InitPredictor_2bitsat() {
	for (int i = 0; i < 4096; i++)
	{
		// initial all the prediction values to weak not-taken
		TwoBitSatPHT[i] = WEAK_NOT_TAKEN;
	}
}

bool GetPrediction_2bitsat(UINT32 PC) {
    UINT32 index = GetPHTIndex(PC);
    
    bool result = TAKEN;
    
    // according to the table entries to give the prediction
    switch (TwoBitSatPHT[index])
    {
        case STRONG_NOT_TAKEN:
        case WEAK_NOT_TAKEN:
            result = NOT_TAKEN;
            break;
        case STRONG_TAKEN:
        case WEAK_TAKEN:
            result = TAKEN;
            break;
        default:
            break;
    };

    return result;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    UINT32 index = GetPHTIndex(PC);
    
    // update 2-bit bimodal according to the case
    // only update WEAK_TAKEN and WEAK_NOT_TAKEN
    if (resolveDir == TAKEN && TwoBitSatPHT[index] != STRONG_TAKEN)
        TwoBitSatPHT[index]++;
    if (resolveDir == NOT_TAKEN && TwoBitSatPHT[index] != STRONG_NOT_TAKEN)
        TwoBitSatPHT[index]--;
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

// 512 entries BHT, 6-bit history bits
static UINT32 BHT[512];

// 8 PH tables, with each tables have 2^6 = 64 entries 2-bit sat counter
static UINT32 PHTs[8][64];

UINT32 GetBHTIndex(UINT32 PC)
{
    UINT32 index = (PC >> 3) & 0x000001ff;
    
    return index;
}

UINT32 GetPHTsIndex(UINT32 PC)
{
    UINT32 index = PC & 0x00000007;
    
    return index;
}

UINT32 GetPHTEntryIndex(UINT32 BHTIndex)
{
    return BHT[BHTIndex];
}

void InitPredictor_2level() {
    // initialize the BHRs to all not taken
    for (int i = 0; i < 512; i++)
    {
        BHT[i] = 0;
    }
    
    // initialize all the entries of 2-bit saturating couters to weak not-taken
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            PHTs[i][j] = WEAK_NOT_TAKEN;
        }
    }
}

bool GetPrediction_2level(UINT32 PC) {
    UINT32 BHTIndex = GetBHTIndex(PC);
    UINT32 PHTsIndex = GetPHTsIndex(PC);
    UINT32 PHTIndex = GetPHTEntryIndex(BHTIndex);
    
    bool result = TAKEN;
    
    // according to the table entries to give the prediction
    switch (PHTs[PHTsIndex][PHTIndex])
    {
        case STRONG_NOT_TAKEN:
        case WEAK_NOT_TAKEN:
            result = NOT_TAKEN;
            break;
        case STRONG_TAKEN:
        case WEAK_TAKEN:
            result = TAKEN;
            break;
        default:
            break;
    };

    return result;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    UINT32 BHTIndex = GetBHTIndex(PC);
    UINT32 PHTsIndex = GetPHTsIndex(PC);
    UINT32 PHTIndex = GetPHTEntryIndex(BHTIndex);
    
    BHT[BHTIndex] = ((BHT[BHTIndex] << 1) & 0x0000003f) | resolveDir;
    
    if (resolveDir == TAKEN && PHTs[PHTsIndex][PHTIndex] != STRONG_TAKEN)
        PHTs[PHTsIndex][PHTIndex]++;
    if (resolveDir == NOT_TAKEN && PHTs[PHTsIndex][PHTIndex] != STRONG_NOT_TAKEN)
        PHTs[PHTsIndex][PHTIndex]--;
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////
#define OPENBIMODALSIZE 4096
static UINT32 OpenBimodalPHT[OPENBIMODALSIZE];

UINT32 GetOpenBimodalPHTIndex(UINT32 PC)
{
    UINT32 index = PC & 0x00000fff;
    return index;
}

void InitPredictor_OpenBimodal() {
	for (int i = 0; i < OPENBIMODALSIZE; i++)
	{
		OpenBimodalPHT[i] = WEAK_NOT_TAKEN;
	}
}

bool GetPrediction_OpenBimodal(UINT32 PC) {
    UINT32 index = GetOpenBimodalPHTIndex(PC);
    bool result = TAKEN;
    // according to the table entries to give the prediction
    switch (OpenBimodalPHT[index])
    {
        case STRONG_NOT_TAKEN:
        case WEAK_NOT_TAKEN:
            result = NOT_TAKEN;
            break;
        case STRONG_TAKEN:
        case WEAK_TAKEN:
            result = TAKEN;
            break;
        default:
            break;
    };
    return result;
}

void UpdatePredictor_OpenBimodal(UINT32 PC, bool resolveDir) {
    UINT32 index = GetOpenBimodalPHTIndex(PC);
    if (resolveDir == TAKEN && OpenBimodalPHT[index] != STRONG_TAKEN)
        OpenBimodalPHT[index]++;
    if (resolveDir == NOT_TAKEN && OpenBimodalPHT[index] != STRONG_NOT_TAKEN)
        OpenBimodalPHT[index]--;
}

#define PHTSIZE 20000
#define SELECTORSIZE 1024
static UINT32 BHR;
static UINT32 PHT1[PHTSIZE];
static UINT32 PHT2[PHTSIZE];
static UINT32 PHT3[PHTSIZE];
static UINT32 HybridSelector[SELECTORSIZE];

bool skewedPrevResult = TAKEN;
bool bimodalPrevResult = TAKEN;

void InitPredictor_skewed()
{
    // initialize the BHR to 0
    BHR = 0;

    // initialize the PHT values to 01
    for (int i = 0; i < PHTSIZE; i++)
    {
        PHT1[i] = WEAK_NOT_TAKEN;
        PHT2[i] = WEAK_NOT_TAKEN;
        PHT3[i] = WEAK_NOT_TAKEN;
    }
    
    // initialize the hybrid selector values to 01
    for (int i = 0; i < SELECTORSIZE; i++)
    {
        HybridSelector[i] = WEAK_NOT_TAKEN;
    }
}

UINT32 GetBHRIndex(UINT32 PC)
{
    return ((PC >> 2) ^ BHR);
}


UINT32 HashFcn1(UINT32 index)
{
    UINT32 y1 = index & 0x00000001;
    UINT32 yn = index & 0x00002000;
    UINT32 changed = (index >> 1) |  yn;
    return y1 ^ changed;
}

UINT32 HashFcn2(UINT32 index)
{
    UINT32 yn = index & 0x00002000;
    UINT32 yn_1 = index & 0x00001000;
    UINT32 changed = ((index << 1) & 0x00003fff) | (yn_1 >> 12);
    return changed ^ yn;
}

// three hash functions
UINT32 GetPHT1Index(UINT32 index)
{
    return HashFcn1(index) % PHTSIZE;
}

UINT32 GetPHT2Index(UINT32 index)
{
    return HashFcn2(index) % PHTSIZE;
}

UINT32 GetPHT3Index(UINT32 index)
{
    return (HashFcn1(index) ^ HashFcn2(index)) % PHTSIZE;
}

bool GetPHT123Result(UINT32 *PHT, UINT32 PHTIndex)
{
    bool result = TAKEN;
    switch (PHT[PHTIndex])
    {
        case STRONG_NOT_TAKEN:
        case WEAK_NOT_TAKEN:
            result = NOT_TAKEN;
            break;
        case STRONG_TAKEN:
        case WEAK_TAKEN:
            result = TAKEN;
            break;
        default:
            break;
    };
    return result;
}

bool GetOpenendPredictorResult(UINT32 PHT1Index, UINT32 PHT2Index, UINT32 PHT3Index) {
    // get the 3 PHT predictor results
    bool result = TAKEN;
    bool result1 = GetPHT123Result(PHT1, PHT1Index);
    bool result2 = GetPHT123Result(PHT2, PHT2Index);
    bool result3 = GetPHT123Result(PHT3, PHT3Index);

    // vote the majority
    if ((result1 == TAKEN && result2 == TAKEN) || (result1 == TAKEN && result3 == TAKEN) || (result2 == TAKEN && result3 == TAKEN)) 
        result = TAKEN;
    else
        result = NOT_TAKEN;

    return result;
}

UINT32 GetHybridSelectorIndex(UINT32 PC)
{
    return (PC >> 2) & 0x000003ff;
}

void UpdateSkewedPHT(bool resolveDir, UINT32 *PHT, UINT32 Index)
{
    if (resolveDir == TAKEN && PHT[Index] != STRONG_TAKEN)
        PHT[Index]++;
    if (resolveDir == NOT_TAKEN && PHT[Index] != STRONG_NOT_TAKEN)
        PHT[Index]--;
}

void UpdateSkewedPredicotr(UINT32 PC, bool resolveDir)
{
    UINT32 BHRIndex = GetBHRIndex(PC);
    UINT32 PHT1Index = GetPHT1Index(BHRIndex);
    UINT32 PHT2Index = GetPHT2Index(BHRIndex);
    UINT32 PHT3Index = GetPHT3Index(BHRIndex);

    BHR = (BHR << 1) | resolveDir;
    
    UpdateSkewedPHT(resolveDir, PHT1, PHT1Index);
    UpdateSkewedPHT(resolveDir, PHT2, PHT2Index);
    UpdateSkewedPHT(resolveDir, PHT3, PHT3Index);
}

void UpdateHybridSelector(UINT32 PC, bool resolveDir)
{
    UINT32 hsindex = GetHybridSelectorIndex(PC);
    UINT32 selector = HybridSelector[hsindex];
    
    if (resolveDir == skewedPrevResult && resolveDir != bimodalPrevResult) {
        if (selector < STRONG_TAKEN)
            HybridSelector[hsindex] ++;
    }
    if (resolveDir != skewedPrevResult && resolveDir == bimodalPrevResult) {
        if (selector > STRONG_NOT_TAKEN)
            HybridSelector[hsindex]--;
    }
}

void InitPredictor_openend() {
    InitPredictor_skewed();
    InitPredictor_OpenBimodal();
}

bool GetPrediction_openend(UINT32 PC) {
    UINT32 BHRIndex = GetBHRIndex(PC);
    UINT32 PHT1Index = GetPHT1Index(BHRIndex);
    UINT32 PHT2Index = GetPHT2Index(BHRIndex);
    UINT32 PHT3Index = GetPHT3Index(BHRIndex);

    // select the skewed predictor and previously defined bimodal predictor
    bool skewedResult = GetOpenendPredictorResult(PHT1Index, PHT2Index, PHT3Index);
    bool bimodal = GetPrediction_OpenBimodal(PC);
    skewedPrevResult = skewedResult;
    bimodalPrevResult = bimodal;
    
    // get the hybrid selector, choose the predictor to use
    UINT32 hsindex = GetHybridSelectorIndex(PC);
    UINT32 selector = HybridSelector[hsindex];
    if(selector >= WEAK_TAKEN){
        return skewedPrevResult;
    }
    else{
        return bimodalPrevResult;
    }
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    UpdateSkewedPredicotr(PC, resolveDir);
    UpdatePredictor_OpenBimodal(PC, resolveDir);

    UpdateHybridSelector(PC, resolveDir);
}
