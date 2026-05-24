#include "CrashHandler.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cstdio>
#include <psapi.h>

void LogStackTrace(PCONTEXT context) {
    // Build module list for address resolution
    struct ModuleEntry {
        DWORD64 base;
        DWORD64 end;
        char name[MAX_PATH];
    };
    ModuleEntry modules[1024];
    unsigned int moduleCount = 0;

    HANDLE hProc = GetCurrentProcess();
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)) && moduleCount < 1024; i++) {
            MODULEINFO mi;
            if (GetModuleInformation(hProc, hMods[i], &mi, sizeof(mi))) {
                modules[moduleCount].base = (DWORD64)mi.lpBaseOfDll;
                modules[moduleCount].end = (DWORD64)mi.lpBaseOfDll + mi.SizeOfImage;
                GetModuleFileNameA(hMods[i], modules[moduleCount].name, MAX_PATH);
                moduleCount++;
            }
        }
    }

    auto resolveAddr = [&](DWORD64 addr) -> std::string {
        for (unsigned int i = 0; i < moduleCount; i++) {
            if (addr >= modules[i].base && addr < modules[i].end) {
                const char* fileName = modules[i].name;
                const char* lastSlash = strrchr(fileName, '\\');
                if (lastSlash) fileName = lastSlash + 1;
                char buf[512];
                snprintf(buf, sizeof(buf), "%s +0x%llX", fileName, addr - modules[i].base);
                return std::string(buf);
            }
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "0x%016llX", addr);
        return std::string(buf);
    };

    logger->critical("=== Exception Call Stack ===");

#if defined(_M_X64)
    CONTEXT ctx = *context;
    for (int i = 0; i < 100; ++i) {
        DWORD64 rip = ctx.Rip;
        if (rip == 0) break;

        logger->critical("{:>3} {} (0x{:016X})", i, resolveAddr(rip), rip);

        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fnEntry = RtlLookupFunctionEntry(rip, &imageBase, NULL);
        if (!fnEntry) break;

        PVOID handlerData = nullptr;
        DWORD64 establisherFrame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, rip, fnEntry,
                         &ctx, &handlerData, &establisherFrame, NULL);
    }
#else
    STACKFRAME64 stackFrame = {};
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrStack.Offset = context->Esp;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    for (int i = 0; i < 100; ++i) {
        if (!StackWalk64(machineType, hProc, GetCurrentThread(),
                         &stackFrame, context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (stackFrame.AddrPC.Offset == 0) break;
        logger->critical("{:>3} {} (0x{:016X})", i, resolveAddr(stackFrame.AddrPC.Offset), stackFrame.AddrPC.Offset);
    }
#endif
}

void LogModuleInfo(PVOID address) {
    IMAGEHLP_MODULE64 moduleInfo = {};
    moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (SymGetModuleInfo64(GetCurrentProcess(), (DWORD64)address, &moduleInfo)) {
        logger->critical("Module: {} (Base: 0x{:016X}, Size: {})",
                         moduleInfo.ModuleName, moduleInfo.BaseOfImage, moduleInfo.ImageSize);
    } else {
        DWORD errorCode = GetLastError();
        logger->critical("SymGetModuleInfo64 failed: [{}] for address 0x{:016X}", errorCode, (DWORD64)address);

        // Fallback: use VirtualQuery + GetModuleFileName to identify the module
        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(address, &mbi, sizeof(mbi))) {
            HMODULE hMod = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    (LPCSTR)mbi.AllocationBase, &hMod) && hMod) {
                char modName[MAX_PATH] = {};
                GetModuleFileNameA(hMod, modName, MAX_PATH);
                logger->critical("Module (fallback): {} (Base: 0x{:016X})", modName, (DWORD64)mbi.AllocationBase);
            } else {
                logger->critical("Region: Base=0x{:016X} Type=0x{:X} State=0x{:X} Protect=0x{:X}",
                                 (DWORD64)mbi.AllocationBase, mbi.Type, mbi.State, mbi.Protect);
            }
        } else {
            logger->critical("VirtualQuery also failed: [{}]", GetLastError());
        }

        // Check if this address is inside any loaded module by enumerating them
        HMODULE hMods[1024];
        DWORD cbNeeded;
        HANDLE hProc = GetCurrentProcess();
        if (EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
            DWORD64 addr = (DWORD64)address;
            for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
                MODULEINFO mi;
                if (GetModuleInformation(hProc, hMods[i], &mi, sizeof(mi))) {
                    DWORD64 base = (DWORD64)mi.lpBaseOfDll;
                    DWORD64 end = base + mi.SizeOfImage;
                    if (addr >= base && addr < end) {
                        char modName[MAX_PATH] = {};
                        GetModuleFileNameA(hMods[i], modName, MAX_PATH);
                        logger->critical("AddrMatched: {} +0x{:X}", modName, (DWORD)(addr - base));
                    }
                }
            }
        }
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
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), nullptr, FALSE);

    HMODULE hMods[1024];
    DWORD cbNeeded;
    HANDLE hProc = GetCurrentProcess();
    if (EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char modName[MAX_PATH];
            MODULEINFO mi;
            if (GetModuleInformation(hProc, hMods[i], &mi, sizeof(mi)) &&
                GetModuleFileNameA(hMods[i], modName, MAX_PATH)) {
                SymLoadModuleEx(hProc, NULL, modName, NULL,
                               (DWORD64)mi.lpBaseOfDll, mi.SizeOfImage, NULL, 0);
            }
        }
    }

    SetUnhandledExceptionFilter(ExceptionHandler);
}

void ShutdownCrashHandler() {
	SymCleanup(GetCurrentProcess());
}

