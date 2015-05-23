#include "Precompiled.h"
#include "ilstuff.h"

//*sigh* DevIL's exported API is not expressive enough...
#include "IL/devil_internal_exports.h"

ILboolean ilHasAlpha()
{
  switch(ilGetInteger(IL_IMAGE_FORMAT))
  {
    case IL_RGBA:
    case IL_BGRA:
    case IL_LUMINANCE_ALPHA:
      return IL_TRUE;
    case IL_COLOUR_INDEX:
      if(ilGetInteger(IL_PALETTE_TYPE) == IL_PAL_RGBA32
        || ilGetInteger(IL_PALETTE_TYPE) == IL_PAL_BGRA32)
        return IL_TRUE;
    default:
      return IL_FALSE;
  }
}

ILboolean ilIsCubemap()
{
  int layerCount = ilGetInteger(IL_NUM_IMAGES);
  int currImg = ilGetInteger(IL_CUR_IMAGE);

  if(layerCount != 5)
    return IL_FALSE;

  int w = ilGetInteger(IL_IMAGE_WIDTH);
  if(w != ilGetInteger(IL_IMAGE_HEIGHT))
    return IL_FALSE;

  int cubeFlags[] =
  {
    IL_CUBEMAP_POSITIVEX, IL_CUBEMAP_NEGATIVEX,
    IL_CUBEMAP_POSITIVEY, IL_CUBEMAP_NEGATIVEY,
    IL_CUBEMAP_POSITIVEZ, IL_CUBEMAP_NEGATIVEZ
  };
  for(int i = 0; i <= layerCount; ++i)
  {
    ilBindImage(currImg);
    ilActiveImage(i);
    if(ilGetInteger(IL_IMAGE_WIDTH) != w ||
       ilGetInteger(IL_IMAGE_HEIGHT) != w ||
       ilGetInteger(IL_IMAGE_CUBEFLAGS) != cubeFlags[i])
      return IL_FALSE;
  }
  ilBindImage(currImg);
  return IL_TRUE;
}

HBITMAP ilutConvertAlphaSliceToHBitmap(HDC hDc, ILuint slice)
{
  ILint w = ilGetInteger(IL_IMAGE_WIDTH);
  ILint h = ilGetInteger(IL_IMAGE_HEIGHT);
  ILenum format = ilGetInteger(IL_IMAGE_FORMAT);
  ILenum type = ilGetInteger(IL_IMAGE_TYPE);
  ILenum origin = ilGetInteger(IL_IMAGE_ORIGIN);
  ILuint channelCount = ilGetInteger(IL_IMAGE_BPP)/ilGetInteger(IL_IMAGE_BPC);
  ILuint si = 0, di = 0;

  ILimage* temp = NULL, * curr = ilGetCurImage();
  ILuint depthBackup = ilGetInteger(IL_IMAGE_DEPTH);
  ILubyte* dataBackup = ilGetData();

  if(format == IL_RGB || format == IL_BGR || format == IL_LUMINANCE ||
     (format == IL_COLOR_INDEX
       && (ilGetInteger(IL_PALETTE_TYPE) != IL_PAL_RGBA32 && ilGetInteger(IL_PALETTE_TYPE) != IL_PAL_BGRA32)))
  {
    //no alpha - return an all-white bitmap (TODO: return NULL???)
    HBITMAP hBm = CreateCompatibleBitmap(hDc, w, h);
    HDC bmDc = CreateCompatibleDC(hDc);
    HBITMAP oldBm = (HBITMAP)SelectObject(bmDc, hBm);

    RECT r = { 0, 0, w, h };
    FillRect(bmDc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SelectObject(bmDc, oldBm);
    DeleteDC(bmDc);
    return hBm;
  }

  //fool DevIL into thinking that this is a 2d image
  curr->Depth = 1;
  curr->Data += slice*curr->SizeOfPlane;

  //convert type in a copy
  if(type != IL_UNSIGNED_BYTE)
  {
    temp = iConvertImage(curr, format, IL_UNSIGNED_BYTE);
    ilSetCurImage(temp);
  }
  else
    temp = curr;

  int pad = (4 - w%4)%4;

  ILubyte* data = (ILubyte*)malloc((w + pad)*h), * imgData;
  imgData = ilGetData();

  if(format == IL_COLOUR_INDEX)
  {
    ILubyte* palette = ilGetPalette() + 3;

    int c = 0;
    for(di = 0; di < (ILuint)(w + pad)*h;)
    {
      data[di] = *(palette + imgData[si]*4);
      di++, si++, c++;
      if(c == w) //end of row
      {
        c = 0;
        di += pad;
      }
    }
  }
  else
  {
    if(format == IL_RGBA || format == IL_BGRA)
      si = 3;
    else //IL_LUMINANCE_ALPHA
      si = 1;

    int c = 0;
    for(di = 0; di < (ILuint)(w + pad)*h;)
    {
      data[di] = imgData[si];
      di++, si += channelCount, c++;
      if(c == w) //end of row
      {
        c = 0;
        di += pad;
      }
    }
  }

  if(temp != curr)
  {
    ilSetCurImage(curr);
    ilCloseImage(temp);
  }

  //restore current image to full 3d image
  curr->Depth = depthBackup;
  curr->Data = dataBackup;

  HBITMAP hBm = CreateCompatibleBitmap(hDc, w, h);

  ILubyte bmInfo[sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD)];

  BITMAPINFO* pInfo = (BITMAPINFO*)bmInfo;

  pInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pInfo->bmiHeader.biWidth = w;
  if(origin == IL_ORIGIN_UPPER_LEFT)
    pInfo->bmiHeader.biHeight = -h;
  else
    pInfo->bmiHeader.biHeight = h;
  pInfo->bmiHeader.biPlanes = 1;
  pInfo->bmiHeader.biBitCount = 8;
  pInfo->bmiHeader.biCompression = 0;
  pInfo->bmiHeader.biSizeImage = 0;
  pInfo->bmiHeader.biXPelsPerMeter = 0;
  pInfo->bmiHeader.biYPelsPerMeter = 0;
  pInfo->bmiHeader.biClrUsed = 256;
  pInfo->bmiHeader.biClrImportant = 0;

  for(int t = 0; t < 256; t++) //generate greyscale palette
  {
    pInfo->bmiColors[t].rgbRed = t;
    pInfo->bmiColors[t].rgbGreen = t;
    pInfo->bmiColors[t].rgbBlue = t;
  }

  SetDIBits(hDc, hBm, 0, h, data, pInfo, DIB_RGB_COLORS);

  //delete image copy
  free(data);

  return hBm;
}

HBITMAP ilutConvertAlphaToHBitmap(HDC hDc)
{
  return ilutConvertAlphaSliceToHBitmap(hDc, 0);
}

HBITMAP ilutConvertAlphaBlendedSliceToHBitmap(HDC hDc, ILuint slice)
{
  ILint w = ilGetInteger(IL_IMAGE_WIDTH);
  ILint h = ilGetInteger(IL_IMAGE_HEIGHT);
  ILenum format = ilGetInteger(IL_IMAGE_FORMAT);
  ILenum type = ilGetInteger(IL_IMAGE_TYPE);
  ILenum origin = ilGetInteger(IL_IMAGE_ORIGIN);
  ILuint channelCount = ilGetInteger(IL_IMAGE_BPP)/ilGetInteger(IL_IMAGE_BPC);
  ILuint si = 0, di = 0;

  ILimage* temp = NULL, * curr = ilGetCurImage();
  ILuint depthBackup = ilGetInteger(IL_IMAGE_DEPTH);
  ILubyte* dataBackup = ilGetData();

  if(format == IL_RGB || format == IL_BGR || format == IL_LUMINANCE ||
     (format == IL_COLOR_INDEX
       && (ilGetInteger(IL_PALETTE_TYPE) != IL_PAL_RGBA32 && ilGetInteger(IL_PALETTE_TYPE) != IL_PAL_BGRA32)))
  {
    //no alpha - return bitmap itself (TODO: return NULL???)
    return ilutConvertSliceToHBitmap(hDc, slice);
  }

  //fool DevIL into thinking that this is a 2d image
  curr->Depth = 1;
  curr->Data += slice*curr->SizeOfPlane;

  //convert type in a copy
  if(type != IL_UNSIGNED_BYTE)
  {
    temp = iConvertImage(curr, format, IL_UNSIGNED_BYTE);
    ilSetCurImage(temp);
  }
  else
    temp = curr;

  int pad = (4 - (3*w)%4)%4;

  ILubyte* data = (ILubyte*)malloc((3*w + pad)*h), * imgData;
  imgData = ilGetData();

#define PIXEL_HACK \
    if(grid) { \
      data[di + 2] = (ILubyte)((r*a)/255 + 255 - a); \
      data[di + 1] = (ILubyte)((g*a)/255 + 0); \
      data[di + 0] = (ILubyte)((b*a)/255 + 255 - a); \
    } \
    else { \
      data[di + 2] = (ILubyte)((r*a)/255 + 0); \
      data[di + 1] = (ILubyte)((g*a)/255 + 255 - a); \
      data[di + 0] = (ILubyte)((b*a)/255 + 255 - a); \
    }


  if(format == IL_COLOUR_INDEX)
  {
    ILubyte* palette = ilGetPalette();
    int ri = 0, bi = 2;

    if(ilGetInteger(IL_PALETTE_TYPE) == IL_PAL_BGRA32)
    {
      ri = 2;
      bi = 0;
    }
    ILboolean grid = IL_FALSE;
    ILuint r, g, b, a;
    int cx = 0, cy = 0;
    for(di = 0; di < (ILuint)(3*w + pad)*h;)
    {
      r = *(palette + imgData[si]*4 + ri);
      g = *(palette + imgData[si]*4 + 1);
      b = *(palette + imgData[si]*4 + bi);
      a = *(palette + imgData[si]*4 + 3);
      PIXEL_HACK;
      di += 3, si++, cx++;

      if(cx == w) //end of row
      {
        cx = 0;
        ++cy;
        di += pad;
      }
      grid = ((cx % 64)/32 + (cy % 64)/32) == 1;
    }
  }
  else
  {
    int ri = 0, gi = 1, bi = 2, ai = 3;

    if(format == IL_BGRA)
    {
      ri = 2;
      bi = 0;
    }
    else if(format == IL_LUMINANCE_ALPHA)
    {
      gi = bi = 0;
      ai = 1;
    }

    ILboolean grid = IL_FALSE;
    ILuint r, g, b, a;
    int cx = 0, cy = 0;
    for(di = 0; di < (ILuint)(3*w + pad)*h;)
    {
      r = imgData[si + ri];
      g = imgData[si + gi];
      b = imgData[si + bi];
      a = imgData[si + ai];
      PIXEL_HACK;
      di += 3, si += channelCount, cx++;
      if(cx == w) //end of row
      {
        cx = 0;
        ++cy;
        di += pad;
      }
      grid = ((cx % 64)/32 + (cy % 64)/32) == 1;
    }
  }
#undef PIXEL_HACK

  if(temp != curr)
  {
    ilSetCurImage(curr);
    ilCloseImage(temp);
  }

  //restore current image to full 3d image
  curr->Depth = depthBackup;
  curr->Data = dataBackup;

  HBITMAP hBm = CreateCompatibleBitmap(hDc, w, h);


  BITMAPINFO info;
  BITMAPINFO* pInfo = &info;

  pInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pInfo->bmiHeader.biWidth = w;
  if(origin == IL_ORIGIN_UPPER_LEFT)
    pInfo->bmiHeader.biHeight = -h;
  else
    pInfo->bmiHeader.biHeight = h;
  pInfo->bmiHeader.biPlanes = 1;
  pInfo->bmiHeader.biBitCount = 24;
  pInfo->bmiHeader.biCompression = 0;
  pInfo->bmiHeader.biSizeImage = 0;
  pInfo->bmiHeader.biXPelsPerMeter = 0;
  pInfo->bmiHeader.biYPelsPerMeter = 0;
  pInfo->bmiHeader.biClrUsed = 0;
  pInfo->bmiHeader.biClrImportant = 0;

  SetDIBits(hDc, hBm, 0, h, data, pInfo, DIB_RGB_COLORS);

  //delete image copy
  free(data);

  return hBm;
}

HBITMAP ilutConvertAlphaBlendedToHBitmap(HDC hDc)
{
  return ilutConvertAlphaBlendedSliceToHBitmap(hDc, 0);
}


//TODO: return descriptions
#ifndef IL_TEXT
#define IL_TEXT
#endif

const ILstring iluTypeString(ILenum imageType)
{
  switch(imageType)
  {
    case IL_TGA:
      return IL_TEXT("tga");
    case IL_JPG:
      return IL_TEXT("jpg");
    case IL_DDS:
      return IL_TEXT("dds");
    case IL_PNG:
      return IL_TEXT("png");
    case IL_BMP:
      return IL_TEXT("bmp");
    case IL_GIF:
      return IL_TEXT("gif");
    case IL_HDR:
      return IL_TEXT("hdr");
    case IL_CUT:
      return IL_TEXT("cut");
    case IL_ICO:
      return IL_TEXT("ico");
//    case IL_JNG:
//      return IL_TEXT("jng");
    case IL_LIF:
      return IL_TEXT("lif");
    case IL_MDL:
      return IL_TEXT("mdl");
    case IL_MNG:
      return IL_TEXT("mng");
    case IL_PCD:
      return IL_TEXT("pcd");
    case IL_PCX:
      return IL_TEXT("pcx");
    case IL_PIC:
      return IL_TEXT("pic");
    case IL_PIX:
      return IL_TEXT("pix");
    case IL_PNM:
      return IL_TEXT("pnm");
    case IL_PSD:
      return IL_TEXT("psd");
    case IL_PSP:
      return IL_TEXT("psp");
    case IL_PXR:
      return IL_TEXT("pxr");
    case IL_SGI:
      return IL_TEXT("sgi");
    case IL_TIF:
      return IL_TEXT("tif");
    case IL_WAL:
      return IL_TEXT("wal");
    case IL_XPM:
      return IL_TEXT("xpm");
    case IL_TYPE_UNKNOWN:
    default:
      return "(unknown)";
  }
}
