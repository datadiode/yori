/**
 * @file wininfo/wininfo.c
 *
 * Yori shell reposition or resize windows
 *
 * Copyright (c) 2018 Malcolm J. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <yoripch.h>
#include <yorilib.h>
#include <remimu.hxx>

/**
 Help text to display to the user.
 */
const
CHAR strWinInfoHelpText[] =
        "\n"
        "Return information about a window.\n"
        "\n"
        "WININFO [-license] [-f <fmt>] [-c <class>] [-d] [-b <id>] [-p]\n"
        "        [[-i] [-e] -t <title>]\n"
        "\n"
        "   -c <class>     Look for a window of the given class\n"
        "   -d             Look for a dialog window\n"
        "   -t <title>     Look for a window with the given title\n"
        "   -i             Match title case insensitively\n"
        "   -e             Perform regex match\n"
        "   -b <id>        Operate the button with the specified id\n"
        "   -p             Paste the clipboarded content of the window\n"
        "\n"
        "Format specifiers are:\n"
        "   $left$         The offset from the left of the screen to the window\n"
        "   $top$          The offset from the top of the screen to the window\n"
        "   $width$        The width of the window\n"
        "   $height$       The height of the window\n";

/**
 Display usage text to the user.
 */
BOOL
WinInfoHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("WinInfo %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strWinInfoHelpText);
    return TRUE;
}

/**
 Context information used when populating the output string.
 */
typedef struct _WININFO_CONTEXT {

    /**
     Handle to the window to return information for.
     */
    HWND Window;

    /**
     The coordinates of the window.
     */
    RECT WindowRect;

    /**
     The title of the window.
     */
    PYORI_STRING WindowClass;

    /**
     The title of the window.
     */
    PYORI_STRING WindowTitle;

    /**
     TRUE if any text matching should be case insensitive.  If FALSE, text
     matching is case sensitive."
     */
    BOOLEAN CaseInsensitive;

    /**
     TRUE if a regex match should be performed.
     */
    BOOL RegexMatch;

    /**
     Encoding to use.
     */
    DWORD EncodingToUse;

    /**
     Regex tokens.
     */
    RegexToken tokens[1024];
    int16_t token_count;

} WININFO_CONTEXT, *PWININFO_CONTEXT;

/**
 A callback invoked for every window on the desktop.  Each window is checked
 to see if it matches the search criteria.

 @param hWnd Pointer to a window on the desktop.

 @param lParam Application defined context, in this case a pointer to the
        application context.

 @return TRUE to continue enumerating, or FALSE to stop enumerating.
 */
BOOL WINAPI
WinInfoWindowFound(
    __in HWND hWnd,
    __in LPARAM lParam
    )
{
    PWININFO_CONTEXT WinInfoContext = (PWININFO_CONTEXT)lParam;
    YORI_STRING WindowTitle;
    WCHAR Buffer[1024];

    YoriLibInitEmptyString(&WindowTitle);
    WindowTitle.StartOfString = Buffer;

    if (WinInfoContext->WindowClass) {
        WindowTitle.LengthInChars = DllUser32.pGetClassNameW(hWnd, Buffer, sizeof(Buffer)/sizeof(Buffer[0]));
        if (YoriLibCompareStringIns(&WindowTitle, WinInfoContext->WindowClass) != 0) {
            return TRUE;
        }
    }

    if (WinInfoContext->WindowTitle) {
        BOOL MatchFound = FALSE;
        WindowTitle.LengthInChars = DllUser32.pGetWindowTextW(hWnd, Buffer, sizeof(Buffer)/sizeof(Buffer[0]));
        if (WinInfoContext->RegexMatch) {
            int offset = 0;
            const int cbtext = WideCharToMultiByte(WinInfoContext->EncodingToUse, 0, WindowTitle.StartOfString, WindowTitle.LengthInChars, NULL, 0, NULL, NULL);
            char *text = (char *)_alloca(cbtext + 1);
            if (WinInfoContext->CaseInsensitive && DllUser32.pCharLowerBuffW) {
                DllUser32.pCharLowerBuffW(WindowTitle.StartOfString, WindowTitle.LengthInChars);
            }
            WideCharToMultiByte(WinInfoContext->EncodingToUse, 0, WindowTitle.StartOfString, WindowTitle.LengthInChars, text, cbtext, NULL, NULL);
            text[cbtext] = '\0';
            while (offset <= cbtext) {
                int length;
                if ((text[offset] & '\xC0') == '\x80') {
                    ++offset;
                    continue;
                }
                length = (int)regex_match(WinInfoContext->tokens, text, offset, 0, NULL, NULL);
                if (length >= 0) {
                    MatchFound = TRUE;
                    break;
                }
                ++offset;
            }
        } else if (WinInfoContext->CaseInsensitive) {
            if (YoriLibCompareStringIns(&WindowTitle, WinInfoContext->WindowTitle) == 0) {
                MatchFound = TRUE;
            }
        } else {
            if (YoriLibCompareString(&WindowTitle, WinInfoContext->WindowTitle) == 0) {
                MatchFound = TRUE;
            }
        }
        if (!MatchFound) {
            return TRUE;
        }
    }

    WinInfoContext->Window = hWnd;
    return FALSE;
}

/**
 A callback function to expand any known variables found when parsing the
 format string.

 @param OutputString A pointer to the output string to populate with data
        if a known variable is found.  The length allocated contains the
        length that can be populated with data.

 @param VariableName The variable name to expand.

 @param Context Pointer to a @ref WININFO_CONTEXT structure containing
        the data to populate.
 
 @return The number of characters successfully populated, or the number of
         characters required in order to successfully populate, or zero
         on error.
 */
YORI_ALLOC_SIZE_T
WinInfoExpandVariables(
    __inout PYORI_STRING OutputString,
    __in PYORI_STRING VariableName,
    __in PVOID Context
    )
{
    YORI_ALLOC_SIZE_T CharsNeeded;
    PWININFO_CONTEXT WinInfoContext = (PWININFO_CONTEXT)Context;

    if (YoriLibCompareStringLit(VariableName, _T("left")) == 0) {
        CharsNeeded = YoriLibSPrintfSize(_T("%i"), WinInfoContext->WindowRect.left);
    } else if (YoriLibCompareStringLit(VariableName, _T("top")) == 0) {
        CharsNeeded = YoriLibSPrintfSize(_T("%i"), WinInfoContext->WindowRect.top);
    } else if (YoriLibCompareStringLit(VariableName, _T("width")) == 0) {
        CharsNeeded = YoriLibSPrintfSize(_T("%i"), WinInfoContext->WindowRect.right - WinInfoContext->WindowRect.left);
    } else if (YoriLibCompareStringLit(VariableName, _T("height")) == 0) {
        CharsNeeded = YoriLibSPrintfSize(_T("%i"), WinInfoContext->WindowRect.bottom - WinInfoContext->WindowRect.top);
    } else {
        return 0;
    }

    if (OutputString->LengthAllocated < CharsNeeded) {
        return CharsNeeded;
    }

    if (YoriLibCompareStringLit(VariableName, _T("left")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), WinInfoContext->WindowRect.left);
    } else if (YoriLibCompareStringLit(VariableName, _T("top")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), WinInfoContext->WindowRect.top);
    } else if (YoriLibCompareStringLit(VariableName, _T("width")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), WinInfoContext->WindowRect.right - WinInfoContext->WindowRect.left);
    } else if (YoriLibCompareStringLit(VariableName, _T("height")) == 0) {
        CharsNeeded = YoriLibSPrintf(OutputString->StartOfString, _T("%i"), WinInfoContext->WindowRect.bottom - WinInfoContext->WindowRect.top);
    }

    OutputString->LengthInChars = CharsNeeded;
    return CharsNeeded;
}

#ifdef YORI_BUILTIN
/**
 The main entrypoint for the wininfo builtin command.
 */
#define ENTRYPOINT YoriCmd_WININFO
#else
/**
 The main entrypoint for the wininfo standalone application.
 */
#define ENTRYPOINT ymain
#endif

/**
 The main entrypoint for the wininfo cmdlet.

 @param ArgC The number of arguments.

 @param ArgV An array of arguments.

 @return Exit code of the child process on success, or failure if the child
         could not be launched.
 */
DWORD
ENTRYPOINT(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    BOOLEAN ArgumentUnderstood;
    BOOLEAN ClipboardedText = FALSE;
    DWORD ButtonId = 0;
    YORI_MAX_SIGNED_T Temp;
    YORI_ALLOC_SIZE_T CharsConsumed;
    YORI_ALLOC_SIZE_T StartArg = 0;
    YORI_ALLOC_SIZE_T i;
    YORI_STRING Arg;
    YORI_STRING DialogClass;
    YORI_STRING DisplayString;
    WININFO_CONTEXT Context;
    LPCTSTR FormatString = _T("Position: $left$*$top$\n")
                           _T("Size:     $width$*$height$\n");
    YORI_STRING YsFormatString;

    YoriLibInitEmptyString(&YsFormatString);
    ZeroMemory(&Context, sizeof(Context));
    Context.EncodingToUse = YoriLibGetUsableEncoding();
    YoriLibConstantString(&DialogClass, _T("#32770"));

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;
        ASSERT(YoriLibIsStringNullTerminated(&ArgV[i]));

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringLitIns(&Arg, _T("?")) == 0) {
                WinInfoHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2018"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("f")) == 0) {
                if (ArgC >= i + 1) {
                    YsFormatString.StartOfString = ArgV[i + 1].StartOfString;
                    YsFormatString.LengthInChars = ArgV[i + 1].LengthInChars;
                    ArgumentUnderstood = TRUE;
                    i++;
                }
            } else if (YoriLibCompareStringLitIns(&Arg, _T("b")) == 0) {
                if (ArgC >= i + 1) {
                    if (YoriLibStringToNumber(&ArgV[i + 1], TRUE, &Temp, &CharsConsumed)) {
                        ButtonId = (DWORD)Temp;
                        ArgumentUnderstood = TRUE;
                        i++;
                    }
                }
            } else if (YoriLibCompareStringLitIns(&Arg, _T("p")) == 0) {
                ClipboardedText = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("c")) == 0) {
                if (ArgC >= i + 1) {
                    Context.WindowClass = &ArgV[i + 1];
                    ArgumentUnderstood = TRUE;
                    i++;
                }
            } else if (YoriLibCompareStringLitIns(&Arg, _T("d")) == 0) {
                Context.WindowClass = &DialogClass;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("i")) == 0) {
                Context.CaseInsensitive = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("e")) == 0) {
                Context.RegexMatch = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("t")) == 0) {
                if (ArgC >= i + 1) {
                    Context.WindowTitle = &ArgV[i + 1];
                    ArgumentUnderstood = TRUE;
                    i++;
                }
            }
        } else {
            ArgumentUnderstood = TRUE;
            StartArg = i;
            break;
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

    YoriLibLoadUser32Functions();

    if (Context.WindowTitle != NULL) {
        if (Context.RegexMatch) {
            int cbpattern = WideCharToMultiByte(Context.EncodingToUse, 0, Context.WindowTitle->StartOfString, Context.WindowTitle->LengthInChars, NULL, 0, NULL, NULL);
            char* pattern = _alloca(cbpattern + 1);

            if (Context.CaseInsensitive)
            {
                if (DllUser32.pCharLowerBuffW)
                {
                    YORI_ALLOC_SIZE_T Index;
                    BOOL Escaped = FALSE;

                    for (Index = 0; Index < Context.WindowTitle->LengthInChars; ++Index)
                    {
                        if (Escaped)
                        {
                            Escaped = FALSE;
                        }
                        else if (Context.WindowTitle->StartOfString[Index] == '\\')
                        {
                            Escaped = TRUE;
                        }
                        else
                        {
                            DllUser32.pCharLowerBuffW(Context.WindowTitle->StartOfString + Index, 1);
                        }
                    }
                }
            }

            WideCharToMultiByte(Context.EncodingToUse, 0, Context.WindowTitle->StartOfString, Context.WindowTitle->LengthInChars, pattern, cbpattern, NULL, NULL);
            pattern[cbpattern] = '\0';
            Context.token_count = 1024;
            if (regex_parse(pattern, Context.tokens, &Context.token_count, 0) != 0) {
                YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: invalid regex\n"));
                return EXIT_FAILURE;
            }
        }

        if (DllUser32.pEnumWindows == NULL) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: operating system support not present\n"));
            return EXIT_FAILURE;
        }

        DllUser32.pEnumWindows(WinInfoWindowFound, (LPARAM)&Context);

        if (Context.Window == NULL) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: window not found\n"));
            return EXIT_FAILURE;
        }
        if (DllUser32.pGetWindowRect != NULL) {
            DllUser32.pGetWindowRect(Context.Window, &Context.WindowRect);
        }
    } else {
        if (DllUser32.pGetDesktopWindow == NULL) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: operating system support not present\n"));
            return EXIT_FAILURE;
        }

        Context.Window = DllUser32.pGetDesktopWindow();
        if (DllUser32.pGetClientRect != NULL) {
            DllUser32.pGetClientRect(Context.Window, &Context.WindowRect);
        }
    }

    YoriLibInitEmptyString(&DisplayString);
    if (ClipboardedText) {
        if (!YoriLibCopyText(&DisplayString)) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: could not clear clipboard\n"));
            return EXIT_FAILURE;
        }
        if (!DllUser32.pSendMessageTimeoutW(Context.Window, WM_COPY, 0, 0, SMTO_NORMAL, 200, NULL)) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: window did not respond to WM_COPY\n"));
            return EXIT_FAILURE;
        }
        if (!YoriLibPasteText(&DisplayString)) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: could not read clipboard\n"));
            return EXIT_FAILURE;
        }
    } else {
        if (YsFormatString.StartOfString == NULL) {
            YoriLibConstantString(&YsFormatString, FormatString);
        }
        YoriLibExpandCommandVariables(&YsFormatString, '$', WinInfoExpandVariables, &Context, &DisplayString);
    }
    if (DisplayString.StartOfString != NULL) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%y"), &DisplayString);
        YoriLibFreeStringContents(&DisplayString);
    }

    if (ButtonId) {
        if (!DllUser32.pSendMessageTimeoutW(Context.Window, WM_COMMAND, ButtonId, 0, SMTO_NORMAL, 200, NULL)) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("wininfo: window did not respond to WM_COMMAND\n"));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// vim:sw=4:ts=4:et:
