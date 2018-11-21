#include "StdAfx.h"
#include "MFTClassFactory.h"



MFTClassFactory::MFTClassFactory(void)
{
    InterlockedIncrement(&g_dllLockCount);
    m_cRef = 1;
}



MFTClassFactory::~MFTClassFactory(void)
{
    InterlockedDecrement(&g_dllLockCount);
}




// 
// IClassFactory::CreateInstance implementation.  Attempts to create an instance of the 
// specified COM object, and return the object in the ppvObject pointer.
//
HRESULT STDMETHODCALLTYPE MFTClassFactory::CreateInstance(
    IUnknown *pUnkOuter,      // aggregation object - used only for for aggregation
    REFIID riid,              // IID of the object to create
    void **ppvObject)         // on return contains pointer to the new object
{
    HRESULT hr = S_OK;
    FFmpegMFT* pMft;

    // this is a non-aggregating COM object - return a failure if we are asked to
    // aggregate
    if ( pUnkOuter != NULL )
        return CLASS_E_NOAGGREGATION;

    // create a new instance of the MFT COM object
    pMft = new (std::nothrow) FFmpegMFT();

    // if we failed to create the object, this must be because we are out of memory -
    // return a corresponding error
    if(pMft == NULL)
        return E_OUTOFMEMORY;

    // Attempt to QI the new object for the requested interface
    hr = pMft->QueryInterface(riid, ppvObject);

    // if we failed to QI for the interface for any reason, then this must be the wrong object,
    // delete it and make sure the ppvObject pointer contains NULL.
    if(FAILED(hr))
    {
        delete pMft;
        *ppvObject = NULL;
    }

    return hr;
}



// 
// IClassFactory::LockServer implementation.  This function is used to lock
// the object in memory in order to improve performance and reduce unloading and
// reloading of the DLL.
//
HRESULT STDMETHODCALLTYPE MFTClassFactory::LockServer(BOOL fLock)
{
    if(fLock)
    {
        InterlockedIncrement(&g_dllLockCount);
    }
    else
    {
        InterlockedDecrement(&g_dllLockCount);
    }

    return S_OK;
}



//
// IUnknown methods
//
HRESULT MFTClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (ppv == NULL)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    } 
    else if(riid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>(this);
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

ULONG MFTClassFactory::AddRef(void)  
{ 
    return InterlockedIncrement(&m_cRef); 
}
        
ULONG MFTClassFactory::Release(void) 
{ 
    long cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}