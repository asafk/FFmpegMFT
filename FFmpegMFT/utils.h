#pragma once
//high resolution timer using c++11 and later
#include <chrono>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

inline void DebugOut(const LPCWSTR fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	wchar_t dbg_out[4096];
	vswprintf_s(dbg_out, fmt, argp);
	va_end(argp);
	OutputDebugString(dbg_out);
}

inline std::string basename(const std::string& file)
{
	unsigned int i = file.find_last_of("\\/");
	return i == std::string::npos ? file : file.substr(i + 1);
}

inline std::string stackTrace()
{
	DWORD machine = IMAGE_FILE_MACHINE_I386;
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	SymSetOptions(SYMOPT_LOAD_ANYTHING);

	if (!SymInitialize(hProcess, NULL, TRUE))
	{
		// SymInitialize failed
		DWORD error = GetLastError();
		Logger::getInstance().LogError("SymInitialize returned error : %d", error);
		return NULL;
	}

	CONTEXT    context = {};
	context.ContextFlags = CONTEXT_FULL;
	RtlCaptureContext(&context);

	STACKFRAME frame = {};
	frame.AddrPC.Offset = context.Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Esp;
	frame.AddrStack.Mode = AddrModeFlat;

	std::stringstream stackTraceStr;
	stackTraceStr << "\n";

	std::string lastname;
	while (StackWalk(machine, hProcess, hThread, &frame, &context, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL))
	{
		DWORD64 moduleBase = 0;
		DWORD  dwAddress;
		std::string name;
		std::string module;

		dwAddress = frame.AddrPC.Offset;

		DWORD64  dwDisplacement = 0;
		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		if (SymFromAddr(hProcess, dwAddress, &dwDisplacement, pSymbol))
		{
			name = pSymbol->Name;
			moduleBase = pSymbol->ModBase;
		}
		else
		{
			// SymFromAddr failed
			DWORD error = GetLastError();
			Logger::getInstance().LogError("SymFromAddr returned error : %d", error);
		}

		if (!moduleBase)
		{
			//try load without symbol
			moduleBase = SymGetModuleBase(hProcess, dwAddress);
		}

		//only if it's different from last function name or we have module base
		if (!moduleBase || lastname == name)continue;

		char moduelBuff[MAX_PATH] = "NA";
		if (!GetModuleFileNameA((HINSTANCE)moduleBase, moduelBuff, MAX_PATH))
		{
			DWORD error = GetLastError();
			Logger::getInstance().LogError("GetModuleFileNameA returned error : %d", error);
		}
		module = basename(moduelBuff);

		IMAGEHLP_LINE line;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

		stackTraceStr << " at " << name << " from " << module;

		if (!SymGetLineFromAddr(hProcess, dwAddress, (PDWORD)&dwDisplacement, &line))
		{
			// SymGetLineFromAddr64 failed
			DWORD error = GetLastError();
			if (error == 487)
			{				
				stackTraceStr << "\n";
			}
			else
				Logger::getInstance().LogError("SymGetLineFromAddr64 returned error : %d", error);
		}
		else
		{
			stackTraceStr << 
				" (" << line.FileName << ":" <<	(std::dec) << line.LineNumber << ")\n";
		}

		lastname = name;
	}

	if (!SymCleanup(hProcess))
	{
		// SymCleanup failed
		DWORD error = GetLastError();
		Logger::getInstance().LogError("SymCleanup returned error : %d", error);
	}

	return stackTraceStr.str();
}