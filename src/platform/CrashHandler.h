#ifndef PLATFORM_CRASHHANDLER_H
#define PLATFORM_CRASHHANDLER_H

#include <windows.h>
#include <DbgHelp.h>

void InitCrashHandler();
LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);
void LogStackTrace(PCONTEXT context);
void LogRegisterState(PCONTEXT pContext);
void LogModuleInfo(PVOID address);

#endif
