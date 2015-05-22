#ifndef CHECKY_NICO_WINUTILS_INC
#define CHECKY_NICO_WINUTILS_INC CHECKY_NICO_WINUTILS_INC

#include <string>
#include <windows.h>

bool inputBoxString(HWND owner, const std::string text, const std::string& caption,
                    std::string& dest, int inputWidth = 50);
std::string getWindowText(HWND hWnd);


#endif //CHECKY_NICO_WINUTILS_INC
