#ifndef MISC_H
#define MISC_H

#include <windows.h>
#include <string>
#include <strsafe.h>

namespace logger {

void Log(const wchar_t* fmt, ...);

}

#endif