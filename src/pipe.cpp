#include <windows.h>
#include "..\pipe.h"

#define CLOSEHANDLE(X) {if(X)CloseHandle(X);}

Pipe::Pipe(HANDLE r,HANDLE w) : hWrite(r),hRead(w)
{
}

Pipe::Pipe(DWORD dwFlagsInherit = PIPE_READWRITE_INHERIT)
{
  Create(hRead,hWrite);

  if((dwFlagsInherit & PIPE_READ_INHERIT) != PIPE_READ_INHERIT)
    SwapHandle(hRead);
  
  if((dwFlagsInherit & PIPE_WRITE_INHERIT) != PIPE_WRITE_INHERIT)
    SwapHandle(hWrite);
}

Pipe::~Pipe()
{
  Close();
}

Pipe::Pipe(const Pipe& src)
{
  hWrite = DuplicateHandle(src.hWrite);
  hRead  = DuplicateHandle(src.hRead);
}

Pipe& Pipe::operator=(const Pipe& rhs)
{
  return *(new Pipe(rhs));
}

BOOL Pipe::Create(HANDLE& r,HANDLE& w)
{
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES),NULL,TRUE};
  return ::CreatePipe(&r,&w,&sa,0);
}
  
void Pipe::Close()
{
  CLOSEHANDLE(hWrite);
  CLOSEHANDLE(hRead);
}

HANDLE Pipe::DuplicateHandle(HANDLE h)
{
  HANDLE hProcess = GetCurrentProcess();
  HANDLE hDup = NULL;
  DWORD dwFlags = 0;
  BOOL bInherit = FALSE;
 
  if(TRUE == GetHandleInformation(h,&dwFlags))
    {
      if((dwFlags & HANDLE_FLAG_INHERIT) == HANDLE_FLAG_INHERIT)
        bInherit = TRUE;

      ::DuplicateHandle(hProcess,h,hProcess,&hDup,0,bInherit,DUPLICATE_SAME_ACCESS);
    }
  return hDup;
}


BOOL Pipe::SwapHandle(HANDLE& hSrc)
{
  BOOL bRet = FALSE;
  HANDLE hTemp = NULL;
  HANDLE hProcess = ::GetCurrentProcess();

  if(::DuplicateHandle(hProcess,hSrc,hProcess,&hTemp,0,FALSE,DUPLICATE_SAME_ACCESS))
    {
      if(::CloseHandle(hSrc))
        {
          hSrc = hTemp;
          bRet = TRUE;
        }
    }

  return bRet;
}
