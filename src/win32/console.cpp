/**
 * @file win32/console.cpp
 * @brief Win32 console I/O
 *
 * (c) 2013-2018 by Mega Limited, Auckland, New Zealand
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
#include "megaapi.h"
#include <windows.h>
#include <conio.h>

#include <io.h>
#include <fcntl.h>
#include <algorithm>

namespace mega {

static int clamp(int v, int lo, int hi)
{
    // todo: switch to c++17 std version when we can
    if (v < lo)
    {
        return lo;
    }
    else if (v > hi)
    {
        return hi;
    }
    else
    {
        return v;
    }
}

void ConsoleModel::addInputChar(wchar_t c)
{
    insertPos = clamp(insertPos, 0, (int)buffer.size());
    if (c == 13)
    {
        buffer.push_back(c);
        insertPos = (int)buffer.size();
        newlinesBuffered += 1;
        consoleNewlineNeeded = true;
    }
    else
    {
        buffer.insert(insertPos, 1, c);
        insertPos += 1;
        redrawInputLineNeeded = true;
    }
    autocompleteState.active = false;
}

void ConsoleModel::getHistory(int index, int offset)
{
    if (inputHistory.empty() && offset == 1)
        buffer.clear();
    else
    {
        index = clamp(index, 0, (int)inputHistory.size() - 1) + (enteredHistory ? offset : (offset == -1 ? -1 : 0));
        if (index < 0 || index >= (int)inputHistory.size())
        {
            return;
        }
        inputHistoryIndex = index;
        buffer = inputHistory[inputHistoryIndex];
        enteredHistory = true;
    }
    insertPos = (int)buffer.size();
    redrawInputLineNeeded = true;
}

void ConsoleModel::redrawInputLine(int p)
{
    insertPos = clamp(p, 0, (int)buffer.size());
    redrawInputLineNeeded = true;
}

static std::string inputLineAsUtf8String(const std::wstring& ws)
{
    std::string s;
    ::mega::MegaApi::utf16ToUtf8(ws.data(), (int)ws.size(), &s);
    return s;
}

static std::wstring inputLineFromUtf8String(const std::string& s)
{
    std::string ws;
    ::mega::MegaApi::utf8ToUtf16(s.c_str(), &ws);
    return std::wstring((wchar_t*)ws.data(), ws.size()/2);
}

void ConsoleModel::autoComplete(bool forwards)
{
    if (autocompleteSyntax)
    {
        if (!autocompleteState.active)
        {
            std::string u8line = inputLineAsUtf8String(buffer);
            autocompleteState = autocomplete::autoComplete(u8line, autocompleteSyntax, unixCompletions);
            autocompleteState.active = true;
        }
        autocomplete::applyCompletion(autocompleteState, forwards);
        buffer = inputLineFromUtf8String(autocompleteState.line);
        insertPos = (int)buffer.size();
        redrawInputLineNeeded = true;
    }
}

static bool isWordBoundary(int i, const std::wstring s)
{
    return i <= 0 || i >= s.size() || isspace(s[i - 1]) && !isspace(s[i + 1]);
}

int ConsoleModel::detectWordBoundary(int start, bool forward)
{
    start = clamp(start, 0, (int)buffer.size());
    do
    {
        start += (forward ? 1 : -1);
    } while (!isWordBoundary(start, buffer));
    return start;
}

void ConsoleModel::deleteCharRange(int start, int end)
{
    start = clamp(start, 0, (int)buffer.size());
    end = clamp(end, 0, (int)buffer.size());
    if (start < end)
    {
        buffer.erase(start, end - start);
        redrawInputLine(start);
    }
}

void ConsoleModel::performLineEditingAction(lineEditAction action)
{
    if (action != AutoCompleteForwards && action != AutoCompleteBackwards)
    {
        autocompleteState.active = false;
    }

    switch (action)
    {
    case CursorLeft: redrawInputLine(insertPos - 1); break;
    case CursorRight: redrawInputLine(insertPos + 1); break;
    case CursorStart: redrawInputLine(0);  break;
    case CursorEnd: redrawInputLine((int)buffer.size()); break;
    case WordLeft: redrawInputLine(detectWordBoundary(insertPos, false));  break;
    case WordRight: redrawInputLine(detectWordBoundary(insertPos, true));  break;
    case HistoryUp: getHistory(inputHistoryIndex, 1);  break;
    case HistoryDown: getHistory(inputHistoryIndex, -1); break;
    case HistoryStart: getHistory((int)inputHistory.size() - 1, 0);  break;
    case HistoryEnd: getHistory(0, 0);  break;
    case ClearLine: deleteCharRange(0, (int)buffer.size());  break;
    case DeleteCharLeft: deleteCharRange(insertPos - 1, insertPos);  break;
    case DeleteCharRight: deleteCharRange(insertPos, insertPos + 1); break;
    case DeleteWordLeft: deleteCharRange(detectWordBoundary(insertPos, false), insertPos);  break;
    case DeleteWordRight: deleteCharRange(insertPos, detectWordBoundary(insertPos, true));  break;
    case AutoCompleteForwards: autoComplete(true); break;
    case AutoCompleteBackwards: autoComplete(false); break;
    }
}

bool ConsoleModel::checkForCompletedInputLine(std::wstring& ws)
{
    auto newlinePos = std::find(buffer.begin(), buffer.end(), 13);
    if (newlinePos != buffer.end())
    {
        ws.assign(buffer.begin(), newlinePos);
        buffer.erase(buffer.begin(), newlinePos + 1);
        insertPos = 0;
        newlinesBuffered -= 1;
        bool sameAsLastCommand = !inputHistory.empty() && inputHistory[0] == ws;
        bool sameAsChosenHistory = !inputHistory.empty() &&
            inputHistoryIndex >= 0 && inputHistoryIndex < inputHistory.size() &&
            inputHistory[inputHistoryIndex] == ws;
        if (echoOn && !sameAsLastCommand && !ws.empty())
        {
            if (inputHistory.size() + 1 > MaxHistoryEntries)
            {
                inputHistory.pop_back();
            }
            inputHistory.push_front(ws);
            inputHistoryIndex = sameAsChosenHistory ? inputHistoryIndex + 1 : -1;
        }
        enteredHistory = false;
        return true;
    }
    return false;
}


WinConsole::WinConsole()
{
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD dwMode;
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

void WinConsole::setAutocompleteSyntax(autocomplete::ACN a)
{
    model.autocompleteSyntax = a;
}

void WinConsole::setAutocompleteStyle(bool unix)
{
    model.unixCompletions = true;
}

HANDLE WinConsole::inputAvailableHandle()
{
    // returns a handle that will be signalled when there is console input to process (ie records available for PeekConsoleInput)
    // client can wait on this handle with other handles as higher priority
    return hInput;
}

bool WinConsole::consolePeek()
{
    // Read keypreses up to the first newline (or multiple newlines if 
    bool checkPromptOnce = true;
    for (;;)
    {
        INPUT_RECORD ir;
        DWORD nRead;
        BOOL ok = PeekConsoleInput(hInput, &ir, 1, &nRead);  // peek first so we never wait
        assert(ok);
        if (!nRead)
        {
            break;
        }

        bool isCharacterGeneratingKeypress =
            ir.EventType == 1 && ir.Event.KeyEvent.uChar.UnicodeChar != 0 &&
            (ir.Event.KeyEvent.bKeyDown ||  // key press
            (!ir.Event.KeyEvent.bKeyDown && ((ir.Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED) || ir.Event.KeyEvent.wVirtualKeyCode == VK_MENU)));  // key release that emits a unicode char

        if (isCharacterGeneratingKeypress && (currentPrompt.empty() || model.newlinesBuffered))
        {
            // wait until the next prompt is output before echoing and processing
            break;
        }

        ok = ReadConsoleInputW(hInput, &ir, 1, &nRead);  // discard the event record
        assert(ok);
        assert(nRead == 1);

        ConsoleModel::lineEditAction action = interpretLineEditingKeystroke(ir);

        if ((action != ConsoleModel::nullAction || isCharacterGeneratingKeypress) && checkPromptOnce)
        {
            redrawPromptIfLoggingOccurred();
            checkPromptOnce = false;
        }
        if (action != ConsoleModel::nullAction)
        {
            model.performLineEditingAction(action);
        }
        else if (isCharacterGeneratingKeypress)
        {
            for (int i = ir.Event.KeyEvent.wRepeatCount; i--; )
            {
                model.addInputChar(ir.Event.KeyEvent.uChar.UnicodeChar);
            }
            if (model.newlinesBuffered)  // todo: address case where multiple newlines were added from this one record (as we may get stuck in wait())
            {
                break;
            }
        }
    }
    if (model.redrawInputLineNeeded && model.echoOn)
    {
        redrawInputLine();
    }
    if (model.consoleNewlineNeeded)
    {
        std::cout << '\n' << std::flush;
    }
    if (model.redrawInputLineNeeded || model.consoleNewlineNeeded)
    {
        prepareDetectLogging();
    }
    model.redrawInputLineNeeded = false;
    model.consoleNewlineNeeded = false;
    return model.newlinesBuffered;
}



ConsoleModel::lineEditAction WinConsole::interpretLineEditingKeystroke(INPUT_RECORD &ir)
{
    if (ir.EventType == 1 && ir.Event.KeyEvent.bKeyDown)
    {
        bool ctrl = ir.Event.KeyEvent.dwControlKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED);
        bool shift = ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED;
        switch (ir.Event.KeyEvent.uChar.UnicodeChar)
        {
        case '\b': return ConsoleModel::DeleteCharLeft;
        case '\t': return shift ? ConsoleModel::AutoCompleteBackwards : ConsoleModel::AutoCompleteForwards;
        case VK_ESCAPE: return ConsoleModel::ClearLine;                 
        case 0:
            switch (ir.Event.KeyEvent.wVirtualKeyCode)
            {
            case VK_LEFT: return ctrl ? ConsoleModel::WordLeft : ConsoleModel::CursorLeft;
            case VK_RIGHT: return ctrl ? ConsoleModel::WordRight : ConsoleModel::CursorRight;
            case VK_UP: return ConsoleModel::HistoryUp;
            case VK_DOWN: return ConsoleModel::HistoryDown;
            case VK_PRIOR: return ConsoleModel::HistoryStart; // pageup
            case VK_NEXT: return ConsoleModel::HistoryEnd; // pagedown
            case VK_HOME: return ConsoleModel::CursorStart;
            case VK_END: return ConsoleModel::CursorEnd;
            case VK_DELETE: return ConsoleModel::DeleteCharRight;
            case VK_INSERT: return ConsoleModel::Paste;  // the OS takes care of this; we don't see it
            }
        }
    }
    return ConsoleModel::nullAction;
}




void WinConsole::redrawInputLine()
{
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    BOOL ok = GetConsoleScreenBufferInfo(hOutput, &sbi);
    assert(ok);
    if (ok)
    {
        if (currentPrompt.size() + model.buffer.size() + 1 < sbi.dwSize.X || !model.echoOn)
        {
            inputLineOffset = 0;
        }
        else
        {
            // scroll the line if the cursor reaches the end, or moves back within 15 of the start
            size_t showleft = 15;
            if (inputLineOffset + showleft >= model.insertPos)
            {
                inputLineOffset = model.insertPos - std::min<size_t>(showleft, model.insertPos);
            } else if (currentPrompt.size() + model.insertPos + 1 >= inputLineOffset + sbi.dwSize.X)
            {
                inputLineOffset = currentPrompt.size() + model.insertPos + 1 - sbi.dwSize.X;
            }
        }

        size_t width = std::max<size_t>(currentPrompt.size() + model.buffer.size() + 1 + inputLineOffset, sbi.dwSize.X); // +1 to show character under cursor 
        std::unique_ptr<CHAR_INFO[]> line(new CHAR_INFO[width]);

        for (size_t i = width; i--; )
        {
            line[i].Attributes = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN;
            if (i < inputLineOffset)
            {
                line[i].Char.UnicodeChar = ' ';
            }
            else if (inputLineOffset && i + 1 == inputLineOffset + currentPrompt.size())
            {
                line[i].Char.UnicodeChar = '|';
                line[i].Attributes = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
            }
            else if (i < inputLineOffset + currentPrompt.size())
            {
                line[i].Char.UnicodeChar = currentPrompt[i - inputLineOffset];
                line[i].Attributes |= FOREGROUND_INTENSITY;
            }
            else if (i < currentPrompt.size() + model.buffer.size() && model.echoOn)
            {
                line[i].Char.UnicodeChar = model.buffer[i - currentPrompt.size()];
            }
            else
            {
                line[i].Char.UnicodeChar = ' ';
            }
        }

        SMALL_RECT screenarea2{ 0, sbi.dwCursorPosition.Y, sbi.dwSize.X, sbi.dwCursorPosition.Y };
        ok = WriteConsoleOutputW(hOutput, line.get(), COORD{ SHORT(width), 1 }, COORD{ SHORT(inputLineOffset), 0 }, &screenarea2);
        assert(ok);

        COORD cpos{ SHORT(currentPrompt.size() + model.insertPos - inputLineOffset), sbi.dwCursorPosition.Y };
        ok = SetConsoleCursorPosition(hOutput, cpos);
        assert(ok);

        prepareDetectLogging();
    }
}

bool WinConsole::consoleGetch(wchar_t& c)
{
    // todo: remove this function once we don't need to support readline for any version of megacli on windows
    if (consolePeek())
    {
        c = model.buffer.front();
        model.buffer.erase(0, 1);
        model.newlinesBuffered -= c == 13;
        return true;
    }
    return false;
}


void WinConsole::readpwchar(char* pw_buf, int pw_buf_size, int* pw_buf_pos, char** line)
{
    // todo: remove/stub this function once we don't need to support readline for any version of megacli on windows
    wchar_t c;
    if (consoleGetch(c))  // only processes once newline is buffered, so no backspace processing needed
    {
        if (c == 13)
        {
            std::string s;
            ::mega::MegaApi::utf16ToUtf8(&c, 1, &s);
            *line = _strdup(s.c_str());
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
    model.echoOn = echo;
}

static bool operator==(COORD& a, COORD& b) 
{
    return a.X == b.X && a.Y == b.Y; 
}

void WinConsole::prepareDetectLogging()
{
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    BOOL ok = GetConsoleScreenBufferInfo(hOutput, &sbi);
    assert(ok);
    knownCursorPos = sbi.dwCursorPosition;
}

void WinConsole::redrawPromptIfLoggingOccurred()
{
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    BOOL ok = GetConsoleScreenBufferInfo(hOutput, &sbi);
    assert(ok);
    if (ok && !currentPrompt.empty())
    {
        if (!(knownCursorPos == sbi.dwCursorPosition))
        {
            if (sbi.dwCursorPosition.X != 0)
            {
                std::cout << endl;
            }
            redrawInputLine();
            prepareDetectLogging();
        }
    }
}

void WinConsole::updateInputPrompt(const std::string& newprompt)
{
    currentPrompt = newprompt;
    redrawInputLine();
}

char* WinConsole::checkForCompletedInputLine()
{
    redrawPromptIfLoggingOccurred();
    if (consolePeek())
    {
        std::wstring ws;
        if (model.checkForCompletedInputLine(ws))
        {
            currentPrompt.clear();
            return _strdup(inputLineAsUtf8String(ws).c_str());
        }
    }
    return NULL;
}

void WinConsole::clearScreen()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    BOOL ok = GetConsoleScreenBufferInfo(hOutput, &csbi);
    assert(ok);
    if (ok)
    {
        // Fill the entire buffer with spaces 
        DWORD count;
        ok = FillConsoleOutputCharacter(hOutput, (TCHAR) ' ', csbi.dwSize.X *csbi.dwSize.Y, { 0, 0 }, &count);
        assert(ok);

        // Fill the entire buffer with the current colors and attributes 
        ok = FillConsoleOutputAttribute(hOutput, csbi.wAttributes, csbi.dwSize.X *csbi.dwSize.Y, { 0, 0 }, &count);
        assert(ok);
    }
    ok = SetConsoleCursorPosition(hOutput, { 0, 0 });
    assert(ok);
    currentPrompt.clear();
}

} // namespace
