#include "Misc.h"

namespace logger {

void Log(const wchar_t* fmt, ...)
{
	wchar_t buf[512] = { 0 };
	va_list arg;
	va_start(arg, fmt);
	StringCchVPrintfW(buf, _countof(buf), fmt, arg);
	va_end(arg);
	OutputDebugStringW(buf);
}

}
