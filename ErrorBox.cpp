#include "Precompiled.h"
#include "ErrorBox.h"
#include <windows.h>
#include <stdio.h> //sprintf

void ErrorBox(const char* msg)
{
  DWORD error = GetLastError();
  LPVOID errorText = NULL;

  if(FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL, error, 0, (LPTSTR)&errorText, 0, NULL) == 0)
  {
    MessageBox(NULL, msg,
               "ERROR:", MB_OK | MB_ICONERROR);
  }
  else
  {
    const int BUFFER_SIZE = 1024;
    const char* MESSAGE = "%s:\n\nErrorCode %i - %s";
    char buffer[BUFFER_SIZE];

    int msgLen = strlen(msg);
    int errLen = strlen((char*)errorText);
    int requiredLen = msgLen + errLen + 26;

    //try to create string on stack
    if(requiredLen <= BUFFER_SIZE)
    {
      sprintf(buffer, MESSAGE, msg, error, errorText);
      MessageBox(NULL, buffer, "ERROR:", MB_OK | MB_ICONERROR);
    }
    else
    {
      //use heap, if string is too large for the stack
      char* buffer2 = (char*)LocalAlloc(LMEM_FIXED, requiredLen);
      sprintf(buffer2, MESSAGE, msg, error, errorText);
      MessageBox(NULL, buffer2, "ERROR:", MB_OK | MB_ICONERROR);
      LocalFree(buffer2);
    }

    LocalFree(errorText);
  }
}
