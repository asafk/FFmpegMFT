#include "stdafx.h"
#include "FFmpegMFT.h"

#include <evr.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <uuids.h>


#include "CBufferLock.h"

void DebugOut(const LPCWSTR fmt, ...)
{
  va_list argp; 
  va_start(argp, fmt); 
  wchar_t dbg_out[4096];
  vswprintf_s(dbg_out, fmt, argp);
  va_end(argp); 
  OutputDebugString(dbg_out);
}


FFmpegMFT::FFmpegMFT(void) :
    m_cRef(1),
	m_sampleTime(0),
	m_h3dDevice(NULL)/*,
	m_pConfigs(NULL),
	m_pRenderTargetFormats(NULL)*/
{
	//OutputDebugString(_T("\n\nFFmpegMFT\n\n"));
}

FFmpegMFT::~FFmpegMFT(void)
{
	//OutputDebugString(_T("\n\n~FFmpegMFT\n\n"));
    // reduce the count of DLL handles so that we can unload the DLL when 
    // components in it are no longer being used
    InterlockedDecrement(&g_dllLockCount);
}

//
// IUnknown interface implementation
//
ULONG FFmpegMFT::AddRef()
{
	//OutputDebugString(_T("\n\nAddRef\n\n"));
    return InterlockedIncrement(&m_cRef);
}

ULONG FFmpegMFT::Release()
{
	//OutputDebugString(_T("\n\nRelease\n\n"));

    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT FFmpegMFT::QueryInterface(REFIID riid, void** ppv)
{
	//OutputDebugString(_T("\n\nQueryInterface\n\n"));

    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_IMFTransform)
    {
        *ppv = static_cast<IMFTransform*>(this);
    }
    else
    {
        *ppv = NULL;
        hr = E_NOINTERFACE;
    }

    if(SUCCEEDED(hr))
        AddRef();

    return hr;
}

//*************************************************************************************
//
// IMFTransform interface implementation
//
//*************************************************************************************


//
// Get the maximum and minimum number of streams that this MFT supports.
//
HRESULT FFmpegMFT::GetStreamLimits(
    DWORD   *pdwInputMinimum,
    DWORD   *pdwInputMaximum,
    DWORD   *pdwOutputMinimum,
    DWORD   *pdwOutputMaximum)
{
	//OutputDebugString(_T("\n\nGetStreamLimits\n\n"));

    if (pdwInputMinimum == NULL ||
        pdwInputMaximum == NULL ||
        pdwOutputMinimum == NULL ||
        pdwOutputMaximum == NULL)
    {
        return E_POINTER;
    }

    // This MFT supports only one input stream and one output stream.
    // There can't be more or less than one input or output streams.
    *pdwInputMinimum = 1;
    *pdwInputMaximum = 1;
    *pdwOutputMinimum = 1;
    *pdwOutputMaximum = 1;

    return S_OK;
}

//
// Get the actual number of streams that the MFT is currently set up to
// process.  This is needed in cases the number of streams that an MFT is 
// processing can change depending on various conditions.
//
HRESULT FFmpegMFT::GetStreamCount(
    DWORD   *pcInputStreams,
    DWORD   *pcOutputStreams)
{
	//OutputDebugString(_T("\n\nGetStreamCount\n\n"));
    // check the pointers
    if (pcInputStreams == NULL  ||  pcOutputStreams == NULL)
    {
        return E_POINTER;
    }

    // The MFT supports only one input stream and one output stream.
    *pcInputStreams = 1;
    *pcOutputStreams = 1;

    return S_OK;
}

//
// Get IDs for the input and output streams. This function doesn't need to be implemented in
// this case because the MFT supports only a single input stream and a single output stream, 
// and we can set its ID to 0.
//
HRESULT FFmpegMFT::GetStreamIDs(
    DWORD   dwInputIDArraySize,
    DWORD   *pdwInputIDs,
    DWORD   dwOutputIDArraySize,
    DWORD   *pdwOutputIDs)
{
	//OutputDebugString(_T("\n\nGetStreamIDs\n\n"));
    return E_NOTIMPL;
}

//
// Get a structure with information about an input stream with the specified index.
//
HRESULT FFmpegMFT::GetInputStreamInfo(
    DWORD                   dwInputStreamID,  // stream being queried.
    MFT_INPUT_STREAM_INFO*  pStreamInfo)      // stream information
{
	//OutputDebugString(_T("\n\nGetInputStreamInfo\n\n"));

    HRESULT hr = S_OK;

    do
    {
        // lock the MFT - the lock will disengage when variable goes out of scope
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pStreamInfo, E_POINTER);

        // This MFT supports only stream with ID of zero
        if (dwInputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            break;
        }

        // The dwFlags variable contains the required configuration of the input stream. The
        // flags specified here indicate:
        //   - MFT accepts samples with whole units of data.  In this case this means that 
        //      each sample should contain a whole uncompressed frame.
        //   - The samples returned will have only a single buffer.
        pStreamInfo->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES | 
            MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER ;
        
        // maximum amount of input data that the MFT requires to start returning samples
        pStreamInfo->cbMaxLookahead = 0;

        // memory alignment of the sample buffers
        pStreamInfo->cbAlignment = 0;

        // maximum latency between an input sample arriving and the output sample being
        // ready
        pStreamInfo->hnsMaxLatency = 0;

        // required input size of a sample - 0 indicates that any size is acceptable
        pStreamInfo->cbSize = 0;
    }
    while(false);

    return hr;
}


//
// Get information about the specified output stream.  Note that the returned structure 
// contains information independent of the media type set on the MFT, and thus should 
// always return values indicating its internal behavior.
//
HRESULT FFmpegMFT::GetOutputStreamInfo(
    DWORD                     dwOutputStreamID,
    MFT_OUTPUT_STREAM_INFO *  pStreamInfo)
{

	//OutputDebugString(_T("\n\nGetOutputStreamInfo\n\n"));
    HRESULT hr = S_OK;

    do
    {
        // lock the MFT - the lock will disengage when variable goes out of scope
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pStreamInfo, E_POINTER);

        // The MFT supports only a single stream with ID of 0
        if (dwOutputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            break;
        }

        // The dwFlags variable contains a set of flags indicating how the MFT behaves.
        // The flags shown below indicate the following:
        //   - MFT provides samples with whole units of data.  This means that each sample 
        //     contains a whole uncompressed frame.
        //   - The samples returned will have only a single buffer.

        //   - only on HW - The MFT provides samples and there is no need to give it output samples to 
        //     fill in during its ProcessOutput() calls.
		if(m_p3DDeviceManager == NULL){
	        pStreamInfo->dwFlags =
	            MFT_OUTPUT_STREAM_WHOLE_SAMPLES |                
	            MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
				MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
		}
		else{
			pStreamInfo->dwFlags =
	            MFT_OUTPUT_STREAM_WHOLE_SAMPLES |                
	            MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
				MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE |
	            MFT_OUTPUT_STREAM_PROVIDES_SAMPLES ;
		}

        // the cbAlignment variable contains information about byte alignment of the sample 
        // buffers, if one is needed.  Zero indicates that no specific alignment is needed.
        pStreamInfo->cbAlignment = 0;

        // Size of the samples returned by the MFT.  Since the MFT provides its own samples,
        // this value must be zero.
        pStreamInfo->cbSize = 0;
    }
    while(false);

    return hr;
}


HRESULT FFmpegMFT::GetAttributes(IMFAttributes** pAttributes)
{
	//OutputDebugString(_T("\n\nGetAttributes\n\n"));
    
	HRESULT hr = S_OK;
    CComPtr<IMFAttributes> att;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pAttributes, E_POINTER);
		hr = MFCreateAttributes(&att, 0);
		BREAK_ON_FAIL(hr);

		hr = att->SetUINT32(MF_SA_D3D_AWARE, TRUE);
		BREAK_ON_FAIL(hr);       

        // return the resulting attribute
        *pAttributes = att;
        (*pAttributes)->AddRef();
    }
    while(false);

    if(FAILED(hr) && pAttributes != NULL)
    {
        *pAttributes = NULL;
    }

    return hr;
}



//
// Gets the store of attributes associated with a specified input stream of the 
// MFT.  This method can be left unimplemented if custom input stream attributes 
// are not supported.
//
HRESULT FFmpegMFT::GetInputStreamAttributes(
    DWORD           dwInputStreamID,
    IMFAttributes** ppAttributes)
{
	//OutputDebugString(_T("\n\nGetInputStreamAttributes\n\n"));
    // This MFT does not support any attributes, so the method is not implemented.
    return E_NOTIMPL;
}



//
// Gets the store of attributes associated with a specified output stream of the 
// MFT.  This method can be left unimplemented if custom output stream attributes 
// are not supported.
//
HRESULT FFmpegMFT::GetOutputStreamAttributes(
    DWORD           dwOutputStreamID,
    IMFAttributes** ppAttributes)
{
	//OutputDebugString(_T("\n\nGetOutputStreamAttributes\n\n"));
    // This MFT does not support any attributes, so the method is not implemented.
    return E_NOTIMPL;
}



//
// Deletes the specified input stream from the MFT.  This MFT has only a single 
// constant stream, and therefore this method is not implemented.
//
HRESULT FFmpegMFT::DeleteInputStream(DWORD dwStreamID)
{
	//OutputDebugString(_T("\n\nDeleteInputStream\n\n"));
    return E_NOTIMPL;
}



//
// Add the specified input stream from the MFT.  This MFT has only a single 
// constant stream, and therefore this method is not implemented.
//
HRESULT FFmpegMFT::AddInputStreams(
    DWORD   cStreams,
    DWORD*  adwStreamIDs)
{
	//OutputDebugString(_T("\n\nAddInputStreams\n\n"));
    return E_NOTIMPL;
}



//
// Return one of the preferred input media types for this MFT, specified by
// media type index and by stream ID.
//
HRESULT FFmpegMFT::GetInputAvailableType(
    DWORD           dwInputStreamID,
    DWORD           dwTypeIndex, 
    IMFMediaType    **ppType)
{

	//OutputDebugString(_T("\n\nGetInputAvailableType\n\n"));

    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> pmt;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(ppType, E_POINTER);

        // only a single stream is supported
        if (dwInputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            BREAK_ON_FAIL(hr);
        }


        hr = GetSupportedInputMediaType(dwTypeIndex, &pmt);
        BREAK_ON_FAIL(hr);

        // return the resulting media type
        *ppType = pmt.Detach();
    }
    while(false);

    if(FAILED(hr) && ppType != NULL)
    {
        *ppType = NULL;
    }

    return hr;
}



//
// Description: Return a preferred output type.
//
HRESULT FFmpegMFT::GetOutputAvailableType(
    DWORD           dwOutputStreamID,
    DWORD           dwTypeIndex, // 0-based
    IMFMediaType    **ppType)
{
	//OutputDebugString(_T("\n\nGetOutputAvailableType\n\n"));

    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> pmt;

    do
    {
        // lock the MFT - the lock will disengage when variable goes out of scope
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(ppType, E_POINTER);

        if (dwOutputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            BREAK_ON_FAIL(hr);
        }

		if (m_pInputType == NULL)
	    {
	        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
			BREAK_ON_FAIL(hr);
	    }
		
    	hr = GetSupportedOutputMediaType(dwTypeIndex, &pmt);
        BREAK_ON_FAIL(hr);

        // return the resulting media type
        *ppType = pmt;
        (*ppType)->AddRef();
    }
    while(false);

    return hr;
}



//
// Set, test, or clear the input media type for the MFT.
//
HRESULT FFmpegMFT::SetInputType(DWORD dwInputStreamID, IMFMediaType* pType, 
    DWORD dwFlags)
{
	//OutputDebugString(_T("\n\nSetInputType\n\n"));

    HRESULT hr = S_OK;

	do
    {
        // lock the MFT - the lock will disengage when variable goes out of scope
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // this MFT supports only a single stream - fail if a different stream value is
        // suggested
        if (dwInputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            BREAK_ON_FAIL(hr);
        }

		if (m_pInputType == pType)
			break;

		if(pType == NULL) //clear input type
		{
			m_pInputType = NULL;
			break;
		}

        // verify that the specified media type is acceptable to the MFT
        hr = CheckInputMediaType(pType);
		BREAK_ON_FAIL(hr);

        // If the MFT is already processing a sample internally, fail out, since the MFT
        // can't change formats on the fly.
        if (m_pSample != NULL)
        {
            hr = MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
            BREAK_ON_FAIL(hr);
        }

        // If we got here, then the media type is acceptable - set it if the caller
        // explicitly tells us to set it, and is not just checking for compatible
        // types.
        if (dwFlags != MFT_SET_TYPE_TEST_ONLY)
        {
            m_pInputType = pType;         
        }
    }
    while(false);
    
    return hr;
}


//
// Set, test, or clear the output media type of the MFT
//
HRESULT FFmpegMFT::SetOutputType(
    DWORD           dwOutputStreamID,
    IMFMediaType*   pmt, // Can be NULL to clear the output type.
    DWORD           dwFlags)
{
	//OutputDebugString(_T("\n\nSetOutputType\n\n"));

    HRESULT hr = S_OK;

    CComPtr<IMFMediaType> pType = pmt;

    do
    {
        // lock the MFT - the lock will disengage when variable goes out of scope
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        // this MFT supports only a single stream - fail if a different stream value
        // is suggested
        if (dwOutputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
            BREAK_ON_FAIL(hr);
        }

		if (m_pOutputType == pType)
			break;

		if(pType == NULL)
		{
			m_pOutputType = pType;
			break;
		}

       // verify that the specified media type is acceptable to the MFT
        hr = CheckOutputMediaType(pType);
        BREAK_ON_FAIL(hr);

        // If the MFT is already processing a sample internally, fail out, since the
        // MFT can't change formats on the fly.
        if (m_pSample != NULL)
        {
            hr = MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
            BREAK_ON_FAIL(hr);
        }

        // If we got here, then the media type is acceptable - set it if the caller
        // explicitly tells us to set it, and is not just checking for compatible
        // types.
        if (dwFlags != MFT_SET_TYPE_TEST_ONLY)
        {
            m_pOutputType = pType;
        }
    }
    while(false);

    
    return hr;
}


// 
// Get the current input media type.
//
HRESULT FFmpegMFT::GetInputCurrentType(
    DWORD           dwInputStreamID,
    IMFMediaType**  ppType)
{
	//OutputDebugString(_T("\n\nGetInputCurrentType\n\n"));

    HRESULT hr = S_OK;

    do
    {
        // lock the MFT
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(ppType, E_POINTER);

        if (dwInputStreamID != 0)
        {
            hr = MF_E_INVALIDSTREAMNUMBER;
        }
        else if (m_pInputType == NULL)
        {
            hr = MF_E_TRANSFORM_TYPE_NOT_SET;
        }
        else
        {
            *ppType = m_pInputType;
            (*ppType)->AddRef();
        }
    }
    while(false);
    
    return hr;
}


//
// Get the current output type
//
HRESULT FFmpegMFT::GetOutputCurrentType(
    DWORD           dwOutputStreamID,
    IMFMediaType**  ppType)
{
	//OutputDebugString(_T("\n\nGetOutputCurrentType\n\n"));

    HRESULT hr = S_OK;

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    if (ppType == NULL)
    {
        return E_POINTER;
    }

    // verify the correct output stream ID and that the output
    // type has been set
    if (dwOutputStreamID != 0)
    {
        hr = MF_E_INVALIDSTREAMNUMBER;
    }
    else if (m_pOutputType == NULL)
    {
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    }
    else
    {
        *ppType = m_pOutputType;
        (*ppType)->AddRef();
    }

    return hr;
}


//
// Check to see if the MFT is ready to accept input samples
//
HRESULT FFmpegMFT::GetInputStatus(
    DWORD           dwInputStreamID,
    DWORD*          pdwFlags)
{
	//OutputDebugString(_T("\n\nGetInputStatus\n\n"));

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    if (pdwFlags == NULL)
    {
        return E_POINTER;
    }

    // the MFT supports only a single stream.
    if (dwInputStreamID != 0)
    {
        return MF_E_INVALIDSTREAMNUMBER;
    }

    // if there is no sample queued in the MFT, it is ready to accept data. If there already 
    // is a sample in the MFT, the MFT can't accept any more samples until somebody calls  
    // ProcessOutput to extract that sample, or flushes the MFT.
    if (m_pSample == NULL)
    {
        // there is no sample in the MFT - ready to accept data
        *pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;
    }
    else
    {
        // a value of zero indicates that the MFT can't accept any more data
        *pdwFlags = 0;
    }

    return S_OK;
}


//
// Get the status of the output stream of the MFT - IE verify whether there
// is a sample ready in the MFT.  This method can be left unimplemented.
//
HRESULT FFmpegMFT::GetOutputStatus(DWORD* pdwFlags)
{
	//OutputDebugString(_T("\n\nGetOutputStatus\n\n"));
    return E_NOTIMPL;
}


//
// Set the range of time stamps that the MFT will output.  This MFT does
// not implement this behavior, and is left unimplemented.
//
HRESULT FFmpegMFT::SetOutputBounds(
    LONGLONG        hnsLowerBound,
    LONGLONG        hnsUpperBound)
{
	//OutputDebugString(_T("\n\nSetOutputBounds\n\n"));

    return E_NOTIMPL;
}


//
// Send an event to an input stream.  Since this MFT does not handle any
// such commands, this method is left unimplemented.
//
HRESULT FFmpegMFT::ProcessEvent(
    DWORD              dwInputStreamID,
    IMFMediaEvent*     pEvent)
{
	//OutputDebugString(_T("\n\nProcessEvent\n\n"));

    return E_NOTIMPL;
}



//
// Receive and process a message or command to the MFT, specifying a
// requested behavior.
//
HRESULT FFmpegMFT::ProcessMessage(
    MFT_MESSAGE_TYPE    eMessage,
    ULONG_PTR           ulParam)
{
	//OutputDebugString(_T("\n\nProcessMessage\n\n"));

    HRESULT hr = S_OK;

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);


	switch (eMessage)
	{
		case MFT_MESSAGE_COMMAND_FLUSH:
		case MFT_MESSAGE_COMMAND_DRAIN:
			// Flush the MFT - release all samples in it and reset the state
			m_decoder.flush();
			m_pSample = NULL;
		break;

		case MFT_MESSAGE_SET_D3D_MANAGER:
			if(ulParam == NULL) //fallback to SW decoding
			{
				if(m_h3dDevice != NULL)
				{
					HRESULT hr = m_p3DDeviceManager->CloseDeviceHandle(m_h3dDevice);
					if(FAILED(hr))
					{
						//log
					}
					m_h3dDevice = NULL;
				}
				m_pdxVideoDecoderService.Release();
				m_pdxVideoDecoderService = NULL;
				m_p3DDeviceManager.Release();
				m_p3DDeviceManager = NULL;

				m_decoder.setDecoderStrategy(new cpu_decoder_impl());
			}
			else //get the pointer to the Direct3D device manger
			{
				((IUnknown*)ulParam)->QueryInterface(IID_IDirect3DDeviceManager9, (void**)&m_p3DDeviceManager);
				BREAK_ON_NULL(m_p3DDeviceManager, E_POINTER);
			
				hr = m_p3DDeviceManager->OpenDeviceHandle(&m_h3dDevice);
				BREAK_ON_FAIL(hr);

			    // Get the video processor service 
			    hr = m_p3DDeviceManager->GetVideoService(m_h3dDevice, IID_PPV_ARGS(&m_pdxVideoDecoderService));
				if(FAILED(hr) && hr == DXVA2_E_NEW_VIDEO_DEVICE)
				{
					hr = m_p3DDeviceManager->OpenDeviceHandle(&m_h3dDevice);
					BREAK_ON_FAIL(hr);

					// Get the video processor service 
					hr = m_p3DDeviceManager->GetVideoService(m_h3dDevice, IID_PPV_ARGS(&m_pdxVideoDecoderService));
				}

				m_decoder.setDecoderStrategy(new hw_decoder_impl());
			}
		break;

		case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
		case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
			// Extract the subtype to make sure that the subtype is one that we support
		    GUID subtype;
		    hr = m_pInputType->GetGUID(MF_MT_SUBTYPE, &subtype);
		    BREAK_ON_FAIL(hr);

		    // verify that the specified media type has one of the acceptable subtypes -
		    // this filter will accept only H.264/HEVC compressed subtypes.
		    if(InlineIsEqualGUID(subtype, MFVideoFormat_H264) == TRUE)
		    {
			    m_decoder.init("H264");
		    }
			else if(InlineIsEqualGUID(subtype,MFVideoFormat_H265) == TRUE || InlineIsEqualGUID(subtype,MFVideoFormat_HEVC) == TRUE )
			{
				m_decoder.init("HEVC");
			}
			m_sampleTime = 0;
		break;

		case MFT_MESSAGE_NOTIFY_END_STREAMING:
		case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
			m_decoder.release();
		break;

		default:
		break;
	}
    return hr;
}



//
// Receive and process an input sample.
//
HRESULT FFmpegMFT::ProcessInput(
    DWORD               dwInputStreamID,
    IMFSample*          pSample,
    DWORD               dwFlags)
{
	//OutputDebugString(_T("\n\nProcessInput\n\n"));

    HRESULT hr = S_OK;
    DWORD dwBufferCount = 0;

    do
    {
        // lock the MFT
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pSample, E_POINTER);

        // This MFT accepts only a single output sample at a time, and does not accept any
        // flags.
        if (dwInputStreamID != 0 || dwFlags != 0)
        {
            hr = E_INVALIDARG;
            break;
        }

        // Both input and output media types must be set in order for the MFT to function.
        BREAK_ON_NULL(m_pInputType, MF_E_NOTACCEPTING);
        BREAK_ON_NULL(m_pOutputType, MF_E_NOTACCEPTING);

        // The MFT already has a sample that has not yet been processed.
        if(m_pSample != NULL)
        {
            hr = MF_E_NOTACCEPTING;
            break;
        }

        // Store the sample for later processing.
        m_pSample = pSample;
    }
    while(false);

    
    return hr;
}



//
// Get an output sample from the MFT.
//
HRESULT FFmpegMFT::ProcessOutput(
    DWORD                   dwFlags,
    DWORD                   cOutputBufferCount,
    MFT_OUTPUT_DATA_BUFFER* pOutputSampleBuffer,
    DWORD*                  pdwStatus)
{
	//OutputDebugString(_T("\n\nProcessOutput\n\n"));

    HRESULT hr = S_OK;

    do
    {
        // lock the MFT
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pOutputSampleBuffer, E_POINTER);
        BREAK_ON_NULL(pdwStatus, E_POINTER);

        // This MFT accepts only a single output sample at a time, and does
        // not accept any flags.
        if (cOutputBufferCount != 1  ||  dwFlags != 0)
        {
            hr = E_INVALIDARG;
            break;
        }

        // If we don't have an input sample, we need some input before
        // we can generate any output - return a flag indicating that more 
        // input is needed.
        BREAK_ON_NULL(m_pSample, MF_E_TRANSFORM_NEED_MORE_INPUT);

		//input to buffer
		CComPtr<IMFMediaBuffer> pInputMediaBuffer;
		hr = m_pSample->ConvertToContiguousBuffer(&pInputMediaBuffer);
		BREAK_ON_FAIL(hr);

		//output sample/buffer
		IMFSample* outputSample = NULL;
		CComPtr<IMFMediaBuffer> pOutputMediaBuffer = NULL;

		if(m_p3DDeviceManager == NULL){ //sw decoding , evr will provide the samples			
			outputSample = pOutputSampleBuffer[0].pSample;			
			hr = outputSample->GetBufferByIndex(0, &pOutputMediaBuffer);
			BREAK_ON_FAIL(hr);

			///do the decoding
			hr = decode(pInputMediaBuffer,pOutputMediaBuffer);
			BREAK_ON_FAIL(hr);
		}else{
			//hr = m_p3DDeviceManager->TestDevice(m_h3dDevice);
			//if(hr == DXVA2_E_NEW_VIDEO_DEVICE)
			//{
			//	/* https://docs.microsoft.com/en-us/windows/desktop/medfound/supporting-dxva-2-0-in-media-foundation#decoding
			//	1. Close the device handle by calling IDirect3DDeviceManager9::CloseDeviceHandle.
			//	2. Release the IDirectXVideoDecoderService and IDirectXVideoDecoder pointers.
			//	3. Open a new device handle.
			//	4. Negotiate a new decoder configuration, as described in "Finding a Decoder Configuration" earlier on this page.
			//	5. Create a new decoder device.
			//	 */
			//	BREAK_ON_FAIL(hr);
			//}

			CComPtr<IMFSample> outSample;

			UINT32 uiWidthInPixels, uiHeightInPixels;
			hr = MFGetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, &uiWidthInPixels, &uiHeightInPixels);
			BREAK_ON_FAIL(hr);
			//now let's allocate uncompressed buffers for the outputs
			
			hr = m_pdxVideoDecoderService->CreateSurface(
				uiWidthInPixels,
				uiHeightInPixels,
				0,
				static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')),
				D3DPOOL_DEFAULT,
				0,
				DXVA2_VideoDecoderRenderTarget,
				&m_surface,
				NULL);

			BREAK_ON_FAIL(hr);

			hr = MFCreateVideoSampleFromSurface(m_surface, &outSample);
			BREAK_ON_FAIL(hr);

			outputSample = outSample;			
			hr = outputSample->GetBufferByIndex(0, &pOutputMediaBuffer);
			BREAK_ON_FAIL(hr);

			pOutputSampleBuffer[0].pSample = outputSample;
			pOutputSampleBuffer[0].pSample->AddRef();

			///do the decoding
			hr = decode(pInputMediaBuffer,pOutputMediaBuffer);
			BREAK_ON_FAIL(hr);
		}	

		LONGLONG sampleTime;
		hr = m_pSample->GetSampleTime(&sampleTime);
		BREAK_ON_FAIL(hr);
		LONGLONG sampleDuration;
		hr = m_pSample->GetSampleDuration(&sampleDuration);
		BREAK_ON_FAIL(hr);
		
		hr = outputSample->SetSampleTime(m_sampleTime);
		BREAK_ON_FAIL(hr);
		
		m_sampleTime += sampleDuration;

		m_pSample.Release();

		/*wchar_t buf[1024];
		wsprintf(buf,_T("sampleTime %I64d (%I64d) sampleDuration %I64d\n"),sampleTime, m_sampleTime, sampleDuration);
    	OutputDebugString(buf);*/
		DebugOut((L"sampleTime %I64d (%I64d) sampleDuration %I64d\n"),sampleTime, m_sampleTime, sampleDuration);

        // Set status flags for output
        pOutputSampleBuffer[0].dwStatus = 0;
        *pdwStatus = 0;
    }
    while(false);

    return hr;
}


HRESULT FFmpegMFT::decode(IMFMediaBuffer* inputMediaBuffer, IMFMediaBuffer* pOutputMediaBuffer)
{
	HRESULT hr = S_OK;

	CBufferLock videoBuffer(pOutputMediaBuffer);

	BYTE *pIn = NULL;
	DWORD lenIn = 0;

	BYTE *pOut = NULL;
    LONG lDefaultStride = 0;
    LONG lActualStride = 0;

	hr = GetDefaultStride(m_pOutputType, &lDefaultStride);

	UINT32 width, height;
	if (SUCCEEDED(hr))
    {		
		hr = MFGetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, &width, &height);
    }

    if (SUCCEEDED(hr) && m_p3DDeviceManager == NULL)
    {
        hr = videoBuffer.LockBuffer(lDefaultStride, height, &pOut, &lActualStride);
    }
	else if(SUCCEEDED(hr))
	{
		pOut = (BYTE*)m_surface;
	}

	if (SUCCEEDED(hr))
    {
        hr = inputMediaBuffer->Lock(&pIn, NULL, &lenIn);
    }

	bool bret = m_decoder.decode(pIn,lenIn,(void*&)pOut,lActualStride);

	hr = inputMediaBuffer->Unlock();

	//return bret ? hr : MF_E_UNEXPECTED;
	return hr;
}

//
// Construct and return a partial media type with the specified index from the list of media
// types supported by this MFT.
//
HRESULT FFmpegMFT::GetSupportedInputMediaType(
    DWORD           dwTypeIndex, 
    IMFMediaType**  ppMT)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> pmt;

    do
    {
        // create a new media type object
        hr = MFCreateMediaType(&pmt);
        BREAK_ON_FAIL(hr);

        // set the major type of the media type to video
        hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        BREAK_ON_FAIL(hr);

        if(dwTypeIndex == 0) //H.264
        {
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        }
		else if(dwTypeIndex == 1) //HEVC
        {
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H265); //MFVideoFormat_HEVC
        }
        else 
        { 
            // if we don't have any more media types, return an error signifying
            // that there is no media type with that index
            hr = MF_E_NO_MORE_TYPES;
        }
        BREAK_ON_FAIL(hr);

		

        // detach the underlying IUnknown pointer from the pmt CComPtr without
        // releasing the pointer so that we can return that object to the caller.
        *ppMT = pmt.Detach();
    }
    while(false);

    return hr;
}


// Construct and return a partial media type with the specified index from the list of media
// types supported by this MFT.
//
HRESULT FFmpegMFT::GetSupportedOutputMediaType(
    DWORD           dwTypeIndex, 
    IMFMediaType**  ppMT)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaType> pmt;

    do
    {
        // create a new media type object
        hr = MFCreateMediaType(&pmt);
        BREAK_ON_FAIL(hr);

        // set the major type of the media type to video
        hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        BREAK_ON_FAIL(hr);

        // set the subtype of the video type by index.  The indexes of the media types
        // that are supported by this filter are:  0 - UYVY, 1 - NV12
      /* if(dwTypeIndex == 0)
        {
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_UYVY);
        }*/
    ///*    else*/
    /*	if(dwTypeIndex == 0)
        {
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_NV12);
        } */
		if(dwTypeIndex == 0)
        {
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_YV12);
        }
        else 
        { 
            // if we don't have any more media types, return an error signifying
            // that there is no media type with that index
            hr = MF_E_NO_MORE_TYPES;
        }
        BREAK_ON_FAIL(hr);

		//Find width and height from the input type
		UINT32 width, height;
		hr = MFGetAttributeSize(m_pInputType, MF_MT_FRAME_SIZE, &width, &height);
		BREAK_ON_FAIL(hr);

		hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		BREAK_ON_FAIL(hr);

		hr = MFSetAttributeSize(pmt, MF_MT_FRAME_SIZE, width, height);
		BREAK_ON_FAIL(hr);

		hr = pmt->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		BREAK_ON_FAIL(hr);

        // detach the underlying IUnknown pointer from the pmt CComPtr without
        // releasing the pointer so that we can return that object to the caller.
        *ppMT = pmt.Detach();
    }
    while(false);

    return hr;
}


//
// Description: Validates a media type for this transform.
//
HRESULT FFmpegMFT::CheckOutputMediaType(IMFMediaType* pmt)
{
    GUID majorType = GUID_NULL;
    GUID subtype = GUID_NULL;
    MFVideoInterlaceMode interlacingMode = MFVideoInterlace_Unknown;
    HRESULT hr = S_OK;

    // store the media type pointer in the CComPtr so that it's reference counter
    // is incremented and decrimented properly.
    CComPtr<IMFMediaType> pType = pmt;

    do
    {
        BREAK_ON_NULL(pType, E_POINTER);
		BREAK_ON_NULL(m_pInputType, MF_E_TRANSFORM_TYPE_NOT_SET); // Input type must be set first.

        // Extract the major type to make sure that the major type is video
        hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
        BREAK_ON_FAIL(hr);

        if (majorType != MFMediaType_Video)
        {
            hr = MF_E_INVALIDMEDIATYPE;
            break;
        }


        // Extract the subtype to make sure that the subtype is one that we support
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        BREAK_ON_FAIL(hr);

        // verify that the specified media type has one of the acceptable subtypes -
        // this filter will accept only NV12 and UYVY uncompressed subtypes.
        if(subtype != MEDIASUBTYPE_NV12 && subtype != MEDIASUBTYPE_UYVY && subtype != MEDIASUBTYPE_YV12)
        {
            hr = MF_E_INVALIDMEDIATYPE;
            break;
        }

        // get the requested interlacing format
        hr = pType->GetUINT32(MF_MT_INTERLACE_MODE, (UINT32*)&interlacingMode);
        BREAK_ON_FAIL(hr);

        // since we want to deal only with progressive (non-interlaced) frames, make sure
        // we fail if we get anything but progressive
        if (interlacingMode != MFVideoInterlace_Progressive)
        {
            hr = MF_E_INVALIDMEDIATYPE;
            break;
        }
    }
    while(false);

    return hr;
}


//
// Description: Validates a media type for this transform.
//
HRESULT FFmpegMFT::CheckInputMediaType(IMFMediaType* pmt)
{
    GUID majorType = GUID_NULL;
    GUID subtype = GUID_NULL;
    MFVideoInterlaceMode interlacingMode = MFVideoInterlace_Unknown;
    HRESULT hr = S_OK;

    // store the media type pointer in the CComPtr so that it's reference counter
    // is incremented and decrimented properly.
    CComPtr<IMFMediaType> pType = pmt;

    do
    {
        BREAK_ON_NULL(pType, E_POINTER);

        // Extract the major type to make sure that the major type is video
        hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
        BREAK_ON_FAIL(hr);

        if (majorType != MFMediaType_Video)
        {
            hr = MF_E_INVALIDMEDIATYPE;
            break;
        }


        // Extract the subtype to make sure that the subtype is one that we support
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        BREAK_ON_FAIL(hr);

        // verify that the specified media type has one of the acceptable subtypes -
        // this filter will accept only H.264/HEVC compressed subtypes.
        if(InlineIsEqualGUID(subtype, MFVideoFormat_H264) != TRUE && 
			InlineIsEqualGUID(subtype,MFVideoFormat_H265) != TRUE &&
			InlineIsEqualGUID(subtype,MFVideoFormat_HEVC) != TRUE )
        {
            hr = MF_E_INVALIDMEDIATYPE;
            break;
        }

		// get the requested interlacing format
       /* hr = pType->GetUINT32(MF_MT_INTERLACE_MODE, (UINT32*)&interlacingMode);
        BREAK_ON_FAIL(hr);*/

		//Find fps from the input type
		UINT32 framerate_n, framerate_d;
		hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &framerate_n, &framerate_d);
		BREAK_ON_FAIL(hr);

		//Find width and height from the input type
		UINT32 uiWidthInPixels, uiHeightInPixels;
		hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &uiWidthInPixels, &uiHeightInPixels);
		BREAK_ON_FAIL(hr);

		//Disable Supporting DXVA 2.0
		if(m_pdxVideoDecoderService != NULL){
			hr = MF_E_UNSUPPORTED_D3D_TYPE;
		}

		//Supporting DXVA 2.0 - Finding a Decoder Configuration
		//if(m_pdxVideoDecoderService != NULL)
		//{
		//	bool bFindDecoderGuid = false;
		//	GUID guid;
		//	GUID* pGuids = NULL;
		//	UINT uiCount = 0;
		//	hr = m_pdxVideoDecoderService->GetDecoderDeviceGuids(&uiCount,&pGuids);
		//	if(FAILED(hr)){
		//		return MF_E_UNSUPPORTED_D3D_TYPE;
		//	}

		//	for(UINT ui = 0; ui < uiCount; ui++){
		//		guid = pGuids[ui];
		//		if(InlineIsEqualGUID(subtype, MFVideoFormat_H264) != TRUE ){
		//			
		//		}
		//		
		//		if(InlineIsEqualGUID(subtype,MFVideoFormat_H265) != TRUE ||
		//			InlineIsEqualGUID(subtype,MFVideoFormat_HEVC) != TRUE){
		//			if(InlineIsEqualGUID(guid,DXVA2_ModeHEVC_VLD_Main) || InlineIsEqualGUID(guid,DXVA2_ModeHEVC_VLD_Main10)){
		//				bFindDecoderGuid = true;
		//				break;
		//			}
		//		}
		//	}			

		//	if(bFindDecoderGuid){

		//		if(m_pRenderTargetFormats){
		//			CoTaskMemFree(m_pRenderTargetFormats);
		//			m_pRenderTargetFormats = NULL;
		//		}

		//		hr = m_pdxVideoDecoderService->GetDecoderRenderTargets(guid, &uiCount, &m_pRenderTargetFormats);
		//		if(FAILED(hr)){
		//			CoTaskMemFree(pGuids);
		//			return MF_E_UNSUPPORTED_D3D_TYPE;
		//		}

		//		if(m_pConfigs){
		//			CoTaskMemFree(m_pConfigs);
		//			m_pConfigs = NULL;
		//		}

		//		memset(&m_Dxva2Desc, 0, sizeof(DXVA2_VideoDesc));

		//		DXVA2_Frequency Dxva2Freq;
		//		Dxva2Freq.Numerator = framerate_n;
		//		Dxva2Freq.Denominator = framerate_d;

		//		m_Dxva2Desc.SampleWidth = uiWidthInPixels;
		//		m_Dxva2Desc.SampleHeight = uiHeightInPixels;

		//		m_Dxva2Desc.SampleFormat.SampleFormat = interlacingMode;

		//		m_Dxva2Desc.Format = static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2'));
		//		m_Dxva2Desc.InputSampleFreq = Dxva2Freq;
		//		m_Dxva2Desc.OutputFrameFreq = Dxva2Freq;

		//		hr = m_pdxVideoDecoderService->GetDecoderConfigurations(guid, &m_Dxva2Desc, NULL, &uiCount, &m_pConfigs);
		//		if(FAILED(hr)){
		//			CoTaskMemFree(pGuids);
		//			return MF_E_UNSUPPORTED_D3D_TYPE;
		//		}
		//	}

		//	//now let's allocate uncompressed buffers for the outputs
		//	LPDIRECT3DSURFACE9 surface_list[NUM_DIRECT3D_SURFACE];
		//	hr = m_pdxVideoDecoderService->CreateSurface(
		//		uiWidthInPixels,
		//		uiHeightInPixels,
		//		NUM_DIRECT3D_SURFACE - 1,
		//		static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')),
		//		D3DPOOL_DEFAULT,
		//		0,
		//		DXVA2_VideoDecoderRenderTarget,
		//		surface_list,
		//		NULL);

		//	if(FAILED(hr)){
		//		CoTaskMemFree(pGuids);
		//		return MF_E_UNSUPPORTED_D3D_TYPE;
		//	}

		//	for(UINT ui = 0; ui < NUM_DIRECT3D_SURFACE; ui++){
		//		hr = MFCreateVideoSampleFromSurface(surface_list[ui], &m_pSampleOut[ui]);
		//	}

		//	if(FAILED(hr)){
		//		CoTaskMemFree(pGuids);
		//		return MF_E_UNSUPPORTED_D3D_TYPE;
		//	}

		//	hr = m_pdxVideoDecoderService->CreateVideoDecoder(guid,
		//		&m_Dxva2Desc,
		//		&m_pConfigs[0],
		//		surface_list,
		//		NUM_DIRECT3D_SURFACE,
		//		&m_pVideoDecoder);

		//	if(FAILED(hr)){
		//		CoTaskMemFree(pGuids);
		//		return MF_E_UNSUPPORTED_D3D_TYPE;
		//	}

		//	CoTaskMemFree(pGuids);
		//}
    }
    while(false);

    return hr;
}





