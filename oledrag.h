#ifndef NICO_OLEDRAG_H
#define NICO_OLEDRAG_H NICO_OLEDRAG_H

#include <windows.h>
#include <shlobj.h> //DROPFILES

#include <vector>
#include <string>

class DefaultDropSource : public IDropSource
{
 public:

  //IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID ri, LPVOID* dat);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //IDropSource
  HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState);
  HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect);
};

class FileDataObject : public IDataObject, IEnumFORMATETC
{
 public:
  FileDataObject();

  void addFile(const std::string& str);

  //IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID ri, LPVOID* dat);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //IDataObject
  HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium);
  HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* pformatetcIn, STGMEDIUM* pmedium);
  HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc);
  HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* pformatetcIn, FORMATETC* pformatetcOut);
  HRESULT STDMETHODCALLTYPE SetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium, BOOL fRelease);
  HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc);
  HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSind, DWORD* pdwConnection);
  HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection);
  HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** ppenumAdvise);

  //IEnumFORMATETC
  HRESULT STDMETHODCALLTYPE Next(ULONG celt, FORMATETC* rgelt, ULONG * pceltFetched);
  HRESULT STDMETHODCALLTYPE Skip(ULONG celt);
  HRESULT STDMETHODCALLTYPE Reset();
  HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** ppenum);

 private:
  int _enumPos;
  std::vector<std::string> _files;
};


#endif //NICO_OLEDRAG_H
