/*******************************************************************

 ■コマンドプロンプトのすべての操作をテキストファイルへ書き出します。

  ※ほとんどエラーチェック・処理をしていないテストコード
  ※書き出すファイルは、#define LOGFILE TEXT("～") で指定。

********************************************************************/

#include <windows.h>

//Cランタイム
#include <tchar.h>

//printf関数のため
#include <stdio.h>

//str*系APIの代替
#include <strsafe.h>

//パイプ、スレッドに渡すパラメータをラップした構造体を定義したヘッダー
#include "pipe.h"
#include "context.h"

#define CLOSEHANDLE(X) {if(X){CloseHandle(X);(X)=NULL;}}
#define BUFFER_SIZE 1024
#define SPIN_COUNT 4000

//書き出すファイル名の指定
#define LOGFILE TEXT("cmdlog.txt")

//CTRLハンドラ
BOOL WINAPI CtrlHandler(DWORD dwCtrlType)
{
  BOOL bRetVal = FALSE;

  if(dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT)
    bRetVal = GenerateConsoleCtrlEvent(dwCtrlType,Context::ProcessInformation.dwProcessId);
  
  return bRetVal;
}

//標準入出力・ファイルハンドルへの読取と書込スレッド
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

//ファイルを書込用にオープン(同期)
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

//環境変数COMSPECからコマンドプロンプトのパスを得て起動
// 起動するコマンドプロンプトのエコーをオフにするため、'/Q' スイッチを指定する。
BOOL CreateCommandPrompt(STARTUPINFO& si,PROCESS_INFORMATION& pi)
{
  BOOL bRetVal = FALSE;

  //CMD.EXEのパスを格納するバッファ
  TCHAR szCmdPath[MAX_PATH+1] = {0};

  //CMD.EXEのパスを環境(システム)変数から得る。
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

//スタートアップ
int _tmain(int argc,_TCHAR **argv)
{
  LPTSTR szLogFile = (argc > 1) ? argv[1] : const_cast<LPTSTR>(LOGFILE);
  
  /*------------------------------------------------------------
    スレッドハンドル、パイプ、スレッドに渡すコンテキスト
    配列はそれぞれ、0:標準入力用 1:標準出力用 2:標準エラー出力用
  --------------------------------------------------------------*/
  HANDLE hThreads[3] = {NULL};
  Pipe Std[3] = {PIPE_READ_INHERIT,PIPE_WRITE_INHERIT,PIPE_WRITE_INHERIT};
  Context ctx[3];

  //ログを書き込むファイル
  HANDLE hFile = NULL;

  //標準入力
  HANDLE hStdin = NULL;
  
  //CreateProcessに必要な構造体
  STARTUPINFO si = {sizeof(STARTUPINFO)};

  //標準入力ハンドルの複製
  HANDLE hProcess = GetCurrentProcess();
  if( FALSE == DuplicateHandle(hProcess,
                               GetStdHandle(STD_INPUT_HANDLE),
                               hProcess,
                               &hStdin,
                               0,
                               FALSE,
                               DUPLICATE_SAME_ACCESS))
    {
      _tprintf(TEXT("標準入力ハンドルの複製に失敗しました"));
      goto cleanup;
    }


  //クリティカルセクション初期化
  InitializeCriticalSectionAndSpinCount(&Context::CriticalSection,SPIN_COUNT);
  
  if((hFile = OpenLogFile(szLogFile)) == INVALID_HANDLE_VALUE)
    {
      _tprintf(TEXT("ログファイルのオープンが失敗しました"));
      goto cleanup;
    }

  //入出力ハンドルをリダイレクトしたコマンドプロンプトを生成
  si.dwFlags    = STARTF_USESTDHANDLES;
  si.hStdInput  = Std[0].hRead;   //パイプにリダイレクト
  si.hStdOutput = Std[1].hWrite; //パイプにリダイレクト
  si.hStdError  = Std[2].hWrite; //パイプにリダイレクト

  if(FALSE == CreateCommandPrompt(si,Context::ProcessInformation))
    {	
      _tprintf(TEXT("CreateProcess 失敗 : %d"),GetLastError());
      goto cleanup;
    }

  //コンテキスト構造体のメンバをセット
  ctx[0].Set(hStdin,Std[0].hWrite,hFile);
  ctx[1].Set(Std[1].hRead,GetStdHandle(STD_OUTPUT_HANDLE),hFile);
  ctx[2].Set(Std[2].hRead,GetStdHandle(STD_ERROR_HANDLE),hFile);

  //CTRLハンドラを挿入
  if(FALSE == SetConsoleCtrlHandler(CtrlHandler,TRUE))
    {
      _tprintf(TEXT("SetConsoleCtrlHandlerが失敗しました\n"));
      goto cleanup;
    }

  //スレッド作成
  for(int i=0;i<3;i++)
    hThreads[i] = CreateThread(NULL,0,ReadAndWriteProc,&ctx[i],0,NULL);
  
  //新しいコマンドプロンプトが終了するまで待機
  WaitForSingleObject(Context::ProcessInformation.hProcess,INFINITE);
  
  //スレッド終了通知をセット
  Context::bThreadMustTerminate = TRUE;

  //パイプハンドルのクローズ
  for(int i=0;i<3;i++)
    Std[i].Close();

  //標準入力ハンドルを閉じる。(スレッドの制御を戻すため)
  CloseHandle(hStdin);

  //起動したスレッドが終了するまで待機
  WaitForMultipleObjects(3,hThreads,TRUE,INFINITE);

  //クリティカルセクションを削除
  DeleteCriticalSection(&Context::CriticalSection);

cleanup:
  //ハンドルを閉じる
  CLOSEHANDLE(hFile);
  CLOSEHANDLE(Context::ProcessInformation.hThread);
  
  for(int i=0;i<3;i++)
    CLOSEHANDLE(hThreads[i]);
  
  CLOSEHANDLE(Context::ProcessInformation.hProcess);

  return 0;
}
