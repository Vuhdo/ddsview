#include "Precompiled.h"
#include "saveimage.h"
#include <windows.h>

#include <string>
using namespace std;

#include "IL/il.h"
#include "ilstuff.h"
#include "IL/devil_internal_exports.h"

const int OPTIONS_ID = 1;
const int NO_OPTIONS_LABEL_ID = 2;
const int JPG_LABEL_ID = 3;
const int JPG_EDIT_ID = 4;
const int DDS_LABEL_ID = 5;
const int DDS_FORMAT_BOX_ID = 6;
const int DDS_MIPMAPS_CHECK_ID = 6;

void notify(const string& msg);
void denotify();

HWND createStatic(char* text, int x, int y, HWND hWnd, HINSTANCE hInst, int id)
{
  HWND ret = CreateWindow("STATIC", text,
        WS_CHILD | WS_CLIPSIBLINGS,
        x, y, 200, 14, hWnd, (HMENU)id, hInst, NULL);
  SendMessage(ret, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
  return ret;
}

UINT CALLBACK hookProc(HWND hDlg, UINT msg, WPARAM wP, LPARAM lP)
{
  static HWND s_optionsButton = NULL, s_noOptionsLabel = NULL;
  static HWND s_jpgLabel = NULL, s_jpgSlider = NULL, s_jpgEdit = NULL;
  static HWND s_ddsLabel = NULL, s_ddsCombobox = NULL, s_ddsMipmapsCheck;

  switch(msg)
  {
    case WM_INITDIALOG:
    {
      OPENFILENAME* params = (OPENFILENAME*)lP;

      HWND hParent = GetParent(hDlg);
      HINSTANCE hInst = GetModuleHandle(NULL);
      HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

      //Get size of dialog box, hdlg is only the handle to the
      //window we may put stuff in, not the whole dialog
      RECT r;
      GetClientRect(hParent, &r);
      int w = r.right - r.left;
      int h = 80;
      SetWindowPos(hDlg, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);

      //that's important, if you leave it out it looks quite screwy
      LONG style = GetWindowLong(hDlg, GWL_STYLE);
      style |= WS_CLIPSIBLINGS;
      SetWindowLong(hDlg, GWL_STYLE, style);

      //create child windows

      //jpg
      s_jpgLabel = createStatic("Compression (0-99, 80 is reasonable)", 15, 25, hDlg, hInst, JPG_LABEL_ID);
      char buff[80];
      int i = ilGetInteger(IL_JPG_QUALITY);
      sprintf(buff, "%i", i);
      s_jpgEdit = CreateWindow("EDIT", buff,
        WS_CHILD | WS_CLIPSIBLINGS | ES_NUMBER,
        15, 45, 30, 14, hDlg, (HMENU)JPG_EDIT_ID, hInst, NULL);
      SendMessage(s_jpgEdit, WM_SETFONT, (WPARAM)hFont, 0);

      //dds
      s_ddsLabel = createStatic("Save format:", 15, 32, hDlg, hInst, DDS_LABEL_ID);
      s_ddsCombobox = CreateWindow("COMBOBOX", "",
        WS_CHILD | WS_CLIPSIBLINGS | CBS_HASSTRINGS | CBS_DROPDOWNLIST,
        15, 48, 200, 400, hDlg, (HMENU)DDS_FORMAT_BOX_ID, hInst, NULL);
      SendMessage(s_ddsCombobox, WM_SETFONT, (WPARAM)hFont, 0);
      SendMessage(s_ddsCombobox, CB_ADDSTRING, 0, (LPARAM)"DXTC1");
      SendMessage(s_ddsCombobox, CB_ADDSTRING, 0, (LPARAM)"DXTC3");
      SendMessage(s_ddsCombobox, CB_ADDSTRING, 0, (LPARAM)"DXTC5");
      SendMessage(s_ddsCombobox, CB_ADDSTRING, 0, (LPARAM)"3DC/ATI2N");
      SendMessage(s_ddsCombobox, CB_ADDSTRING, 0, (LPARAM)"Doom3/RXGB");
      SendMessage(s_ddsCombobox, CB_ADDSTRING, 0, (LPARAM)"3DC+/ATI1N");
      s_ddsMipmapsCheck = CreateWindow("BUTTON", "Store Mipmaps",
        WS_CHILD | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX | WS_DISABLED,
        15, 16, 100, 14, hDlg, (HMENU)DDS_MIPMAPS_CHECK_ID, hInst, NULL);
      SendMessage(s_ddsMipmapsCheck, WM_SETFONT, (WPARAM)hFont, 0);
      SendMessage(s_ddsMipmapsCheck, BM_SETCHECK,
        (WPARAM)ilGetInteger(IL_NUM_MIPMAPS) != 0, 0);

      switch(ilGetInteger(IL_DXTC_FORMAT))
      {
        case IL_DXT1:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 0, 0);
          break;
        case IL_DXT3:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 1, 0);
          break;
        case IL_DXT5:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 2, 0);
          break;
        case IL_3DC:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 3, 0);
          break;
        case IL_RXGB:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 4, 0);
          break;
        case IL_ATI1N:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 5, 0);
          break;
        default:
          SendMessage(s_ddsCombobox, CB_SETCURSEL, 2, 0);
          break;
      }

      //common
      s_noOptionsLabel = createStatic("No options for this format", 15, 25, hDlg, hInst, NO_OPTIONS_LABEL_ID);
      s_optionsButton = CreateWindow("BUTTON", "Options",
        WS_CHILD | WS_CLIPSIBLINGS | BS_GROUPBOX | WS_VISIBLE,
        0, 0, w, h, hDlg, (HMENU)OPTIONS_ID, hInst, NULL);
      SendMessage(s_optionsButton, WM_SETFONT, (WPARAM)hFont, 0);

      //somehow windows doesn't tell us about our first filetype...
      //do that ourselves:
      OFNOTIFY ofnot;
      ofnot.lpOFN = params;
      ofnot.hdr.code = CDN_TYPECHANGE;
      SendMessage(hDlg, WM_NOTIFY, 0, (LPARAM)&ofnot);

      return 0; //default behaviour
    }

    case WM_NOTIFY:
    {
      OFNOTIFY* ofn = (OFNOTIFY*)lP;
      switch(ofn->hdr.code)
      {
        case CDN_FILEOK:
        {
          if(ofn->lpOFN->nFilterIndex == 1)
          {
            char buff[80];
            GetWindowText(s_jpgEdit, buff, 80);
            int a = atoi(buff);
            ilSetInteger(IL_JPG_QUALITY, a);
          }
        }break;
        case CDN_FOLDERCHANGE: break;
        case CDN_HELP: break;
        case CDN_INITDONE: break;
        case CDN_TYPECHANGE:
        {
          if(ofn->lpOFN->nFilterIndex == 1) //jpg
          {
            ShowWindow(s_noOptionsLabel, SW_HIDE);
            ShowWindow(s_jpgLabel, SW_SHOWNORMAL);
            ShowWindow(s_jpgEdit, SW_SHOWNORMAL);
            ShowWindow(s_ddsLabel, SW_HIDE);
            ShowWindow(s_ddsCombobox, SW_HIDE);
            ShowWindow(s_ddsMipmapsCheck, SW_HIDE);
          }
          else if(ofn->lpOFN->nFilterIndex == 6) //dds
          {
            ShowWindow(s_noOptionsLabel, SW_HIDE);
            ShowWindow(s_jpgLabel, SW_HIDE);
            ShowWindow(s_jpgEdit, SW_HIDE);
            ShowWindow(s_ddsLabel, SW_SHOWNORMAL);
            ShowWindow(s_ddsCombobox, SW_SHOWNORMAL);
            ShowWindow(s_ddsMipmapsCheck, SW_SHOWNORMAL);
          }
          else
          {
            ShowWindow(s_noOptionsLabel, SW_SHOWNORMAL);
            ShowWindow(s_jpgLabel, SW_HIDE);
            ShowWindow(s_jpgEdit, SW_HIDE);
            ShowWindow(s_ddsLabel, SW_HIDE);
            ShowWindow(s_ddsCombobox, SW_HIDE);
            ShowWindow(s_ddsMipmapsCheck, SW_HIDE);
          }
          InvalidateRect(hDlg, NULL, TRUE);
        }break;
        case CDN_SHAREVIOLATION: break;
        case CDN_SELCHANGE: break;
      }

      //tell windows not to send the deprecated
      //FILEOKSTRING, LBSELCHSTRING, SHAREVISTRING and
      //HELPMSGSTRING messages
      SetWindowLong(hDlg, DWL_MSGRESULT, TRUE);

      return 0; //default behaviour
    }

    case WM_COMMAND:
      if(LOWORD(wP) == JPG_EDIT_ID && HIWORD(wP) == EN_UPDATE)
      {
        char buff[80];
        GetWindowText(s_jpgEdit, buff, 80);
        int a = atoi(buff);
        int oldA = a;
        if(a < 0) a = 0; if(a > 99) a = 99; //clamp quality
        if(oldA != a || buff[0] == '\0')
        {  
          sprintf(buff, "%i", a);
          SetWindowText(s_jpgEdit, buff);
        }
      }
      else if(LOWORD(wP) == DDS_FORMAT_BOX_ID && HIWORD(wP) == CBN_SELCHANGE)
      {
        switch(SendMessage(s_ddsCombobox, CB_GETCURSEL, 0, 0))
        {
          case 0:
            ilSetInteger(IL_DXTC_FORMAT, IL_DXT1);
            break;
          case 1:
            ilSetInteger(IL_DXTC_FORMAT, IL_DXT3);
            break;
          case 2:
            ilSetInteger(IL_DXTC_FORMAT, IL_DXT5);
            break;
          case 3:
            ilSetInteger(IL_DXTC_FORMAT, IL_3DC);
            break;
          case 4:
            ilSetInteger(IL_DXTC_FORMAT, IL_RXGB);
            break;
          case 5:
            ilSetInteger(IL_DXTC_FORMAT, IL_ATI1N);
            break;
        }
      }
      return 0; //default behaviour
    default:
      return 0; //default behaviour
  }
}


bool saveIlImage(HWND hWnd, const std::string& name, std::string* saveName)
{
  static bool s_isInitialized = false;
  static OPENFILENAME s_saveFileName;
  if(!s_isInitialized)
  {
    //initialize "save copy as..." openfilename struct
    ZeroMemory(&s_saveFileName, sizeof(s_saveFileName));
    s_saveFileName.lStructSize = sizeof(s_saveFileName);
    s_saveFileName.hwndOwner = hWnd;

    s_saveFileName.lpstrFilter =
      "Jpeg (*.jpg,*.jpeg)\0*.jpg;*.jpeg\0"
      "Portable Network Graphic (*.png)\0*.png\0"
      "ZSoft PCX (*.pcx)\0*.pcx\0"
      "Targa (*.tga)\0*.tga\0"
      "TIFF (*.tif, *.tiff)\0*.tif;*.tiff\0"
      "DirectDrawSurface (experimental) (*.dds)\0*.dds\0"
      "Photoshop (*.psd)\0*.psd\0"
      "Windows Bitmap (*.bmp)\0*.bmp\0"
      "C Header (*.h)\0*.h\0"
      "Silicon Graphics (*.sgi, *.bw, *.rgb, *.rgba)\0*.sgi;*.bw;*.rgb;*.rgba\0"
      "Portable Bitmap (*.pbm)\0*.pbm\0"
      "Portable Greymap (*.pgm)\0*.pgm\0"
      "Portable Pixelmap (*.ppm)\0*.ppm\0";

    s_saveFileName.lpstrTitle = "Save Copy as";
    s_saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER;
    s_saveFileName.lpstrDefExt = "XXX";
    s_saveFileName.lpfnHook = hookProc;
    s_isInitialized = true;
  }

  //display save as dialog
  char buffer[MAX_PATH];
  //buffer[0] = '\0';
  strcpy(buffer, name.c_str());
  std::string::size_type pos = name.rfind('.');
  if(pos != std::string::npos)
    buffer[pos] = '\0';
  s_saveFileName.lpstrFile = buffer;
  s_saveFileName.nMaxFile = MAX_PATH;

  //set dds saving parameters - just in case
  ILuint dxtFormat = ilGetInteger(IL_DXTC_DATA_FORMAT);
  if(dxtFormat != IL_DXT_NO_COMP)
  {
    ilSetInteger(IL_DXTC_FORMAT, dxtFormat);
  }
  else if(ilHasAlpha())
  {
    //dxt1 sucks for alpha images...
    ilSetInteger(IL_DXTC_FORMAT, IL_DXT5);
  }
  else
    ilSetInteger(IL_DXTC_FORMAT, IL_DXT1);

  //initialize jpg quality to 80
  ilSetInteger(IL_JPG_QUALITY, 80);

  if(GetSaveFileName(&s_saveFileName))
  {
    UpdateWindow(hWnd);
    notify(string("Saving ") + s_saveFileName.lpstrFile + string("..."));
    if(!ilSaveImage(s_saveFileName.lpstrFile))
    {
      std::string err = "Saving of \"";
      err += s_saveFileName.lpstrFile;
      err += "\" failed, reasons:\n";
      ILenum ilErr = ilGetError();
      while(ilErr != IL_NO_ERROR)
      {
        err += iluErrorString(ilErr) + std::string("\n");
        ilErr = ilGetError();
      }
      denotify();
      MessageBox(hWnd, err.c_str(), "ERROR:", MB_OK);
      return false;
    }
    denotify();
    if(saveName != NULL)
      *saveName = s_saveFileName.lpstrFile;
    return true;
  }
  else
    return false;
}
