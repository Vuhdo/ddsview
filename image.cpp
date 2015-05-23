#include "Precompiled.h"
#include "Image.h"
using namespace std;

//image state
ILuint g_currILImage = 0;
string g_imageName, g_oldImageName, g_imageFileSize;
float g_bmLoadTime = 0.f, g_hbmConvTime = 0.f, g_alphaConvTime = 0.f;

//il stuff
EXT* SUPPORTED_FORMATS = NULL;
int NUM_FORMATS = 0;

//image display state
ILint g_bmDep = 0, g_bmLayers = 0, g_bmMipmaps = 0;
int g_currLayer = 0;
int g_currMipmapLevel = 0; //stores current mipmap level for *all* layers
int g_currSlice = 0;
bool g_displayAlpha = false;
bool g_displayBlended = false;
DisplayMode g_displayMode = DM_2D;
ROTATION g_rotation = ROT_NONE;
bool g_lockWindowSize = false;



void initIL()
{
  ilInit();
  iluInit();
  ilutInit();
  ilutRenderer(ILUT_WIN32);

  //these are no longer neccessary because of improvements
  //in the IL library
  //ilEnable(IL_CONV_PAL); //conv palettized image to rgba on load time
  //ilEnable(IL_ORIGIN_SET); //conv images to have origin in
  //ilOriginFunc(IL_ORIGIN_UPPER_LEFT); //the upper left corner

  ilEnable(IL_FILE_OVERWRITE);

  //TODO: add this as it improves dds writing (no need to reencode,
  //can keep the original data) - but it increases memory usage...)
  //ilEnable(IL_KEEP_DXTC_DATA);

  //for il debugging:
  //ilHint(IL_MEM_SPEED_HINT, IL_LESS_MEM);


  //get extensions supported by devil
  const char* exts = ilGetString(IL_LOAD_EXT);
  int extlen = strlen(exts);
  NUM_FORMATS = 0;
  int i = 0;

  //count extensions (strings separated by spaces)
  while(exts[i] != '\0')
  {
    while(exts[i] == ' ') ++i;
    if(exts[i] != '\0')
    {
      ++NUM_FORMATS;
      while(exts[i] != ' ' && exts[i] != '\0') ++i;
    }
  }

  //extract extensions
  SUPPORTED_FORMATS = new EXT[NUM_FORMATS];
  int currI = 0, currJ = 0;
  for(i = 0; i < extlen; ++i)
  {
    if(exts[i] == ' ')
    {
      SUPPORTED_FORMATS[currI++][currJ++] = '\0';
      currJ = 0;
    }
    else
      SUPPORTED_FORMATS[currI][currJ++] = exts[i];
  }
}


bool isSupported(const string& file)
{
  string::size_type s = file.rfind('.');
  if(s == string::npos) //no extension
    return false;
  string ext = file.substr(s + 1);
  for(int i = 0; i < NUM_FORMATS; ++i)
    if(ext == SUPPORTED_FORMATS[i])
      return true;

  return false;
}
