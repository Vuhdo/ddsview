#include "Precompiled.h"
#include "rotbmp.h"

HBITMAP rotateHBitmapL(HBITMAP hBm)
{
  if(hBm == NULL)
    return NULL;

  HDC hDc = GetDC(NULL);

  //todo: fix this to use bmps native format
  BITMAP bm;
  GetObject(hBm, sizeof(bm), &bm);

  DWORD* data = new DWORD[bm.bmWidth*bm.bmHeight];
  DWORD* data2 = new DWORD[bm.bmWidth*bm.bmHeight];

  BITMAPINFO bmpInfo;
  BITMAPINFOHEADER& bih = bmpInfo.bmiHeader;
  bih.biSize = sizeof(bih);
  bih.biWidth = bm.bmWidth;
  bih.biHeight = -bm.bmHeight;
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = 0;
  bih.biSizeImage = 0;
  bih.biXPelsPerMeter = 0;
  bih.biYPelsPerMeter = 0;
  bih.biClrUsed = 0;
  bih.biClrImportant = 0;

  GetDIBits(hDc, hBm, 0, bm.bmHeight, data, &bmpInfo, DIB_RGB_COLORS);

  for(int y = 0; y < bm.bmHeight; ++y)
    for(int x = 0; x < bm.bmWidth; ++x)
      data2[x*bm.bmHeight + y] = data[y*bm.bmWidth + x];

  HBITMAP ret = CreateCompatibleBitmap(hDc, bm.bmHeight, bm.bmWidth);
  bih.biWidth = bm.bmHeight;
  bih.biHeight = bm.bmWidth; //flip diag + flip horz = lrotate, so leave hyt positive after flip

  SetDIBits(hDc, ret, 0, bm.bmWidth, data2, &bmpInfo, DIB_RGB_COLORS);

  delete [] data2;
  delete [] data;
  ReleaseDC(NULL, hDc);

  return ret;
}

HBITMAP rotateHBitmapR(HBITMAP hBm)
{
  if(hBm == NULL)
    return NULL;

  HDC hDc = GetDC(NULL);

  //todo: fix this to use bmps native format
  BITMAP bm;
  GetObject(hBm, sizeof(bm), &bm);

  DWORD* data = new DWORD[bm.bmWidth*bm.bmHeight];
  DWORD* data2 = new DWORD[bm.bmWidth*bm.bmHeight];

  BITMAPINFO bmpInfo;
  BITMAPINFOHEADER& bih = bmpInfo.bmiHeader;
  bih.biSize = sizeof(bih);
  bih.biWidth = bm.bmWidth;
  bih.biHeight = bm.bmHeight; //flip horz  + flip diag = rrotate, so leave hyt positive before flip
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = 0;
  bih.biSizeImage = 0;
  bih.biXPelsPerMeter = 0;
  bih.biYPelsPerMeter = 0;
  bih.biClrUsed = 0;
  bih.biClrImportant = 0;

  GetDIBits(hDc, hBm, 0, bm.bmHeight, data, &bmpInfo, DIB_RGB_COLORS);

  for(int y = 0; y < bm.bmHeight; ++y)
    for(int x = 0; x < bm.bmWidth; ++x)
      data2[x*bm.bmHeight + y] = data[y*bm.bmWidth + x];

  HBITMAP ret = CreateCompatibleBitmap(hDc, bm.bmHeight, bm.bmWidth);
  bih.biWidth = bm.bmHeight;
  bih.biHeight = -bm.bmWidth;

  SetDIBits(hDc, ret, 0, bm.bmWidth, data2, &bmpInfo, DIB_RGB_COLORS);

  delete [] data2;
  delete [] data;
  ReleaseDC(NULL, hDc);

  return ret;
}
