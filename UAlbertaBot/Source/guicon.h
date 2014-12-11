// copied from http://dslweb.nwnexus.com/~ast/dload/guicon.htm

#ifndef __GUICON_H__
#define __GUICON_H__

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501             // windows XP or later, for changing console window size
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>

void RedirectIOToConsole();

#endif