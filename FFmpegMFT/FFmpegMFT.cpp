#include "stdafx.h"
#include "FFmpegMFT.h"

#include <evr.h>

#include "CBufferLock.h"
#include "Utils.h"
#include "Logger.h"
#include "cpu_decoder_impl.h"
#include "hw_decoder_impl.h"

//#include <VersionHelpers.h>

#if 0
	#include "hybrid_decoder_impl.h"
	#define HYBRID_DECODER
#endif

FFmpegMFT::FFmpegMFT(void) :
    m_cRef(0),
	m_sampleTime(0),
	m_hD3d9Device(NULL),
	m_pConfigs(NULL),
	m_pRenderTargetFormats(NULL),
	m_uiNumOfRenderTargetFormats(0),
	m_unEffectiveFrameWidth(-1),
	m_unEffectiveFrameHeight(-1),
	m_bEffectiveResMatch(true)
{
	Logger::getInstance().LogDebug("FFmpegMFT::FFmpegMFT");

	m_decoder.setDecoderStrategy(
#ifdef HYBRID_DECODER
					new hybrid_decoder_impl()
#else
					new cpu_decoder_impl()
#endif
		);
	m_pSampleOutMap.clear();
}

FFmpegMFT::~FFmpegMFT(void)
{
	Logger::getInstance().LogDebug("FFmpegMFT::~FFmpegMFT");
    // reduce the count of DLL handles so that we can unload the DLL when 
    // components in it are no longer being used
    InterlockedDecrement(&g_dllLockCount);

	if(m_pRenderTargetFormats != NULL){
		CoTaskMemFree(m_pRenderTargetFormats);
		m_pRenderTargetFormats = NULL;
	}

	if(m_pConfigs){
		CoTaskMemFree(m_pConfigs);
		m_pConfigs = NULL;
	}

	ReleaseSurfaces();
}

//
// IUnknown interface implementation
//
ULONG FFmpegMFT::AddRef()
{
	//Logger::getInstance().LogDebug("FFmpegMFT::AddRef %d", m_cRef);
    return InterlockedIncrement(&m_cRef);
}

ULONG FFmpegMFT::Release()
{
	//Logger::getInstance().LogDebug("FFmpegMFT::Release %d", m_cRef);

    ULONG refCount = InterlockedDecrement(&m_cRef);
    if (refCount == 0)
    {
        delete this;
    }
    
    return refCount;
}

HRESULT FFmpegMFT::QueryInterface(REFIID riid, void** ppv)
{
	//Logger::getInstance().LogDebug("FFmpegMFT::QueryInterface");

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetStreamLimits");

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetStreamCount");

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
	Logger::getInstance().Logger::getInstance().LogDebug("FFmpegMFT::GetStreamIDs");
    return E_NOTIMPL;
}

//
// Get a structure with information about an input stream with the specified index.
//
HRESULT FFmpegMFT::GetInputStreamInfo(
    DWORD                   dwInputStreamID,  // stream being queried.
    MFT_INPUT_STREAM_INFO*  pStreamInfo)      // stream information
{
	Logger::getInstance().LogDebug("FFmpegMFT::GetInputStreamInfo");

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
        }
		BREAK_ON_FAIL(hr);

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetOutputStreamInfo");
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
        }
		BREAK_ON_FAIL(hr);

        // The dwFlags variable contains a set of flags indicating how the MFT behaves.
        // The flags shown below indicate the following:
        //   - MFT provides samples with whole units of data.  This means that each sample 
        //     contains a whole uncompressed frame.
        //   - The samples returned will have only a single buffer.

        //   - only on HW - The MFT provides samples and there is no need to give it output samples to 
        //     fill in during its ProcessOutput() calls.
		pStreamInfo->dwFlags =
	            MFT_OUTPUT_STREAM_WHOLE_SAMPLES |                
	            MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
				MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;

		if(m_pdxVideoDecoderService != NULL)
		{
	        pStreamInfo->dwFlags |= 
	            MFT_OUTPUT_STREAM_PROVIDES_SAMPLES ;
		}		

        // the cbAlignment variable contains information about byte alignment of the sample 
        // buffers, if one is needed.  Zero indicates that no specific alignment is needed.
        pStreamInfo->cbAlignment = 0;

        // Size of the samples returned by the MFT.  Since the MFT provides its own samples,
        // this value must be zero.
        pStreamInfo->cbSize = 0;

		if(m_pOutputType && m_pdxVideoDecoderService == NULL)
		{
			UINT32 width, height;
			hr = MFGetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, &width, &height);
			BREAK_ON_FAIL(hr);

			pStreamInfo->cbSize = (height + (height / 2)) * width;
		}
    }
    while(false);

    return hr;
}


HRESULT FFmpegMFT::GetAttributes(IMFAttributes** pAttributes)
{
	Logger::getInstance().LogDebug("FFmpegMFT::GetAttributes");
    
	HRESULT hr = S_OK;
    CComPtr<IMFAttributes> att;

    do
    {
        CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

        BREAK_ON_NULL(pAttributes, E_POINTER);
		hr = MFCreateAttributes(&att, 1);
		BREAK_ON_FAIL(hr);

		hr = att->SetUINT32(MF_SA_D3D_AWARE, TRUE);
		BREAK_ON_FAIL(hr);

		//when need to support DX11
		/*if(IsWindows8OrGreater())
		{
			hr = att->SetUINT32(MF_SA_D3D11_AWARE, TRUE);
			BREAK_ON_FAIL(hr);
		}*/

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetInputStreamAttributes");
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
	Logger::getInstance().LogDebug("FFmpegMFT::GetOutputStreamAttributes");
    // This MFT does not support any attributes, so the method is not implemented.
    return E_NOTIMPL;
}



//
// Deletes the specified input stream from the MFT.  This MFT has only a single 
// constant stream, and therefore this method is not implemented.
//
HRESULT FFmpegMFT::DeleteInputStream(DWORD dwStreamID)
{
	Logger::getInstance().LogDebug("FFmpegMFT::DeleteInputStream");
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
	Logger::getInstance().LogDebug("FFmpegMFT::AddInputStreams");
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
	Logger::getInstance().LogDebug("FFmpegMFT::GetInputAvailableType");

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
		if(hr == MF_E_NO_MORE_TYPES)
		{
			break;
		}

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetOutputAvailableType");

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
	Logger::getInstance().LogDebug("FFmpegMFT::SetInputType");

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
	Logger::getInstance().LogDebug("FFmpegMFT::SetOutputType");

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
        }
		BREAK_ON_FAIL(hr);	

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetInputCurrentType");

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetOutputCurrentType");

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
	Logger::getInstance().LogDebug("FFmpegMFT::GetInputStatus");

	HRESULT hr = S_OK;

	do
	{
		CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

		BREAK_ON_NULL(pdwFlags, E_POINTER);

		// the MFT supports only a single stream.
		if (dwInputStreamID != 0)
		{
			hr = MF_E_INVALIDSTREAMNUMBER;
		}
		BREAK_ON_FAIL(hr);

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
	}
	while (false);

    return hr;
}


//
// Get the status of the output stream of the MFT - IE verify whether there
// is a sample ready in the MFT.  This method can be left unimplemented.
//
HRESULT FFmpegMFT::GetOutputStatus(DWORD* pdwFlags)
{
	Logger::getInstance().LogDebug("FFmpegMFT::GetOutputStatus");
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
	Logger::getInstance().LogDebug("FFmpegMFT::SetOutputBounds");
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
	Logger::getInstance().LogDebug("FFmpegMFT::ProcessEvent");

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
	Logger::getInstance().LogDebug("FFmpegMFT::ProcessMessage");

    HRESULT hr = S_OK;

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

	switch (eMessage)
	{
		case MFT_MESSAGE_COMMAND_FLUSH:
			Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_COMMAND_FLUSH");
			m_pSample = NULL;
		break;

		case MFT_MESSAGE_COMMAND_DRAIN:
			// Flush the MFT - release all samples in it and reset the state
			Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_COMMAND_DRAIN");			
		break;

		case MFT_MESSAGE_SET_D3D_MANAGER:
			if(ulParam == NULL) //fallback to SW decoding
			{
				Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_SET_D3D_MANAGER - fallback to SW decoding");
				if(m_hD3d9Device != NULL && m_pDirect3DDeviceManager9 != NULL)
				{
					HRESULT hr = m_pDirect3DDeviceManager9->CloseDeviceHandle(m_hD3d9Device);
					if(FAILED(hr))
					{
						Logger::getInstance().LogWarn("ProcessMessage - MFT_MESSAGE_SET_D3D_MANAGER - failed to close device handle");
					}
					m_hD3d9Device = NULL;
				}
				m_pdxVideoDecoderService.Release();
				m_pdxVideoDecoderService = NULL;

				m_pDirect3DDeviceManager9.Release();
				m_pDirect3DDeviceManager9 = NULL;

				m_decoder.setDecoderStrategy(
#ifdef HYBRID_DECODER
					new hybrid_decoder_impl()
#else
					new cpu_decoder_impl()
#endif
				);
			}
			else //get the pointer to the Direct3D device manger
			{
				Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_SET_D3D_MANAGER - HW decoding");

				//validation
				if(m_pDirect3DDeviceManager9 == NULL && m_hD3d9Device == NULL && m_pdxVideoDecoderService == NULL)
				{
					((IUnknown*)ulParam)->QueryInterface(IID_IDirect3DDeviceManager9, (void**)&m_pDirect3DDeviceManager9);
					BREAK_ON_NULL(m_pDirect3DDeviceManager9, E_POINTER);
				
					hr = m_pDirect3DDeviceManager9->OpenDeviceHandle(&m_hD3d9Device);
					BREAK_ON_FAIL(hr);

				    // Get the video processor service 
				    hr = m_pDirect3DDeviceManager9->GetVideoService(m_hD3d9Device, IID_PPV_ARGS(&m_pdxVideoDecoderService));
					if(FAILED(hr) && hr == DXVA2_E_NEW_VIDEO_DEVICE)
					{
						hr = m_pDirect3DDeviceManager9->OpenDeviceHandle(&m_hD3d9Device);
						BREAK_ON_FAIL(hr);

						// Get the video processor service 
						hr = m_pDirect3DDeviceManager9->GetVideoService(m_hD3d9Device, IID_PPV_ARGS(&m_pdxVideoDecoderService));
					}
					BREAK_ON_FAIL(hr);

					m_decoder.setDecoderStrategy(new hw_decoder_impl(m_pDirect3DDeviceManager9));
				}
				else
				{
					Logger::getInstance().LogWarn("ProcessMessage - MFT_MESSAGE_SET_D3D_MANAGER - already initialize Direct3D manager!");
					hr = MF_E_UNEXPECTED;
				}
			}
		break;

		case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
			Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
			// Extract the subtype to make sure that the subtype is one that we support
		    GUID subtype_in, subtype_out;
		    hr = m_pInputType->GetGUID(MF_MT_SUBTYPE, &subtype_in);
		    BREAK_ON_FAIL(hr);

			hr = m_pOutputType->GetGUID(MF_MT_SUBTYPE, &subtype_out);
		    BREAK_ON_FAIL(hr);

			ReleaseSurfaces();
			//Release the old decoder
			m_decoder.release();
		    // verify that the specified media type has one of the acceptable subtypes -
		    // this filter will accept only H.264/HEVC compressed subtypes.
		    if(InlineIsEqualGUID(subtype_in, MFVideoFormat_H264) == TRUE)
		    {
				Logger::getInstance().LogInfo("ProcessMessage - Create H264 Decoder");
			    hr = m_decoder.init("H264", subtype_out.Data1) != true? S_FALSE : S_OK;
		    }
			else if(InlineIsEqualGUID(subtype_in,MFVideoFormat_H265) == TRUE || InlineIsEqualGUID(subtype_in,MFVideoFormat_HEVC) == TRUE )
			{
				Logger::getInstance().LogInfo("ProcessMessage - Create HEVC Decoder");
				hr = m_decoder.init("HEVC", subtype_out.Data1) != true? S_FALSE : S_OK;
			}

			BREAK_ON_FAIL(hr);
		break;

		case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
			Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_NOTIFY_START_OF_STREAM");
			m_bEffectiveResMatch = true;
			m_sampleTime = 0;
		break;

		case MFT_MESSAGE_NOTIFY_END_STREAMING:
			Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_NOTIFY_END_STREAMING");
		break;

		case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
			Logger::getInstance().LogInfo("ProcessMessage - MFT_MESSAGE_NOTIFY_END_OF_STREAM");
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
	Logger::getInstance().LogDebug("FFmpegMFT::ProcessInput");

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
			BREAK_ON_FAIL(hr);
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
	Logger::getInstance().LogDebug("FFmpegMFT::ProcessOutput");

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);

    HRESULT hr = S_OK;
	DWORD pOutputSampleBufferStatus = 0;

    do
    {
        BREAK_ON_NULL(pOutputSampleBuffer, E_POINTER);
        BREAK_ON_NULL(pdwStatus, E_POINTER);

        // This MFT accepts only a single output sample at a time, and does
        // not accept any flags.
        if (cOutputBufferCount != 1  ||  dwFlags != 0)
        {
            hr = E_INVALIDARG;
			BREAK_ON_FAIL(hr);
        }        

        // If we don't have an input sample, we need some input before
        // we can generate any output - return a flag indicating that more 
        // input is needed.
        BREAK_ON_NULL(m_pSample, MF_E_TRANSFORM_NEED_MORE_INPUT);

		//start count the time consuming to decode
		auto t1 = std::chrono::steady_clock::now();

		//input to buffer
		CComPtr<IMFMediaBuffer> pInputMediaBuffer;
		hr = m_pSample->ConvertToContiguousBuffer(&pInputMediaBuffer);
		BREAK_ON_FAIL(hr);

		//output sample/buffer
		IMFSample* outputSample = NULL;
		CComPtr<IMFMediaBuffer> pOutputMediaBuffer = NULL;

		if(m_pDirect3DDeviceManager9 == NULL){ //sw decoding , evr will provide the samples			
			outputSample = pOutputSampleBuffer[0].pSample;			
			hr = outputSample->GetBufferByIndex(0, &pOutputMediaBuffer);
			BREAK_ON_FAIL(hr);

			DWORD pcbMaxLength;
			hr = pOutputMediaBuffer->GetMaxLength(&pcbMaxLength);
			BREAK_ON_FAIL(hr);

			if(pcbMaxLength <= 0)
			{
				hr = MF_E_INSUFFICIENT_BUFFER;
				Logger::getInstance().LogError("FFmpegMFT::ProcessOutput output buffer has 0 size! can't decode on this buffer.");
				BREAK_ON_FAIL(hr);
			}

			//do the decoding
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

			IDirect3DSurface9* pSurface = NULL;

			//do the decoding
			hr = decode(pInputMediaBuffer,&pSurface);
			BREAK_ON_FAIL(hr);

			if(pSurface == NULL) 
			{
				ReleaseSurfaces();
				break;
			}

			D3DSURFACE_DESC desc;
			hr = pSurface->GetDesc(&desc);
			BREAK_ON_FAIL(hr);

			UINT32 width, height;
			hr = MFGetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, &width, &height);
			BREAK_ON_FAIL(hr);
			
			if(m_bEffectiveResMatch == true && (width != desc.Width || height != desc.Height))
			{
				Logger::getInstance().LogInfo("FFmpegMFT::ProcessOutput The format has changed on an output stream, effective resolution is %dX%d for video resolution %dX%d ", desc.Width, desc.Height, width, height);
				pOutputSampleBufferStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;
				m_unEffectiveFrameWidth = desc.Width;
				m_unEffectiveFrameHeight = desc.Height;
				m_bEffectiveResMatch = false;
				hr = MF_E_TRANSFORM_STREAM_CHANGE;
				break;
			}

			SampleToSurfaceMapIter it = m_pSampleOutMap.find(pSurface);
			if(it != m_pSampleOutMap.end())
			{
				outputSample = it->second;
			}
			else
			{
				hr = MFCreateVideoSampleFromSurface(pSurface, &outputSample);
				BREAK_ON_FAIL(hr);

				m_pSampleOutMap.insert(pair<IDirect3DSurface9*,IMFSample*>(pSurface, outputSample));
				Logger::getInstance().LogDebug("FFmpegMFT::ProcessOutput Add new surface/VideoSample to map surface=0x%x, sample=0x%x", pSurface, outputSample);
			}			

			pOutputSampleBuffer[0].pSample = outputSample;
			pOutputSampleBuffer[0].pSample->AddRef();

			Logger::getInstance().LogDebug("FFmpegMFT::ProcessOutput surface=0x%x, sample=0x%x", pSurface, outputSample);
		}

		//ends counting the decoding process
		auto t2 = std::chrono::steady_clock::now();

	    const LONGLONG duration100Nano = (LONGLONG)floor(std::chrono::duration <double, std::nano> (t2-t1).count() / 100);

		/* Timing and duration handle here */
		LONGLONG sampleTime;
		hr = m_pSample->GetSampleTime(&sampleTime);
		BREAK_ON_FAIL(hr);

		LONGLONG sampleDuration;
		hr = m_pSample->GetSampleDuration(&sampleDuration);
		if(hr == MF_E_NO_SAMPLE_DURATION){ //Stream behavior

			//set output sample time
			hr = outputSample->SetSampleTime(sampleTime);
			BREAK_ON_FAIL(hr);			
#ifdef DEBUG		
    		DebugOut((L"S.t %10I64d DecodeTime= %6.3f\n"),sampleTime, (double)(duration100Nano* 100) / 1000000);
#endif

		} else { //File container behavior

			BREAK_ON_FAIL(hr);
			//set out sample duration
			hr = outputSample->SetSampleDuration(sampleDuration);
			BREAK_ON_FAIL(hr);
			sampleDuration = max(sampleDuration,duration100Nano);

			//set output sample time
			hr = outputSample->SetSampleTime(m_sampleTime);
			BREAK_ON_FAIL(hr);			
#ifdef DEBUG
    		DebugOut((L"S.t %10I64d S.d %10I64d t %10I64d d %10I64d DecodeTime= %6.3f\n"),sampleTime, sampleDuration, m_sampleTime, duration100Nano, (double)(duration100Nano* 100) / 1000000);
#endif
		
    		m_sampleTime += sampleDuration;
		}
    }
    while(false);

	m_pSample.Release();
		
    // Set status flags for output
    pOutputSampleBuffer[0].dwStatus = pOutputSampleBufferStatus;
    *pdwStatus = 0;

    return hr;
}


HRESULT FFmpegMFT::decode(IMFMediaBuffer* inputMediaBuffer, IMFMediaBuffer* pOutputMediaBuffer)
{
	Logger::getInstance().LogDebug("FFmpegMFT::decode SW");		

	bool bRet = true;
	HRESULT hr = S_OK;

	BYTE *pIn = NULL, *pOut = NULL;
	DWORD lenIn = 0;
    LONG lDefaultStride = 0, lActualStride = 0;
	UINT32 width = 0, height = 0;

	do
	{
		CBufferLock videoBuffer(pOutputMediaBuffer);

		hr = GetDefaultStride(m_pOutputType, &lDefaultStride);
		BREAK_ON_FAIL(hr);
	
		hr = MFGetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, &width, &height);
		BREAK_ON_FAIL(hr);

        hr = videoBuffer.LockBuffer(lDefaultStride, height, &pOut, &lActualStride);
		BREAK_ON_FAIL(hr);

        hr = inputMediaBuffer->Lock(&pIn, NULL, &lenIn);
		BREAK_ON_FAIL(hr);	    

		bRet = m_decoder.decode(pIn,lenIn,reinterpret_cast<void*&>(pOut),lActualStride);

		hr = inputMediaBuffer->Unlock();
		BREAK_ON_FAIL(hr);
	}
	while (false);	

	return bRet ? S_OK : MF_E_TRANSFORM_NEED_MORE_INPUT;
}

HRESULT FFmpegMFT::decode(IMFMediaBuffer* inputMediaBuffer, IDirect3DSurface9** ppSurface)
{
	Logger::getInstance().LogDebug("FFmpegMFT::decode HW");

	HRESULT hr = S_OK;
	bool bRet = true;

	BYTE *pIn = NULL;
	DWORD lenIn = 0;

	do
	{
		hr = inputMediaBuffer->Lock(&pIn, NULL, &lenIn);
		BREAK_ON_FAIL(hr);
		
		bRet = m_decoder.decode(pIn,lenIn, reinterpret_cast<void*&>(*ppSurface), 0);

		hr = inputMediaBuffer->Unlock();
		BREAK_ON_FAIL(hr);
	}
	while (false);   

	return bRet ? S_OK : MF_E_TRANSFORM_NEED_MORE_INPUT;
}


//
// Construct and return a partial media type with the specified index from the list of media
// types supported by this MFT.
//
HRESULT FFmpegMFT::GetSupportedInputMediaType(
    DWORD           dwTypeIndex, 
    IMFMediaType**  ppMT)
{
	Logger::getInstance().LogDebug("FFmpegMFT::GetSupportedInputMediaType");

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
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_HEVC); // MFVideoFormat_H265 from win SDK 10.X and up, now we will support 8.1 SDK
        }
        else 
        { 
            // if we don't have any more media types, return an error signifying
            // that there is no media type with that index
            return MF_E_NO_MORE_TYPES;
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
	Logger::getInstance().LogDebug("FFmpegMFT::GetSupportedOutputMediaType");

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

		//DXVA supported
		if(m_pdxVideoDecoderService != NULL)
		{
			//https://docs.microsoft.com/en-us/windows/desktop/medfound/video-subtype-guids#creating-subtype-guids-from-fourccs-and-d3dformat-values
			//preferred
			//hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);

			GUID format = MFVideoFormat_Base;			

			if(dwTypeIndex < m_uiNumOfRenderTargetFormats)
			{
				m_pRenderTargetFormats[dwTypeIndex];
				format.Data1 = m_pRenderTargetFormats[dwTypeIndex];
				hr = pmt->SetGUID(MF_MT_SUBTYPE, format);
			}
			else 
	        { 
	            // if we don't have any more media types, return an error signifying
	            // that there is no media type with that index
				Logger::getInstance().LogError("FFmpegMFT::GetSupportedOutputMediaType - type index (%d) is out of range. Render target formats length is %d", dwTypeIndex, m_uiNumOfRenderTargetFormats);
	            hr = MF_E_UNSUPPORTED_D3D_TYPE;
	        }
		}
		else
		{
			// set the subtype of the video type by index.  The indexes of the media types
	        // that are supported by this MFT are: 2 - YUY2, 1 - YV12, 0 - NV12
    		if(dwTypeIndex == 0)
	        {
	            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
	        } 
			else if(dwTypeIndex == 1)
	        {
	            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
	        }
			else if(dwTypeIndex == 2)
			{
				hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
			}
	        else 
	        { 
	            // if we don't have any more media types, return an error signifying
	            // that there is no media type with that index
	            hr = MF_E_NO_MORE_TYPES;
	        }
		}       
        BREAK_ON_FAIL(hr);

		//Find width and height from the input type
		UINT32 width, height;
		hr = MFGetAttributeSize(m_pInputType, MF_MT_FRAME_SIZE, &width, &height);
		BREAK_ON_FAIL(hr);

		if(m_bEffectiveResMatch != true)
		{
			//set also the geometric aperture to the preferred resolution
			const MFVideoArea area = MakeArea(0, 0, width, height);
			pmt->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area, sizeof(MFVideoArea));

			width = m_unEffectiveFrameWidth;
			height = m_unEffectiveFrameHeight;
			m_bEffectiveResMatch = true;			
		}

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
	Logger::getInstance().LogDebug("FFmpegMFT::CheckOutputMediaType");

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
        }
		BREAK_ON_FAIL(hr);

        // Extract the subtype to make sure that the subtype is one that we support
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        BREAK_ON_FAIL(hr);

		//DXVA supported
		if(m_pdxVideoDecoderService != NULL)
		{
			//https://docs.microsoft.com/en-us/windows/desktop/medfound/video-subtype-guids#creating-subtype-guids-from-fourccs-and-d3dformat-values

			GUID format = MFVideoFormat_Base;
			bool bFoundSupportedFormat = false;
			for(UINT i = 0; i < m_uiNumOfRenderTargetFormats; i++)
			{
				format.Data1 = m_pRenderTargetFormats[i];
				if(subtype == format)
				{
					bFoundSupportedFormat = true;
					break;
				}
			}
			
			if(!bFoundSupportedFormat)
	        { 
				Logger::getInstance().LogWarn("FFmpegMFT::CheckOutputMediaType - media type '%S' not supported by any of the render target formats",
					GetGUIDNameConst(subtype));
				for(UINT i = 0; i < m_uiNumOfRenderTargetFormats; i++)
				{
					format.Data1 = m_pRenderTargetFormats[i];
					Logger::getInstance().LogWarn("FFmpegMFT::CheckOutputMediaType - : '%S'",
						GetGUIDNameConst(format));
				}
				hr = MF_E_UNSUPPORTED_D3D_TYPE;
	        }
		}
		else
		{
			// verify that the specified media type has one of the acceptable subtypes -
	        // this filter will accept only NV12, YV12 and YUY2 uncompressed subtypes.
	        if(subtype != MFVideoFormat_NV12 && subtype != MFVideoFormat_YV12 && subtype != MFVideoFormat_YUY2)
	        {
	            hr = MF_E_INVALIDMEDIATYPE;
	        }
		}
		BREAK_ON_FAIL(hr);

        // get the requested interlacing format
        hr = pType->GetUINT32(MF_MT_INTERLACE_MODE, (UINT32*)&interlacingMode);
        BREAK_ON_FAIL(hr);

        // since we want to deal only with progressive (non-interlaced) frames, make sure
        // we fail if we get anything but progressive
        if (interlacingMode != MFVideoInterlace_Progressive)
        {
            hr = MF_E_INVALIDMEDIATYPE;
        }
		BREAK_ON_FAIL(hr);
    }
    while(false);

    return hr;
}


//
// Description: Validates a media type for this transform.
//
HRESULT FFmpegMFT::CheckInputMediaType(IMFMediaType* pmt)
{
	Logger::getInstance().LogDebug("FFmpegMFT::CheckInputMediaType");

    GUID majorType = GUID_NULL;
    GUID subtype = GUID_NULL;
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
        }
		BREAK_ON_FAIL(hr);

        // Extract the subtype to make sure that the subtype is one that we support
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        BREAK_ON_FAIL(hr);

        // verify that the specified media type has one of the acceptable subtypes -
        // this filter will accept only H.264/HEVC compressed subtypes.
        if(InlineIsEqualGUID(subtype, MFVideoFormat_H264) != TRUE && 
			InlineIsEqualGUID(subtype, MFVideoFormat_H265) != TRUE &&
			InlineIsEqualGUID(subtype, MFVideoFormat_HEVC) != TRUE )
        {
            hr = MF_E_INVALIDMEDIATYPE;
        }
        BREAK_ON_FAIL(hr);


		//Supporting DXVA 2.0 - 
    	//Finding a Decoder Configuration https://docs.microsoft.com/en-us/windows/desktop/medfound/supporting-dxva-2-0-in-media-foundation#finding-a-decoder-configuration
		if(m_pdxVideoDecoderService != NULL){
			bool bFindDecoderGuid = false;
			GUID decoderDeviceGuid;
			GUID* pGuids = NULL;
			UINT uiCount = 0;
			UINT ui;

			hr = m_pdxVideoDecoderService->GetDecoderDeviceGuids(&uiCount,&pGuids);
			if(FAILED(hr)){
				Logger::getInstance().LogWarn("FFmpegMFT::CheckInputMediaType failed to GetDecoderDeviceGuids, return code 0x%x", hr);
				hr = MF_E_UNSUPPORTED_D3D_TYPE;
			}
			BREAK_ON_FAIL(hr);

			for(ui = 0; ui < uiCount; ui++){
				decoderDeviceGuid = pGuids[ui];
				//H.264
				if(InlineIsEqualGUID(subtype, MFVideoFormat_H264) == TRUE ){
					if(	   InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_A) 
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_B)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_C)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_D)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_E)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_F)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_VLD_WithFMOASO_NoFGT)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_VLD_Stereo_NoFGT)
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeH264_VLD_Multiview_NoFGT)){
						bFindDecoderGuid = true;
						break;
					}
				}
				//H.265
				if(InlineIsEqualGUID(subtype, MFVideoFormat_H265) == TRUE ||
					InlineIsEqualGUID(subtype, MFVideoFormat_HEVC) == TRUE){
					if(	   InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeHEVC_VLD_Main) 
						|| InlineIsEqualGUID(decoderDeviceGuid, DXVA2_ModeHEVC_VLD_Main10)){
						bFindDecoderGuid = true;
						break;
					}
				}
			}			

			if(bFindDecoderGuid){

				if(m_pRenderTargetFormats){
					CoTaskMemFree(m_pRenderTargetFormats);
					m_pRenderTargetFormats = NULL;
				}

				hr = m_pdxVideoDecoderService->GetDecoderRenderTargets(decoderDeviceGuid, &uiCount, &m_pRenderTargetFormats);
				if(FAILED(hr)){
					Logger::getInstance().LogWarn("FFmpegMFT::CheckInputMediaType failed to GetDGetDecoderRenderTargets, return code 0x%x", hr);
					CoTaskMemFree(pGuids);
					hr = MF_E_UNSUPPORTED_D3D_TYPE;
				}
				BREAK_ON_FAIL(hr);
				
				m_uiNumOfRenderTargetFormats = uiCount;

				if(m_pConfigs){
					CoTaskMemFree(m_pConfigs);
					m_pConfigs = NULL;
				}

				DXVA2_VideoDesc dxva2_desc_output;
				hr = ConvertMFTypeToDXVAType(pType, &dxva2_desc_output);
				BREAK_ON_FAIL(hr);

				Logger::getInstance().LogInfo("FFmpegMFT::CheckInputMediaType using DXVA2 device guid %S res:%uX%u fps:%u/%u interlace:%d",
					guid_dxva2_string(decoderDeviceGuid),
					dxva2_desc_output.SampleWidth,
					dxva2_desc_output.SampleHeight,
					dxva2_desc_output.InputSampleFreq.Numerator,
					dxva2_desc_output.InputSampleFreq.Denominator,					
					dxva2_desc_output.SampleFormat.SampleFormat);

				hr = m_pdxVideoDecoderService->GetDecoderConfigurations(decoderDeviceGuid, &dxva2_desc_output, NULL, &uiCount, &m_pConfigs);
				if(FAILED(hr)){
					Logger::getInstance().LogWarn("FFmpegMFT::CheckInputMediaType failed to GetDecoderConfigurations, return code 0x%x", hr);
					CoTaskMemFree(pGuids);
					hr = MF_E_UNSUPPORTED_D3D_TYPE;
				}
				BREAK_ON_FAIL(hr);
			}
			else{
				Logger::getInstance().LogWarn("FFmpegMFT::CheckInputMediaType didn't found proper HW supporting decoder");
				hr = MF_E_UNSUPPORTED_D3D_TYPE;
				CoTaskMemFree(pGuids);
				BREAK_ON_FAIL(hr);
			}

			CoTaskMemFree(pGuids);
		}
    }
    while(false);

    return hr;
}

//https://docs.microsoft.com/en-us/windows/desktop/api/dxva2api/ns-dxva2api-_dxva2_videodesc#examples
// Fills in the DXVA2_ExtendedFormat structure.
HRESULT FFmpegMFT::GetDXVA2ExtendedFormatFromMFMediaType(IMFMediaType *pType, DXVA2_ExtendedFormat *pFormat)
{
    // Get the interlace mode.
    MFVideoInterlaceMode interlace = (MFVideoInterlaceMode)MFGetAttributeUINT32(
            pType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown);

    // The values for interlace mode translate directly, except for
    // "mixed interlace or progressive" mode.
    if (interlace == MFVideoInterlace_MixedInterlaceOrProgressive)
    {
        // Default to interleaved fields.
        pFormat->SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;
    }
    else
    {
        pFormat->SampleFormat = (UINT)interlace;
    }

    // The remaining values translate directly.
    // Use the "no-fail" attribute functions and default to "unknown."
    pFormat->VideoChromaSubsampling = MFGetAttributeUINT32(pType, 
        MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_Unknown);

    pFormat->NominalRange = MFGetAttributeUINT32(pType, 
        MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_Unknown);

    pFormat->VideoTransferMatrix = MFGetAttributeUINT32(pType, 
        MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown);

    pFormat->VideoLighting = MFGetAttributeUINT32(pType, 
        MF_MT_VIDEO_LIGHTING, MFVideoLighting_Unknown);

    pFormat->VideoPrimaries = MFGetAttributeUINT32(pType, 
        MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_Unknown);

    pFormat->VideoTransferFunction = MFGetAttributeUINT32(pType, 
        MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_Unknown);

    return S_OK;
}

void FFmpegMFT::ReleaseSurfaces()
{
	for (SampleToSurfaceMapIter itr = m_pSampleOutMap.begin(); itr != m_pSampleOutMap.end(); ++itr) {
		Logger::getInstance().LogDebug("FFmpegMFT::ReleaseSurfaces Release surface/VideoSample, surface=0x%x, sample=0x%x", itr->first, itr->second);
		SafeRelease(&itr->second);
	}
	m_pSampleOutMap.clear();
}

HRESULT FFmpegMFT::ConvertMFTypeToDXVAType(IMFMediaType *pType, DXVA2_VideoDesc *pDesc)
{
    ZeroMemory(pDesc, sizeof(DXVA2_VideoDesc));

    GUID                    subtype = GUID_NULL;
    UINT32                  width = 0;
    UINT32                  height = 0;
    UINT32                  fpsNumerator = 0;
    UINT32                  fpsDenominator = 0;

    // The D3D format is the first DWORD of the subtype GUID.
    //pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    //pDesc->Format = (D3DFORMAT)subtype.Data1;//TODO: looks wrong, use the output format instead
	if(m_uiNumOfRenderTargetFormats > 0)
	{
		pDesc->Format = m_pRenderTargetFormats[0];
	}

    // Frame size.
	MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
    pDesc->SampleWidth = width;
    pDesc->SampleHeight = height;

    // Frame rate.
    MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);
	if(fpsNumerator != 0 && fpsDenominator != 0)
	{
		pDesc->InputSampleFreq.Numerator = fpsNumerator;
		pDesc->InputSampleFreq.Denominator = fpsDenominator;
	} //else set unknown

    // For progressive or single-field types, the output frequency is the same as
    // the input frequency. For interleaved-field types, the output frequency is
    // twice the input frequency.  
    pDesc->OutputFrameFreq = pDesc->InputSampleFreq;
    if ((pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedEvenFirst) ||
        (pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedOddFirst))
    {
        pDesc->OutputFrameFreq.Numerator *= 2;
    }

    // Extended format information.
    GetDXVA2ExtendedFormatFromMFMediaType(pType, &pDesc->SampleFormat);

    return S_OK;
}
