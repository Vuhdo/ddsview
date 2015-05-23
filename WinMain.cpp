#include "Precompiled.h"

/*
This fastens up drawing:
No doublebuffer
Scrollwindow
No Background-brush, erase background self
SetStretchBltMode(BLACKONWHITE) for zoom out
*/

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
using namespace std;

#include <zmouse.h> //mouse wheel on win95/NT3.5
#define COMPILE_NEWAPIS_STUBS
#define WANT_GETLONGPATHNAME_WRAPPER
#include "GetLongPathName.h" //GetLongPathName() for win95

//win95 has no multimonitor functions build in -
//but luckily the win32 SDK takes care of that				
//and fakes them for us if we tell it to
//(if you don't have this file in your win32 SDK,
//grab multimon.h from the net or update your SDK)
#define COMPILE_MULTIMON_STUBS
#include <multimon.h>

#include "il/il.h"
#include "il/ilu.h"
#include "il/ilut.h"

#include "globalstuff.h"

#include "ErrorBox.h"
#include "InputBox.h"
#include "resource.h"
#include "rotbmp.h"
#include "saveimage.h"
#include "registerfiletypes.h"
#include "wintoolbar.h"

#include "image.h"

//#include "imdebug.h"
#include "ilstuff.h"

#include "oledrag.h"
#include "file.h"


// GLOBALS

const string CLIPBOARD_IMAGE = "--@CLIPBOARD IMAGE";

//OS DEPENDENT

const char* CLASS_NAME = ".dds viewer window class";
const char* WINDOW_TITLE = "www.amnoid.de/ddsview/ v.663b (press h for controls)";
const int WINDOW_STYLE =  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
HINSTANCE g_hInst;
HWND g_hWnd = NULL, g_statusbar = NULL, g_toolbar = NULL;
bool g_displayToolbar = true;

//Win95/NT3.5 mousewheel stuff
bool g_win95HasMousewheel = false;
UINT g_win95MousewheelMessage;


//DRAWAPI DEPENDENT

HBITMAP g_hBm[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
HBITMAP g_hBmAlpha[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
HBITMAP g_hBmBlended[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

//if the current image has more than 6 layers, the current layer bitmap
//is loaded onto this bitmap (if it's not one of the first 6, of course)
HBITMAP g_layerBuff = NULL, g_layerBuffAlpha = NULL, g_layerBuffBlended = NULL;
ILuint g_layerNr = 0, g_layerAlphaNr = 0, g_layerBlendedNr = 0;


//COMMON STATE


string g_waitMessage;
string g_baseDir;

//viewport state
bool g_isFullscreen = false;
int g_w = 0, g_h = 0, g_dx = 0, g_dy = 0, g_mx, g_my, g_startDx = 0, g_startDy = 0;
float g_scale = 1.0f;
int g_border = 0;



//win95 has no multimonitor functions, but multimon.h emulates
//them for us

//returns workarea rect of the monitor the ddsview window is on
RECT getWorkareaRect()
{
  RECT ret;
  HMONITOR monitor = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(monitor, &mi);
  ret = mi.rcWork;
  return ret;
}

//returns fullscreen rect of the monitor the ddsview window is on
RECT getFullscreenRect()
{
  RECT ret;
  HMONITOR monitor = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(monitor, &mi);
  ret = mi.rcMonitor;
  return ret;
}

int getStatusbarHeight()
{
  RECT r;
  GetClientRect(g_statusbar, &r);
  return r.bottom;
}

int getToolbarHeight()
{
  if(g_displayToolbar && !g_isFullscreen)
  {
    RECT r;
    //this has to be changed if the toolbar is
    //created without CCS_NODIVIDER style
    GetClientRect(g_toolbar, &r);
    return r.bottom;
  }
  else
    return 0;
}

void getRealClientRect(RECT& r)
{
  GetClientRect(g_hWnd, &r);

  if(!g_isFullscreen)
  {
    //subtract statusbar height
    r.bottom -= getStatusbarHeight();
    r.top += getToolbarHeight();
  }
}

bool isCursorInClientRect()
{
  //get cursor pos in client coords
  POINT cursorPos;
  GetCursorPos(&cursorPos);
  ScreenToClient(g_hWnd, &cursorPos);

  //get client rect (without toolbar)
  RECT clientRect;
  getRealClientRect(clientRect);

  return PtInRect(&clientRect, cursorPos) == TRUE;
}

int getClientWidth()
{
  RECT t;
  getRealClientRect(t);
  return t.right - t.left;
}

int getClientHeight()
{
  RECT t;
  getRealClientRect(t);
  return t.bottom - t.top;
}

void bindCurrImage()
{
  if(g_currILImage == 0)
    return;

  ilBindImage(g_currILImage);
  ilActiveImage(g_currLayer);
  ilActiveMipmap(g_currMipmapLevel);
}

int getRealImageWidth()
{
  if(g_currILImage == 0)
    return 0;

  bindCurrImage();
  return ilGetInteger(IL_IMAGE_WIDTH);
}

int getRealImageHeight()
{
  if(g_currILImage == 0)
    return 0;

  bindCurrImage();
  return ilGetInteger(IL_IMAGE_HEIGHT);
}

int getImageBpp()
{
  if(g_currILImage == 0)
    return 0;

  bindCurrImage();
  return ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
}

ILboolean hasImageAlpha()
{
  if(g_currILImage == 0)
    return IL_FALSE;

  bindCurrImage();
  return ilHasAlpha();
}

ILboolean isImageCubemap()
{
  if(g_currILImage == 0)
    return IL_FALSE;

  ilBindImage(g_currILImage); // needed for ilIsCubemap()
  bool ret = ilIsCubemap() == IL_TRUE;
  bindCurrImage(); //undo ilBindImage() which was needed for ilIsCubemap()
  return ret;
}

int getDisplayImageWidth()
{
  int rw = getRealImageWidth();
  int rh = getRealImageHeight();

  if(g_displayMode == DM_CUBE_1)
  {
    rw = 4*rw;
    rh = 3*rh;
  }

  if(g_displayAlpha && g_displayBlended) //TODO rot
    return (int)(g_scale*rw + .5f);

  switch(g_rotation)
  {
    case ROT_NONE:
    case ROT_180:
      return (int)(g_scale*rw + .5f);
    case ROT_90:
    case ROT_270:
      return (int)(g_scale*rh + .5f);
  } return -1; //shut up compiler warning
}

int getDisplayImageHeight()
{
  int rw = getRealImageWidth();
  int rh = getRealImageHeight();

  if(g_displayMode == DM_CUBE_1)
  {
    rw = 4*rw;
    rh = 3*rh;
  }

  if(g_displayAlpha && g_displayBlended) //TODO rot
    return (int)(g_scale*rh + .5f);

  switch(g_rotation)
  {
    case ROT_NONE:
    case ROT_180:
      return (int)(g_scale*rh + .5f);
    case ROT_90:
    case ROT_270:
      return (int)(g_scale*rw + .5f);
  } return -1; //shut up compiler warning
}

bool doesImageFitWidth()
{
  return getDisplayImageWidth() + 2*g_border <= getClientWidth();
}

bool doesImageFitHeight()
{
  return getDisplayImageHeight() + 2*g_border <= getClientHeight();
}

bool doesImageFit()
{
  return doesImageFitWidth() && doesImageFitHeight();
}

void setCursor()
{
  if(isCursorInClientRect() //if the cursor is in the client area...
     && !doesImageFit() )//...and the image is taller than this area...
    //...set pan cursor
    SetCursor(LoadCursor(NULL, IDC_SIZEALL));
  else
    //...set default cursor
    SetCursor((HCURSOR)GetClassLong(g_hWnd, GCL_HCURSOR));
}

//updates window size except when the window
//is maximized or fullscreen
void setWindowSize(const RECT& r)
{
  if(g_isFullscreen)
    return;

  //only resize if window is not maximized
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  GetWindowPlacement(g_hWnd, &wp);
  if(wp.showCmd == SW_MAXIMIZE)
    return;


  //SWP_NOCOPYBITS makes the resize look smoother (for
  //example, if tile view is toggled). This is required
  //because the window class was created without the
  //CS_V/HREDRAW styles (to prevent toolbar flicker)
  SetWindowPos(g_hWnd, NULL, r.left, r.top,
    r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOCOPYBITS);
}

void updateWindowSize()
{
  //cursor can change even if window size does
  //not change - for example, if the display mode
  //is changed when the window is fullscreen
  setCursor();

  //only resize if not fullscreen and if the window
  //size is not locked
  if(g_isFullscreen || g_lockWindowSize)
    return;

  //get window rect
  //(try to keep window center where it is)
  RECT windowRect;
  GetWindowRect(g_hWnd, &windowRect);
  int middleX = (windowRect.left + windowRect.right)/2;
  int middleY = (windowRect.top + windowRect.bottom)/2;

  int bmWid = getDisplayImageWidth();
  int bmHyt = getDisplayImageHeight();

  //calculate needed size
  int statusbarHeight = getStatusbarHeight();
  RECT r = { 0, 0, bmWid + 2*g_border,
    bmHyt + 2*g_border + statusbarHeight + getToolbarHeight() };
  AdjustWindowRect(&r, WINDOW_STYLE, FALSE);

  int w = r.right - r.left;
  int h = r.bottom - r.top;

  //don't make window larger than desktop area
  RECT workarea = getWorkareaRect();
  int screenW = workarea.right - workarea.left;
  int screenH = workarea.bottom - workarea.top;

  if(w > screenW) w = screenW; else if(w < 640) w = 640;
  if(h > screenH) h = screenH;

  //keep window on screen
  int x = middleX - w/2;
  if(x < workarea.left) x = workarea.left;
  if(x + w > workarea.right) x = workarea.right - w - 1;
  int y = middleY - h/2;
  if(y < workarea.top) y = workarea.top;
  if(y + h > workarea.bottom) y = workarea.bottom - h - 1;

  RECT r2 = { x, y, x + w, y + h };
  setWindowSize(r2);
}

string getPixelDesc(int x, int y)
{
  if(g_currILImage == 0)
    return "";

  if(x < 0 || y < 0)
    return "";

  int w = ilGetInteger(IL_IMAGE_WIDTH);
  int h = ilGetInteger(IL_IMAGE_HEIGHT);

  if(x >= w || y >= h)
    return "";


  char buff[1024];
  int pos = sprintf(buff, " (%d, %d) = ", x, y);
  int bytePerPixel = ilGetInteger(IL_IMAGE_BPP);
  int channelCount = bytePerPixel/ilGetInteger(IL_IMAGE_BPC);

  //use correct image origin
  switch(ilGetInteger(IL_IMAGE_ORIGIN))
  {
    case IL_ORIGIN_UPPER_LEFT:
      break;

    case IL_ORIGIN_LOWER_LEFT:
      y = h - 1 - y;
      break;
  }

  //label channels
  char type[4] = { 'r', 'g', 'b', 'a' };
  switch(ilGetInteger(IL_IMAGE_FORMAT))
  {
    case IL_BGR:
    case IL_BGRA:
      type[0] = 'b'; type[2] = 'r';
      break;
    case IL_COLOUR_INDEX:
      type[0] = 'i';
      break;
    case IL_LUMINANCE:
    case IL_LUMINANCE_ALPHA:
      type[0] = 'l'; type[1] = 'a';
      break;
  }

  //get channel values
  ILubyte* data = ilGetData() + (y*w + x)*bytePerPixel;
  int index = -1;
  for(int i = 0; i < channelCount; ++i)
  {
    switch(ilGetInteger(IL_IMAGE_TYPE))
    {
      case IL_BYTE:
      case IL_UNSIGNED_BYTE:
        if(index == -1) index = *(ILubyte*)(data + i);
        pos += sprintf(buff + pos, "%c%d ", type[i], *(ILubyte*)(data + i));
        break;

      case IL_SHORT:
      case IL_UNSIGNED_SHORT:
        if(index == -1) index = *(ILushort*)(data + 2*i);
        pos += sprintf(buff + pos, "%c%d ", type[i], *(ILushort*)(data + 2*i));
        break;

      case IL_INT:
      case IL_UNSIGNED_INT:
        if(index == -1) index = *(ILuint*)(data + 4*i);
        pos += sprintf(buff + pos, "%c%d ", type[i], *(ILuint*)(data + 4*i));
        break;

      case IL_FLOAT:
        pos += sprintf(buff + pos, "%c%f ", type[i], *(ILfloat*)(data + 4*i));
        break;

      case IL_DOUBLE:
        pos += sprintf(buff + pos, "%c%f ", type[i], *(ILdouble*)(data + 8*i));
        break;
    }
  }

  //for palettized images, display the color the index refers to as well
  if(ilGetInteger(IL_IMAGE_FORMAT) == IL_COLOUR_INDEX
     && index < ilGetInteger(IL_PALETTE_NUM_COLS))
  {
    ILubyte* pal = ilGetPalette() + index*ilGetInteger(IL_PALETTE_BPP);
    switch(ilGetInteger(IL_PALETTE_TYPE))
    {
      case IL_PAL_RGB24:
      case IL_PAL_RGB32:
        pos += sprintf(buff + pos, "(-> r%d g%d b%d)", pal[0], pal[1], pal[2]);
        break;
      case IL_PAL_BGR24:
      case IL_PAL_BGR32:
        pos += sprintf(buff + pos, "(-> r%d g%d b%d)", pal[2], pal[1], pal[0]);
        break;
      case IL_PAL_RGBA32:
        pos += sprintf(buff + pos, "(-> r%d g%d b%d a%d)", pal[0], pal[1], pal[2], pal[3]);
        break;
      case IL_PAL_BGRA32:
        pos += sprintf(buff + pos, "(-> r%d g%d b%d a%d)", pal[2], pal[1], pal[0], pal[3]);
        break;
    }
  }

  return string(buff);
}

void updateStatusBar(bool doUpdateToolbar = true)
{
  if(g_imageName != "")
  {
    char buff[1024];

    //a statusbar field can only store up to 127 chars, clip string at left
    //end if it is too long (TODO: find a way to display more than 127 chars...)
    int len = g_imageName.length();
    if(g_hBm[0] != NULL) len += strlen(" (FAILED)");
    len += strlen("\t\t      ");
    sprintf(buff, "\t\t%s%s      ", //leave some room for the sizing grabber
      g_imageName.c_str() + max(0, len - 127), (g_hBm[0] == NULL)?" (FAILED)":"");
    SendMessage(g_statusbar, SB_SETTEXT, 5, (LPARAM)buff);


    sprintf(buff, "\t\t%.2f%%", g_scale*100);
    SendMessage(g_statusbar, SB_SETTEXT, 2, (LPARAM)buff);
    
    sprintf(buff, "%.2f sec", g_bmLoadTime + g_hbmConvTime + g_alphaConvTime);
    SendMessage(g_statusbar, SB_SETTEXT, 3, (LPARAM)buff);

    SendMessage(g_statusbar, SB_SETTEXT, 4, (LPARAM)g_imageFileSize.c_str());

    int writtenCount = sprintf(buff, "%ix%i", getRealImageWidth(), getRealImageHeight());
    if(g_bmDep != 1)
      writtenCount += sprintf(buff + writtenCount, "x%i (%i)", g_bmDep, g_currSlice + 1);

    if(g_bmLayers != 0 || g_bmMipmaps != 0)
    {
      writtenCount += sprintf(buff + writtenCount, " (");
      if(g_bmLayers != 0)
      {
        if(isImageCubemap())
          writtenCount += sprintf(buff + writtenCount, "cubemap");
        else
          writtenCount += sprintf(buff + writtenCount, "layer %i/%i", g_currLayer + 1, g_bmLayers + 1);
      }
      if(g_bmLayers != 0 && g_bmMipmaps != 0)
        writtenCount += sprintf(buff + writtenCount, ", ");
      if(g_bmMipmaps != 0)
        writtenCount += sprintf(buff + writtenCount, "mipmap %i/%i",
                                  g_currMipmapLevel + 1, g_bmMipmaps + 1);
      writtenCount += sprintf(buff + writtenCount, ")");
    }
    sprintf(buff + writtenCount, ", %i bpp%s", getImageBpp(), hasImageAlpha()?
      (g_displayAlpha?(g_displayBlended?" +ALPHA":" *ALPHA"):" ALPHA"):"");

    SendMessage(g_statusbar, SB_SETTEXT, 1, (LPARAM)buff);

  }

  if(doUpdateToolbar && g_displayToolbar)
    updateToolbar(g_toolbar);
}

void updateTitle(bool updateStatus = true, int x = -1, int y = -1)
{
  string title = WINDOW_TITLE;

  if(g_lockWindowSize)
    title += " LOCKED";

  if((GetWindowLong(g_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0)
    title += " TOPMOST";

  if(g_imageName != "")
  {
    if(g_hBm[0] == NULL)
      title += " (failed to load)";
  }
  title += getPixelDesc(x, y);
  SetWindowText(g_hWnd, title.c_str());

  if(updateStatus)
    updateStatusBar();
}

void repaint()
{
  InvalidateRect(g_hWnd, NULL, FALSE);
}

string byteCountToString(int bytes)
{
  //terrabyte should suffice...it'll take long enough to load that ;-)
  const char* suffix[] = { "byte", "kb", "Mb", "Gb", "Tb" };
  int i = 0;
  float size = (float)bytes;
  while(size >= 1024.f && i <= 4)
  {
    size /= 1024.f;
    ++i;
  }
  char buff[30];
  sprintf(buff, "%.2f %s", size, suffix[i]);
  return buff;
}

void deleteHBitmaps()
{
  //delete previously loaded images
  for(int i = 0; i < 6; ++i)
  {
    if(g_hBm[i] != NULL)
    {
      DeleteObject(g_hBm[i]);
      g_hBm[i] = NULL;
    }
    if(g_hBmAlpha[i] != NULL)
    {
      DeleteObject(g_hBmAlpha[i]);
      g_hBmAlpha[i] = NULL;
    }
    if(g_hBmBlended[i] != NULL)
    {
      DeleteObject(g_hBmBlended[i]);
      g_hBmBlended[i] = NULL;
    }
  }

  if(g_layerBuff != NULL)
  {
     DeleteObject(g_layerBuff);
     g_layerBuff = NULL;
  }
  if(g_layerBuffAlpha != NULL)
  {
     DeleteObject(g_layerBuffAlpha);
     g_layerBuffAlpha = NULL;
  }
  if(g_layerBuffBlended != NULL)
  {
    DeleteObject(g_layerBuffBlended);
    g_layerBuffBlended = NULL;
  }
  g_layerNr = 0;
  g_layerAlphaNr = 0;
  g_layerBlendedNr = 0;
}

void notify(const string& msg)
{
  g_waitMessage = msg;
  HDC dc = GetDC(g_hWnd);
  HFONT oldFont = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
  RECT r;
  getRealClientRect(r);
  DrawText(dc, g_waitMessage.data(), g_waitMessage.length(),
           &r, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
  SelectObject(dc, oldFont);
  ReleaseDC(g_hWnd, dc);
}

void denotify()
{
  g_waitMessage = "";
  repaint();
}

void zoomToFit();
void loadImage(string name)
{
  //if name is a directory, start recbrowse:
  if(isDirectory(name))
  {
    if(g_baseDir != "")
    {
      MessageBox(g_hWnd, ("Base dir already set to " + g_baseDir + ", won't change it to " + 
        name + ". Please mail author (nicolasweber@gmx.de).").c_str(), "ERROR:", MB_OK);
    }

    notify("Searching for images...");

    TraverseInfo i;
    if(!traverse(i, "", "", FORWARD, name))
    {
      denotify();
      return;
    }
    denotify();

    if(i.count == 0)
    {
      MessageBox(g_hWnd, ("No images found in directory \"" + name + "\"").c_str(),
        "Note:", MB_OK);
      return;
    }

    g_baseDir = name;
    name = i.name;
  }


  //draw loading screen
  notify("Loading \"" + name + "\"...");

  int oldW = getRealImageWidth();
  int oldH = getRealImageHeight();


  deleteHBitmaps();
  if(g_currILImage != 0)
    iluDeleteImage(g_currILImage);
  g_currILImage = 0;


  //this is no good idea: it makes the computer unresponsive
  //during image loading:
  //cheat a bit:
  //HANDLE currProcess = GetCurrentProcess();
  //DWORD oldPriority = GetPriorityClass(currProcess);
  //SetPriorityClass(currProcess, REALTIME_PRIORITY_CLASS);

  //start actual loading
  g_bmLoadTime = g_alphaConvTime = g_hbmConvTime = 0;
  DWORD startTime = GetTickCount();

  ILuint ilImg = iluGenImage();
  ilBindImage(ilImg);

  //perhaps the loading will fail because the file's
  //extension is different from its real type.
  //check this case
  ILenum extType = ilTypeFromExt((char*)name.c_str());
  ILenum realType = ilDetermineType((char*)name.c_str());

  //kill error msgs from type determination
  while(ilGetError() != IL_NO_ERROR);

  ILboolean loadResult;
  if(realType != extType && realType != IL_TYPE_UNKNOWN)
  {
    //don't measure time spent in message box
    g_bmLoadTime += (GetTickCount() - startTime)*.001f;
    char buff[256];
    sprintf(buff, "%s has extension %s, but its type is probably %s.\n"
                  "You may want to rename the image to the right extension (Shift+F2).",
                  name.c_str(), iluTypeString(extType), iluTypeString(realType));
    MessageBox(g_hWnd, buff, "WARNING:", MB_OK);
    startTime = GetTickCount();
    loadResult = ilLoad(realType, (char*)name.c_str());
  }
  else
    loadResult = ilLoadImage((char*)name.c_str());


  g_bmLoadTime += (GetTickCount() - startTime)*.001f;

  if(!loadResult)
  {
  }
  /*else*/
  {
    int bmWid = ilGetInteger(IL_IMAGE_WIDTH);
    int bmHyt = ilGetInteger(IL_IMAGE_HEIGHT);

    //collect infomation about the loaded image
    g_bmDep = ilGetInteger(IL_IMAGE_DEPTH);
    g_bmLayers = ilGetInteger(IL_NUM_IMAGES);
    g_bmMipmaps = ilGetInteger(IL_NUM_MIPMAPS);

    //load image onto hbitmaps
    HDC hDc = GetDC(g_hWnd);
    for(int j = 0; j <= min(g_bmLayers, 5); ++j)
    {
      ilBindImage(ilImg);
      ilActiveImage(j); //cube faces or whatever

      startTime = GetTickCount();
      g_hBm[j] = ilutConvertToHBitmap(hDc);
      g_hbmConvTime += (GetTickCount() - startTime)*.001f;
      if(ilHasAlpha() == IL_TRUE)
      {
        startTime = GetTickCount();
        g_hBmAlpha[j] = ilutConvertAlphaToHBitmap(hDc);
        g_alphaConvTime += (GetTickCount() - startTime)*.001f;
      }
    }
    ReleaseDC(g_hWnd, hDc);

    //iluDeleteImage(ilImg);
    g_currILImage  = ilImg;
  }
  ilBindImage(ilImg);

  //return to default priority
  //SetPriorityClass(currProcess, oldPriority);

  string errMsg;
  if(!loadResult)
    errMsg += "\nProblems while loading.";

  ILuint err = ilGetError();
  while(err != IL_NO_ERROR)
  {
    const ILstring t = iluErrorString(err);
    errMsg += string("\n");
    if(t != NULL) errMsg += t;
    else errMsg += "SERIOUS DEVIL PROBLEM, PLEASE CONTACT AUTHOR (nicolasweber@gmx.de)";
    err = ilGetError();
  }

  if(errMsg != "")
  {
    errMsg = name + string(":") + errMsg;
    errMsg += "\nThe image data is incomplete, if available at all.";
    MessageBox(g_hWnd, errMsg.c_str(), "Sorry...", MB_OK);
  }

  g_imageName = name;
  g_oldImageName = "";
  g_currLayer = 0;
  g_currMipmapLevel = 0;
  g_currSlice = 0;

  //get file size
  HANDLE hFile = CreateFile(g_imageName.c_str(), 0, 0, NULL, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, NULL);
  if(hFile != NULL)
  {
    g_imageFileSize = byteCountToString(GetFileSize(hFile, NULL));
    CloseHandle(hFile);
  }
  else
    g_imageFileSize = "N/A";

  g_rotation = ROT_NONE;
  ilBindImage(g_currILImage);
  ILboolean b = ilIsCubemap();
  ilActiveImage(0);
  if(g_displayMode == DM_CUBE_1 && b == IL_FALSE)
    g_displayMode = DM_2D;
  if(ilHasAlpha() == IL_FALSE)
    g_displayAlpha = false;
  denotify();

  if(getRealImageWidth() != oldW || getRealImageHeight() != oldH)
  {
    g_dx = g_dy = 0;
    g_scale = 1;
    //updateScale();
    if(g_lockWindowSize && !doesImageFit() && !g_isFullscreen)
      zoomToFit();
  }
  else
  {
    //if the new images has the same size as the old one,
    //keep scale etc (useful if one wants to compare two images)
  }

  updateWindowSize();
  updateTitle();
  repaint();
}

void updateScale()
{
  updateStatusBar();
  updateWindowSize();
  repaint();
}

RECT g_windowRectBeforeFullscreen = { 0, 0, 0, 0 };

void setFullscreen()
{
  if(g_isFullscreen) return;

  g_isFullscreen = true;

  //store old window size
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  GetWindowPlacement(g_hWnd, &wp);
  g_windowRectBeforeFullscreen = wp.rcNormalPosition;

  LONG style = GetWindowLong(g_hWnd, GWL_STYLE);
  style &= ~WS_OVERLAPPEDWINDOW;
  style |= WS_POPUP;
  SetWindowLong(g_hWnd, GWL_STYLE, style);


  RECT r = getFullscreenRect();
  SetWindowPos(g_hWnd, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_FRAMECHANGED);

  ShowWindow(g_statusbar, SW_HIDE);
  if(g_displayToolbar)
    ShowWindow(g_toolbar, SW_HIDE);

  repaint();
  setCursor();
}

void setWindowed()
{
  if(!g_isFullscreen) return;

  g_isFullscreen = false;

  LONG style = GetWindowLong(g_hWnd, GWL_STYLE);
  style &= ~WS_POPUP;
  style |= WS_OVERLAPPEDWINDOW;
  SetWindowLong(g_hWnd, GWL_STYLE, style);
  SetWindowPos(g_hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);

  if(g_lockWindowSize) //set size window had before going fullscreen
    setWindowSize(g_windowRectBeforeFullscreen);
  else
    updateWindowSize();

  ShowWindow(g_statusbar, SW_SHOW);
  if(g_displayToolbar)
    ShowWindow(g_toolbar, SW_SHOW);

  repaint();
  setCursor();
}

void toggleFullscreen()
{
  if(!g_isFullscreen)
    setFullscreen();
  else
    setWindowed();
}

void clamp(int& v, int minValue, int maxValue)
{
  v = max(minValue, min(v, maxValue));
}

void clampDx(int& dx)
{
  clamp(dx, 0, getDisplayImageWidth() + 2*g_border - getClientWidth());
}

void clampDy(int& dy)
{
  clamp(dy, 0, getDisplayImageHeight() + 2*g_border - getClientHeight());
}

void clampDxDy(int& dx, int& dy)
{
  clampDx(dx);
  clampDy(dy);
}

HBITMAP getActiveBitmap()
{
  HBITMAP activeBitmap = NULL;
  HDC hDc = GetDC(g_hWnd);
  if(!g_displayAlpha)
  {
    if(g_currLayer >= 0 && g_currLayer < 6)
      activeBitmap = g_hBm[g_currLayer];
    else if(g_currLayer == g_layerNr)
      activeBitmap = g_layerBuff;
    else
    {
      //TODO: this doesn't work with mipmaps (not that tragic,
      //because mipmapping is only supported for up to 6 layers)
      if(g_layerBuff != NULL)
        DeleteObject(g_layerBuff);
      ilActiveImage(g_currILImage);
      ilActiveLayer(g_currLayer);
      g_layerNr = g_currLayer;
      activeBitmap = g_layerBuff = ilutConvertToHBitmap(hDc);
    }
  }
  else
  {
    if(!g_displayBlended)
    {
      if(g_currLayer >= 0 && g_currLayer < 6)
        activeBitmap = g_hBmAlpha[g_currLayer];
      else if(g_currLayer == g_layerAlphaNr)
        activeBitmap = g_layerBuffAlpha;
      else
      {
        //TODO: this doesn't work with mipmaps (not that tragic,
        //because mipmapping is only supported for up to 6 layers)
        if(g_layerBuffAlpha != NULL)
          DeleteObject(g_layerBuffAlpha);
        ilActiveImage(g_currILImage);
        ilActiveLayer(g_currLayer);
        g_layerAlphaNr = g_currLayer;
        activeBitmap = g_layerBuffAlpha = ilutConvertAlphaToHBitmap(hDc);
      }
    }
    else
    {
      if(g_currLayer >= 0 && g_currLayer < 6)
      {
        if(g_hBmBlended[g_currLayer] == NULL)
        {
          notify("Generating blended image...");
          g_hBmBlended[g_currLayer] = ilutConvertAlphaBlendedToHBitmap(hDc);
          denotify();
        }
        activeBitmap = g_hBmBlended[g_currLayer];
      }
      else if(g_currLayer == g_layerBlendedNr)
        activeBitmap = g_layerBuffBlended;
      else
      {
        //TODO: this doesn't work with mipmaps (not that tragic,
        //because mipmapping is only supported for up to 6 layers)
        if(g_layerBuffBlended != NULL)
          DeleteObject(g_layerBuffBlended);
        ilActiveImage(g_currILImage);
        ilActiveLayer(g_currLayer);
        g_layerBlendedNr = g_currLayer;
        activeBitmap = g_layerBuffBlended = ilutConvertAlphaBlendedToHBitmap(hDc);
      }
    }
  }

  ReleaseDC(g_hWnd, hDc);
  return activeBitmap;
}

void leftMouseDown(int x, int y)
{
  g_startDx = g_dx;
  g_startDy = g_dy;
  g_mx = x;
  g_my = y;
}

void zoomIn()
{
  //THIS NEEDS A REWRITE TO CLARIFY WHAT'S GOING ON!!!
  //TODO TODO TODO (also for zoom out...)
  g_scale *= 2.f;
  updateScale();

  int displayWid = getDisplayImageWidth();
  int displayHyt = getDisplayImageHeight();

  if(g_w >= displayWid + 2*g_border) //image fits
    g_dx = 0;
  else if(g_w >= displayWid/2 + 2*g_border) //image doesn't fit,
    g_dx = g_startDx = (displayWid + 2*g_border - g_w)/2; //but did last time
  else
  {
    g_dx = 2*g_dx - g_border + g_w/2 + .5;
    clampDx(g_dx);
  }

  if(g_h >= displayHyt + 2*g_border)
     g_dy = 0;
  else if(g_h >= displayHyt/2 + 2*g_border)
    g_dy = g_startDy = (displayHyt + 2*g_border - g_h)/2;
  else
  {
    g_dy = 2*g_dy - g_border + g_h/2 + .5;
    clampDy(g_dy);
  }
}

void zoomOut()
{
  g_scale *= .5f;
  updateScale();

  int displayWid = getDisplayImageWidth();
  int displayHyt = getDisplayImageHeight();

  if(g_w >= displayWid + 2*g_border)
     g_dx = 0;
  else
  {
    g_dx = (g_dx + g_border - g_w/2 - .5)/2;
    clampDx(g_dx);
  }

  if(g_h >= displayHyt + 2*g_border)
     g_dy = 0;
  else
  {
    g_dy = (g_dy + g_border - g_h/2 - .5)/2;
    clampDy(g_dy);
  }
}

void noZoom()
{
  g_scale = 1.f;
  clampDxDy(g_dx, g_dy);
  updateScale();
}

void zoomToFit()
{
  HBITMAP activeBitmap = getActiveBitmap();
  if(activeBitmap != NULL)
  {
    BITMAP bm;
    GetObject(activeBitmap, sizeof(bm), &bm);
    if(g_displayMode == DM_CUBE_1)
    {
      bm.bmWidth *= 4; //evil hack
      bm.bmHeight *= 3;
    }

    int maxW = g_w, maxH = g_h;

    if(!IsZoomed(g_hWnd) && !g_isFullscreen && !g_lockWindowSize)
    {
      //calculate the max client area for a 
      //non-maximized window
      /*
      maxW = GetSystemMetrics(SM_CXFULLSCREEN)
               - 2*GetSystemMetrics(SM_CXFRAME);
      int statusbarHeight = getStatusbarHeight();
      maxH = GetSystemMetrics(SM_CYFULLSCREEN)
               - 2*GetSystemMetrics(SM_CYFRAME) - statusbarHeight
               - getToolbarHeight();
      */
      //this is multimonitor compatible
      RECT wa = getWorkareaRect();
      maxW = wa.right - wa.left - 2*GetSystemMetrics(SM_CXFRAME);
      maxH = wa.bottom - wa.top - 2*GetSystemMetrics(SM_CYFRAME)
               - GetSystemMetrics(SM_CYCAPTION)
               - getStatusbarHeight() - getToolbarHeight();
    }

    g_scale = min(float(maxW - 2*g_border)/float(bm.bmWidth),
                  float(maxH - 2*g_border)/float(bm.bmHeight));
    g_dx = g_dy = 0;
    updateScale();
  }
}



LRESULT CALLBACK eventListener(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
  static bool s_isWindowsXP = false; //needed for a hack in WM_SIZE/WM_PAINT
  static OPENFILENAME s_saveFileName;
  static bool s_isOleInitialized = false;

  if(g_win95HasMousewheel && msg == g_win95MousewheelMessage)
    return SendMessage(hWnd, WM_MOUSEWHEEL, wP << 16, lP);

  switch(msg)
  {
    case WM_CREATE:
    {
      g_w = ((CREATESTRUCT*)lP)->cx;
      g_h = ((CREATESTRUCT*)lP)->cy;

      //create status bar
      INITCOMMONCONTROLSEX ic;
      ic.dwSize = sizeof(ic);
      ic.dwICC = ICC_WIN95_CLASSES;
      InitCommonControlsEx(&ic);

      g_statusbar = CreateWindowEx(0, STATUSCLASSNAME, "",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd, (HMENU)555, g_hInst, NULL);

      if(g_displayToolbar)
        g_toolbar = createToolbar(g_hInst, hWnd);

      int partSizes[] = { 50, 280, 340, 400, 460, -1 };
      SendMessage(g_statusbar, SB_SETPARTS, 6, (LPARAM)partSizes);

      if(g_statusbar == NULL)
        ErrorBox("Failed to create status bar");

      updateScale();

      //register as drag-n-drop target (the old way, without ole etc...)
      DragAcceptFiles(hWnd, TRUE);


      //check if we're running under XP (for WM_SIZE/WM_PAINT hack)
      OSVERSIONINFO vi;
      vi.dwOSVersionInfoSize = sizeof(vi);
      GetVersionEx(&vi);
      s_isWindowsXP = (vi.dwMajorVersion > 5) || (vi.dwMajorVersion == 5 && vi.dwMinorVersion >= 1);

      return 0;
    }break;

    case WM_DROPFILES:
    {
      HDROP hDrop = (HDROP)wP;

      //only accept the first file... (TODO??)
      int len = DragQueryFile(hDrop, 0, NULL, 0) + 1;
      char* buff = new char[len];
      DragQueryFile(hDrop, 0, buff, len);

      g_baseDir = "";
      loadImage(tolower(buff));
      SendMessage(g_statusbar, SB_SETTEXT, 0, (LPARAM)"");

      delete [] buff;
      DragFinish(hDrop);

      return 0;
    }break;

    case WM_PAINT:
    {
      HRGN update = CreateRectRgn(0, 0, 0, 0);
      HRGN imgRgn = CreateRectRgn(0, 0, 0, 0);
      GetUpdateRgn(hWnd, update, FALSE); //need to call this before BeginPaint...

      PAINTSTRUCT ps;
      HDC hDc = BeginPaint(hWnd, &ps);

      HDC target = hDc;
      HBITMAP activeBitmap = getActiveBitmap();
//*
      if(activeBitmap != NULL)
      {
        HDC bmDc = CreateCompatibleDC(hDc);
        HBITMAP bmOld;

        int displayWid = getDisplayImageWidth();
        int displayHyt = getDisplayImageHeight();

        int x, y, w = displayWid, h = displayHyt;
        if(g_w >= displayWid + 2*g_border)
          //center image horizontally if window is large enough
          x = (g_w - displayWid - 2*g_border)/2 + g_border;
        else
          //scroll image horizontally if it doesn't fit into the window
          x = g_border - g_dx;

        if(g_h >= displayHyt + 2*g_border)
          //center image vertically if window is large enough
          y = (g_h - displayHyt - 2*g_border)/2 + g_border;
        else
          //scroll image vertically if it doesn't fit into the window
          y = g_border - g_dy;

        y += getToolbarHeight();

        BITMAP bm;
        GetObject(activeBitmap, sizeof(bm), &bm);
        int bmWid = bm.bmWidth, bmHyt = bm.bmHeight;

        if(g_scale >= 1.0f)
        {
          //cheap magnification filter
          SetStretchBltMode(target, BLACKONWHITE);
          //SetStretchBltMode(bmDc, BLACKONWHITE);
          //SetStretchBltMode(target, WHITEONBLACK);
        }
        else
        {
          //good minification filter
          //SetStretchBltMode(target, COLORONCOLOR);
          SetStretchBltMode(target, HALFTONE); //??
          SetBrushOrgEx(target, 0, 0, NULL);
          //SetStretchBltMode(target, BLACKONWHITE);
        }

        if(g_displayMode != DM_CUBE_1)
        {
          bmOld = (HBITMAP)SelectObject(bmDc, activeBitmap);

          SetRectRgn(imgRgn, x, y, x + w, y + h);
          StretchBlt(target, x, y, w, h, bmDc, 0, 0, bmWid, bmHyt, SRCCOPY);

          if(g_displayMode == DM_2D_TILED)
          {
            HRGN tmp = CreateRectRgn(x - w, y, x, y + h);
            CombineRgn(imgRgn, imgRgn, tmp, RGN_OR);
            SetRectRgn(tmp, x + w, y, x + 2*w, y + h);
            CombineRgn(imgRgn, imgRgn, tmp, RGN_OR);
            SetRectRgn(tmp, x, y + h, x + w, y + 2*h);
            CombineRgn(imgRgn, imgRgn, tmp, RGN_OR);
            SetRectRgn(tmp, x, y - h, x + w, y);
            CombineRgn(imgRgn, imgRgn, tmp, RGN_OR);
            DeleteObject(tmp);
            //rely on windows' clipping mechanisms
            StretchBlt(target, x - w, y, w, h, bmDc, 0, 0, bmWid, bmHyt, SRCCOPY);
            StretchBlt(target, x + w, y, w, h, bmDc, 0, 0, bmWid, bmHyt, SRCCOPY);
            StretchBlt(target, x, y + h, w, h, bmDc, 0, 0, bmWid, bmHyt, SRCCOPY);
            StretchBlt(target, x, y - h, w, h, bmDc, 0, 0, bmWid, bmHyt, SRCCOPY);

            //frame rect
            #if 0
            HPEN oldPen = (HPEN)SelectObject(target, s_pen);

            MoveToEx(target, tx - 1, ty - 1, NULL);
            LineTo(target, tx + w, ty - 1);
            LineTo(target, tx + w, ty + h);
            LineTo(target, tx - 1, ty + h);
            LineTo(target, tx - 1, ty - 1);

            SelectObject(target, oldPen);
            #endif
          }

          SelectObject(bmDc, bmOld);
        }
        else //DM_CUBE
        {
          w /= 4;
          h /= 3; //evil hack :-)

          //in this order: right, left, top, down, front, back (?)
          int deltas[] = { 2*w, h, 0, h, w, 0, w, 2*h, w, h, 3*w, h };
          HRGN tmp = CreateRectRgn(0, 0, 0, 0);
          for(int i = 0; i < 6; ++i)
          {
            if(!g_displayAlpha)
              bmOld = (HBITMAP)SelectObject(bmDc, g_hBm[i]);
            else if(!g_displayBlended)
              bmOld = (HBITMAP)SelectObject(bmDc, g_hBmAlpha[i]);
            else
              bmOld = (HBITMAP)SelectObject(bmDc, g_hBmBlended[i]);
            StretchBlt(target, x + deltas[2*i], y + deltas[2*i + 1], w, h, bmDc, 0, 0, bmWid, bmHyt, SRCCOPY);
            SelectObject(bmDc, bmOld);

            SetRectRgn(tmp, x + deltas[2*i], y + deltas[2*i + 1],
                            x + deltas[2*i] + w, y + deltas[2*i + 1] + h);
            CombineRgn(imgRgn, imgRgn, tmp, RGN_OR);
          }
          DeleteObject(tmp);
        }

        DeleteDC(bmDc);
      }
//*/
      //clear background where needed
      CombineRgn(imgRgn, update, imgRgn, RGN_DIFF);
      DeleteObject(update);

      //make sure only non-child-window stuff is cleared
      RECT clientRect;
      getRealClientRect(clientRect);
      HRGN safeguard = CreateRectRgnIndirect(&clientRect);
      CombineRgn(imgRgn, imgRgn, safeguard, RGN_AND);
      DeleteObject(safeguard);

      FillRgn(target, imgRgn, GetSysColorBrush(COLOR_WINDOW));
      DeleteObject(imgRgn);

      //display wait message
      if(g_waitMessage != "")
      {
        RECT r = { 0, 0, g_w, g_h };
        SetBkMode(target, TRANSPARENT);
        DrawText(target, g_waitMessage.data(), g_waitMessage.length(),
                 &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      }

      EndPaint(hWnd, &ps);

      //see comment in WM_SIZE
      if(s_isWindowsXP)
        updateStatusBar(false);

      return 0;
    }break; //Phew! WM_PAINT is done...long one... :)

    case WM_LBUTTONDOWN:
    {
      if((wP & MK_CONTROL) != 0 || (wP & MK_SHIFT) != 0) //drag
      {
        if(!s_isOleInitialized)
        {
          OleInitialize(NULL);
          s_isOleInitialized = true;
        }

        DefaultDropSource ds;
        FileDataObject data;
        data.addFile(g_imageName);

        DWORD effect;
        DWORD result = DoDragDrop(&data, &ds, DROPEFFECT_MOVE|DROPEFFECT_COPY, &effect);

        if(result == DRAGDROP_S_DROP) 
        {
          if(effect != DROPEFFECT_COPY)
          //go to next file
          SendMessage(hWnd, WM_CHAR, (WPARAM)' ', 0);
        }
      }
      else
      {
        leftMouseDown((int)(short)LOWORD(lP), (int)(short)HIWORD(lP));
        SetCapture(hWnd);
      }
      return 0;
    }break;

    case WM_LBUTTONUP:
    {
      ReleaseCapture();
      return 0;
    }break;

    case WM_RBUTTONUP:
    {
      if((wP & MK_LBUTTON) != 0)
        toggleFullscreen();
      return 0;
    }break;

    case WM_MBUTTONDOWN:
    {
      toggleFullscreen();
      return 0;
    }break;

    case WM_MOUSEMOVE:
    {
      int mx = (int)(short)LOWORD(lP);
      int my = (int)(short)HIWORD(lP);
      if((wP & MK_LBUTTON) != 0)
      {
        int dx = 0, dy = 0;

        int displayWid = getDisplayImageWidth();
        int displayHyt = getDisplayImageHeight();

        if(displayWid + 2*g_border > g_w)
        {
          dx = g_startDx + g_mx - mx;
          if(dx < 0)
            dx = 0;
          else if(dx > displayWid + 2*g_border - g_w)
            dx = displayWid + 2*g_border - g_w;
        }

        if(displayHyt + 2*g_border > g_h)
        {
          dy = g_startDy + g_my - my;
          if(dy < 0)
            dy = 0;
          else if(dy > displayHyt + 2*g_border - g_h)
            dy = displayHyt + 2*g_border - g_h;
        }

        if(dx != g_dx || dy != g_dy)
        {
          //ScrollWindow is MUCH faster than a repaint via
          //stretchblt! :-))
          ScrollWindowEx(hWnd, g_dx - dx, g_dy - dy,
                         NULL, NULL, NULL, NULL, SW_INVALIDATE);
          g_dx = dx;
          g_dy = dy;
          //repaint();
        }
      }
      //update color info to pixel below cursor
      //again, this code should go in a function
      int displayWid = getDisplayImageWidth();
      int displayHyt = getDisplayImageHeight();

      int x, y;
      if(g_w >= displayWid + 2*g_border)
        //center image horizontally if window is large enough
        x = (g_w - displayWid - 2*g_border)/2 + g_border;
      else
        //scroll image horizontally if it doesn't fit into the window
        x = g_border - g_dx;

      if(g_h >= displayHyt + 2*g_border)
        //center image vertically if window is large enough
        y = (g_h - displayHyt - 2*g_border)/2 + g_border;
      else
        //scroll image vertically if it doesn't fit into the window
        y = g_border - g_dy;

      if(g_displayToolbar)
        y += getToolbarHeight();


      x = mx - x; //int((mx - x)/g_scale + .0f);
      y = my - y; //int((my - y)/g_scale + .0f);

      if(g_displayMode == DM_CUBE_1)
      {
        int realW = displayWid/4; //TODO: evil hack
        int realH = displayHyt/3;
        int ix = x/realW;
        int iy = y/realH;
        x %= realW;
        y %= realH;
        ilBindImage(g_currILImage);
        if(ix == 1 && iy ==0)
        {
          ilActiveImage(2);
        }
        else if(iy == 1)
        {
          switch(ix)
          {
            case 0:
              ilActiveImage(1);
              break;
            case 1:
              ilActiveImage(4);
              break;
            case 2:
              ilActiveImage(0);
              break;
            case 3:
              ilActiveImage(5);
              break;
          }
        }
        else if(ix == 1 && iy == 2)
        {
          ilActiveImage(3);
        }
        else
        {
          x = y = -1; //no cubeface below cursor
          ilActiveImage(g_currLayer);
        }
        ilActiveMipmap(g_currMipmapLevel);
      }
      else
      {
        bindCurrImage();

        if(g_displayMode == DM_2D_TILED)
        {
          if(x >= displayWid)
            x -= displayWid;
          else if(x < 0)
            x += displayWid;

          //This "else" is intended: without it, there's a
          //color value to nw, sw, ne, se of the
          //image as well, only n, s, w, e is wanted
          else if(y >= displayHyt)
            y -= displayHyt;
          else if(y < 0)
            y += displayHyt;
        }
      }

      x = int(x/g_scale + .0f);
      y = int(y/g_scale + .0f);

      updateTitle(false, x, y);
      //updateTitle(true, x, y);
      return 0;
    }break;

    case WM_KEYDOWN:
    {
      if(wP == VK_ESCAPE)  //quit program
        DestroyWindow(hWnd);
      else if(wP == VK_NEXT) //display next layer
      {
        if(g_bmLayers <= 0)
          return 0;

        //get width of old layer
        int prevW = getRealImageWidth();
        int prevH = getRealImageHeight();

        //go to next layer
        ++g_currLayer;
        if(g_currLayer > g_bmLayers)
          g_currLayer = 0;
        bindCurrImage();

        updateTitle();
        if(getRealImageWidth() != prevW || 
           getRealImageHeight() != prevH)
          updateWindowSize();

        repaint();
      }
      else if(wP == VK_PRIOR) //display previous layer
      {
        if(g_bmLayers <= 0)
          return 0;

        int prevW = getRealImageWidth();
        int prevH = getRealImageWidth();

        --g_currLayer;
        if(g_currLayer < 0)
          g_currLayer = g_bmLayers;
        bindCurrImage();

        updateTitle();
        if(getRealImageWidth() != prevW || 
           getRealImageHeight() != prevH)
          updateWindowSize();

        repaint();
      }
      else if(wP == VK_RIGHT || wP == VK_LEFT) //display next/previous mipmap
      {
        if(g_bmMipmaps <= 0)
          return 0;

        //only dds files support mipmaps afaik, so it's a reasonable
        //assumption that only images with <= 6 layers will have mipmaps. (TODO:?)
        if(g_bmLayers > 5)
        {
          MessageBox(hWnd, "Only images with <= 6 layers support mipmap display", "Sorry...", MB_OK);
          return 0;
        }

        int level = g_currMipmapLevel;
        float scale;
        if(wP == VK_RIGHT)
        {
          ++level;
          scale = 2.f;
        }
        else //VK_LEFT
        {
          --level;
          scale = .5f;
        }
        clamp(level, 0, g_bmMipmaps);

        if(level != g_currMipmapLevel)
        {
          deleteHBitmaps();
          HDC hDc = GetDC(hWnd);
          notify("Loading next mipmap level...");

          for(int i = 0; i <= g_bmLayers; ++i)
          {
            ilBindImage(g_currILImage);
            ilActiveImage(i);
            ilActiveMipmap(level);

            g_hBm[i] = ilutConvertToHBitmap(hDc);
            if(ilHasAlpha())
              g_hBmAlpha[i] = ilutConvertAlphaToHBitmap(hDc);
          }

          ReleaseDC(hWnd, hDc);
          g_scale *= scale;
          g_currMipmapLevel = level;
          denotify();
          updateStatusBar();
          updateScale();
          repaint();
        }
      }
      else if(wP == VK_UP || wP == VK_DOWN) //go through 3d image slices
      {
        if(g_bmDep <= 1)
          return 0;

        if(g_bmLayers != 0)
        {
          MessageBox(hWnd, "Only images with one layer support slice display", "Sorry...", MB_OK);
          return 0;
        }

        int slice = g_currSlice;
        if(wP == VK_UP)
          --slice;
        else
          ++slice;
        //wrapping around may be better than clamping because
        //you can sort of see if the image is tileable in z
        //clamp(slice, 0, g_bmDep - 1);
        if(slice >= g_bmDep)
          slice = 0;
        if(slice < 0)
          slice = g_bmDep - 1;

        if(slice != g_currSlice)
        {
          deleteHBitmaps();
          HDC hDc = GetDC(hWnd);
          notify("Loading next slice...");

          g_hBm[0] = ilutConvertSliceToHBitmap(hDc, slice);
          if(ilHasAlpha())
            g_hBmAlpha[0] = ilutConvertAlphaSliceToHBitmap(hDc, slice);

          ReleaseDC(hWnd, hDc);
          g_currSlice = slice;
          denotify();
          updateStatusBar();
          repaint();
        }
      }
      else if(wP == VK_DELETE) //delete current image
      {
        SHFILEOPSTRUCT s;
        ZeroMemory(&s, sizeof(s));
        char buff[MAX_PATH];
        ZeroMemory(buff, MAX_PATH);
        sprintf(buff, "%s", g_imageName.c_str());
        s.wFunc = FO_DELETE;
        s.pFrom = buff;
        s.fFlags = FOF_ALLOWUNDO;
        if(SHFileOperation(&s) == 0 && s.fAnyOperationsAborted == FALSE)
        {
          //go to next file
          SendMessage(hWnd, WM_CHAR, (WPARAM)' ', 0);

          //notify shell (updates the recycle bin image for example)
          SHChangeNotify(SHCNE_DELETE, SHCNF_IDLIST | SHCNF_PATH, s.pFrom, NULL);
        }
        if(s.hNameMappings != NULL) //MSDN says this is always created on NT
          SHFreeNameMappings(s.hNameMappings);
      }
      else if(wP == VK_F1) //show help
      {
        SendMessage(hWnd, WM_CHAR, (WPARAM)'h', 0);
      }
      else if(wP == VK_F2)
      {
        if(g_imageName == "" || g_imageName == "")
          return 0;

        string input = g_imageName;
        string path, ext;

        bool renameExtension = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        string::size_type t = input.rfind('.'), t1, t2;

        //strip extension (TODO: you can't change file extension that way...)
        if(!renameExtension && t != string::npos)
        {
          ext = input.substr(t);
          input = input.substr(0, t);
        }

        //strip path
        t1 = input.rfind('/');
        t2 = input.rfind('\\');

        if(t2 == string::npos)
          t = t1;
        else if(t1 == string::npos)
          t = t2;
        else
        {
          if(t1 > t2)
            t = t1;
          else
            t = t2;
        }

        if(t != string::npos)
        {
          path = input.substr(0, t + 1);
          input = input.substr(t + 1);
        }

        string oldInput = input;
        if(inputBoxString(hWnd, "Enter new filename:", "Rename Image", input))
        {
          if(oldInput == input)
            return 0;

          string temp = path + input + ext;
          //MessageBox(hWnd, input.c_str(), "Bla:", MB_OK);
          if(MoveFile(g_imageName.c_str(), temp.c_str()))
          {
            g_oldImageName = g_imageName;
            g_imageName = temp;
            updateTitle();
          }
          else
          {
            string msg = "Failed to rename \"" + oldInput + ext + "\" to \"" + input + ext +
              "\".\nProbably there already exists a file of this name.";
            MessageBox(hWnd, msg.c_str(), "Failed", MB_OK);
          }
          //use g_oldImageName to go to the next
          //image with respect to the old filename
        }
      }
      return 0;
    }break;

    case WM_MOUSEWHEEL: //522
    {
      //TODO: don't forward this message
      //but call a function

      if((short)HIWORD(wP) < 0) //next image
        SendMessage(hWnd, WM_CHAR, MAKEWPARAM(' ', 0), 0);
      else //prev image
        SendMessage(hWnd, WM_CHAR, MAKEWPARAM('\b', 0), 0);

      return 0;
    }break;

    case WM_CHAR:
    {
      //TODO: call a function instead of implementing
      //this here
      if(LOWORD(wP) == ' ' || LOWORD(wP) == '\b') //next image
      {
        string name = g_imageName;
        if(g_oldImageName != "")
          name = g_oldImageName;

        if(g_baseDir != "")
          notify("Searching for images...");

        TraverseInfo i;
        if(!traverse(i, name, g_imageName,
          (LOWORD(wP) == ' ')?FORWARD:BACKWARD, g_baseDir))
        {
          if(g_baseDir != "")
            denotify();
          return 0;
        }

        if(g_baseDir != "")
          denotify();

        if(i.index < 0 && g_baseDir != "")
        {
          MessageBox(g_hWnd, ("DeepBrowsing \"" + g_baseDir + "\" BACKWARD is complete.").c_str(),
            "Done DeepBrowsing:", MB_OK);
          return 0;
        }
        else if(i.index >= i.count && g_baseDir != "")
        {
          MessageBox(g_hWnd, ("DeepBrowsing \"" + g_baseDir + "\" FORWARD is complete.").c_str(),
            "Done DeepBrowsing:", MB_OK);
          return 0;
        }

        //update status bar
        char buff[80];
        if(i.count > 0)
          sprintf(buff, "%i/%i%s", i.index + 1, i.count, (g_baseDir != "")?"+":"");
        else
          sprintf(buff, "0/0");
        SendMessage(g_statusbar, SB_SETTEXT, 0, (LPARAM)buff);

        if(i.count > 1 //only load new image if more than one image was
                  //found (so one image is not loaded over and over again).

          || //This happens if two images were in current folder and one is
          (i.count == 1 && g_imageName != i.name)) //deleted
        {
          loadImage(i.name);
        }
      }
      else if(toupper(LOWORD(wP)) == 'M') //switch display mode to tiled
      {
        if(g_displayMode != DM_2D_TILED)
        {
          g_displayMode = DM_2D_TILED;
          g_border = 30;
        }
        else
        {
          g_displayMode = DM_2D;
          g_border = 0;
        }

        clampDxDy(g_dx, g_dy);
        updateWindowSize();
        if(g_displayToolbar)
          updateToolbar(g_toolbar);
        repaint();
      }
      else if(toupper(LOWORD(wP)) == 'C') //switch display mode to cube
      {
        //XXXX
        //ilBindImage(g_currILImage); //needed for ilIsCubemap to work
        //ILboolean b = ilIsCubemap();
        //ilActiveImage(g_currLayer);
        //ilActiveMipmap(g_currMipmapLevel);
        if(!isImageCubemap())
          return 0;

        bindCurrImage();

        if(g_displayMode != DM_CUBE_1)
          g_displayMode = DM_CUBE_1;
        else
          g_displayMode = DM_2D;

        g_border = 0;
        clampDxDy(g_dx, g_dy);

        if(g_displayBlended && g_displayMode == DM_CUBE_1)
        {
          notify("Generating blended images...");
          HDC hDc = GetDC(hWnd);
          for(int i = 0; i < 6; ++i)
            if(g_hBmBlended[i] == NULL)
            {
              ilBindImage(g_currILImage);
              ilActiveImage(i);
              ilActiveMipmap(g_currMipmapLevel);
              g_hBmBlended[i] = ilutConvertAlphaBlendedToHBitmap(hDc);
            }
          ReleaseDC(hWnd, hDc);
          bindCurrImage();
          denotify();
        }

        updateWindowSize();
        if(g_displayToolbar)
          updateToolbar(g_toolbar);
        repaint();
      }
      else if(LOWORD(wP) == '+') //zoom in
      {
        zoomIn();
      }
      else if(LOWORD(wP) == '-') //zoom out
      {
        zoomOut();
      }
      else if(LOWORD(wP) == '/') //no zoom
      {
        noZoom();
      }
      else if(LOWORD(wP) == '*') //zoom to fit
      {
        zoomToFit();
      }
      else if((toupper(LOWORD(wP)) == 'A' || toupper(LOWORD(wP)) == 'B')
        && hasImageAlpha()) //toggle alpha display
      {
        if(g_displayAlpha)
        {
          if(!g_displayBlended)
          {
            if(toupper(LOWORD(wP)) == 'A')
              g_displayAlpha = g_displayBlended = false;
            else //'B'
              g_displayBlended = true;
          }
          else
          {
            if(toupper(LOWORD(wP)) == 'B')
              g_displayAlpha = g_displayBlended = false;
            else //'A'
              g_displayBlended = false;
          }
        }
        else
        {
          g_displayAlpha = true;
          g_displayBlended = toupper(LOWORD(wP)) == 'B';
        }

        if(g_displayAlpha && g_displayBlended)
          g_rotation = ROT_NONE; //TODO rot (this changes nothing atm)


        //load blended images of cubemap if cubemap is displayed
        if(g_displayBlended && g_displayMode == DM_CUBE_1)
        {
          notify("Generating blended images...");
          HDC hDc = GetDC(hWnd);
          for(int i = 0; i < 6; ++i)
            if(g_hBmBlended[i] == NULL)
            {
              ilBindImage(g_currILImage);
              ilActiveImage(i);
              ilActiveMipmap(g_currMipmapLevel);
              g_hBmBlended[i] = ilutConvertAlphaBlendedToHBitmap(hDc);
            }
          ReleaseDC(hWnd, hDc);
          bindCurrImage();
          denotify();
        }

        updateTitle();
        repaint();
      }
      else if(toupper(LOWORD(wP)) == 'F')
        toggleFullscreen();
      else if(toupper(LOWORD(wP)) == 'R')
      {
        registerFileTypesDialog(g_hWnd, SUPPORTED_FORMATS, NUM_FORMATS);
      }
      else if(toupper(LOWORD(wP)) == 'Q' || toupper(LOWORD(wP)) == 'W')
      {
        if(g_bmLayers > 0)
        {
          MessageBox(hWnd, "Rotation does only work for images "
            "with only one layer currently.", "Sorry...", MB_OK);
          return 0;
        }

        if(g_displayAlpha && g_displayBlended)
        {
          //blended images are created the first time they are displayed,
          //so their rotation might be out-of-sync with the rest of the images
          //(TODO rot!)
          MessageBox(hWnd, "Rotation does not work with blended images currently",
            "Sorry...", MB_OK);
          return 0;
        }

        //rotate left/right
        notify("Rotating...");
        HBITMAP t, tAlpha; //, tBlend;
        if(toupper(LOWORD(wP)) == 'Q')
        {
          //transform image origin to keep viewport centered
          int tdx = g_dx;
          g_dx = g_dy + (getClientHeight() - getClientWidth())/2;
          g_dy = getDisplayImageWidth() - tdx - getClientWidth()/2 - getClientHeight()/2;

          //transform image
          t = rotateHBitmapL(g_hBm[g_currLayer]);
          tAlpha = rotateHBitmapL(g_hBmAlpha[g_currLayer]);
          //tBlend = rotateHBitmapL(g_hBmBlended[g_currLayer]);
          if(g_rotation != ROT_270)
            g_rotation = (ROTATION)(g_rotation + 1);
          else
           g_rotation = ROT_NONE;
        }
        else
        {
          //transform image origin to keep viewport centered
          int tdx = g_dx;
          g_dx = getDisplayImageHeight() - g_dy - getClientHeight()/2 - getClientWidth()/2;
          g_dy = tdx + (getClientWidth() - getClientHeight())/2;

          //transform image
          t = rotateHBitmapR(g_hBm[g_currLayer]);
          tAlpha = rotateHBitmapR(g_hBmAlpha[g_currLayer]);
          //tBlend = rotateHBitmapR(g_hBmBlended[g_currLayer]);
          if(g_rotation != ROT_NONE)
            g_rotation = (ROTATION)(g_rotation - 1);
          else
            g_rotation = ROT_270;
        }
        clampDxDy(g_dx, g_dy);

        if(t != NULL)
        {
          if(g_hBm[g_currLayer] != NULL)
            DeleteObject(g_hBm[g_currLayer]);
          g_hBm[g_currLayer] = t;
          if(g_hBmAlpha[g_currLayer] != NULL)
            DeleteObject(g_hBmAlpha[g_currLayer]);
          g_hBmAlpha[g_currLayer] = tAlpha;
          //if(g_hBmBlended[g_currLayer] != NULL)
          //  DeleteObject(g_hBmBlended[g_currLayer]);
          //g_hBmBlended[g_currLayer] = tBlend;
          updateWindowSize();
          updateTitle();
          clampDxDy(g_dx, g_dy);
          repaint();
        }
        denotify();
      }
      else if(toupper(LOWORD(wP)) == 'T')
      {
        //toggle always-on-top mode
        LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
        if((exStyle & WS_EX_TOPMOST) != 0)
          SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        else
          SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        updateTitle();
      }
      else if(toupper(LOWORD(wP)) == 'S') //save image
      {
        if(g_currILImage == 0)
        {
          MessageBox(hWnd, "No image loaded, unable to save", "ERROR:", MB_OK);
          return 0;
        }

        bindCurrImage();
        string newName;
        if(saveIlImage(hWnd, g_imageName, &newName))
        {
          if(g_imageName == CLIPBOARD_IMAGE)
          {
            g_imageName = tolower(newName);
            g_oldImageName = "";
          }
        }
      }
      else if(toupper(LOWORD(wP)) == 'D') //display developer data
      {
        //TODO: use an sstream instead of sprintf!
        MEMORYSTATUS ms;
        GlobalMemoryStatus(&ms);
        char buff[1024];

        //How to distinguish DDB and DIB HBITMAPS:
        //http://www.codeproject.com/bitmap/DFB_vs_DIB.asp
        BITMAP bm;
        GetObject(getActiveBitmap(), sizeof(bm), &bm);

        sprintf(buff, "Time to load: %.2f sec\n"
                      "Time to convert rgb to hbitmap: %.2f\n"
                      "Time to convert alpha to hbitmap: %.2f\n"
                      "Bitmap type: %s\n\n"
                      "Memory load: %d %%\nTotal physical RAM: %s\n"
                      "Avail physical RAM: %s\nTotal virtual RAM: %s\n"
                      "Avail virtual RAM: %s",
                      g_bmLoadTime, g_hbmConvTime, g_alphaConvTime,
                      getActiveBitmap() == NULL?"NULL":bm.bmBits == NULL?"DDB":"DIBSection",
                      ms.dwLength, 
                      byteCountToString(ms.dwTotalPhys).c_str(),
                      byteCountToString(ms.dwAvailPhys).c_str(),
                      byteCountToString(ms.dwTotalVirtual).c_str(),
                      byteCountToString(ms.dwAvailVirtual).c_str());

        string s = buff;

        s += "\n\nI'm using the following DevIL libraries:\n";

        const char* t = ilGetString(IL_VERSION_NUM);
        if(t != NULL)
        {
          s += t;
          s += "\n";
        }

        t = iluGetString(ILU_VERSION_NUM);
        if(t != NULL)
        {
          s += t;
          s += "\n";
        }

        t = ilutGetString(ILUT_VERSION_NUM);
        if(t != NULL)
        {
          s += t;
          s += "\n";
        }

        MessageBox(hWnd, s.c_str(), "Load Times", MB_OK);
      }
      else if(toupper(LOWORD(wP)) == 'H') //display help (for yann)
      {
        MessageBox(hWnd,
          "Controls:\n"
          "ESC - quit\n"
          "SPACE - next image in same folder\n"
          "BACKSPACE - previous image in same folder (see SPACE)\n"
          "MouseWheel - same as SPACE and BACKSPACE\n"
          "PRIOR/NEXT (aka PAGE UP/PAGE DOWN) - go through image layers (if the current image has\n"
          "             more than only one layer - cubemaps for example)\n"
          "M - toggle tiled image drawing (to check for seamless images)\n"
          "A - toggle alpha channel display (if ALPHA is displayed in status bar)\n"
          "B - toggle alpha blend display (if ALPHA is displayed in status bar)\n"
          "F - toggle fullscreen\n"
          "rclick while left mouse is down - toggle fullscreen\n"
          "mclick - toggle fullscreen\n"
          "leftdrag - translate image that doesn't fit into the window\n"
          "Ctrl/Shift + leftdrag - copies/moves current image to the place you drag it\n"
          "R - register supported file types in explorer (asks you if you're serious first)\n"
          "Q/W - rotate left/right 90 degree\n"
          "T - toggle always on top\n"
          "DEL - moves current image to the recycle bin (asks you if you're serious first)\n"
          "S - saves a copy of the current image at the given position with the given format\n"
          "    (no RLE coding is used. if the current image is a cubemap, the current face\n"
          "     is saved. cubemap saving is not yet supported).\n"
          "C - cubemap view if the current image is a cubemap (6 layers of same size)\n"
          "D - detailed loading times\n"
          "+ - zoom in\n"
          "- - zoom out\n"
          "/ - set zoom to 100%\n"
          "* - zoom image to fit screen\n"
          "F1/H - display this help screen\n"
          "Drag image from explorer onto ddsview - opens the dragged image\n"
          "Drag folder from explorer onto ddsview - opens the dragged folder in DeepBrowse mode\n"
          "CTRL-C/V - copy/paste image to/from clipboard\n"
          "F2 - Rename current image\n"
          "Shift+F2 - Rename current image with extension\n"
          "Right/Left Arrow - View next/previous mipmap\n"
          "Up/Down Arrow - Go through slices in a 3d image. Down goes one slice to the front, up to the back\n"
          "Doubleclick toolbar to customize it. Alt-Drag toolbar icons to move them around.\n"
          "O - Open Image\n"
          "I - Toggle toolbar icons\n"
          "L - Lock window size/autozoom too large images to fit\n"
          ,
          "Help", MB_OK);
      }
      else if(LOWORD(wP) == 3/*control-c, don't know the official name :)*/) //copy to clipboard
      {
        if(!ilutSetWinClipboard())
          MessageBox(hWnd, "Failed to copy image", "ERROR", MB_OK);
      }
      else if(LOWORD(wP) == 22 /*control-v, don't know the official name :)*/) //paste from clipboard
      {
        if(!IsClipboardFormatAvailable(CF_DIB))
          return 0;

        //delete previously loaded images
        deleteHBitmaps();

        int oldW = getRealImageWidth();
        int oldH = getRealImageHeight();

        //TODO: XP requires these 4 lines to make image pasting work.
        //This suggests there's a bug deep in DevIL that happens
        //when existing images are overwritten. However, this bug
        //occurs only under very special circumstances on XP:
        //you have to start ddsview from the explorer (if you start
        //it from MSVC, everything is fine) and the release build
        //of the DevIL libraries is required - so the bug is probably
        //quite subtle. I'll have to find it :(
        //(I guess it has to do with binding issues: I could probably
        //bind the base image instead of iluDeleteImage() (if another layer
        //is bound, but I haven't tried it)
        if(g_currILImage != 0)
          iluDeleteImage(g_currILImage);
        g_currILImage = iluGenImage();
        ilBindImage(g_currILImage);



        //draw loading screen
        notify("Loading image from clipboard...");

        DWORD startTime = GetTickCount();
        if(!ilutGetWinClipboard())
          MessageBox(hWnd, "Failed to paste image", "ERROR", MB_OK);
        g_bmLoadTime = (GetTickCount() - startTime)*.001f;

        int bmWid = ilGetInteger(IL_IMAGE_WIDTH);
        int bmHyt = ilGetInteger(IL_IMAGE_HEIGHT);

        //collect infomation about the loaded image
        g_bmDep = ilGetInteger(IL_IMAGE_DEPTH);
        g_bmLayers = ilGetInteger(IL_NUM_IMAGES);
        g_bmMipmaps = ilGetInteger(IL_NUM_MIPMAPS);
        if(g_bmLayers > 5)
            MessageBox(g_hWnd, "Only 6 layers are currently supported, "
            "you won't be able to view all the image's layers.", "Note:", MB_OK);

        //load image onto hbitmaps
        HDC hDc = GetDC(g_hWnd);
        g_alphaConvTime = g_hbmConvTime = 0;
        ILuint ilImg = g_currILImage;
        for(int j = 0; j <= min(g_bmLayers, 5); ++j) //a bmp has only one layer, of course
        {
          ilBindImage(ilImg);
          ilActiveImage(j); //cube faces or whatever

          startTime = GetTickCount();
          g_hBm[j] = ilutConvertToHBitmap(hDc);
          g_hbmConvTime += (GetTickCount() - startTime)*.001f;;
          if(ilHasAlpha() == IL_TRUE)
          {
            startTime = GetTickCount();
            g_hBmAlpha[j] = ilutConvertAlphaToHBitmap(hDc);
            g_alphaConvTime += (GetTickCount() - startTime)*.001f;;
          }
        }
        ReleaseDC(g_hWnd, hDc);

        ilBindImage(ilImg);

        ILuint err = ilGetError();
        string errMsg;
        while(err != IL_NO_ERROR)
        {
          const ILstring t = iluErrorString(err);
          errMsg += string("\n");
          if(t != NULL) errMsg += t;
          else errMsg += "SERIOUS DEVIL PROBLEM, PLEASE CONTACT AUTHOR (nicolasweber@gmx.de)";
          err = ilGetError();
        }

        if(errMsg != "")
        {
          errMsg += "\nThe image data is incomplete, if available at all.";
          MessageBox(g_hWnd, errMsg.c_str(), "Sorry...", MB_OK);
        }

        if(g_imageName != CLIPBOARD_IMAGE)
          g_oldImageName = g_imageName;
        g_imageName = CLIPBOARD_IMAGE;
        g_imageFileSize = "N/A";
        g_currLayer = 0;
        g_currMipmapLevel = 0;
        g_currSlice = 0;
        g_dx = g_dy = 0;
        g_scale = 1.f;

        g_rotation = ROT_NONE;
        ilBindImage(g_currILImage);
        ILboolean b = ilIsCubemap();
        ilActiveImage(0);
        if(g_displayMode == DM_CUBE_1 && b == IL_FALSE)
          g_displayMode = DM_2D;
        if(ilHasAlpha() == IL_FALSE)
          g_displayAlpha = false;
        denotify();

        if(getRealImageWidth() != oldW || getRealImageHeight() != oldH)
        {
          g_dx = g_dy = 0;
          g_scale = 1;
          //updateScale();
          if(g_lockWindowSize && !doesImageFit() && !g_isFullscreen)
            zoomToFit();
        }
        else
        {
          //if the new images has the same size as the old one,
          //keep scale etc (useful if one wants to compare two images)
        }

        updateWindowSize();
        updateTitle();
        repaint();
      }
      else if(toupper(LOWORD(wP)) == 'O')
      {
        //display file open dialog, load image
        static char buffer[MAX_PATH] = "";
        static bool isInited = false;
        static OPENFILENAME ofn;
        if(!isInited)
        {
          isInited = true;
          ZeroMemory(&ofn, sizeof(OPENFILENAME));
          ofn.lStructSize = sizeof(OPENFILENAME);
          ofn.hwndOwner = hWnd;
          ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
          ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
          ofn.lpstrFile = buffer;
          ofn.nMaxFile = MAX_PATH;
        }

        if(GetOpenFileName(&ofn))
        {
          g_baseDir = "";
          loadImage(tolower(ofn.lpstrFile));
        }

      }
      else if(toupper(LOWORD(wP)) == 'I')
      {
        //toggle toolbar
        if(g_isFullscreen)
          return 0;

        bool b = g_displayToolbar;
        g_displayToolbar = !g_displayToolbar;

        if(b)
        {
          DestroyWindow(g_toolbar);
          g_toolbar = NULL;
        }
        else
        {
          g_toolbar = createToolbar(g_hInst, hWnd);
          updateToolbar(g_toolbar);
        }


        RECT r;
        getRealClientRect(r);
        g_w = r.right - r.left;
        g_h = r.bottom - r.top;
        clampDxDy(g_dx, g_dy);
        //updateWindowSize(); //<- sucks...just feels plain wrong
        repaint();
      }
      else if(toupper(LOWORD(wP)) == 'L')
      {
        //toggle zoom lock

        if(g_isFullscreen)
          return 0;

        g_lockWindowSize = !g_lockWindowSize;
        updateTitle();
      }

      return 0;
    }break;

    case WM_SIZE:
    {
      RECT r;
      getRealClientRect(r);
      g_w = r.right - r.left;
      g_h = r.bottom - r.top;


      clampDxDy(g_dx, g_dy);

      //This was moved to WM_PAINT
      if(false) //s_isWindowsXP)
      {
        //windows XP's status bar seems to be buggy when
        //using the new visual styles: right-aligned text
        //in a status bar part overlaps into the other
        //parts without the following hack (you can still
        //see the it flicker during the resizing).
        //TODO: find a better hack

        UpdateWindow(hWnd);
        SendMessage(g_statusbar, WM_SIZE, wP, lP);
        InvalidateRect(g_statusbar, NULL, FALSE);
        UpdateWindow(g_statusbar);
        updateStatusBar();
      }
      else
        SendMessage(g_statusbar, WM_SIZE, wP, lP);

      if(g_displayToolbar)
        SendMessage(g_toolbar, TB_AUTOSIZE, 0, 0);

      //We have to do that ourselves because our window
      //doesn't have CS_H/VREDRAW style set. This way,
      //flicker in toolbar and statusbar is considerably
      //minimized
      InvalidateRect(hWnd, &r, FALSE);

      return 0;
    }break;

    case WM_SETCURSOR:
    {
#if 1
      POINT mousePos;
      GetCursorPos(&mousePos);

      //if the cursor is in the window's client area...
      if(LOWORD(lP) == HTCLIENT && (HWND)wP == hWnd)
      {
        //...and the image is taller than this area...
        if(!doesImageFit())
        {
          //...set another cursor
          SetCursor(LoadCursor(NULL, IDC_SIZEALL));
          return TRUE;
        }
      }
      return DefWindowProc(hWnd, msg, wP, lP);
#else
      //this cannot be used because it overwrites
      //even the window resize cursors - and we want
      //to keep them
      setCursor();
      return TRUE;
#endif
    }break;

    case WM_NOTIFY:
    {
      if(g_displayToolbar)
        return toolbarNotify((NMHDR*)lP, g_toolbar);
      else
        return 0;
    }break;

    case WM_COMMAND:
    {
      if(g_displayToolbar)
        return toolbarCommand(hWnd, g_toolbar, LOWORD(wP));
      else
        return 0;
    }break;

    case WM_DESTROY:
    {
      deleteHBitmaps();

      if(g_currILImage != 0)
        iluDeleteImage(g_currILImage);
      g_currILImage = 0;

      if(s_isOleInitialized)
        OleUninitialize();

      PostQuitMessage(0);
      return 0;
    }break;

    default:
      return DefWindowProc(hWnd, msg, wP, lP);
  }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE old, LPSTR params, int showCmd)
{
  //TODO:
  //SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

  g_hInst = hInst;

  //enable mousewheel support on win95/NT3.5
  HWND mouseWnd = FindWindow(MOUSEZ_CLASSNAME, MOUSEZ_TITLE);
  if(mouseWnd != NULL)
  {
    UINT isWheelEnabledMsg = RegisterWindowMessage(MSH_WHEELSUPPORT);
    if(SendMessage(mouseWnd, isWheelEnabledMsg, 0, 0))
    {
      g_win95HasMousewheel = true;
      g_win95MousewheelMessage = RegisterWindowMessage(MSH_MOUSEWHEEL);
    }
  }

  //register a window class

  //This comment comes from MSDN sample code:
  /* Don't specify CS_HREDRAW or CS_VREDRAW if you're going to use the
  ** commctrl status or toolbar -- invalidate the (remaining) client
  ** area yourself if you want this behavior. This will allow the child
  ** control redraws to be much more efficient.
  */
  WNDCLASS wc = { 0, eventListener, 0, 0, hInst,
                  LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_APP)), LoadCursor(NULL, IDC_ARROW),
                  /*(HBRUSH)(COLOR_WINDOW + 1)*/NULL, NULL, CLASS_NAME };

  if(!RegisterClass(&wc))
  {
    ErrorBox("RegisterClass() failed");
    return -1;
  }

  //create window
  int x, y, wid = 640, hyt = 150;
  RECT desk;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &desk, 0);
  x = (desk.left + desk.right - wid)/2;
  y = (desk.top + desk.bottom - hyt)/2;
  g_hWnd = CreateWindow(CLASS_NAME, WINDOW_TITLE, WINDOW_STYLE,
                        x, y, wid, hyt, NULL, NULL, hInst, NULL);

  if(g_hWnd == NULL)
  {
    ErrorBox("CreateWindow() failed");
    return -1;
  }

  initIL();

  ShowWindow(g_hWnd, showCmd);
  UpdateWindow(g_hWnd);

    //remove " chars windoze puts around spaced filenames
    if(params != NULL)
    {
      bool found = false;
      while(*params != '\0')
        if(*params == '\"')
          ++params;
        else
        {
          found = true;
          break;
        }

      if(found) // " found, find second one
      {
        int i = 0;
        while(params[i] != '\0' && params[i] != '\"')
          ++i;
        params[i] = '\0';
      }
    }

  //convert param to full long pathname
  //(that is bla\hallo~1.bmp becomes c:\data\bla\halloween.bmp for example)
  if(params != NULL && params[0] != '\0')
  {
    char buff1[MAX_PATH];
    char buff2[MAX_PATH];
    char* t;
    //MessageBox(0, params, 0, 0);
    if(GetFullPathName(params, MAX_PATH, buff1, &t))
    {
      //MessageBox(0, buff1, 0, 0);
      if(GetLongPathName(buff1, buff2, MAX_PATH) != 0)
        loadImage(tolower(buff2));
      else
        loadImage(buff1);
      //MessageBox(0, buff2, 0, 0);
    }
    else
    {
      if(GetLongPathName(params, buff1, MAX_PATH) != 0)
        loadImage(tolower(buff1));
      else
        loadImage(params);
    }
  }

  MSG msg;
  while(GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  ilShutDown();
  delete [] SUPPORTED_FORMATS;

  return msg.wParam;
}
