#ifndef DDSVIEW_IMAGE_H
#define DDSVIEW_IMAGE_H DDSVIEW_IMAGE_H

#include <string>
#include "IL/ilut.h"

#include "globalstuff.h"

//image state
extern ILuint g_currILImage;
extern std::string g_imageName, g_oldImageName, g_imageFileSize;
extern float g_bmLoadTime, g_hbmConvTime, g_alphaConvTime;

//il stuff
extern EXT* SUPPORTED_FORMATS;
extern int NUM_FORMATS;


//image display state
enum DisplayMode
{
  DM_2D, DM_2D_TILED,
  DM_CUBE_1,
  DM_STANDARD_2D = DM_2D
};

extern ILint g_bmDep, g_bmLayers, g_bmMipmaps;
extern int g_currLayer;
extern int g_currMipmapLevel; //stores current mipmap level for *all* layers
extern int g_currSlice;
extern bool g_displayAlpha;
extern bool g_displayBlended;
extern DisplayMode g_displayMode;
enum ROTATION { ROT_NONE, ROT_90, ROT_180, ROT_270 };
extern ROTATION g_rotation;
extern bool g_lockWindowSize;

void initIL();
bool isSupported(const std::string& file);

ILboolean hasImageAlpha();
ILboolean isImageCubemap();


//these don't really belong here...
int getClientWidth();
int getClientHeight();


#endif //DDSVIEW_IMAGE_H
