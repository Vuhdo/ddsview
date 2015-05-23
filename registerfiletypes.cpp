#include "Precompiled.h"
#include "registerfiletypes.h"

#include <shlobj.h>
#include "ErrorBox.h"
#include "resource.h"
using namespace std;

const char* RECBROWSE_STRING = "DeepBrowse with ddsview";

struct DlgData
{
  EXT* exts;
  bool* take;
  int count;
  bool addRecBrowse;
  bool overwriteFaxDisplay;
  string appName;
};

void registerSupportedFileTypes(const DlgData& data);

const int LABEL_ID = 230;
const int BUTTON_BASE_ID = 250;

struct EnumData
{
  HDWP hDWP;
  int yTrans;
};

BOOL CALLBACK enumProc(HWND child, LPARAM lP)
{
  EnumData* d = (EnumData*)lP;

  RECT r;
  GetWindowRect(child, &r);
  ScreenToClient(GetParent(child), (POINT*)&r);

  d->hDWP = DeferWindowPos(d->hDWP, child, NULL,
    r.left, r.top + d->yTrans, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

  return d->hDWP != NULL;
}

BOOL CALLBACK dlgProc(HWND hDlg, UINT msg, WPARAM wP, LPARAM lP)
{
  DlgData* data = (DlgData*)GetWindowLong(hDlg, GWL_USERDATA);

  switch(msg)
  {
    case WM_INITDIALOG:
    {
      SetWindowLong(hDlg, GWL_USERDATA, (LONG)lP);
      data = (DlgData*)lP;

      RECT rect, parentRect;
      GetWindowRect(GetParent(hDlg), &parentRect);
      GetWindowRect(hDlg, &rect);
      int w = rect.right - rect.left;
      int h = rect.bottom - rect.top;

      int extraSpace = 15*data->count/3 + 40;
      h += extraSpace;

      //center window
      SetWindowPos(hDlg, NULL, (parentRect.right + parentRect.left - w)/2,
        (parentRect.bottom + parentRect.top - h)/2, w, h, SWP_NOZORDER);

      //move existing windows
      EnumData enumData;
      enumData.yTrans = extraSpace;
      enumData.hDWP = BeginDeferWindowPos(5);
      EnumChildWindows(hDlg, enumProc, (LPARAM)&enumData);
      if(enumData.hDWP != NULL)
        EndDeferWindowPos(enumData.hDWP);

      //create checkboxes
      int countOver2 = data->count/3;
      for(int i = 0; i < data->count; ++i)
      {
        HWND tempWnd = CreateWindow("BUTTON", data->exts[i],
          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
          90 + 60*(i/countOver2), 50 + (i%countOver2)*15,
           50, 15, hDlg, (HMENU)(BUTTON_BASE_ID + i),
          (HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), NULL);
        SendMessage(tempWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
        CheckDlgButton(hDlg, BUTTON_BASE_ID + i, TRUE);
      }
      CheckDlgButton(hDlg, ADD_RECBROWSE_ID, TRUE);
      CheckDlgButton(hDlg, OVERWRITE_FAX_ID, FALSE);

      //create label
      string label = "The selected file types will be opened with\n" + data->appName;
      HWND tempWnd = CreateWindow("STATIC", label.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 10, 300, 28, hDlg, (HMENU)LABEL_ID,
        (HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), NULL);
      SendMessage(tempWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

      return TRUE;
    }break;

    case WM_COMMAND:
    {
      switch(LOWORD(wP))
      {
        case IDOK:
        {
          data->overwriteFaxDisplay = IsDlgButtonChecked(hDlg, OVERWRITE_FAX_ID) == TRUE;
          data->addRecBrowse = IsDlgButtonChecked(hDlg, ADD_RECBROWSE_ID) == TRUE;
          for(int i = 0; i < data->count; ++i)
            data->take[i] = IsDlgButtonChecked(hDlg, BUTTON_BASE_ID + i) == TRUE;

          EndDialog(hDlg, TRUE);
        }break;

        case IDCANCEL:
          EndDialog(hDlg, FALSE);
          break;

        case SELECT_ALL_ID:
        case SELECT_NONE_ID:
        {
          for(int i = 0; i < data->count; ++i)
            CheckDlgButton(hDlg, BUTTON_BASE_ID + i, LOWORD(wP) == SELECT_ALL_ID);
        }break;

        case SELECT_FREE_ID:
        {
          for(int i = 0; i < data->count; ++i)
          {
            BOOL select = TRUE;
            string t = "."; t += data->exts[i];

            HKEY key;
            if(RegOpenKeyEx(HKEY_CLASSES_ROOT, t.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
            {
              DWORD type, buffSize = 80;
              unsigned char buff[80];
              if(RegQueryValueEx(key, "", NULL, &type, buff, &buffSize) == ERROR_SUCCESS)
              {
                if(type != REG_SZ || (strcmp((char*)buff, "ddsview") != 0 && strcmp((char*)buff, "") != 0))
                  select = FALSE;
              }
              RegCloseKey(key);
            }

            CheckDlgButton(hDlg, BUTTON_BASE_ID + i, select);
          }
        }break;
      }
      return TRUE;
    }break;

    default:
      return FALSE;
  }
}

bool registerFileTypesDialog(HWND hWnd, EXT* exts, int extCount)
{
  //register supported formats in the registry

  //get file name of ddsview.exe (with path)
  char appName[MAX_PATH];
  int appNameLength = GetModuleFileName(NULL, appName, MAX_PATH);
  if(appNameLength == 0)
    return 0; //error

  DlgData data;
  data.exts = exts;
  data.count = extCount;
  data.take = new bool[extCount];
  data.addRecBrowse = true;
  data.overwriteFaxDisplay = false;
  data.appName = appName;

  if(DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_REGISTER_FILETYPES),
    hWnd, dlgProc, (LONG)&data))
  {
    registerSupportedFileTypes(data);
    delete [] data.take;
    return true;
  }

  delete [] data.take;
  return false;
}

//Taken from MSDN examples:
//*************************************************************
//
//  RegDelnodeRecurse()
//
//  Purpose:    Deletes a registry key and all it's subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

BOOL RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey)
{
  LPTSTR lpEnd;
  LONG lResult;
  DWORD dwSize;
  TCHAR szName[MAX_PATH];
  HKEY hKey;
  FILETIME ftWrite;

  // First, see if we can delete the key without having
  // to recurse.

  lResult = RegDeleteKey(hKeyRoot, lpSubKey);

  if(lResult == ERROR_SUCCESS) 
    return TRUE;

  lResult = RegOpenKeyEx (hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);

  if(lResult != ERROR_SUCCESS) 
  {
    if(lResult == ERROR_FILE_NOT_FOUND)
      return TRUE;
    else
      return FALSE;
  }

  // Check for an ending slash and add one if it is missing.

  lpEnd = lpSubKey + lstrlen(lpSubKey);

  if(*(lpEnd - 1) != TEXT('\\')) 
  {
    *lpEnd =  TEXT('\\');
    lpEnd++;
    *lpEnd =  TEXT('\0');
  }

  // Enumerate the keys

  dwSize = MAX_PATH;
  lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                         NULL, NULL, &ftWrite);

  if(lResult == ERROR_SUCCESS) 
  {
    do
    {
      lstrcpy (lpEnd, szName);

      if (!RegDelnodeRecurse(hKeyRoot, lpSubKey))
        break;

      dwSize = MAX_PATH;

      lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                             NULL, NULL, &ftWrite);

    } while(lResult == ERROR_SUCCESS);
  }

  lpEnd--;
  *lpEnd = TEXT('\0');

  RegCloseKey (hKey);

  // Try again to delete the key.

  lResult = RegDeleteKey(hKeyRoot, lpSubKey);

  if (lResult == ERROR_SUCCESS) 
    return TRUE;

  return FALSE;
}

//*************************************************************
//
//  RegDelnode()
//
//  Purpose:    Deletes a registry key and all it's subkeys / values.
//
//  Parameters: hKeyRoot    -   Root key
//              lpSubKey    -   SubKey to delete
//
//  Return:     TRUE if successful.
//              FALSE if an error occurs.
//
//*************************************************************

BOOL RegDelnode (HKEY hKeyRoot, LPTSTR lpSubKey)
{
  TCHAR szDelKey[2 * MAX_PATH];

  lstrcpy (szDelKey, lpSubKey);
  return RegDelnodeRecurse(hKeyRoot, szDelKey);
}

//this writes to the registry, only call it if you really want to...
void registerSupportedFileTypes(const DlgData& data)
{
  string regStr = data.appName + " %1";
  string regStr2 = data.appName + ",1";

  HKEY key;

  //Create program key
  RegCreateKeyEx(HKEY_CLASSES_ROOT, "ddsview", 0,
    NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);

  HKEY shell, open, command, defaultIcon;
  RegCreateKeyEx(key, "shell", 0,
    NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &shell, NULL);
  RegCreateKeyEx(shell, "open", 0,
    NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &open, NULL);
  RegCreateKeyEx(open, "command", 0,
    NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &command, NULL);

  // tell windows what the open command does
  RegSetValueEx(command, "", 0, REG_SZ, (CONST BYTE*)regStr.c_str(), regStr.length() + 1);

  //this sets the default action
  RegSetValueEx(shell, "", 0, REG_SZ, (CONST BYTE*)"open", 4);

  RegCloseKey(command);
  RegCloseKey(open);
  RegCloseKey(shell);

  // set icon
  RegCreateKeyEx(key, "DefaultIcon", 0,
    NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &defaultIcon, NULL);

  RegSetValueEx(defaultIcon, "", 0, REG_SZ, (CONST BYTE*)regStr2.c_str(), regStr2.length() + 1);

  RegCloseKey(defaultIcon);

  RegCloseKey(key);


  //add RecBrowse to context menu of all folders
  if(RegOpenKeyEx(HKEY_CLASSES_ROOT, "Folder\\shell",
    0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
  {
    if(data.addRecBrowse)
    {
      //add RecBrowse command
      HKEY recBrowse, command;
      RegCreateKeyEx(key, "ddsviewRecBrowse", 0,
        NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &recBrowse, NULL);

      RegCreateKeyEx(recBrowse, "command", 0,
        NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &command, NULL);
      RegSetValueEx(command, "", 0, REG_SZ, (CONST BYTE*)regStr.c_str(), regStr.length() + 1);
      RegCloseKey(command);

      RegSetValueEx(recBrowse, "", 0, REG_SZ,
        (CONST BYTE*)RECBROWSE_STRING, strlen(RECBROWSE_STRING) + 1);
      RegCloseKey(recBrowse);
    }
    else
    {
      //remove RecBrowse command
      //(RegDeleteKeyEx() cannot be used because it deletes
      //only keys with no subkeys on NT style systems...)
      RegDelnode(key, "ddsviewRecBrowse");
    }

    RegCloseKey(key);
  }

  //overwrite image & fax display on xp
  if(RegOpenKeyEx(HKEY_CLASSES_ROOT, "SystemFileAssociations\\image\\ShellEx\\ContextMenuHandlers\\ShellImagePreview",
    0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
  {
    if(data.overwriteFaxDisplay)
    {
       //no idea if this is the correct way to do that...
      if(RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE*)"ddsview", 7 + 1) != ERROR_SUCCESS)
        ErrorBox("Failed to overwrite image & fax display as default image viewer");
    }
    else
    {
      //but this is right for sure (well, quite sure)

      //key exists, check if it is associated with ddsview
      char buff[20] = "";
      DWORD size = 20, type;
      if(RegQueryValueEx(key, "", 0, &type, (BYTE*)buff, &size) == ERROR_SUCCESS
        && type == REG_SZ && strcmp(buff, "ddsview") == 0)
      {
        //default viewer was overwritten, restore default
        RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE*)"{e84fda7c-1d6a-45f6-b725-cb260c236066}", 38 + 1);
      }
    }

    RegCloseKey(key);
  }



  //create file associations
  int numTaken = 0;
  for(int i = 0; i < data.count; ++i)
  {
    string t = "."; t += data.exts[i];

    if(!data.take[i])
    {
      //remove association
      if(RegOpenKeyEx(HKEY_CLASSES_ROOT, t.c_str(), 0, KEY_ALL_ACCESS,
        &key) == ERROR_SUCCESS)
      {
        //key exists, check if it is associated with ddsview
        char buff[20] = "";
        DWORD size = 20, type;
        if(RegQueryValueEx(key, "", 0, &type, (BYTE*)buff, &size) == ERROR_SUCCESS
          && type == REG_SZ && strcmp(buff, "ddsview") == 0)
        {
          //this file is associated with ddsview, remove association:
          RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE*)"", 0 + 1);
        }

        RegCloseKey(key);
      }
    }
    else
    {
      //add association
      if(RegCreateKeyEx(HKEY_CLASSES_ROOT, t.c_str(), 0,
         NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL)
         == ERROR_SUCCESS)
      {
        ++numTaken;
        RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE*)"ddsview", 7 + 1);

        //RegDeleteKey(key, "DefaultIcon");
        //RegDeleteKey(key, "shell");

        RegCloseKey(key);
      }
      else
        ErrorBox((string("Failed to access key ") + data.exts[i]).c_str());
    }
  }

  if(!data.addRecBrowse && !data.overwriteFaxDisplay && numTaken == 0)
  {
    //nothing selected - remove ddsview key from the registry
    RegDelnode(HKEY_CLASSES_ROOT, "ddsview");
  }

  //tell the system we changed some associations
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

  //also update the app's image
  SHFILEINFO info;
  SHGetFileInfo(data.appName.c_str(), 0, &info, sizeof(info), SHGFI_ICON);
  DestroyIcon(info.hIcon);
  SHChangeNotify(SHCNE_UPDATEIMAGE, SHCNF_DWORD, &info.iIcon, NULL);
}
