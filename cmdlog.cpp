/*******************************************************************

 ���R�}���h�v�����v�g�̂��ׂĂ̑�����e�L�X�g�t�@�C���֏����o���܂��B

  ���قƂ�ǃG���[�`�F�b�N�E���������Ă��Ȃ��e�X�g�R�[�h
  �������o���t�@�C���́A#define LOGFILE TEXT("�`") �Ŏw��B

********************************************************************/

#include <windows.h>

//C�����^�C��
#include <tchar.h>

//printf�֐��̂���
#include <stdio.h>

//str*�nAPI�̑��
#include <strsafe.h>

//�p�C�v�A�X���b�h�ɓn���p�����[�^�����b�v�����\���̂��`�����w�b�_�[
#include "pipe.h"
#include "context.h"

#define CLOSEHANDLE(X) {if(X){CloseHandle(X);(X)=NULL;}}
#define BUFFER_SIZE 1024
#define SPIN_COUNT 4000

//�����o���t�@�C�����̎w��
#define LOGFILE TEXT("cmdlog.txt")

//CTRL�n���h��
BOOL WINAPI CtrlHandler(DWORD dwCtrlType)
{
  BOOL bRetVal = FALSE;

  if(dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT)
    bRetVal = GenerateConsoleCtrlEvent(dwCtrlType,Context::ProcessInformation.dwProcessId);
  
  return bRetVal;
}

//�W�����o�́E�t�@�C���n���h���ւ̓ǎ�Ə����X���b�h
DWORD WINAPI ReadAndWriteProc(LPVOID lpParameter)
{
  Context *context = (Context*)lpParameter;

  TCHAR pBuffer[BUFFER_SIZE] = {0};
  DWORD dwRead = 0,dwWrite = 0;
  BOOL bRet = FALSE;
  while(!(context->bThreadMustTerminate))
    {
      do
        {
          bRet = ReadFile(context->hRead,(PVOID)pBuffer,BUFFER_SIZE,&dwRead,NULL);
          if(dwRead > 0)
            {
              EnterCriticalSection(&(context->CriticalSection));
              if(context->hFile)
                WriteFile(context->hFile,(PVOID)pBuffer,dwRead,&dwWrite,NULL);
              
              if(context->hWrite)
                WriteFile(context->hWrite,(PVOID)pBuffer,dwRead,&dwWrite,NULL);
              LeaveCriticalSection(&(context->CriticalSection));
            }
        }
      while(bRet == TRUE && dwRead > 0);
    }

  return 0;
}

//�t�@�C���������p�ɃI�[�v��(����)
HANDLE OpenLogFile(PCTSTR szLogName)
{
  return CreateFile(szLogName,
                    GENERIC_WRITE,
                    0,
                    NULL,
                    CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
}

//���ϐ�COMSPEC����R�}���h�v�����v�g�̃p�X�𓾂ċN��
// �N������R�}���h�v�����v�g�̃G�R�[���I�t�ɂ��邽�߁A'/Q' �X�C�b�`���w�肷��B
BOOL CreateCommandPrompt(STARTUPINFO& si,PROCESS_INFORMATION& pi)
{
  BOOL bRetVal = FALSE;

  //CMD.EXE�̃p�X���i�[����o�b�t�@
  TCHAR szCmdPath[MAX_PATH+1] = {0};

  //CMD.EXE�̃p�X����(�V�X�e��)�ϐ����瓾��B
  if(0 == GetEnvironmentVariable(TEXT("COMSPEC"),szCmdPath,MAX_PATH+1))
    goto cleanup;

  if(FAILED(StringCchCat(szCmdPath,MAX_PATH+1,TEXT(" /Q"))))
    goto cleanup;
    
  bRetVal = CreateProcess(NULL,
                          szCmdPath,
                          NULL,
                          NULL,
                          TRUE,
                          NORMAL_PRIORITY_CLASS | CREATE_NEW_PROCESS_GROUP,
                          NULL,
                          NULL,
                          &si,
                          &pi);

cleanup:
  return bRetVal;
}

//�X�^�[�g�A�b�v
int _tmain(int argc,_TCHAR **argv)
{
  LPTSTR szLogFile = (argc > 1) ? argv[1] : const_cast<LPTSTR>(LOGFILE);
  
  /*------------------------------------------------------------
    �X���b�h�n���h���A�p�C�v�A�X���b�h�ɓn���R���e�L�X�g
    �z��͂��ꂼ��A0:�W�����͗p 1:�W���o�͗p 2:�W���G���[�o�͗p
  --------------------------------------------------------------*/
  HANDLE hThreads[3] = {NULL};
  Pipe Std[3] = {PIPE_READ_INHERIT,PIPE_WRITE_INHERIT,PIPE_WRITE_INHERIT};
  Context ctx[3];

  //���O���������ރt�@�C��
  HANDLE hFile = NULL;

  //�W������
  HANDLE hStdin = NULL;
  
  //CreateProcess�ɕK�v�ȍ\����
  STARTUPINFO si = {sizeof(STARTUPINFO)};

  //�W�����̓n���h���̕���
  HANDLE hProcess = GetCurrentProcess();
  if( FALSE == DuplicateHandle(hProcess,
                               GetStdHandle(STD_INPUT_HANDLE),
                               hProcess,
                               &hStdin,
                               0,
                               FALSE,
                               DUPLICATE_SAME_ACCESS))
    {
      _tprintf(TEXT("�W�����̓n���h���̕����Ɏ��s���܂���"));
      goto cleanup;
    }


  //�N���e�B�J���Z�N�V����������
  InitializeCriticalSectionAndSpinCount(&Context::CriticalSection,SPIN_COUNT);
  
  if((hFile = OpenLogFile(szLogFile)) == INVALID_HANDLE_VALUE)
    {
      _tprintf(TEXT("���O�t�@�C���̃I�[�v�������s���܂���"));
      goto cleanup;
    }

  //���o�̓n���h�������_�C���N�g�����R�}���h�v�����v�g�𐶐�
  si.dwFlags    = STARTF_USESTDHANDLES;
  si.hStdInput  = Std[0].hRead;   //�p�C�v�Ƀ��_�C���N�g
  si.hStdOutput = Std[1].hWrite; //�p�C�v�Ƀ��_�C���N�g
  si.hStdError  = Std[2].hWrite; //�p�C�v�Ƀ��_�C���N�g

  if(FALSE == CreateCommandPrompt(si,Context::ProcessInformation))
    {	
      _tprintf(TEXT("CreateProcess ���s : %d"),GetLastError());
      goto cleanup;
    }

  //�R���e�L�X�g�\���̂̃����o���Z�b�g
  ctx[0].Set(hStdin,Std[0].hWrite,hFile);
  ctx[1].Set(Std[1].hRead,GetStdHandle(STD_OUTPUT_HANDLE),hFile);
  ctx[2].Set(Std[2].hRead,GetStdHandle(STD_ERROR_HANDLE),hFile);

  //CTRL�n���h����}��
  if(FALSE == SetConsoleCtrlHandler(CtrlHandler,TRUE))
    {
      _tprintf(TEXT("SetConsoleCtrlHandler�����s���܂���\n"));
      goto cleanup;
    }

  //�X���b�h�쐬
  for(int i=0;i<3;i++)
    hThreads[i] = CreateThread(NULL,0,ReadAndWriteProc,&ctx[i],0,NULL);
  
  //�V�����R�}���h�v�����v�g���I������܂őҋ@
  WaitForSingleObject(Context::ProcessInformation.hProcess,INFINITE);
  
  //�X���b�h�I���ʒm���Z�b�g
  Context::bThreadMustTerminate = TRUE;

  //�p�C�v�n���h���̃N���[�Y
  for(int i=0;i<3;i++)
    Std[i].Close();

  //�W�����̓n���h�������B(�X���b�h�̐����߂�����)
  CloseHandle(hStdin);

  //�N�������X���b�h���I������܂őҋ@
  WaitForMultipleObjects(3,hThreads,TRUE,INFINITE);

  //�N���e�B�J���Z�N�V�������폜
  DeleteCriticalSection(&Context::CriticalSection);

cleanup:
  //�n���h�������
  CLOSEHANDLE(hFile);
  CLOSEHANDLE(Context::ProcessInformation.hThread);
  
  for(int i=0;i<3;i++)
    CLOSEHANDLE(hThreads[i]);
  
  CLOSEHANDLE(Context::ProcessInformation.hProcess);

  return 0;
}
