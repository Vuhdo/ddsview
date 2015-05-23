#include "Precompiled.h"
#include "wintoolbar.h"

#include <commctrl.h>

#include "resource.h"
#include "image.h"

const int TOOLBAR_FIRST_ICON = 100;

const int TOOLBAR_OPEN = TOOLBAR_FIRST_ICON;
const int TOOLBAR_SAVE = TOOLBAR_FIRST_ICON + 1;
const int TOOLBAR_DELETE = TOOLBAR_FIRST_ICON + 2;
const int TOOLBAR_RENAME = TOOLBAR_FIRST_ICON + 3;

const int TOOLBAR_PREV_IMAGE = TOOLBAR_FIRST_ICON + 5;
const int TOOLBAR_NEXT_IMAGE = TOOLBAR_FIRST_ICON + 6;

const int TOOLBAR_ZOOM_IN = TOOLBAR_FIRST_ICON + 8;
const int TOOLBAR_ZOOM_OUT = TOOLBAR_FIRST_ICON + 9;
const int TOOLBAR_ZOOM_100 = TOOLBAR_FIRST_ICON + 10;
const int TOOLBAR_ZOOM_FIT = TOOLBAR_FIRST_ICON + 11;
const int TOOLBAR_ZOOM_LOCK = TOOLBAR_FIRST_ICON + 12;
const int TOOLBAR_ROTATE_LEFT = TOOLBAR_FIRST_ICON + 13;
const int TOOLBAR_ROTATE_RIGHT = TOOLBAR_FIRST_ICON + 14;

const int TOOLBAR_COPY = TOOLBAR_FIRST_ICON + 16;
const int TOOLBAR_PASTE = TOOLBAR_FIRST_ICON + 17;

const int TOOLBAR_ALPHA = TOOLBAR_FIRST_ICON + 19;
const int TOOLBAR_BLEND = TOOLBAR_FIRST_ICON + 20;

const int TOOLBAR_TILE_VIEW = TOOLBAR_FIRST_ICON + 22;
const int TOOLBAR_CUBEMAP_VIEW = TOOLBAR_FIRST_ICON + 23;

const int TOOLBAR_PREV_LAYER = TOOLBAR_FIRST_ICON + 25;
const int TOOLBAR_NEXT_LAYER = TOOLBAR_FIRST_ICON + 26;
const int TOOLBAR_NEXT_MIPMAP = TOOLBAR_FIRST_ICON + 27;
const int TOOLBAR_PREV_MIPMAP = TOOLBAR_FIRST_ICON + 28;
const int TOOLBAR_NEXT_SLICE = TOOLBAR_FIRST_ICON + 29;
const int TOOLBAR_PREV_SLICE = TOOLBAR_FIRST_ICON + 30;

const int TOOLBAR_OPTIONS = TOOLBAR_FIRST_ICON + 32;
const int TOOLBAR_ALWAYS_ON_TOP = TOOLBAR_FIRST_ICON + 33;
const int TOOLBAR_LOAD_TIMES = TOOLBAR_FIRST_ICON + 34;
const int TOOLBAR_ABOUT = TOOLBAR_FIRST_ICON + 35;
const int TOOLBAR_HELP = TOOLBAR_FIRST_ICON + 36;

/*

Required toolbar code:

dis/enable alpha buttons (k), layer/mipmap/slice/cube (k)
keep alpha (k), tile/cubeview (k), alwaysontop (k) up to date
keep zoom lock toggle up to date (k)
dis/enable next/prev mipmap on last/first mipmap (k)

keep mouse on next/previous image/layer/mipmap/slice (all icons that change window size) (k)


dis/enable zoom to fit and zoom 100% when already done


*/

extern HWND g_hWnd;

void updateToolbar(HWND toolbar)
{
  LPARAM disableState = TBSTATE_HIDDEN;

  //keep always-on-top toggle up to date
  LONG exStyle = GetWindowLong(g_hWnd, GWL_EXSTYLE);
  if((exStyle & WS_EX_TOPMOST) != 0)
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ALWAYS_ON_TOP, MAKELONG(TRUE, 0));
  else
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ALWAYS_ON_TOP, MAKELONG(FALSE, 0));

  //dis/enable alpha/blend checkboxes and keep them up to date
  if(hasImageAlpha())
  {
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_ALPHA, TBSTATE_ENABLED);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_BLEND, TBSTATE_ENABLED);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_BLEND + 1, TBSTATE_ENABLED);

    if(g_displayAlpha)
    {
      if(g_displayBlended)
      {
        SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ALPHA, MAKELONG(FALSE, 0));
        SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_BLEND, MAKELONG(TRUE, 0));
      }
      else
      {
        SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ALPHA, MAKELONG(TRUE, 0));
        SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_BLEND, MAKELONG(FALSE, 0));
      }
    }
    else
    {
      SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ALPHA, MAKELONG(FALSE, 0));
      SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_BLEND, MAKELONG(FALSE, 0));
    }
  }
  else
  {
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ALPHA, MAKELONG(FALSE, 0));
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_BLEND, MAKELONG(FALSE, 0));

    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_ALPHA, disableState);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_BLEND, disableState);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_BLEND + 1, disableState);
  }

  //keep zoom lock toggle up to date
  if(g_lockWindowSize)
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ZOOM_LOCK, MAKELONG(TRUE, 0));
  else
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_ZOOM_LOCK, MAKELONG(FALSE, 0));

  //dis/enable cubemap view and keep cubemap view andtileview checkboxes up to date

  //TBSTATE_ENABLED overrides the TBSTATE_CHECKED, so this has to happen
  //before the checkbox logic
  if(isImageCubemap())
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_CUBEMAP_VIEW, TBSTATE_ENABLED);
  else
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_CUBEMAP_VIEW, disableState);

  if(g_displayMode == DM_CUBE_1)
  {
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_CUBEMAP_VIEW, MAKELONG(TRUE, 0));
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_TILE_VIEW, MAKELONG(FALSE, 0));
  }
  else if(g_displayMode == DM_2D_TILED)
  {
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_CUBEMAP_VIEW, MAKELONG(FALSE, 0));
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_TILE_VIEW, MAKELONG(TRUE, 0));
  }
  else
  {
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_CUBEMAP_VIEW, MAKELONG(FALSE, 0));
    SendMessage(toolbar, TB_CHECKBUTTON, TOOLBAR_TILE_VIEW, MAKELONG(FALSE, 0));
  }

  bool isGroupEmpty = true;
  //dis/enable layer buttons
  if(g_bmLayers > 0)
  {
    isGroupEmpty = false;
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_LAYER, TBSTATE_ENABLED);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_LAYER, TBSTATE_ENABLED);
  }
  else
  {
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_LAYER, disableState);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_LAYER, disableState);
  }

  //dis/enable mipmap buttons
  if(g_bmMipmaps > 0)
  {
    isGroupEmpty = false;
    if(g_currMipmapLevel != g_bmMipmaps)
      SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_MIPMAP, TBSTATE_ENABLED);
    else
      SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_MIPMAP, TBSTATE_INDETERMINATE);

    if(g_currMipmapLevel != 0)
      SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_MIPMAP, TBSTATE_ENABLED);
    else
      SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_MIPMAP, TBSTATE_INDETERMINATE);
  }
  else
  {
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_MIPMAP, disableState);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_MIPMAP, disableState);
  }

  //dis/enable slice buttons
  if(g_bmDep > 1)
  {
    isGroupEmpty = false;
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_SLICE, TBSTATE_ENABLED);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_SLICE, TBSTATE_ENABLED);
  }
  else
  {
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_NEXT_SLICE, disableState);
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_SLICE, disableState);
  }

  if(isGroupEmpty)
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_SLICE + 1, disableState);
  else
    SendMessage(toolbar, TB_SETSTATE, TOOLBAR_PREV_SLICE + 1, TBSTATE_ENABLED);

}

struct Tool
{
  char* tip;
  int imageIndex;
  int style;
  bool enabled;
};


Tool tools[] =
{
  { "Open image (o)", 0, TBSTYLE_BUTTON, true },
  { "Save image (s)", 1, TBSTYLE_BUTTON, true },
  { "Delete image (del)", 2, TBSTYLE_BUTTON, true },
  { "Rename image, hold Shift to change image's extension as well (F2)", 29, TBSTYLE_BUTTON, true },

  { "", 0, TBSTYLE_SEP, true },

  { "Previous image (backspace/mousewheel up)", 3, TBSTYLE_BUTTON, true },
  { "Next image (space/mousewheel down)", 4, TBSTYLE_BUTTON, true },

  { "", 0, TBSTYLE_SEP, true },

  { "Zoom in (+)", 5, TBSTYLE_BUTTON, true },
  { "Zoom out (-)", 6, TBSTYLE_BUTTON, true },
  { "Zoom 100% (/)", 7, TBSTYLE_BUTTON, true },
  { "Zoom to fit (*)", 8, TBSTYLE_BUTTON, true },
  { "Lock window size/autozoom too large images to fit (l)", 9, TBSTYLE_CHECK, true },
  { "Rotate left (q)", 10, TBSTYLE_BUTTON, true },
  { "Rotate right (w)", 11, TBSTYLE_BUTTON, true },

  { "", 0, TBSTYLE_SEP, true },

  { "Copy (Ctrl+C)", 13, TBSTYLE_BUTTON, true },
  { "Paste (Ctrl+V)", 12, TBSTYLE_BUTTON, true },

  { "", 0, TBSTYLE_SEP, true },

  { "Show alpha channel (a)", 15, TBSTYLE_CHECK, true },
  { "Show image alpha blended (b)", 16, TBSTYLE_CHECK, true },

  { "", 0, TBSTYLE_SEP, true },

  { "Toggle Tile View (m)", 14, TBSTYLE_CHECK, true },
  { "Toggle Cubemap View (c)", 21, TBSTYLE_CHECK, true },

  { "", 0, TBSTYLE_SEP, true },

  { "Previous layer (Page Up (aka Prior))", 17, TBSTYLE_BUTTON, true },
  { "Next layer (Page Down (aka Next))", 18, TBSTYLE_BUTTON, true },
  { "Next Mipmap (right arrow)", 19, TBSTYLE_BUTTON, true },
  { "Previous Mipmap (left arrow)", 20, TBSTYLE_BUTTON, true },
  { "Next slice in 3d image (arrow down)", 23, TBSTYLE_BUTTON, true },
  { "Previous slice in 3d image (arrow up)", 22, TBSTYLE_BUTTON, true },

  { "", 0, TBSTYLE_SEP, true },

  //{ "Show options", 24, TBSTYLE_BUTTON, false },
  { "Set file associations (r)", 24, TBSTYLE_BUTTON, true },
  { "Toggle always on top (t)", 25, TBSTYLE_CHECK, true },
  { "Display load times (d)", 26, TBSTYLE_BUTTON, true },
  { "About", 27, TBSTYLE_BUTTON, false },
  { "Help (F1/h)", 28, TBSTYLE_BUTTON, true },
};

const int numButtons = sizeof(tools)/sizeof(Tool);

TBBUTTON toolToWin32(int i)
{
  TBBUTTON ret = { tools[i].imageIndex, i + TOOLBAR_FIRST_ICON,
    tools[i].enabled?TBSTATE_ENABLED:TBSTATE_INDETERMINATE,
    tools[i].style, 0, -1 };
  return ret;
}

void addTools(HWND toolbar)
{
  int n = 0;

  TBBUTTON buttons[numButtons];
  for(int i = 0; i < numButtons; ++i)
  {
    buttons[i].iBitmap = tools[i].imageIndex;
    buttons[i].idCommand = i + TOOLBAR_FIRST_ICON;
    buttons[i].fsState = tools[i].enabled?TBSTATE_ENABLED:TBSTATE_INDETERMINATE;
    buttons[i].fsStyle = tools[i].style;
    buttons[i].dwData = 0;
    buttons[i].iString = -1;

    if(tools[i].style != TBSTYLE_SEP)
      ++n;
  }

  SendMessage(toolbar, TB_ADDBUTTONS, numButtons, (LPARAM)&buttons);
}


HWND createToolbar(HINSTANCE hInst, HWND hWnd)
{
#if 1
  HBITMAP hBitmap = (HBITMAP)LoadImage(hInst,
                               MAKEINTRESOURCE(IDB_TOOLBAR),
                               IMAGE_BITMAP, 0, 0, 
                               LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS);
#else
  HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, "toolbar.bmp", IMAGE_BITMAP, 0, 0, 
                               LR_LOADFROMFILE | LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS);
#endif

  HWND ret = CreateToolbarEx(hWnd,
    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
    11,
    30,
    NULL, (int)hBitmap,
    NULL, 0, //buttons set later
    0, 0, 0, 0, sizeof(TBBUTTON));

  addTools(ret);
  SendMessage(ret, TB_SETSTYLE, 0, TBSTYLE_FLAT | CCS_TOP | TBSTYLE_TOOLTIPS |
    //wrapable style doesn't work with separators
    //use nodivider to remove the two stupid pixel lines on top of the toolbar
    CCS_ADJUSTABLE | CCS_NODIVIDER | TBSTYLE_ALTDRAG); // | TBSTYLE_WRAPABLE);// );
  SendMessage(ret, TB_AUTOSIZE, 0, 0);
  ShowWindow(ret, SW_SHOW);

  return ret;
}

LRESULT toolbarNotify(NMHDR* hdr, HWND toolbar)
{
  if(hdr->code == TTN_NEEDTEXT)
  {
    TOOLTIPTEXT* ttt = (TOOLTIPTEXT*)hdr;
    ttt->lpszText = tools[hdr->idFrom - TOOLBAR_FIRST_ICON].tip;
    return 1;
  }

  else if(hdr->code == TBN_GETBUTTONINFO)
  {
    NMTOOLBAR* nmt = (NMTOOLBAR*)hdr;
    if(nmt->iItem < numButtons)
    {
      nmt->tbButton = toolToWin32(nmt->iItem);
      nmt->pszText = tools[nmt->iItem].tip;
      return 1;
    }
    else
      return 0;
  }

  else if(hdr->code == TBN_RESET)
  {
    int n = SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0);
    for(int i = 0; i < n; ++i)
      SendMessage(toolbar, TB_DELETEBUTTON, 0, 0);
    addTools(toolbar);
    updateToolbar(toolbar);
    return 1;
  }


  else if(hdr->code == TBN_QUERYDELETE || hdr->code == TBN_QUERYINSERT)
    return 1;

  else if(hdr->code == TBN_CUSTHELP)
  {
    return 1;
  }

  else
    return 0;
}

LRESULT toolbarCommand(HWND viewerWnd, HWND toolbar, int id)
{
  int oldWid = getClientWidth();
  int oldHyt = getClientHeight();

  //This function is completely stupid and
  //unextendable... (TODO:)
  switch(id)
  {
    case TOOLBAR_OPEN:
      SendMessage(viewerWnd, WM_CHAR, 'O', 0);
      break;
    case TOOLBAR_SAVE:
      SendMessage(viewerWnd, WM_CHAR, 'S', 0);
      break;
    case TOOLBAR_DELETE:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_DELETE, 0);
      break;
    case TOOLBAR_RENAME:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_F2, 0);
      break;

    case TOOLBAR_PREV_IMAGE:
      SendMessage(viewerWnd, WM_CHAR, '\b', 0);
      break;
    case TOOLBAR_NEXT_IMAGE:
      SendMessage(viewerWnd, WM_CHAR, ' ', 0);
      break;

    case TOOLBAR_ZOOM_IN:
      SendMessage(viewerWnd, WM_CHAR, '+', 0);
      break;
    case TOOLBAR_ZOOM_OUT:
      SendMessage(viewerWnd, WM_CHAR, '-', 0);
      break;
    case TOOLBAR_ZOOM_100:
      SendMessage(viewerWnd, WM_CHAR, '/', 0);
      break;
    case TOOLBAR_ZOOM_FIT:
      SendMessage(viewerWnd, WM_CHAR, '*', 0);
      break;
    case TOOLBAR_ZOOM_LOCK:
      SendMessage(viewerWnd, WM_CHAR, 'L', 0);
      break;
    case TOOLBAR_ROTATE_LEFT:
      SendMessage(viewerWnd, WM_CHAR, 'Q', 0);
      break;
    case TOOLBAR_ROTATE_RIGHT:
      SendMessage(viewerWnd, WM_CHAR, 'W', 0);
      break;

    case TOOLBAR_COPY:
      SendMessage(viewerWnd, WM_CHAR, 3, 0);
      break;
    case TOOLBAR_PASTE:
      SendMessage(viewerWnd, WM_CHAR, 22, 0);
      break;

    case TOOLBAR_ALPHA:
      SendMessage(viewerWnd, WM_CHAR, 'A', 0);
      break;
    case TOOLBAR_BLEND:
      SendMessage(viewerWnd, WM_CHAR, 'B', 0);
      break;

    case TOOLBAR_TILE_VIEW:
      SendMessage(viewerWnd, WM_CHAR, 'M', 0);
      break;
    case TOOLBAR_CUBEMAP_VIEW:
      SendMessage(viewerWnd, WM_CHAR, 'C', 0);
      break;

    case TOOLBAR_PREV_LAYER:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_PRIOR, 0);
      break;
    case TOOLBAR_NEXT_LAYER:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_NEXT, 0);
      break;
    case TOOLBAR_NEXT_MIPMAP:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_RIGHT, 0);
      break;
    case TOOLBAR_PREV_MIPMAP:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_LEFT, 0);
      break;
    case TOOLBAR_NEXT_SLICE:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_DOWN, 0);
      break;
    case TOOLBAR_PREV_SLICE:
      SendMessage(viewerWnd, WM_KEYDOWN, VK_UP, 0);
      break;

    case TOOLBAR_OPTIONS:
      //show register file types dialog until
      //a "real" options dialog is implemented
      SendMessage(viewerWnd, WM_CHAR, 'R', 0);
      break;
    case TOOLBAR_ALWAYS_ON_TOP:
      SendMessage(viewerWnd, WM_CHAR, 'T', 0);
      break;
    case TOOLBAR_LOAD_TIMES:
      SendMessage(viewerWnd, WM_CHAR, 'D', 0);
      break;
    case TOOLBAR_HELP:
      SendMessage(viewerWnd, WM_CHAR, 'H', 0);
      break;
  }

  //keep cursor in icons that change window size
  if(getClientWidth() != oldWid || getClientHeight() != oldHyt)
  {
    int index = id - TOOLBAR_FIRST_ICON;
    RECT r;
    if(SendMessage(toolbar, TB_GETITEMRECT, index, (LPARAM)&r) == TRUE)
    {
      POINT p = { (r.left + r.right)/2, (r.top + r.bottom)/2 };
      ClientToScreen(toolbar, &p);

      int screenWid = GetSystemMetrics(SM_CXSCREEN);
      int screenHyt = GetSystemMetrics(SM_CYSCREEN);


      mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE,
        (p.x*65535)/screenWid, (p.y*65535)/screenHyt, //dx, dy are normalized to [0, 65535]
        0, 0);
    }
  }

  return 0;
}
