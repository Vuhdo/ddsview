#ifndef NICO_DDSVIEW_REGISTERFILETYPES_H
#define NICO_DDSVIEW_REGISTERFILETYPES_H NICO_DDSVIEW_REGISTERFILETYPES_H

#include <windows.h>
#include <string>


#include "globalstuff.h"


bool registerFileTypesDialog(HWND hWnd, EXT* exts, int extCount);

#endif //NICO_DDSVIEW_REGISTERFILETYPES_H
