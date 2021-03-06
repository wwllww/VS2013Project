
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "dynlink_cuda.h" 

#include "nvEncodeAPI.h"
#include "nvUtils.h"

#define SET_VER(configStruct, type) {configStruct.version = type##_VER;}

#if defined (NV_WINDOWS)
    #include "d3d9.h"
    #define NVENCAPI __stdcall
    #pragma warning(disable : 4996)
#elif defined (NV_UNIX)
    #include <dlfcn.h>
    #include <string.h>
    #define NVENCAPI
#endif

#define DEFAULT_I_QFACTOR 0.8f
#define DEFAULT_B_QFACTOR 1.25f
#define DEFAULT_I_QOFFSET 0.f
#define DEFAULT_B_QOFFSET 1.25f

#include "NvCodecDef.h"

typedef struct _EncodeInputBuffer
{
    unsigned int      dwWidth;
    unsigned int      dwHeight;
#if defined (NV_WINDOWS)
    IDirect3DSurface9 *pNV12Surface;
#endif
    CUdeviceptr       pNV12devPtr;
    uint32_t          uNV12Stride;
    CUdeviceptr       pNV12TempdevPtr;
    uint32_t          uNV12TempStride;
    uint32_t          dwTex;
    void*             nvRegisteredResource;
    NV_ENC_INPUT_PTR  hInputSurface;
    NV_ENC_BUFFER_FORMAT bufferFmt;
}EncodeInputBuffer;

typedef struct _EncodeOutputBuffer
{
    unsigned int          dwBitstreamBufferSize;
    NV_ENC_OUTPUT_PTR     hBitstreamBuffer;
    HANDLE                hOutputEvent;
    bool                  bWaitOnEvent;
    bool                  bEOSFlag;
}EncodeOutputBuffer;

typedef struct _EncodeBuffer
{
    EncodeOutputBuffer      stOutputBfr;
    EncodeInputBuffer       stInputBfr;
}EncodeBuffer;

typedef struct _MotionEstimationBuffer
{
    EncodeOutputBuffer      stOutputBfr;
    EncodeInputBuffer       stInputBfr[2];
    unsigned int            inputFrameIndex;
    unsigned int            referenceFrameIndex;
}MotionEstimationBuffer;

typedef struct _NvEncPictureCommand
{
    bool bResolutionChangePending;
    bool bBitrateChangePending;
    bool bForceIDR;
    bool bForceIntraRefresh;
    bool bInvalidateRefFrames;

    uint32_t newWidth;
    uint32_t newHeight;

    uint32_t newBitrate;
    uint32_t newVBVSize;

    uint32_t  intraRefreshDuration;

    uint32_t  numRefFramesToInvalidate;
    uint32_t  refFrameNumbers[16];
}NvEncPictureCommand;

enum
{
    NV_ENC_H264 = 0,
    NV_ENC_HEVC = 1,
};

struct MEOnlyConfig
{
    unsigned char *yuv[2][3];
    unsigned int stride[3];
    unsigned int width;
    unsigned int height;
    unsigned int inputFrameIndex;
    unsigned int referenceFrameIndex;
};

class CNvHWEncoder
{
public:
    uint32_t                                             m_EncodeIdx;
    uint32_t                                             m_uMaxWidth;
    uint32_t                                             m_uMaxHeight;
    uint32_t                                             m_uCurWidth;
    uint32_t                                             m_uCurHeight;
	void                                                *m_hEncoder;

protected:
    bool                                                 m_bEncoderInitialized;
    GUID                                                 codecGUID;

    NV_ENC_INITIALIZE_PARAMS                             m_stCreateEncodeParams;
    NV_ENC_CONFIG                                        m_stEncodeConfig;

public:
    NVENCSTATUS NvEncOpenEncodeSession(void* device, uint32_t deviceType);
    NVENCSTATUS NvEncGetEncodeGUIDCount(uint32_t* encodeGUIDCount);
    NVENCSTATUS NvEncGetEncodeProfileGUIDCount(GUID encodeGUID, uint32_t* encodeProfileGUIDCount);
    NVENCSTATUS NvEncGetEncodeProfileGUIDs(GUID encodeGUID, GUID* profileGUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);
    NVENCSTATUS NvEncGetEncodeGUIDs(GUID* GUIDs, uint32_t guidArraySize, uint32_t* GUIDCount);
    NVENCSTATUS NvEncGetInputFormatCount(GUID encodeGUID, uint32_t* inputFmtCount);
    NVENCSTATUS NvEncGetInputFormats(GUID encodeGUID, NV_ENC_BUFFER_FORMAT* inputFmts, uint32_t inputFmtArraySize, uint32_t* inputFmtCount);
    NVENCSTATUS NvEncGetEncodeCaps(GUID encodeGUID, NV_ENC_CAPS_PARAM* capsParam, int* capsVal);
    NVENCSTATUS NvEncGetEncodePresetCount(GUID encodeGUID, uint32_t* encodePresetGUIDCount);
    NVENCSTATUS NvEncGetEncodePresetGUIDs(GUID encodeGUID, GUID* presetGUIDs, uint32_t guidArraySize, uint32_t* encodePresetGUIDCount);
    NVENCSTATUS NvEncGetEncodePresetConfig(GUID encodeGUID, GUID  presetGUID, NV_ENC_PRESET_CONFIG* presetConfig);
    NVENCSTATUS NvEncCreateInputBuffer(uint32_t width, uint32_t height, void** inputBuffer, NV_ENC_BUFFER_FORMAT inputFormat);
    NVENCSTATUS NvEncDestroyInputBuffer(NV_ENC_INPUT_PTR inputBuffer);
    NVENCSTATUS NvEncCreateBitstreamBuffer(uint32_t size, void** bitstreamBuffer);
    NVENCSTATUS NvEncDestroyBitstreamBuffer(NV_ENC_OUTPUT_PTR bitstreamBuffer);
    NVENCSTATUS NvEncCreateMVBuffer(uint32_t size, void** bitstreamBuffer);
    NVENCSTATUS NvEncDestroyMVBuffer(NV_ENC_OUTPUT_PTR bitstreamBuffer);
    NVENCSTATUS NvRunMotionEstimationOnly(MotionEstimationBuffer *pMEBuffer, MEOnlyConfig *pMEOnly);
    NVENCSTATUS NvEncLockBitstream(NV_ENC_LOCK_BITSTREAM* lockBitstreamBufferParams);
    NVENCSTATUS NvEncUnlockBitstream(NV_ENC_OUTPUT_PTR bitstreamBuffer);
    NVENCSTATUS NvEncLockInputBuffer(void* inputBuffer, void** bufferDataPtr, uint32_t* pitch);
    NVENCSTATUS NvEncUnlockInputBuffer(NV_ENC_INPUT_PTR inputBuffer);
    NVENCSTATUS NvEncGetEncodeStats(NV_ENC_STAT* encodeStats);
    NVENCSTATUS NvEncGetSequenceParams(NV_ENC_SEQUENCE_PARAM_PAYLOAD* sequenceParamPayload);
    NVENCSTATUS NvEncRegisterAsyncEvent(void** completionEvent);
    NVENCSTATUS NvEncUnregisterAsyncEvent(void* completionEvent);
    NVENCSTATUS NvEncMapInputResource(void* registeredResource, void** mappedResource);
    NVENCSTATUS NvEncUnmapInputResource(NV_ENC_INPUT_PTR mappedInputBuffer);
    NVENCSTATUS NvEncDestroyEncoder();
    NVENCSTATUS NvEncInvalidateRefFrames(const NvEncPictureCommand *pEncPicCommand);
    NVENCSTATUS NvEncOpenEncodeSessionEx(void* device, NV_ENC_DEVICE_TYPE deviceType);
    NVENCSTATUS NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE resourceType, void* resourceToRegister,
                                      uint32_t width, uint32_t height, uint32_t pitch,
                                      void** registeredResource,
                                      NV_ENC_BUFFER_FORMAT bufFormat = NV_ENC_BUFFER_FORMAT_NV12);
    NVENCSTATUS NvEncUnregisterResource(NV_ENC_REGISTERED_PTR registeredRes);
    NVENCSTATUS NvEncReconfigureEncoder(const NvEncPictureCommand *pEncPicCommand);
    NVENCSTATUS NvEncFlushEncoderQueue(void *hEOSEvent);

    CNvHWEncoder();
    virtual ~CNvHWEncoder();
    NVENCSTATUS                                          Initialize(void* device, NV_ENC_DEVICE_TYPE deviceType);
    NVENCSTATUS                                          NvEncEncodeFrame(EncodeBuffer *pEncodeBuffer, NvEncPictureCommand *encPicCommand,
                                                                          uint32_t width, uint32_t height,
                                                                          NV_ENC_PIC_STRUCT ePicStruct = NV_ENC_PIC_STRUCT_FRAME,
                                                                          int8_t *qpDeltaMapArray = NULL, uint32_t qpDeltaMapArraySize = 0, NVENC_EXTERNAL_ME_HINT *meExternalHints = NULL, 
                                                                          NVENC_EXTERNAL_ME_HINT_COUNTS_PER_BLOCKTYPE *meHintCountsPerBlock = NULL);
    NVENCSTATUS                                          CreateEncoder(EncodeConfig *pEncCfg);
    GUID                                                 GetPresetGUID(char* encoderPreset, int codec);
	NVENCSTATUS                                          ProcessOutput(const EncodeBuffer *pEncodeBuffer, unsigned char **pbuffer, int *framelen, bool *is_key, uint64_t *timestamp, NV_ENC_PIC_TYPE *pictureType);
    NVENCSTATUS                                          ProcessMVOutput(const MotionEstimationBuffer *pEncodeBuffer);
	NVENCSTATUS                                          FlushEncoder(VCodecBuffer** pOut);
    NVENCSTATUS                                          ValidateEncodeGUID(GUID inputCodecGuid);
    NVENCSTATUS                                          ValidatePresetGUID(GUID presetCodecGuid, GUID inputCodecGuid);
};

typedef NVENCSTATUS (NVENCAPI *MYPROC)(NV_ENCODE_API_FUNCTION_LIST*); 
