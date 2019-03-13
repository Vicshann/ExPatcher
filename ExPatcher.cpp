
#include "ExPatcher.h"


volatile HINSTANCE hInstance; 

BYTE  InptFile[MAX_PATH];
BYTE  OutpFile[MAX_PATH];
BYTE  ScrtFile[MAX_PATH];
BYTE  ExePath[MAX_PATH];
BYTE  StartUpDir[MAX_PATH];
//====================================================================================
void _stdcall SysMain(DWORD UnkArg)
{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);	 // Crash silently an error happens
 hInstance = GetModuleHandleA(NULL);
 GetModuleFileNameA(hInstance,(LPSTR)&ExePath,sizeof(ExePath)); 
 lstrcpyA((LPSTR)&StartUpDir, (LPSTR)&ExePath);  
 TrimFilePath((LPSTR)&StartUpDir);     // This causes x64dbg to crash in 'dbghelp.dll!AddressMap::getSectionLength(unsigned long)' while loading pdb of this EXE
 LogMode = lmCons;

 int ForcePatch = false;
 LPSTR CmdLine = GetCommandLineA();
 DBGMSG("CmdLine: %s",CmdLine);
 CmdLine = GetCmdLineParam(CmdLine, LPSTR(0));   // Skip EXE file name and path
 int ParCnt = -1;
 BYTE Cmd[64];
 BYTE Arg[MAX_PATH];
 for(;*CmdLine;ParCnt++)
  {
   DBGMSG("Parsing Arg: %s",CmdLine);
   CmdLine = GetCmdLineParam(CmdLine, (LPSTR)&Cmd);
   if(NSTR::IsStrEqualIC("-F",(LPSTR)&Cmd))
    {
     ForcePatch = true;
     continue;
    }  
   if(NSTR::IsStrEqualIC("-S",(LPSTR)&Cmd))
    {
     CmdLine = GetCmdLineParam(CmdLine, (LPSTR)&Arg);
     if(!AssignFilePath((LPSTR)&ScrtFile, (LPSTR)&StartUpDir, (LPSTR)&Arg)){LOGMSG("No Path in '%s'", (LPSTR)&Cmd); break;}
     continue;
    }       
   if(NSTR::IsStrEqualIC("-I",(LPSTR)&Cmd))
    {
     CmdLine = GetCmdLineParam(CmdLine, (LPSTR)&Arg);
     if(!AssignFilePath((LPSTR)&InptFile, (LPSTR)&StartUpDir, (LPSTR)&Arg)){LOGMSG("No Path in '%s'", (LPSTR)&Cmd); break;}
     continue;
    }
   if(NSTR::IsStrEqualIC("-O",(LPSTR)&Cmd))
    {
     CmdLine = GetCmdLineParam(CmdLine, (LPSTR)&Arg);
     if(!AssignFilePath((LPSTR)&OutpFile, (LPSTR)&StartUpDir, (LPSTR)&Arg)){LOGMSG("No Path in '%s'", (LPSTR)&Cmd); break;}
     continue;
    }
  }
 if(ParCnt <= 0)
  {
   OUTMSG("Scriptable Patcher v0.4 by Vicshann [Vicshann@gmail.com]");
   OUTMSG("");
   OUTMSG("-F: Force patch even if not all signatures found");
   OUTMSG("-S: Script file path");
   OUTMSG("-I: Input file path");
   OUTMSG("-O: Output file path");
   OUTMSG("");
   OUTMSG("Example: -F -S \"PatchScript.txt\" -I \"InputFile.exe\" -O \"OutputFile.exe\"");
   OUTMSG("");
   ExitProcess(1);
  }
  else
   {
    OUTMSG("ScrtFile: %s", &ScrtFile);
    OUTMSG("InptFile: %s", &InptFile);
    OUTMSG("OutpFile: %s", &OutpFile);
   }
 CSigPatch patch;

 HANDLE hFile   = CreateFileX(&ScrtFile,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
 if(hFile == INVALID_HANDLE_VALUE){OUTMSG("Failed to open the script file!"); ExitProcess(-1);}
 DWORD Result   = 0;
 DWORD FileSize = GetFileSize(hFile,NULL);
 PBYTE DataBuf  = (PBYTE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileSize+64);   // No need to free it
 if(FileSize)ReadFile(hFile,DataBuf,FileSize,&Result,NULL);
 CloseHandle(hFile);

 if(patch.LoadPatchScript((LPSTR)DataBuf, FileSize) >= 0)
  {
   hFile    = CreateFileX(&InptFile,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
   if(hFile == INVALID_HANDLE_VALUE){OUTMSG("Failed to open the input file!"); ExitProcess(-2);}
   Result   = 0;
   FileSize = GetFileSize(hFile,NULL);
   DataBuf  = (PBYTE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileSize+64);   // No need to free it
   if(FileSize)ReadFile(hFile,DataBuf,FileSize,&Result,NULL);
   CloseHandle(hFile);
   if(patch.ApplyPatches(DataBuf,FileSize,1,CSigPatch::pfForce * ForcePatch) < 0){OUTMSG("Failed to patch the file!"); ExitProcess(0);}
   hFile = CreateFileX(&OutpFile,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
   if(hFile == INVALID_HANDLE_VALUE){OUTMSG("Failed to create the output file!"); ExitProcess(-3);}
   WriteFile(hFile,DataBuf,FileSize,&Result,NULL);
   CloseHandle(hFile);  
  }
   else {OUTMSG("Failed to parse the Patch Script!");}
 OUTMSG("Done!");
 ExitProcess(0);  
}
//---------------------------------------------------------------------------                    

