typedef struct Context
{
  HANDLE hFile;
  HANDLE hWrite;
  HANDLE hRead;

  void Set(HANDLE hRead,HANDLE hWrite,HANDLE hFile = NULL);

  //�N���e�B�J���Z�N�V����
  static CRITICAL_SECTION CriticalSection;
  static BOOL bThreadMustTerminate;
  static PROCESS_INFORMATION ProcessInformation;

} *PContext;

