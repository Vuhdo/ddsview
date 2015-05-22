#include "file.h"

#include "image.h" //isSupported()

#include <windows.h>
#include <vector>
#include <algorithm> //sort()
#include <cassert>

using namespace std;

string tolower(const string& str)
{
  string ret = str;
  int l = ret.length();
  for(int i = 0; i < l; ++i)
    ret[i] = tolower(str[i]);
  return ret;
}

bool isDirectory(const string& fileName)
{
  DWORD atts = GetFileAttributes(fileName.c_str());
  if(atts == 0xFFFFFFFF)
    return false;

  return (atts & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

string getPath(const string& fullName)
{
  string::size_type pos = fullName.rfind('\\');
  if(pos != string::npos)
    return fullName.substr(0, pos + 1);
  else
    return "";
}

string chopLastDir(const string& fullDir)
{
  if(fullDir == "")
    return "";
  string::size_type p = fullDir.length() - 1;
  if(fullDir[p] == '\\')
    --p;
  p = fullDir.rfind('\\', p);
  if(p != string::npos)
    return fullDir.substr(0, p + 1);
  else
    return fullDir;
}

bool compare(const std::string& a, const std::string& b)
{
  int len1 = a.length(), len2 = b.length();
  int i1 = 0, i2 = 0;

  while(i1 < len1 && i2 < len2)
  {
    if(isdigit(a[i1]) && isdigit(b[i2]))
    {
      int num1 = 0, num2 = 0;

      while(i1 < len1 && isdigit(a[i1]))
      {
        num1 = 10*num1 + (a[i1] - '0');
        ++i1;
      }

      while(i2 < len2 && isdigit(b[i2]))
      {
        num2 = 10*num2 + (b[i2] - '0');
        ++i2;
      }

      if(num1 < num2)
        return true;
      else if(num1 > num2)
        return false;
      //else continue
    }
    else if(a[i1] == b[i2])
    {
      ++i1; ++i2;
      continue;
    }
    else
    {
      if((unsigned char)a[i1] < (unsigned char)b[i2]) //for umlauts etc.
        return true;
      else
        return false;
    }
  }

  return len1 < len2;
}

int getIndex(TRAVERSE_DIRECTION dir, const vector<string>& fileNames,
             const string& name1, const string& name2 = "")
{
  //find current image. if it's not found,
  //take the one that's alphabetically next
  int s = fileNames.size(), i = 0;
  //while(i < s && fileNames[i] < g_imageName) ++i;
  while(i < s && compare(fileNames[i], name1)) ++i;

  if(dir == FORWARD)
  {
    if(i < s && fileNames[i] == name1)
      ++i;
    //if image was renamed and the next image after
    //the old name is the new name, go one image further
    //so the current image is not loaded again
    if(i < s && fileNames[i] == name2)
      ++i;
  }
  else
  {
    --i;
    //see above
    if(i >= 0 && fileNames[i] == name2)
      --i;
  }
  return i;
}

bool getSubDirs(const string& dir, vector<string>& subDirs)
{
  WIN32_FIND_DATA findData;
  HANDLE findHandle = FindFirstFile((dir + "*.*").c_str(), &findData);
  if(findHandle == INVALID_HANDLE_VALUE)
    return false;

  do
  {
    string completeName = tolower(dir + findData.cFileName);
    if(isDirectory(completeName)
      && strcmp(findData.cFileName, ".") != 0
      && strcmp(findData.cFileName, "..") != 0)
      subDirs.push_back(completeName);
  }while(FindNextFile(findHandle, &findData));

  FindClose(findHandle);
  sort(subDirs.begin(), subDirs.end(), compare);
  return true;
}

bool getFiles(const string& dir, vector<string>& files)
{
  WIN32_FIND_DATA findData;
  HANDLE findHandle = FindFirstFile((dir + "*.*").c_str(), &findData);
  if(findHandle == INVALID_HANDLE_VALUE)
    return false;

  do
  {
    string completeName = tolower(dir + findData.cFileName);

    //"images.bmp" is a valid name for a directory...so only
    //return supported file names that are *no* directories
    if(isSupported(completeName) && !isDirectory(completeName))
      files.push_back(completeName);
  }while(FindNextFile(findHandle, &findData));

  FindClose(findHandle);
  sort(files.begin(), files.end(), compare);
  return true;
}

bool traverse(TraverseInfo& info, const string& name1, const string& name2,
                     TRAVERSE_DIRECTION dir, string baseDir)
{
  if(baseDir != "" && baseDir[baseDir.size() - 1] != '\\')
    baseDir += "\\";

  string path = getPath(name1);
  if(path == "")
    path = baseDir;

  int s = 0, i = 0;
  std::vector<string> fileNames;

  if(!isDirectory(name1))
  {
    if(!getFiles(path, fileNames))
    {
      if(baseDir == "")
        return false;

      //try to find parent dir (needed if while deepbrowsing subdirs,
      //the currently visited subdirs are deleted)

      //TODO: doesn't work for BACKWARD search
      string parentPath = chopLastDir(path);
      if(parentPath.length() >= baseDir.length())
        return traverse(info, parentPath, "", dir, baseDir);

      return false;
    }

    s = fileNames.size();
    i = getIndex(dir, fileNames, name1, name2);
  }

  if(baseDir == "")
  {
    //non-recursive browse mode - simply wrap around
    if(i >= s) i = 0;
    if(i < 0) i = s - 1;
  }
  else
  {
    //recursive browsing

    if(s == 0)
    {
      //needed for deepbrowse
      if(dir == FORWARD)
        i = 1;
      else //dir == BACKWARD
        i = -1;
    }

    if(i >= s) //dir has to be FORWARD
    {
      assert(dir == FORWARD);

      vector<string> subDirs;
      if(!getSubDirs(path, subDirs))
        return false;

      //search subdirs
      for(int j = 0; j < subDirs.size(); ++j)
      {
        if(!traverse(info, "", "", dir, subDirs[j]))
          return false;

        if(info.index < info.count)
          return true;

        //no files were found in the first subDir...
        //go on searching
      }

      //if we come here, all subdirs were searched and we
      //have to go at least one directory up
      if(path == baseDir)
      {
        //return Search Finished, no files found
        info.count = s;
        info.index = i;
        return true;
      }

      int dirIndex;
      do
      {
        //go one directory up
        string newPath = chopLastDir(path);
        string currSubDir = path.substr(0, path.length() - 1); //chop '\\'

        subDirs.clear();
        if(!getSubDirs(newPath, subDirs))
          return false;

        dirIndex = getIndex(dir, subDirs, currSubDir);

        for(int j = dirIndex; j < subDirs.size(); ++j)
        {
          if(!traverse(info, "", "", dir, subDirs[j]))
            return false;

          if(info.index < info.count)
            return true;

          //no files were found in the that subDir...
          //go on searching
        }

        path = newPath;
      }while(path != baseDir);

      //return Search Finished, no files found
      info.count = subDirs.size();
      info.index = info.count + 1;
      return true;

    }
    else if(i < 0) //dir has to be BACKWARD
    {
      assert(dir == BACKWARD);

      if(path == baseDir && name1 != "")
      {
        //return Search Finished
        info.count = s;
        info.index = i;
        return true;
      }

      if(name1 == "")
      {
        //in RecBrowse mode, if directory is entered backward,
        //the last file should be displayed

        vector<string> subDirs;
        if(!getSubDirs(path, subDirs))
          return false;

        for(int j = subDirs.size() - 1; j >= 0; --j)
        {
          if(!traverse(info, "", "", dir, subDirs[j]))
            return false;

          if(info.index >= 0)
            return true;

          //no files found in the current subdir...
          //go on searching
        }

        //no subdirs contained any files...return last image
        //in this directory
        vector<string> files;
        if(!getFiles(path, files))
          return false;

        info.count = files.size();
        if(files.size() > 0)
        {
          info.index = files.size() - 1;
          info.name = files[info.index];
        }
        else
          info.index = -1;
        return true;
      }

      vector<string> subDirs;
      do
      {
        string newPath = chopLastDir(path);
        string currSubDir = path.substr(0, path.length() - 1); //chop '\\'

        subDirs.clear();
        if(!getSubDirs(newPath, subDirs))
          return false;

        int dirIndex = getIndex(dir, subDirs, currSubDir);

        //traverse subdirs
        for(int j = dirIndex; j >= 0; --j)
        {
          if(!traverse(info, "", "", dir, subDirs[j]))
            return false;

          if(info.index >= 0)
            return true;

          //no files found in the current subdir...
          //go on searching
        }        

        //found nothing in the subdirs, now check file in current
        //dir

        //return last file in this directory
        vector<string> files;
        if(!getFiles(newPath, files))
          return false;

        if(files.size() > 0)
        {
          info.count = files.size();
          info.index = files.size() - 1;
          info.name = files[info.index];
          return true;
        }

        path = newPath;
      }while(path != baseDir);

      //return Search Finished
      info.count = subDirs.size();
      info.index = -1;
      return true;
    }
  }

  info.index = i;
  info.count = s;
  if(s > 0)
    info.name = fileNames[i];
  return true;
}
