#pragma once

#include "mftransform.h"
#include "atlcore.h"
#include "atlcomcli.h"
#include "decoder.h"

#include <d3d9.h>
#include <dxva2api.h>

using namespace ATL;

typedef std::map<IDirect3DSurface9*,IMFSample*> SampleToSurfaceMap;
typedef SampleToSurfaceMap::iterator SampleToSurfaceMapIter;

class FFmpegMFT :  public IMFTransform
{
	public:

		FFmpegMFT();
		~FFmpegMFT();

		//
		// IMFTransform stream handling functions
		STDMETHODIMP GetStreamLimits(  DWORD* pdwInputMinimum, DWORD* pdwInputMaximum, DWORD* pdwOutputMinimum, DWORD* pdwOutputMaximum );

	    STDMETHODIMP GetStreamIDs( DWORD dwInputIDArraySize, DWORD* pdwInputIDs, DWORD dwOutputIDArraySize, DWORD* pdwOutputIDs );

	    STDMETHODIMP GetStreamCount( DWORD* pcInputStreams, DWORD* pcOutputStreams );
	    STDMETHODIMP GetInputStreamInfo( DWORD dwInputStreamID, MFT_INPUT_STREAM_INFO* pStreamInfo );
	    STDMETHODIMP GetOutputStreamInfo( DWORD dwOutputStreamID, MFT_OUTPUT_STREAM_INFO* pStreamInfo );
	    STDMETHODIMP GetInputStreamAttributes( DWORD dwInputStreamID, IMFAttributes** pAttributes );
	    STDMETHODIMP GetOutputStreamAttributes( DWORD dwOutputStreamID, IMFAttributes** pAttributes );
	    STDMETHODIMP DeleteInputStream( DWORD dwStreamID );
	    STDMETHODIMP AddInputStreams( DWORD cStreams, DWORD* adwStreamIDs );

	    //
	    // IMFTransform mediatype handling functions
	    STDMETHODIMP GetInputAvailableType( DWORD dwInputStreamID, DWORD dwTypeIndex, IMFMediaType** ppType );
	    STDMETHODIMP GetOutputAvailableType( DWORD dwOutputStreamID, DWORD dwTypeIndex, IMFMediaType** ppType );
	    STDMETHODIMP SetInputType( DWORD dwInputStreamID, IMFMediaType* pType, DWORD dwFlags );
	    STDMETHODIMP SetOutputType( DWORD dwOutputStreamID, IMFMediaType* pType, DWORD dwFlags );
	    STDMETHODIMP GetInputCurrentType( DWORD dwInputStreamID, IMFMediaType** ppType );
	    STDMETHODIMP GetOutputCurrentType( DWORD dwOutputStreamID, IMFMediaType** ppType );

	    //
	    // IMFTransform status and eventing functions
	    STDMETHODIMP GetInputStatus( DWORD dwInputStreamID, DWORD* pdwFlags );
	    STDMETHODIMP GetOutputStatus( DWORD* pdwFlags );
	    STDMETHODIMP SetOutputBounds( LONGLONG hnsLowerBound, LONGLONG hnsUpperBound);
	    STDMETHODIMP ProcessEvent( DWORD dwInputStreamID, IMFMediaEvent* pEvent );
	    STDMETHODIMP GetAttributes( IMFAttributes** pAttributes );

	    //
	    // IMFTransform main data processing and command functions
	    STDMETHODIMP ProcessMessage( MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam );
	    STDMETHODIMP ProcessInput( DWORD dwInputStreamID, IMFSample* pSample, DWORD dwFlags);
	    STDMETHODIMP ProcessOutput( DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER* pOutputSamples, DWORD* pdwStatus);

	    //
	    // IUnknown interface implementation
	    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
	    virtual ULONG STDMETHODCALLTYPE AddRef(void);
	    virtual ULONG STDMETHODCALLTYPE Release(void);	

	private:

	    volatile long m_cRef;                    // ref count
		CComAutoCriticalSection m_critSec;       // critical section for the MFT

	    CComPtr<IMFSample>  m_pSample;           // Input sample.
	    CComPtr<IMFMediaType> m_pInputType;      // Input media type.
	    CComPtr<IMFMediaType> m_pOutputType;     // Output media type.
		LONGLONG m_sampleTime;

		//DXVA support using Direct3D 9 
		HANDLE m_hD3d9Device;
		CComPtr<IDirect3DDeviceManager9> m_pDirect3DDeviceManager9; //3d device manager using 'Direct3D 9' TODO: add Direct3D 11 interface IMFDXGIDeviceManager 
		CComPtr<IDirect3DDevice9> m_pDirect3DDevice9; //3d device
		CComPtr<IDirectXVideoDecoderService> m_pdxVideoDecoderService; //video DXVA service
		// parameters 
		D3DFORMAT* m_pRenderTargetFormats;
		UINT m_uiNumOfRenderTargetFormats;
		DXVA2_ConfigPictureDecode* m_pConfigs;
		//Uncompressed Buffers
		SampleToSurfaceMap m_pSampleOutMap;

		// private helper functions
		HRESULT GetSupportedOutputMediaType(DWORD dwTypeIndex, IMFMediaType** ppmt);
		HRESULT GetSupportedInputMediaType(DWORD dwTypeIndex, IMFMediaType** ppmt);
		HRESULT CheckInputMediaType(IMFMediaType *pmt);
		HRESULT CheckOutputMediaType(IMFMediaType *pmt);

		//DXVA2 helper functions
		HRESULT ConvertMFTypeToDXVAType(IMFMediaType *pType, DXVA2_VideoDesc *pDesc);
		HRESULT GetDXVA2ExtendedFormatFromMFMediaType(IMFMediaType *pType, DXVA2_ExtendedFormat *pFormat);
		void	ReleaseSurfaces();

		//FFmpeg decoding
		HRESULT decode(IMFMediaBuffer* inputMediaBuffer, IMFMediaBuffer* pOutputMediaBuffer);
		HRESULT decode(IMFMediaBuffer* inputMediaBuffer, IDirect3DSurface9** ppSurface);
		Decoder m_decoder;
};

