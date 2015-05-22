#ifndef ILSTUFF
#define ILSTUFF ILSTUFF

#include "il/ilut.h"

ILboolean ilHasAlpha();
ILboolean ilIsCubemap();
HBITMAP ilutConvertAlphaToHBitmap(HDC hDc);
HBITMAP ilutConvertAlphaSliceToHBitmap(HDC hDc, ILuint slice);
HBITMAP ilutConvertAlphaBlendedToHBitmap(HDC hDc);
HBITMAP ilutConvertAlphaBlendedSliceToHBitmap(HDC hDc, ILuint slice);
const ILstring iluTypeString(ILenum imageType);

#endif //ILSTUFF
