#pragma once
//high resolution timer using c++11 and later
#include <chrono>

inline void DebugOut(const LPCWSTR fmt, ...)
{
  va_list argp; 
  va_start(argp, fmt); 
  wchar_t dbg_out[4096];
  vswprintf_s(dbg_out, fmt, argp);
  va_end(argp); 
  OutputDebugString(dbg_out);
}
