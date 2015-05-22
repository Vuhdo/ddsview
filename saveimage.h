#ifndef NICO_DDSVIEW_SAVEIMAGE_INC
#define NICO_DDSVIEW_SAVEIMAGE_INC NICO_DDSVIEW_SAVEIMAGE_INC

#include <string>
#include <windows.h>

bool saveIlImage(HWND hWnd, const std::string& name, std::string* saveName = NULL);

#endif //NICO_DDSVIEW_SAVEIMAGE_INC
