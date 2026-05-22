#include "CrashHandler.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>

void LogStackTrace(PCONTEXT context) {
    STACKFRAME64 stackFrame = {};
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

#if defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrStack.Offset = context->Esp;
#elif defined(_M_X64)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrStack.Offset = context->Rsp;
#endif

    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    logger->critical("=== Exception Call Stack ===");

    for (int i = 0; i < 100; ++i) {
        if (!StackWalk64(machineType,
                         GetCurrentProcess(),
                         GetCurrentThread(),
                         &stackFrame,
                         context,
                         nullptr,
                         SymFunctionTableAccess64,
                         SymGetModuleBase64,
                         nullptr))
            break;

        if (stackFrame.AddrPC.Offset == 0)
            break;

        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)] = {};
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        DWORD64 address = stackFrame.AddrPC.Offset;
        BOOL ret = SymFromAddr(GetCurrentProcess(), address, &displacement, symbol);

        if (ret)
            logger->critical("{:>3} 0x{:016X} {}", i, address, symbol->Name);
        else
            logger->critical("{:>3} 0x{:016X}", i, address);
    }
}

void LogModuleInfo(PVOID address) {
    IMAGEHLP_MODULE64 moduleInfo = {};
    moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (SymGetModuleInfo64(GetCurrentProcess(), (DWORD64)address, &moduleInfo)) {
        logger->critical("Module: {} (Base: 0x{:016X}, Size: {})",
                         moduleInfo.ModuleName, moduleInfo.BaseOfImage, moduleInfo.ImageSize);
    } else {
        DWORD errorCode = GetLastError();
        logger->critical("SymGetModuleInfo64 failed: [{}]", errorCode);
    }
}

void LogRegisterState(PCONTEXT pContext) {
    std::stringstream ss;
    ss << "=== Register State ===";

#if defined(_M_X64)
    ss << "\nRAX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rax
       << "  RBX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rbx
       << "  RCX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rcx
       << "  RDX: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rdx
       << "\nRSI: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rsi
       << "  RDI: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rdi
       << "  RBP: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rbp
       << "  RSP: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rsp
       << "\nR8:  0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R8
       << "  R9:  0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R9
       << "  R10: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R10
       << "  R11: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R11
       << "\nR12: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R12
       << "  R13: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R13
       << "  R14: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R14
       << "  R15: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->R15
       << "\nRIP: 0x" << std::hex << std::setw(16) << std::setfill('0') << pContext->Rip
       << "  EFLAGS: 0x" << std::hex << pContext->EFlags;
#elif defined(_M_IX86)
    ss << "\nEAX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Eax
       << "  EBX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Ebx
       << "  ECX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Ecx
       << "  EDX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Edx
       << "\nESI: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Esi
       << "  EDI: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Edi
       << "  EBP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Ebp
       << "  ESP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Esp
       << "\nEIP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pContext->Eip
       << "  EFLAGS: 0x" << std::hex << pContext->EFlags;
#endif

    ss << "\nCS: 0x" << std::hex << pContext->SegCs
       << "  DS: 0x" << pContext->SegDs
       << "  ES: 0x" << pContext->SegEs
       << "  FS: 0x" << pContext->SegFs
       << "  GS: 0x" << pContext->SegGs
       << "  SS: 0x" << pContext->SegSs;

    logger->critical(ss.str());
}

LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    LogRegisterState(pExceptionInfo->ContextRecord);
    LogModuleInfo(pExceptionInfo->ExceptionRecord->ExceptionAddress);
    LogStackTrace(pExceptionInfo->ContextRecord);
    return EXCEPTION_CONTINUE_SEARCH;
}

void InitCrashHandler() {
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    SetUnhandledExceptionFilter(ExceptionHandler);
}
