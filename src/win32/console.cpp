/**
 * @file win32/console.cpp
 * @brief Win32 console I/O
 *
 * (c) 2013-2014 by Mega Limited, Wellsford, New Zealand
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

#include "mega.h"
#include <windows.h>
#include <conio.h>

#include <io.h>
#include <fcntl.h>

namespace mega {
WinConsole::WinConsole()
    : echoOn(true)
    , currentPromptLen(0)
{
    DWORD dwMode;

    hInput = GetStdHandle(STD_INPUT_HANDLE);
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(hInput, &dwMode);
    SetConsoleMode(hInput, dwMode & ~(ENABLE_MOUSE_INPUT));
    FlushConsoleInputBuffer(hInput);
}

WinConsole::~WinConsole()
{
    // restore startup config
}

void WinConsole::setShellConsole()
{
    // Call this if your console app is taking live input, with the user editing commands on screen, similar to cmd or powershell

    // use cases covered
    // utf8 output with std::cout (since we already use cout so much and it's compatible with other platforms)
    // unicode input with windows ReadConsoleInput api
    // drag and drop filenames from explorer to the console window
    // upload and download unicode/utf-8 filenames to/from Mega
    // input a unicode/utf8 password without displaying anything
    // normal cmd window type editing (but no autocomplete, as yet - we could add it now though)
    // the console must have a suitable font selected for the characters to diplay properly

    BOOL ok;
    // make sure the console and its buffer support utf
    ok = SetConsoleCP(CP_UTF8);
    assert(ok);
    ok = SetConsoleOutputCP(CP_UTF8);
    assert(ok);

    // Enable buffering to prevent VS from chopping up UTF byte sequences
    setvbuf(stdout, nullptr, _IOFBF, 4096);
}

void WinConsole::addInputChar(wchar_t c)
{
    bool suppressEcho = false;
    if (c == 8 && !echoOn)
    {
        // put it in the buffer so single-char fetch can report it for password edits    
        inputCharBuffer.push_back(c);  
    }
    else if (c == 8)
    {
        if (inputCharBuffer.empty())
        {
            suppressEcho = true;
        }
        else
        {
            inputCharBuffer.pop_back();
        }
    }
    else
    {
        inputCharBuffer.push_back(c);
    }

    if (echoOn && !suppressEcho)
    {
        if (c == '\r')
        {
            c = '\n';
        }
        char s[20];
        int resultSize = WideCharToMultiByte(CP_UTF8, 0, &c, 1, s, sizeof(s), NULL, NULL);
        assert(resultSize < sizeof(s));
        std::cout << std::string(s, resultSize).c_str() << std::flush;
    }
}


bool WinConsole::consolePeek()
{
    for (;;)
    {
        INPUT_RECORD ir;
        DWORD nRead;
        BOOL ok = PeekConsoleInput(hInput, &ir, 1, &nRead);  // peek first so we never wait
        assert(ok);
        if (nRead == 1)
        {
            BOOL ok = ReadConsoleInput(hInput, &ir, 1, &nRead);  // discard anything that's not key down.
            assert(ok);
            assert(nRead == 1);
            if (ir.EventType == 1 && ir.Event.KeyEvent.uChar.UnicodeChar != 0)
            {
                if (ir.Event.KeyEvent.bKeyDown ||  // key press
                    (!ir.Event.KeyEvent.bKeyDown && ((ir.Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED) || ir.Event.KeyEvent.wVirtualKeyCode == VK_MENU)))  // key release that emits a unicode char
                {
                    for (int i = ir.Event.KeyEvent.wRepeatCount; i--; )
                    {
                        addInputChar(ir.Event.KeyEvent.uChar.UnicodeChar);
                    }
                }
            }
        }
        else
        {
            break;
        }
    }
    return !inputCharBuffer.empty();
}

bool WinConsole::consoleGetch(wchar_t& c)
{
    if (consolePeek())
    {
        c = inputCharBuffer.front();
        inputCharBuffer.pop_front();
        return true;
    }
    return false;
}


void WinConsole::readpwchar(char* pw_buf, int pw_buf_size, int* pw_buf_pos, char** line)
{
    wchar_t c;
    if (consoleGetch(c))
    {
        if (c == 8)
        {
            if (*pw_buf_pos > 1)
            {
                (*pw_buf_pos) -= 2;
            }
        }
        else if (c == 13)
        {
            size_t mbcsMax = *pw_buf_pos * 4 + 10;
            std::unique_ptr<char[]> s(new char[mbcsMax]);
            int resultSize = WideCharToMultiByte(CP_UTF8, 0, (LPWCH)pw_buf, *pw_buf_pos / 2, s.get(), (int)mbcsMax, NULL, NULL);
            assert(resultSize + 1 < mbcsMax);
            *line = _strdup(std::string(s.get(), resultSize).c_str());
            memset(pw_buf, 0, pw_buf_size);
        }
        else if (*pw_buf_pos + 2 <= pw_buf_size)
        {
            *(wchar_t*)(pw_buf + *pw_buf_pos) = c;
            *pw_buf_pos += 2;
        }
    }
}

void WinConsole::setecho(bool echo)
{
    echoOn = echo;
}

void WinConsole::updateInputPrompt(const std::string& newprompt)
{
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    BOOL ok = GetConsoleScreenBufferInfo(hOutput, &sbi);
    assert(ok);
    if (ok)
    {
        size_t existingUserChars = (currentPromptLen == 0 || sbi.dwCursorPosition.X < (int)currentPromptLen) ? 0 : sbi.dwCursorPosition.X - currentPromptLen;
        size_t lineLen = std::max<size_t>(newprompt.size(), currentPromptLen) + existingUserChars;

        std::unique_ptr<CHAR_INFO[]> line(new CHAR_INFO[lineLen]);
        for (size_t i = lineLen; i--;)
        {
            line[i].Char.UnicodeChar = ' ';
            line[i].Attributes = 0;
        }

        SMALL_RECT screenarea{ 0, sbi.dwCursorPosition.Y, SHORT(lineLen - 1), sbi.dwCursorPosition.Y };
        ok = ReadConsoleOutputW(hOutput, line.get(), COORD{ SHORT(lineLen), 1 }, COORD{ 0,0 }, &screenarea);
        assert(ok);

        memmove(&line[newprompt.size()], &line[currentPromptLen], sizeof(line[0])*existingUserChars);
        for (size_t i = lineLen; i--;)
        {
            if (i < newprompt.size())
            {
                line[i].Char.UnicodeChar = newprompt[i];
                line[i].Attributes |= FOREGROUND_INTENSITY;
            }
            else
            {
                line[i].Attributes &= ~(FOREGROUND_INTENSITY);
                if (i >= newprompt.size() + existingUserChars)
                {
                    line[i].Char.UnicodeChar = ' ';
                }
            }
        }

        SMALL_RECT screenarea2{ 0, sbi.dwCursorPosition.Y, SHORT(lineLen - 1), sbi.dwCursorPosition.Y };
        ok = WriteConsoleOutputW(hOutput, line.get(), COORD{ SHORT(lineLen), 1 }, COORD{ 0,0 }, &screenarea2);
        assert(ok);

        ok = SetConsoleCursorPosition(hOutput, COORD{ SHORT(newprompt.size() + existingUserChars), sbi.dwCursorPosition.Y });
        assert(ok);

        currentPromptLen = newprompt.size();
    }
}

char* WinConsole::checkForCompletedInputLine()
{
    if (consolePeek())
    {
        unsigned lineCharCount = 0;
        bool lineComplete = false;
        for (auto c : inputCharBuffer)
        {
            if (c == 13)
            {
                lineComplete = true;
                break;
            }
            else
            {
                lineCharCount += 1;
            }
        }
        if (lineComplete)
        {
            std::wstring ws;
            for (int i = lineCharCount; i--; )
            {
                ws += inputCharBuffer.front();
                inputCharBuffer.pop_front();
            }
            inputCharBuffer.pop_front();  // remove newline
            size_t mbcsMax = ws.size() * 6 + 10;
            std::unique_ptr<char[]> ca(new char[mbcsMax]);
            int resultSize = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), ca.get(), (int)mbcsMax, NULL, NULL);
            assert(resultSize + 1 < mbcsMax);
            currentPromptLen = 0;
            return _strdup(std::string(ca.get(), resultSize).c_str());
        }
    }
    return NULL;
}


} // namespace
