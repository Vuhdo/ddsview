#include "Precompiled.h"
#include "oledrag.h"


//DefaultDropSource
//IUnknown
HRESULT STDMETHODCALLTYPE DefaultDropSource::QueryInterface(REFIID ri, LPVOID* dat)
{ 
  *dat = NULL;
  if(IsEqualIID(ri, IID_IUnknown) || IsEqualIID(ri, IID_IDropSource))
  {
    *dat = (LPVOID)this;
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DefaultDropSource::AddRef()
{ return 1; }

ULONG STDMETHODCALLTYPE DefaultDropSource::Release()
{ return 0; }

//IDropSource
HRESULT STDMETHODCALLTYPE DefaultDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
{ 
  if(fEscapePressed
    || (grfKeyState & MK_RBUTTON) != 0
    || (grfKeyState & MK_MBUTTON) != 0)
    return DRAGDROP_S_CANCEL;
  if((grfKeyState & MK_LBUTTON) == 0) return DRAGDROP_S_DROP;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE DefaultDropSource::GiveFeedback(DWORD dwEffect)
{ return DRAGDROP_S_USEDEFAULTCURSORS; }


//FileDataObject

FileDataObject::FileDataObject() : _enumPos(0) {}

void FileDataObject::addFile(const std::string& str)
{ _files.push_back(str); }

//IUnknown
HRESULT STDMETHODCALLTYPE FileDataObject::QueryInterface(REFIID ri, LPVOID* dat)
{ 
  *dat = NULL;
  if(IsEqualIID(ri, IID_IUnknown) || IsEqualIID(ri, IID_IDataObject) || IsEqualIID(ri, IID_IEnumFORMATETC))
  {
    *dat = (LPVOID)this;
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE FileDataObject::AddRef()
{ return 1; }

ULONG STDMETHODCALLTYPE FileDataObject::Release()
{ return 0; }


//IDataObject
HRESULT STDMETHODCALLTYPE FileDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium)
{ 
  if((pformatetcIn->tymed & TYMED_HGLOBAL) == 0)
    return DV_E_TYMED;

  if(pformatetcIn->cfFormat != CF_HDROP)
    return DV_E_FORMATETC;

  //count total length of all file names
  int totalLength = 0;
  for(size_t i = 0; i < _files.size(); ++i)
    totalLength += _files[i].length() + 1; //add one for \0-char

  int size = sizeof(DROPFILES) + totalLength + 1;
  HGLOBAL hGlob = GlobalAlloc(GHND | GMEM_SHARE, size);

  char* p = (char*)GlobalLock(hGlob);
  ((DROPFILES*)p)->pFiles = sizeof(DROPFILES);
  p += sizeof(DROPFILES);

  //copy filenames
  for(size_t j = 0; j < _files.size(); ++j)
  {
    memcpy(p, _files[j].data(), _files[j].size());
    p += _files[j].size() + 1;
  }

  GlobalUnlock(hGlob);


  STGMEDIUM out;
  out.pUnkForRelease = NULL;
  out.tymed = TYMED_HGLOBAL;
  out.hGlobal = hGlob;

  memset(pmedium, 0, sizeof(STGMEDIUM));
  *pmedium = out;

  return S_OK;
}

HRESULT STDMETHODCALLTYPE FileDataObject::GetDataHere(FORMATETC* pformatetcIn, STGMEDIUM* pmedium)
{ return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE FileDataObject::QueryGetData(FORMATETC* pformatetc)
{
  if(pformatetc->cfFormat == CF_HDROP && (pformatetc->tymed & TYMED_HGLOBAL) != 0)
    return S_OK;
  else
    return DV_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE FileDataObject::GetCanonicalFormatEtc(FORMATETC* pformatetcIn, FORMATETC* pformatetcOut)
{ pformatetcOut->ptd = NULL; return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE FileDataObject::SetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium, BOOL fRelease)
{ return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE FileDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc)
{
  if(dwDirection == DATADIR_SET) //you can only get data from this data object
    return E_NOTIMPL;

  *ppenumFormatetc = this;
  AddRef();

  return S_OK;
}

HRESULT STDMETHODCALLTYPE FileDataObject::DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSind, DWORD* pdwConnection)
{ return OLE_E_ADVISENOTSUPPORTED; }

HRESULT STDMETHODCALLTYPE FileDataObject::DUnadvise(DWORD dwConnection)
{ return OLE_E_ADVISENOTSUPPORTED; }

HRESULT STDMETHODCALLTYPE FileDataObject::EnumDAdvise(IEnumSTATDATA** ppenumAdvise)
{ return OLE_E_ADVISENOTSUPPORTED; }

//IEnumFORMATETC
HRESULT STDMETHODCALLTYPE FileDataObject::Next(ULONG celt, FORMATETC* rgelt, ULONG * pceltFetched)
{ 
  if(pceltFetched != NULL)
    *pceltFetched = 0;

  if(_enumPos != 0)
    return S_FALSE;

  FORMATETC ft;
  ft.cfFormat = CF_HDROP;
  ft.ptd = NULL;
  ft.dwAspect = DVASPECT_CONTENT;
  ft.lindex = -1;
  ft.tymed = TYMED_HGLOBAL;

  if(celt >= 1)
    rgelt[0] = ft;
  if(pceltFetched != NULL)
    *pceltFetched = 1;
  ++_enumPos;
  if(celt == 1)
    return S_OK;
  return S_FALSE;
}

HRESULT STDMETHODCALLTYPE FileDataObject::Skip(ULONG celt)
{ _enumPos += celt; return S_OK; }

HRESULT STDMETHODCALLTYPE FileDataObject::Reset()
{ _enumPos = 0; return S_OK; }

HRESULT STDMETHODCALLTYPE FileDataObject::Clone(IEnumFORMATETC** ppenum)
{ return E_NOTIMPL; }
