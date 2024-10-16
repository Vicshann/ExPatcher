
#include "ExPatcher.h"


volatile HINSTANCE hInstance; 

wchar_t  InptFile[MAX_PATH];   
wchar_t  OutpFile[MAX_PATH];
wchar_t  ScrtFile[MAX_PATH];
wchar_t  ExePath[MAX_PATH];
wchar_t  StartUpDir[MAX_PATH];
//====================================================================================
void _stdcall SysMain(DWORD UnkArg)
{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);	 // Crash silently an error happens
 hInstance = GetModuleHandleW(NULL);
 GetModuleFileNameW(hInstance, ExePath, countof(ExePath)); 
 lstrcpyW(StartUpDir, ExePath);  
 TrimFilePath(StartUpDir);     // This causes x64dbg to crash in 'dbghelp.dll!AddressMap::getSectionLength(unsigned long)' while loading pdb of this EXE
 LogMode = lmCons;
#ifdef _DEBUG
 LogMode |= lmFile;
 wsprintfW(LogFilePath, L"%s.%08X.log",&ExePath,GetTickCount());
#endif

 int ForcePatch = false;
 PWSTR CmdLine = GetCommandLineW();
 DBGMSG("CmdLine: %ls", CmdLine);
 CmdLine = GetCmdLineParam(CmdLine, PWSTR(0));   // Skip EXE file name and path
 int ParCnt = -1;
 wchar_t Cmd[64];
 wchar_t Arg[MAX_PATH];
 for(;*CmdLine;ParCnt++)
  {
   DBGMSG("Parsing Arg: %ls", CmdLine);
   CmdLine = GetCmdLineParam(CmdLine, Cmd);
   if(NSTR::IsStrEqualIC("-F", Cmd))
    {
     ForcePatch = true;
     continue;
    }  
   if(NSTR::IsStrEqualIC("-S", Cmd))
    {
     CmdLine = GetCmdLineParam(CmdLine, Arg);
     if(!AssignFilePath(ScrtFile, StartUpDir, Arg)){LOGMSG("No Path in '%ls'", &Cmd); break;}
     continue;
    }       
   if(NSTR::IsStrEqualIC("-I", Cmd))
    {
     CmdLine = GetCmdLineParam(CmdLine, Arg);
     if(!AssignFilePath(InptFile, StartUpDir, Arg)){LOGMSG("No Path in '%ls'", &Cmd); break;}
     AssignFilePath(OutpFile, StartUpDir, Arg);   // Same as input(overwrite)
     continue;
    }
   if(NSTR::IsStrEqualIC("-O", Cmd))
    {
     CmdLine = GetCmdLineParam(CmdLine, Arg);
     if(!AssignFilePath(OutpFile, StartUpDir, Arg)){LOGMSG("No Path in '%ls'", &Cmd); break;}     
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
   OUTMSG("-O: Output file path(optional)");
   OUTMSG("");
   OUTMSG("Example: -F -S \"PatchScript.txt\" -I \"InputFile.exe\" -O \"OutputFile.exe\"");
   OUTMSG("");  
   ExitProcess(1);
  }
  else
   {
    OUTMSG("ScrtFile: %ls", &ScrtFile);
    OUTMSG("InptFile: %ls", &InptFile);
    OUTMSG("OutpFile: %ls", &OutpFile);
   }
 NSIGP::CSigPatch patch;
 bool  HaveOutFile = *OutpFile;

 HANDLE hFile   = CreateFileX(&ScrtFile,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
 if(hFile == INVALID_HANDLE_VALUE){OUTMSG("Failed to open the script file!"); ExitProcess(-1);}
 DWORD Result   = 0;
 DWORD FileSize = GetFileSize(hFile,NULL);
 PBYTE DataBuf  = (PBYTE)VirtualAlloc(NULL,FileSize+64,MEM_COMMIT,PAGE_READWRITE);  // HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileSize+64);   // No need to free it
 if(FileSize)ReadFile(hFile,DataBuf,FileSize,&Result,NULL);
 CloseHandle(hFile);

 if(patch.LoadPatchScript((LPSTR)DataBuf, FileSize) >= 0)
  {
   hFile    = CreateFileX(&InptFile,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);  // Cannot work with > 4G files on x32!
   if(hFile == INVALID_HANDLE_VALUE){OUTMSG("Failed to open the input file!"); ExitProcess(-2);}
   LARGE_INTEGER FileSize = {0};
   GetFileSizeEx(hFile, &FileSize);  //GetFileSize(hFile,NULL);
   OUTMSG("File size: %08X%08X",FileSize.HighPart,FileSize.LowPart);
   HANDLE hMapped = CreateFileMapping(hFile,NULL,HaveOutFile?PAGE_WRITECOPY:PAGE_READWRITE,0,0,NULL);            // NoOutput: PAGE_WRITECOPY
   if(!hMapped){OUTMSG("Failed to create the file mapping!"); ExitProcess(0);}
   PVOID MappedSection = MapViewOfFile(hMapped,(HaveOutFile?FILE_MAP_COPY:FILE_MAP_READ|FILE_MAP_WRITE),0,0,FileSize.QuadPart);   // NoOutput: FILE_MAP_COPY  // NOTE: Won`t map more than 2GB on x32!
   if(!MappedSection){OUTMSG("Failed to map the file!"); ExitProcess(0);}
   OUTMSG("");
   OUTMSG("Patching...");
   if(patch.ApplyPatches(MappedSection,FileSize.QuadPart,1,NSIGP::pfForce * ForcePatch) < 0){OUTMSG("Failed to patch the file!"); ExitProcess(0);}
   if(HaveOutFile)
    {
     HANDLE hOFile = CreateFileX(&OutpFile,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
     if(hOFile == INVALID_HANDLE_VALUE){OUTMSG("Failed to create the output file!"); ExitProcess(-3);}
     int res = WriteFile(hOFile,MappedSection,FileSize.QuadPart,&Result,NULL);         // NOTE: the size is DWORD even on x64
     CloseHandle(hOFile);  
     OUTMSG("Saved size: %08X of %08X%08X",Result,FileSize.HighPart,FileSize.LowPart);
    }
     else FlushViewOfFile(MappedSection, 0);     // NoOutput: Disable
   UnmapViewOfFile(MappedSection);
   CloseHandle(hMapped);
   CloseHandle(hFile);
   OUTMSG("Saved: %ls", HaveOutFile?&OutpFile:&InptFile);   // NoOutput: Pass input
  }
   else {OUTMSG("Failed to parse the Patch Script!");}
 // VirtualFree DataBuf
 OUTMSG("Done!");
 ExitProcess(0);  
}
//---------------------------------------------------------------------------                    
