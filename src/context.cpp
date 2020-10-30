#include <windows.h>
#include "..\context.h"

//‰Šú‰»
CRITICAL_SECTION Context::CriticalSection;
BOOL Context::bThreadMustTerminate = FALSE;
PROCESS_INFORMATION Context::ProcessInformation = {NULL};

void Context::Set(HANDLE hRead,HANDLE hWrite,HANDLE hFile)
{
  this->hRead = hRead;
  this->hWrite = hWrite;
  this->hFile = hFile;
}
