#include <windows.h>

#define PIPE_READ_INHERIT 1
#define PIPE_WRITE_INHERIT 2
#define PIPE_READWRITE_INHERIT 3

typedef struct Pipe
{
  HANDLE hRead;
  HANDLE hWrite;

  Pipe(HANDLE r,HANDLE w);
  Pipe(DWORD dwFlagsInherit);
  ~Pipe();
  Pipe(const Pipe& src);
  Pipe& operator=(const Pipe& rhs);

  void Close();
  
private:
  BOOL Create(HANDLE& r,HANDLE& w);

  // STATIC îÒåˆäJÉÅÉìÉoä÷êî
  static HANDLE DuplicateHandle(HANDLE h);
  static BOOL SwapHandle(HANDLE& hSrc);

} *PPIPE;

