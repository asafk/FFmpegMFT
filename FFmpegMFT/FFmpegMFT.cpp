#include "stdafx.h"
#include "FFmpegMFT.h"

#include <MMSystem.h>
#include <uuids.h>
#include "CBufferLock.h"


FFmpegMFT::FFmpegMFT(void) :
    m_cRef(1)

{
	OutputDebugString(_T("\n\nFFmpegMFT\n\n"));
}

FFmpegMFT::~FFmpegMFT(void)
{
	OutputDebugString(_T("\n\n~FFmpegMFT\n\n"));
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
	OutputDebugString(_T("\n\nGetStreamLimits\n\n"));

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
	OutputDebugString(_T("\n\nGetStreamCount\n\n"));
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
	OutputDebugString(_T("\n\nGetStreamIDs\n\n"));
    return E_NOTIMPL;
}

//
// Get a structure with information about an input stream with the specified index.
//
HRESULT FFmpegMFT::GetInputStreamInfo(
    DWORD                   dwInputStreamID,  // stream being queried.
    MFT_INPUT_STREAM_INFO*  pStreamInfo)      // stream information
{
	OutputDebugString(_T("\n\nGetInputStreamInfo\n\n"));

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

        //   - The MFT provides samples and there is no need to give it output samples to 
        //     fill in during its ProcessOutput() calls.
        pStreamInfo->dwFlags =
            MFT_OUTPUT_STREAM_WHOLE_SAMPLES |                
            MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
			MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
           /* MFT_OUTPUT_STREAM_PROVIDES_SAMPLES ;*/

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




//
// Get the bag of custom attributes associated with this MFT.  If the MFT does not support
// any custom attributes, the method can be left unimplemented.  If an object is returned,
// the object can be used to either get or set attributes of this MFT, and thus provide custom
// parameters and information about the MFT.
//
HRESULT FFmpegMFT::GetAttributes(IMFAttributes** pAttributes)
{
	OutputDebugString(_T("\n\nGetAttributes\n\n"));
    // This MFT does not support any attributes, so the method is not implemented.
    return E_NOTIMPL;
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
	OutputDebugString(_T("\n\nGetInputStreamAttributes\n\n"));
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
	OutputDebugString(_T("\n\nGetOutputStreamAttributes\n\n"));
    // This MFT does not support any attributes, so the method is not implemented.
    return E_NOTIMPL;
}



//
// Deletes the specified input stream from the MFT.  This MFT has only a single 
// constant stream, and therefore this method is not implemented.
//
HRESULT FFmpegMFT::DeleteInputStream(DWORD dwStreamID)
{
	OutputDebugString(_T("\n\nDeleteInputStream\n\n"));
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
	OutputDebugString(_T("\n\nAddInputStreams\n\n"));
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
	OutputDebugString(_T("\n\nGetOutputAvailableType\n\n"));

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
	OutputDebugString(_T("\n\nSetInputType\n\n"));

    HRESULT hr = S_OK;
    CComPtr<IMFAttributes> pTypeAttributes = pType;

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
	OutputDebugString(_T("\n\nSetOutputType\n\n"));

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
	OutputDebugString(_T("\n\nGetInputCurrentType\n\n"));

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
	OutputDebugString(_T("\n\nGetOutputCurrentType\n\n"));

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
	OutputDebugString(_T("\n\nGetInputStatus\n\n"));

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
	OutputDebugString(_T("\n\nGetOutputStatus\n\n"));
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
	OutputDebugString(_T("\n\nSetOutputBounds\n\n"));

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
	OutputDebugString(_T("\n\nProcessEvent\n\n"));

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
	OutputDebugString(_T("\n\nProcessMessage\n\n"));

    HRESULT hr = S_OK;

    CComCritSecLock<CComAutoCriticalSection> lock(m_critSec);    

    if(eMessage == MFT_MESSAGE_COMMAND_FLUSH)
    {
        // Flush the MFT - release all samples in it and reset the state
		if(m_pSample != NULL)
		{
			m_decoder.flush();
		}
        m_pSample = NULL;    	
    }
    else if(eMessage ==  MFT_MESSAGE_COMMAND_DRAIN)
    {
        // The drain command tells the MFT not to accept any more input until
        // all of the pending output has been processed. That is the default 
        // behavior of this MFT, so there is nothing to do.
    }
	else if(eMessage == MFT_MESSAGE_SET_D3D_MANAGER)
	{
		// The pipeline should never send this message unless the MFT
        // has the MF_SA_D3D_AWARE attribute set to TRUE. However, if we
        // do get this message, it's invalid and we don't implement it.
        hr = E_NOTIMPL;
	}
    else if(eMessage == MFT_MESSAGE_NOTIFY_BEGIN_STREAMING)
    {
		do
	    {
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
			else
		    {
			    hr = MF_E_INVALIDMEDIATYPE;
			    break;
		    }
	    }
	    while (false);
    }
    else if(eMessage == MFT_MESSAGE_NOTIFY_END_STREAMING)
    {
		m_decoder.release();
    }
    else if(eMessage == MFT_MESSAGE_NOTIFY_END_OF_STREAM)
    {
    }
    else if(eMessage == MFT_MESSAGE_NOTIFY_START_OF_STREAM)
    {	  
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

		//output buffer
		IMFSample* outputSample = pOutputSampleBuffer[0].pSample;
		CComPtr<IMFMediaBuffer> pOutputMediaBuffer;
		hr = outputSample->GetBufferByIndex(0, &pOutputMediaBuffer);
		BREAK_ON_FAIL(hr);

		///do the decoding
		hr = decode(pInputMediaBuffer,pOutputMediaBuffer);
		BREAK_ON_FAIL(hr);

		//timestamp
		LONGLONG sampleTime;
		hr = m_pSample->GetSampleTime(&sampleTime);
		BREAK_ON_FAIL(hr);
		hr = outputSample->SetSampleTime(sampleTime);
		BREAK_ON_FAIL(hr);
		
		m_pSample.Release();

        //// Detach the output sample from the MFT and put the pointer for
        //// the processed sample into the output buffer
        //pOutputSampleBuffer[0].pSample = m_pSample.Detach();

        // Set status flags for output
        pOutputSampleBuffer[0].dwStatus = 0;
        *pdwStatus = 0;
    }
    while(false);

    return hr;
}

//HRESULT FFmpegMFT::decode(IMFMediaBuffer* inputMediaBuffer, IMFMediaBuffer* pOutputMediaBuffer)
//{
//	HRESULT hr = S_OK;
//
//	CBufferLock videoBuffer(pOutputMediaBuffer);
//
//	BYTE *pIn = NULL;
//	DWORD lenIn = 0;
//
//	BYTE *pOut = NULL;
//	int lenOut = 0;
//    LONG lDefaultStride = 0;
//    LONG lActualStride = 0;
//
//	 hr = GetDefaultStride(m_pOutputType, &lDefaultStride);
//
//    if (SUCCEEDED(hr))
//    {
//        hr = videoBuffer.LockBuffer(lDefaultStride, 1080/*TODO hard coded*/, &pOut, &lActualStride);
//    }
//
//	if (SUCCEEDED(hr))
//    {
//        hr = inputMediaBuffer->Lock(&pIn, NULL, &lenIn);
//    }
//
//	m_decoder.decode(pIn,lenIn,pOut,&lenOut);
//
//	inputMediaBuffer->Unlock();
//
//
//	 if (SUCCEEDED(hr))
//    {
//        hr = pOutputMediaBuffer->SetCurrentLength(lenOut);
//    }
//
//	return hr;
//}

HRESULT FFmpegMFT::decode(IMFMediaBuffer* inputMediaBuffer, IMFMediaBuffer* pOutputMediaBuffer)
{
	HRESULT hr = S_OK;


	BYTE *pIn = NULL;
	DWORD lenIn = 0;

	BYTE *pOut = NULL;
	int lenOut = 0;
    LONG lDefaultStride = 0;

    if (SUCCEEDED(hr))
    {
		hr = pOutputMediaBuffer->Lock(&pOut,NULL,NULL);
    }

	if (SUCCEEDED(hr))
    {
        hr = inputMediaBuffer->Lock(&pIn, NULL, &lenIn);
    }

	m_decoder.decode(pIn,lenIn,pOut,&lenOut);

	inputMediaBuffer->Unlock();
	pOutputMediaBuffer->Unlock();

	 if (SUCCEEDED(hr))
    {
        hr = pOutputMediaBuffer->SetCurrentLength(lenOut);
    }

	return hr;
}

//HRESULT FFmpegMFT::decode(IMFMediaBuffer* inputMediaBuffer, IMFMediaBuffer* pOutputMediaBuffer)
//{
//	HRESULT hr = S_OK;
//
//
//	BYTE *pIn = NULL;
//	DWORD lenIn = 0;
//
//	BYTE *pOut = NULL;
//	int lenOut = 0;
//    LONG lDefaultStride = 0;
//    LONG lActualStride = 0;
//
//	CComPtr<IMF2DBuffer> p2DBuffer;
//	BYTE* pOutputBuffer = NULL;
//				LONG yPitch;
//	hr = pOutputMediaBuffer->QueryInterface(IID_IMF2DBuffer, reinterpret_cast<void**>(&p2DBuffer));
//    if (SUCCEEDED(hr))
//    {
//    	hr = p2DBuffer->Lock2D(&pOutputBuffer, &yPitch);				
//    }
//
//	if (SUCCEEDED(hr))
//    {
//        hr = inputMediaBuffer->Lock(&pIn, NULL, &lenIn);
//    }
//
//	m_decoder.decode(pIn,lenIn,pOut,&lenOut);
//	
//	memcpy(pOutputBuffer,pOut,lenOut);
//
//	inputMediaBuffer->Unlock();
//	p2DBuffer->Unlock2D();
//
//
//	 if (SUCCEEDED(hr))
//    {
//        hr = pOutputMediaBuffer->SetCurrentLength(lenOut);
//    }
//
//	return hr;
//}

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


  //      if(dwTypeIndex == 0) //H.264
  //      {
  //          hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  //      }
		//else if(dwTypeIndex == 1) //HEVC
  //      {
  //          hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H265); //MFVideoFormat_HEVC
  //      }

		//TODO: fix dynamic , now it's hard coded
        if(dwTypeIndex == 0) //HEVC
        {
			//hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
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
   /*     if(dwTypeIndex == 0)
        {
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_UYVY);
        }
        else if(dwTypeIndex == 1)
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

		//Find width and height from the imput type
		UINT32 width, height;
		hr = MFGetAttributeSize(m_pInputType, MF_MT_FRAME_SIZE, &width, &height);
		BREAK_ON_FAIL(hr);
		//Find fps from the imput type
		/*UINT32 framerate_n, framerate_d;
		hr = MFGetAttributeRatio(m_pInputType, MF_MT_FRAME_RATE, &framerate_n, &framerate_d);
		BREAK_ON_FAIL(hr);*/

		/*hr = pmt->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
		BREAK_ON_FAIL(hr);*/

		hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		BREAK_ON_FAIL(hr);

		/*hr = pmt->SetUINT32(MF_MT_SAMPLE_SIZE, width * height * 4); 
		BREAK_ON_FAIL(hr);*/	

		hr = MFSetAttributeSize(pmt, MF_MT_FRAME_SIZE, width, height);
		BREAK_ON_FAIL(hr);

		/*hr = MFSetAttributeRatio(pmt, MF_MT_FRAME_RATE, framerate_n, framerate_d);
		BREAK_ON_FAIL(hr);*/

		hr = pmt->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		BREAK_ON_FAIL(hr);

		/*hr = MFSetAttributeRatio(pmt, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		BREAK_ON_FAIL(hr);*/

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

		//TODO: not work currently with h.264
         //get the requested interlacing format
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





