#include "InputBox.h"

//INPUT BOX
struct StringInputData
{
  std::string inputText;
  std::string staticText;
  int charCount;
  bool done;
  bool aborted;
  WNDPROC editProc;
  HWND okButton, cancelButton, inputEdit, textStatic;
};

LRESULT CALLBACK editSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  StringInputData* data = (StringInputData*)GetWindowLong(GetParent(hWnd), GWL_USERDATA);

  if(msg == WM_CHAR)
  {
    switch(wParam)
    {
      case VK_RETURN:
        SendMessage(GetParent(hWnd), WM_COMMAND, (WPARAM)IDOK, 0);
        return 0;
      case VK_ESCAPE:
        SendMessage(GetParent(hWnd), WM_COMMAND, (WPARAM)IDCANCEL, 0);
        return 0;
        break;
      default:
        return CallWindowProc(data->editProc, hWnd, msg, wParam, lParam);
    }
  }
  else
    return CallWindowProc(data->editProc, hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK inputBoxProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  StringInputData* data = (StringInputData*)GetWindowLong(hWnd, GWL_USERDATA);

  switch(msg)
  {
    case WM_CREATE:
    {
      data = (StringInputData*)(((CREATESTRUCT*)lParam)->lpCreateParams);
      SetWindowLong(hWnd, GWL_USERDATA, (LONG)data);

      //center window on parent window, fit to font
      HFONT font = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
      HDC hDc = GetDC(hWnd);
      SelectObject(hDc, font);
      TEXTMETRIC tm;
      GetTextMetrics(hDc, &tm);
      ReleaseDC(hWnd, hDc);
      int w = data->charCount*tm.tmAveCharWidth;
      int h = 43 + 3*tm.tmHeight + GetSystemMetrics(SM_CYCAPTION) + 2*GetSystemMetrics(SM_CYSIZEFRAME);

      RECT parentRect;
      if(GetParent(hWnd) != NULL)
        GetWindowRect(GetParent(hWnd), &parentRect);
      else
        SystemParametersInfo(SPI_GETWORKAREA, 0, &parentRect, 0);

      RECT r;
      GetWindowRect(hWnd, &r);
      r.left = parentRect.left + (parentRect.right - parentRect.left - w)/2;
      r.top = parentRect.top + (parentRect.bottom - parentRect.top - h)/2;
      SetWindowPos(hWnd, NULL, r.left, r.top, w, h, SWP_NOZORDER);

      int bw = tm.tmAveCharWidth*19;
      int bh = tm.tmHeight + 10;

      GetClientRect(hWnd, &r);
      HINSTANCE hInst = ((CREATESTRUCT*)lParam)->hInstance;
      data->okButton = CreateWindowEx(0, "BUTTON", "OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                r.right - 2*bw - 20, r.bottom - bh - 10, bw,
                                bh, hWnd, (HMENU)IDOK, hInst, NULL);
      data->cancelButton = CreateWindowEx(0, "BUTTON", "Cancel", 
                                    WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                    r.right - bw - 10, r.bottom - bh - 10, bw,
                                    bh, hWnd, (HMENU)IDCANCEL, hInst, NULL);
      data->textStatic = CreateWindowEx(0, "STATIC", data->staticText.c_str(),
                                  WS_VISIBLE | WS_CHILD | SS_LEFT,
                                  10, 5, r.right, tm.tmHeight, hWnd, 0, hInst, NULL);
      data->inputEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", data->inputText.c_str(),
                                 WS_VISIBLE | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL,
                                 10, tm.tmHeight + 10, r.right - 20, tm.tmHeight + 8, hWnd,
                                 0, hInst, NULL);

      //subclass input box
      data->editProc = (WNDPROC)GetWindowLong(data->inputEdit, GWL_WNDPROC);
      SetWindowLong(data->inputEdit, GWL_WNDPROC, (LONG)editSubclassProc);
      SendMessage(data->inputEdit, EM_SETSEL, 0, -1);

      SendMessage(data->textStatic, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
      SendMessage(data->okButton, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
      SendMessage(data->cancelButton, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
      SendMessage(data->inputEdit, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));

      SetFocus(data->inputEdit);

      return 0;
    }break;

    case WM_COMMAND:
    {
      switch(LOWORD(wParam))
      {
        case IDOK:
        {
          data->done = true;
          data->aborted = false;
          data->inputText = getWindowText(data->inputEdit);
        }break;
        case IDCANCEL:
        {
          data->done = true;
          data->aborted = true;
        }break;
      }
      return 0;
    }break;

    case WM_CLOSE:
    {
      data->done = true;
      data->aborted = true;
      return 0;
    }break;

    default:
      return DefWindowProc(hWnd, msg, wParam, lParam);
  }
}

bool inputBoxString(HWND owner, const std::string text, const std::string& caption,
                    std::string& dest, int inputWidth)
{
  HINSTANCE hInst = (HINSTANCE) GetWindowLong(owner, GWL_HINSTANCE);
  static WNDCLASSEX wc = { sizeof(wc), 0, inputBoxProc, 0, 0, hInst,
                           LoadIcon(NULL, IDI_WINLOGO), LoadCursor(NULL, IDC_ARROW),
                           (HBRUSH)(COLOR_BTNFACE+1), NULL, "stringInputBox class",
                           LoadIcon(NULL, IDI_WINLOGO)
                         };

  if(!RegisterClassEx(&wc))
    return false;

  StringInputData data;
  data.inputText = dest;
  data.staticText = text;
  data.charCount = inputWidth;
  data.done = false;
  data.aborted = true;

  HWND hWnd = CreateWindowEx(0, "stringInputBox class", caption.c_str(), 
                             WS_DLGFRAME | WS_BORDER | WS_SYSMENU | WS_POPUPWINDOW,
                             0, 0, 0, 0, owner, NULL, hInst, (void*)&data);
  if(hWnd == NULL)
  {
    UnregisterClass("stringInputBox class", hInst);
    return false;
  }

  ShowWindow(hWnd, SW_SHOWNORMAL);

  HWND parent = owner;
  if(parent != NULL)
    EnableWindow(parent, FALSE);
    
  MSG msg;
  while(GetMessage(&msg, NULL, 0, 0) && !(data.done))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  if(parent != NULL)
    EnableWindow(parent, TRUE);

  DestroyWindow(hWnd);
  UnregisterClass("stringInputBox class", hInst);
  if(data.aborted == true)
    return false;
  
  dest = data.inputText;
  return true;
}

std::string getWindowText(HWND hWnd)
{
  int n = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0) + 1;
  char* buf = new char[n];
  SendMessage(hWnd, WM_GETTEXT, (WPARAM)n, (LPARAM)buf);
  std::string str = std::string(buf);
  delete [] buf;
  return str;
}

