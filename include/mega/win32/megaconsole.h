/**
 * @file mega/win32/megaconsole.h
 * @brief Win32 console I/O
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef CONSOLE_CLASS
#define CONSOLE_CLASS WinConsole

#include <string>
#include <deque>

namespace mega {
struct MEGA_API WinConsole : public Console
{
    void readpwchar(char*, int, int* pw_buf_pos, char**);
    void setecho(bool);

    WinConsole();
    ~WinConsole();

    // functions for native command editing (ie not using readline library)
    static void setShellConsole();
    void addInputChar(wchar_t c);
    bool consolePeek();
    bool consoleGetch(wchar_t& c);
    void updateInputPrompt(const std::string& newprompt);
    char* checkForCompletedInputLine();

    HANDLE hInput;
    HANDLE hOutput;
    std::deque<wchar_t> inputCharBuffer;
    bool echoOn;
    size_t currentPromptLen;
};
} // namespace

#endif
