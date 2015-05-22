#include <iostream>
using namespace std;

#include <X11/Xlib.h>
//#define XK_LATIN1
//#define XK_MISCELLANY
//#include <X11/keysymdef.h> //XK_A, XK_a, XK_Z, XK_z
#include <X11/keysym.h> //same as above, but defines not needed
#include <GL/glx.h>

Display* g_display;

int main(int argc, char* argv[])
{
  g_display = XOpenDisplay(NULL);
  if(g_display == NULL)
  {
    cerr << "Failed to open display" << endl;
    return 1;
  }

  int errorBase, eventBase;
  if(!glXQueryExtension(g_display, &errorBase, &eventBase))
  {
    cerr << "Failed to query glx" << endl;
    return 1;
  }

  int major = 0, minor = 0;
  if(!glXQueryVersion(g_display, &major, &minor)
      || major < 1 || (major == 1 && minor < 3))
  {
    cerr << "glx 1.3 required, only " << major << "."
         << minor << " found." << endl;
    return 1;
  }

  int num;
  int attribs[] = { GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
                    GLX_DOUBLEBUFFER, False,
                    GLX_RENDER_TYPE, GLX_RGBA_BIT,
                    None };
  
  GLXFBConfig* configs = glXChooseFBConfig(g_display,
      XDefaultScreen(g_display), attribs, &num);
  if(configs == NULL || num < 1)
  {
    cerr << "No config found" << endl;
    return 1;
  }
  GLXFBConfig config = configs[0];
  XFree(configs);
    
  XVisualInfo* vis = glXGetVisualFromFBConfig(g_display, config);
  if(vis == NULL)
  {
    cerr << "Couldn't get visual" << endl;
    return 1;
  }


  int x = 100, y = 100, wid = 640, hyt = 150;
  XSetWindowAttributes swa;
  
  swa.event_mask = ExposureMask | StructureNotifyMask
    | ButtonPressMask | ButtonReleaseMask | KeyPressMask
    | PointerMotionMask | StructureNotifyMask;
  
  //It's really important to create an own colormap for
  //this window, else glx context creation might fail on some
  //cards
  swa.colormap = XCreateColormap(g_display, XRootWindow(g_display, vis->screen),
      vis->visual, AllocNone);
  Window win = XCreateWindow(g_display, XRootWindow(g_display, vis->screen),
      x, y, wid, hyt, 0, 24, InputOutput, vis->visual,
     CWEventMask | CWColormap, &swa);
  GLXWindow glXWin = glXCreateWindow(g_display, config, win, NULL);
      
  GLXContext glXContext = glXCreateNewContext(g_display, config,
      GLX_RGBA_TYPE, NULL, True);
  if(glXContext == NULL)
  {
    cerr << "Failed to create glXContext" << endl;
    return 1;
  }
  

  XMapWindow(g_display, win);
  glXMakeContextCurrent(g_display, glXWin, glXWin, glXContext);

  Atom a = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_display, win, &a, 1);


  if(argc > 1)
    cout << "TODO: Load " << argv[1] << endl;
  

  bool done = false;
  XEvent event;
  while(!done)
  {
    XNextEvent(g_display, &event);
    switch(event.type)
    {
      case Expose:
        if(event.xexpose.count > 1)
          break;
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        //glXSwapBuffers(g_display, glXWin);
        break;
      case ConfigureNotify: //size changed
        cout << "Resized to " << event.xconfigure.width << "x"
             << event.xconfigure.height << endl;
        break;
      case ClientMessage:
        done = true;
        break;
      case ButtonPress:
        cout << "ButtonPress: " << event.xbutton.button
             << " at " << event.xbutton.x << ", "
             << event.xbutton.y << endl;
        break;
      case ButtonRelease:
        cout << "ButtonRelease: " << event.xbutton.button
             << " at " << event.xbutton.x << ", "
             << event.xbutton.y << endl;
        break;
      case MotionNotify:
        cout << "MotionNotify at " << event.xmotion.x << ", "
             << event.xmotion.y << endl;
        break;
      case KeyPress:
      {
        //KeySym key = XKeycodeToKeysym(g_display, event.xkey.keycode, 0);
        KeySym key = XLookupKeysym(&event.xkey, 0);
        switch(key)
        {
          case XK_Escape:
            //quit program
            done = true;
            break;
          case XK_Next:
            break; 
          case XK_Right:
          case XK_Left:
            break;
          case XK_Up:
          case XK_Down:
            break;
          case XK_Delete:
            break;
          case XK_F1:
            break;
          case XK_F2:
            break;
          case XK_space:
          case XK_BackSpace:
            break;
          case XK_m:
            break;
          case XK_c:
            break;
          case XK_plus: //TODO: doesn't work with us keyboard layout
          case XK_KP_Add:
            break;
          case XK_minus:
          case XK_KP_Subtract:
            break;
          case XK_slash:
          case XK_KP_Divide:
            break;
          case XK_asterisk:
          case XK_KP_Multiply:
            break;
          case XK_a:
          case XK_b:
            break;
          case XK_f:
            break;
          case XK_r:
            break;
          case XK_q:
          case XK_w:
            break;
          case XK_t:
            break;
          case XK_s:
            break;
          case XK_d:
            break;
          case XK_h:
            break;
          //control-c, control-v missing
        }
        //*
        char c = key;
        if(key >= XK_A && key <= XK_Z)
          c = key - XK_A + 'A';
        if(key >= XK_a && key <= XK_z)
          c = key - XK_a + 'a';
        
        cout << "KeyPress: " << hex << key << " "
             << c << endl;
        //*/
      }break;
    }
  }

  glXMakeContextCurrent(g_display, None, None, NULL);
  glXDestroyContext(g_display, glXContext);
  glXDestroyWindow(g_display, glXWin);
  XFreeColormap(g_display, swa.colormap);
  XCloseDisplay(g_display);
}

