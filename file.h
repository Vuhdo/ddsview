#ifndef NICO_FILE_H
#define NICO_FILE_H NICO_FILE_H

#include <string>


std::string tolower(const std::string& str);
bool isDirectory(const std::string& fileName);







struct TraverseInfo
{
  std::string name;
  int index, count;
};

enum TRAVERSE_DIRECTION
{
  FORWARD, BACKWARD
};

bool traverse(TraverseInfo& out, const std::string& name1, const std::string& name2,
              TRAVERSE_DIRECTION dir,
              std::string baseDir = "");





#endif //NICO_FILE_H
