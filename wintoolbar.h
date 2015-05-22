
#include <windows.h>

extern const int numButtons;
extern const int TOOLBAR_FIRST_ICON;

HWND createToolbar(HINSTANCE hInst, HWND hWnd);
void updateToolbar(HWND toolbar);
LRESULT toolbarNotify(NMHDR* hdr, HWND toolbar);
LRESULT toolbarCommand(HWND viewerWnd, HWND toolbar, int id);