/*
 * Copyright (c) 2012-2026 PaperCut Software Pty Ltd
 * http://www.papercut.com/
 *
 * Author: Chris Dance <chris.dance@papercut.com>
 *
 * $Id: $
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ----
 * Ghost Trap - GPL Ghostscript secured using Google Chrome sandbox
 * technology (aka " the Ecto Containment Unit" :-).
 */

#include <windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <fstream>

// Ghostscript DLL Header files
#include <ierrors.h>
#include <iapi.h>

#include "sandbox_procmgmt.h"

/* 
 * Ghost Trap version number starts at 1 and suffixes the Ghostscript version we've
 * tested/written against.
 */
#define GHOST_TRAP_VERSION      "1.6.10.07.0"
#define GHOST_TRAP_COPYRIGHT    "Copyright (c) 2012-2026 PaperCut Software Pty Ltd"

const wchar_t* PARAM_OUTPUT_FILE = L"OutputFile=";
const wchar_t* PARAM_FAIL_TEST = L"--fail-test=";

// Definitions
typedef struct GSDLL_S {
        HINSTANCE hmodule;  /* DLL module handle */
        PFN_gsapi_revision revision;
        PFN_gsapi_new_instance new_instance;
        PFN_gsapi_delete_instance delete_instance;
        PFN_gsapi_set_stdio set_stdio;
        PFN_gsapi_set_poll set_poll;
        PFN_gsapi_set_display_callback set_display_callback;
        PFN_gsapi_init_with_args init_with_args;
        PFN_gsapi_run_string run_string;
        PFN_gsapi_exit exit;
} GSDLL;

// Globals
GSDLL global_gsdll;
void *global_gsinstance;




/*
 * Load the Ghostscript Windows DLL and store implementation in GSDLL instances.
 * Any errors are printed on STDOUT.
 */
static bool LoadGSDLL(GSDLL *gsdll) {

#ifdef _WIN64
    const wchar_t name[] = L"gsdll64.dll";
#else
    const wchar_t name[] = L"gsdll32.dll";
#endif
    
    wchar_t fullname[1024];
    wchar_t *p;

     /* First try to load DLL from the same directory as EXE */
    ::GetModuleFileName(::GetModuleHandle(NULL), fullname, sizeof(fullname));
    if ((p = wcsrchr(fullname, L'\\')) != NULL) {
        ++p;
    } else {
        p = fullname;
    }
    *p = L'\0';
    wcscat_s(fullname, name);

    gsdll->hmodule = LoadLibrary(fullname);

    if (gsdll->hmodule < (HINSTANCE)HINSTANCE_ERROR) {
        /* Failed */
        DWORD err = GetLastError();
        fprintf(stderr, "Can't load GhostScript DLL, LoadLibrary error code %ld\n", err);
        return false;
    }

    gsdll->revision = (PFN_gsapi_revision) GetProcAddress(gsdll->hmodule,
        "gsapi_revision");
    if (gsdll->revision == NULL) {
        fprintf(stderr, "Can't find gsapi_revision\n");
        return false;
    }

    gsdll->new_instance = (PFN_gsapi_new_instance) GetProcAddress(gsdll->hmodule,
        "gsapi_new_instance");
    if (gsdll->new_instance == NULL) {
        fprintf(stderr, "Can't find gsapi_new_instance\n");
        return false;
    }

    gsdll->delete_instance = (PFN_gsapi_delete_instance) GetProcAddress(gsdll->hmodule,
        "gsapi_delete_instance");
    if (gsdll->delete_instance == NULL) {
        fprintf(stderr, "Can't find gsapi_delete_instance\n");
        return false;
    }

    gsdll->set_stdio = (PFN_gsapi_set_stdio) GetProcAddress(gsdll->hmodule,
        "gsapi_set_stdio");
    if (gsdll->set_stdio == NULL) {
        fprintf(stderr, "Can't find gsapi_set_stdio\n");
        return false;
    }

    gsdll->set_poll = (PFN_gsapi_set_poll) GetProcAddress(gsdll->hmodule,
        "gsapi_set_poll");
    if (gsdll->set_poll == NULL) {
        fprintf(stderr, "Can't find gsapi_set_poll\n");
        return false;
    }

    gsdll->set_display_callback = (PFN_gsapi_set_display_callback)
        GetProcAddress(gsdll->hmodule, "gsapi_set_display_callback");
    if (gsdll->set_display_callback == NULL) {
        fprintf(stderr, "Can't find gsapi_set_display_callback\n");
        return false;
    }

    gsdll->init_with_args = (PFN_gsapi_init_with_args)
        GetProcAddress(gsdll->hmodule, "gsapi_init_with_args");
    if (gsdll->init_with_args == NULL) {
        fprintf(stderr, "Can't find gsapi_init_with_args\n");
        return false;
    }

    gsdll->run_string = (PFN_gsapi_run_string) GetProcAddress(gsdll->hmodule,
        "gsapi_run_string");
    if (gsdll->run_string == NULL) {
        fprintf(stderr, "Can't find gsapi_run_string\n");
        return false;
    }

    gsdll->exit = (PFN_gsapi_exit) GetProcAddress(gsdll->hmodule,
        "gsapi_exit");
    if (gsdll->exit == NULL) {
        fprintf(stderr, "Can't find gsapi_exit\n");
        return false;
    }

    return true;
}


/*
 * A convenience wrapper to read a registry keys using wstrings.
 */
static LONG GetStringRegKey(HKEY hKey, const std::wstring &strValueName, std::wstring &strValue, const std::wstring &strDefaultValue) {
    strValue = strDefaultValue;
    WCHAR szBuffer[512];
    DWORD dwBufferSize = sizeof(szBuffer);
    ULONG nError;
    nError = RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
    if (ERROR_SUCCESS == nError) {
        strValue = szBuffer;
    }
    return nError;
}


/* 
 * Allow access to a particular file. This function also ensures the file path is valid according
 * to Sandbox requirements (an absolute path).  If parent_dir is set to TRUE, access to the whole
 * parent directory (all files) is allowed.  Passing a relative file path will raise an error
 * and no white listing will occure.
 */
static void AllowAccessToFile(sandbox::TargetPolicy &policy, wchar_t *file, BOOL parent_dir) {

    wchar_t drive[8];
    wchar_t dir[512];
    wchar_t filename[512];
    wchar_t ext[256];
    errno_t rc;

    rc = _wsplitpath_s(
        file,       /* the path */
        drive,      /* drive */
        8,          /* drive buffer size */
        dir,        /* dir buffer */
        512,        /* dir buffer size */
        filename,       /* filename */
        512,          /* filename size */
        ext,       /* extension */
        256           /* extension size */
    );

    if (rc != 0) {
        fprintf(stderr, "Ghost Trap: Unable to parse file paths to whitelist - use absolute paths.\n");
    } else {
        wchar_t path_rule[MAX_PATH];

        if (wcslen(drive) == 0) {
            // Relative paths and network paths don't work in the sandbox :-(
            fprintf(stderr, "Ghost Trap: Invalid resource. Please use absolute paths on a local drive.\n");
            return;
        }

        if (parent_dir) {
            _snwprintf_s(path_rule, MAX_PATH - 1, L"%s%s*", 
                                drive, 
                                dir);
        } else {
            _snwprintf_s(path_rule, MAX_PATH - 1, L"%s%s%s%s", 
                                drive, 
                                dir,
                                filename,
                                ext);
        }

        (void) policy.GetConfig()->AllowFileAccess(
            sandbox::FileSemantics::kAllowAny,
            path_rule
        );
    }
}


/*
 * Resolve `path` to an absolute path and confirm it names an existing,
 * non-directory file, writing the result into `out_full_path` (a buffer of
 * `out_len` wchar_ts).  Returns FALSE - leaving `out_full_path` unspecified -
 * if the path cannot be resolved, doesn't fit the buffer, or doesn't exist.
 *
 * This existence check matters because Ghostscript resolves a bare input
 * file name (and an @-argument file) on its own library search path
 * (imainarg.c's argproc() runs ".runfile", which reaches findlibfile() via
 * runlibfile - not a literal open - unless -B is in force), not solely as a
 * path relative to the current working directory.  A name that does not
 * exist relative to CWD must be passed through unchanged: rewriting it to a
 * CWD-absolute path here would point Ghostscript at a location that does not
 * exist, defeating that lookup.  A name present both in CWD and on the search
 * path resolves to the CWD copy, as it already did for the last argument.
 *
 * `out_resolve_failed` is set to FALSE on entry, and to TRUE when FALSE is
 * returned for a genuine error worth reporting: GetFullPathName() itself
 * failed, the path doesn't fit the buffer, or GetFileAttributes() failed for
 * a reason other than the path simply not existing (e.g. access denied, a
 * device not ready, a bad network path, a sharing violation).  It stays
 * FALSE when FALSE is returned because the resolved path doesn't exist as a
 * file (ERROR_FILE_NOT_FOUND/ERROR_PATH_NOT_FOUND/ERROR_INVALID_NAME) - the
 * expected, benign case of a name Ghostscript will resolve on its own
 * library search path instead - or because it names a directory, which
 * Ghostscript cannot open as an input file either way.
 */
static BOOL ResolveExistingFullPath(const wchar_t *path, wchar_t *out_full_path, DWORD out_len, BOOL *out_resolve_failed) {
    *out_resolve_failed = FALSE;

    DWORD len = GetFullPathName(path, out_len, out_full_path, NULL);
    if (len == 0 || len >= out_len) {
        *out_resolve_failed = TRUE;
        return FALSE;
    }
    DWORD attrs = GetFileAttributes(out_full_path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        // Only "genuinely not here" is benign; anything else (access denied,
        // a sharing violation, a bad network path, ...) means the file may
        // well exist and the caller should be told the query failed.
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND && err != ERROR_INVALID_NAME) {
            *out_resolve_failed = TRUE;
        }
        return FALSE;
    }
    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return FALSE;
    }
    return TRUE;
}


/*
 * Classify each command-line argument as an input file to Ghostscript or not,
 * mirroring the argument grammar in Ghostscript's psi/imainarg.c, so the sandbox
 * whitelists the files Ghostscript opens rather than guessing at one positionally.
 *
 * For every argv[i] this writes into prefix_len[i] either -1 (not an input file)
 * or the number of leading characters that are switch punctuation rather than
 * part of the path (0 for a bare file, or a file taken after "--"/"-+"/"-@"; 1
 * for "@file", including the "@file" operand of a "-@"; 2 for
 * "-ffile"/"-Ffile").  The file path is then argv[i] + prefix_len[i].
 *
 * Only -c, -I, -o, --, -+ and -@ have handlers that call arg_next(); every other
 * switch is self-contained, so an unknown switch can never cause the next token
 * to be whitelisted.  imainarg.c's swproc() gates "--" on an exact spelling but
 * not "-+"/"-@": those take the next token as the file even with attached text.
 *
 * After a --/-+/-@ file the remaining tokens are drained into userdict/ARGUMENTS
 * as strings, so classification stops there.  Under -- and -+ they are never
 * opened; -@ is the same grammar with arg_next()'s @-expansion left on, because
 * swproc() enters case '@' directly and only the fall-through from case '+'
 * (taken by -- and -+) clears pal->expand_ats.  So under -@ alone an @-prefixed
 * token is an argument file Ghostscript opens: as the file operand, whitelisted
 * here like any other @file, and in the ARGUMENTS drain, which is not (below).
 * Stopping there can only under-grant, never widen.
 *
 * Paths Ghostscript can read that are not modelled: files named inside an
 * @-argument file (the @-file itself is whitelisted), an @-token drained into
 * ARGUMENTS by a -@ run (above), the @-file operand of a bare -I or -o (those
 * handlers call arg_next() without clearing pal->expand_ats), and arguments
 * supplied through GS_OPTIONS.  In the ordinary case a file Ghostscript opens
 * simply goes unwhitelisted and the sandbox denies the read - that fails
 * closed.  What does not is a desync: an @-file is spliced into the same
 * arg_next() stream that then reads argv, so a token-consuming switch at its
 * end takes its operand from the next argv token(s); this classifier, blind to
 * the @-file's contents, can misclassify such a token as an input file, after
 * which it is granted kAllowAny and rewritten to an absolute path - a wider
 * grant and an altered argument, not a denial.  GS_OPTIONS cannot desync this
 * way: it is drained before argv, and the token after it is this file's own
 * injected -dSAFER.  Nor are these the only paths left unwhitelisted: see the
 * FIXME above ExpandPathsInArgs().
 */
static void ClassifyInputArgs(int argc, wchar_t *argv[], int prefix_len[]) {
    bool skip_next = false;    /* previous -I/-o consumed the next token as a value */
    wchar_t sep = 0;           /* the --/-+/-@ just seen (its second char): next token is its file */
    bool in_code = false;      /* inside a -c run: following tokens are code, not files */
    bool done = false;         /* past a --/-+/-@ file: the rest are ARGUMENTS strings */

    for (int i = 0; i < argc; ++i) {
        prefix_len[i] = -1;

        if (i == 0 || done) {      /* argv[0] is the executable, never a file */
            continue;
        }

        wchar_t *a = argv[i];

        if (skip_next) {           /* value consumed by a previous -I/-o */
            skip_next = false;
            continue;
        }
        if (sep != 0) {            /* file after --/-+/-@, even if it starts with '-' */
            /* Only -@ leaves @-expansion on, so only there is a leading '@'
             * a prefix rather than part of the name (see above). */
            prefix_len[i] = (sep == L'@' && a[0] == L'@') ? 1 : 0;
            done = true;           /* Ghostscript runs this file and then quits */
            continue;
        }
        if (in_code) {
            /* -c operands are code until the next switch (not a negative number)
             * or an @-file, matching the loop in imainarg.c's 'c' case.  The test
             * there is isdigit() on a single byte, so only ASCII 0-9 count. */
            if (a[0] == L'@' ||
                (a[0] == L'-' && !(a[1] >= L'0' && a[1] <= L'9'))) {
                in_code = false;   /* terminator: fall through and classify it */
            } else {
                continue;          /* PostScript code operand, not a file */
            }
        }

        if (a[0] == L'@') {        /* @file: Ghostscript reads this argument file */
            prefix_len[i] = 1;
            continue;
        }
        if (a[0] != L'-') {        /* a bare token is an input file */
            prefix_len[i] = 0;
            continue;
        }

        /* a[0] == '-': a switch. */
        if (a[1] == L'-' && a[2] == 0) {
            sep = a[1];                                /* "--": next token is a file */
        } else if (a[1] == L'+' || a[1] == L'@') {
            sep = a[1];                                /* "-+"/"-@": next token is a file */
        } else if (a[1] == L'c') {
            in_code = true;                            /* "-c": code follows (any attached
                                                        * text is discarded by imainarg.c) */
        } else if ((a[1] == L'f' || a[1] == L'F') && a[2] != 0) {
            prefix_len[i] = 2;                         /* "-ffile"/"-Ffile" */
        } else if ((a[1] == L'I' || a[1] == L'o') && a[2] == 0) {
            skip_next = true;                          /* "-I"/"-o" take the next token */
        }
        /* every other switch is self-contained and consumes no following token */
    }
}


/*
 * Look through the standard Ghostscript cmd-line arguments looking for paths.
 * Expand relative and normalize all paths.  This is required as at the time of
 * writing, any open file request in the Google Chrome sandbox must be an absolute
 * path.  This function will pass back a new **argv instance any any expanded
 * paths will be malloced buffers.
 *
 * Every input file that exists relative to the current working directory is
 * made absolute; input files are identified via ClassifyInputArgs(), which
 * handles multiple files, -f/-F, "--" and @-argument forms, and leaves
 * non-file trailing tokens untouched.  A classified input token that does not
 * exist relative to CWD is also left untouched; see ResolveExistingFullPath()
 * for why.
 *
 * Runs in BOTH processes: in the broker, where the result decides the
 * whitelist, and again in the target, where it only tidies the argument list
 * Ghostscript is handed.  Only the broker can report a failure usefully, hence
 * report_failures.
 *
 * FIXME: What about other paths such as defining a custom libary files or fonts?
 */
static wchar_t ** ExpandPathsInArgs(int argc, wchar_t *argv[], BOOL report_failures) {
    int *input_prefix = (int *) calloc(argc, sizeof(int));
    if (input_prefix == NULL) {
        fprintf(stderr, "Ghost Trap: Failed to initialise due to malloc failure\n");
        return argv;
    }
    ClassifyInputArgs(argc, argv, input_prefix);
    wchar_t **full_path_argv = (wchar_t **) calloc(argc, sizeof(full_path_argv[0]));

    if (full_path_argv == NULL) {
        fprintf(stderr, "Ghost Trap: Failed to initialise due to malloc failure\n");
        free(input_prefix);
        return argv;
    }

    wchar_t new_arg[MAX_PATH + 20];

    for (int i = 0; i < argc; ++i) {
        wchar_t *current_argv = argv[i];

        full_path_argv[i] = current_argv;
        // Convert relative paths to absolute if found.  Skipped for a classified
        // input: PARAM_OUTPUT_FILE is unanchored, so the input branch owns that slot.
        wchar_t *p;
        if (input_prefix[i] < 0 && (p = wcsstr(argv[i], PARAM_OUTPUT_FILE)) != NULL) {
            p += wcslen(PARAM_OUTPUT_FILE);

            wchar_t full_path[MAX_PATH];

            // A bare "> 0" also accepts the buffer-too-small case, where the
            // documented return is the required size rather than the length
            // written.
            DWORD full_path_len = GetFullPathName(p, MAX_PATH, full_path, NULL);

            if (full_path_len > 0 && full_path_len < MAX_PATH) {

                full_path_argv[i] = (wchar_t *) malloc(sizeof(new_arg));
                _snwprintf(full_path_argv[i], sizeof(new_arg), L"-sOutputFile=%s", full_path);
            }
        }

        // Make each input file absolute: every token ClassifyInputArgs() marked
        // as an input file is expanded, and any switch prefix ("-f", "-F", "@") is
        // preserved ahead of the path.  Non-file tokens are left untouched.
        if (input_prefix[i] >= 0) {
            int prefix = input_prefix[i];
            wchar_t input_full_path[MAX_PATH];
            BOOL resolve_failed = FALSE;

            if (ResolveExistingFullPath(argv[i] + prefix, input_full_path, MAX_PATH, &resolve_failed)) {
                size_t nelem = (size_t) prefix + MAX_PATH;
                wchar_t *rewritten = (wchar_t *) malloc(nelem * sizeof(wchar_t));
                if (rewritten != NULL) {
                    _snwprintf(rewritten, nelem, L"%.*s%s", prefix, argv[i], input_full_path);
                    full_path_argv[i] = rewritten;
                } else {
                    fprintf(stderr, "Ghost Trap: Failed to initialise due to malloc failure\n");
                }
            } else if (resolve_failed && report_failures) {
                // Broker only.  Inside the sandbox this same code sees
                // ERROR_ACCESS_DENIED for every path the policy did not
                // whitelist - the ordinary answer there, not a fault - so the
                // target would warn about inputs that resolve perfectly well
                // on Ghostscript's library search path.
                fprintf(stderr, "Ghost Trap: Unable to resolve the input file path for argument %d; it will not be readable.\n", i);
            }
        }
    }

    free(input_prefix);
    return full_path_argv;
}


/*
 * This function is passed into RunConsoleAppInSandbox and is called to apply the sandbox's
 * access policies.  
 * 
 * IMPORTANT: This code does not run in the sandbox (runs in the parent process). Take care!
 */
static void ApplyPolicy(sandbox::TargetPolicy &policy, int argc, wchar_t* argv[]) {
    // Fix up and expand paths in the args
    wchar_t **nargv = ExpandPathsInArgs(argc, argv, TRUE);

    /*
     * These items need to be white listed:
     *   - READ access on the directories on GS_LIB.
     *   - READ access to Ghostscript specific registry keys.
     *   - FULL (write) access to target file output directory.
     *   - FULL (write) access to the user's temp directory.
     *   - READ access to the Windows Font directory.
     */

    // The parent needs to load the DLL to get version information so we
    // can calculate paths (e.g. product and version number).
    if (!LoadGSDLL(&global_gsdll)) {
        fprintf(stderr, "Ghost Trap: Parent unable to load GS DLL\n");
        return;
    }

    gsapi_revision_t rv;
    if (global_gsdll.revision(&rv, sizeof(rv)) != 0) {
        fprintf(stderr, "Ghost Trap: Unable to identify Ghostscript DLL revision\n");
        return;
    }

    // *******************************************************************************************************
    // Registry sandboxing has been removed from the Chromium sandbox. Commenting out the code for prosterity.
    // policy.AddRule(
    //    sandbox::TargetPolicy::SUBSYS_REGISTRY,
    //    sandbox::TargetPolicy::REG_ALLOW_READONLY, 
    //    L"HKEY_CURRENT_USER"
    //    );

    // Allow READ access to OS keys (e.g. Locale lookup) 
    // policy.AddRule(
    //    sandbox::TargetPolicy::SUBSYS_REGISTRY, 
    //    sandbox::TargetPolicy::REG_ALLOW_READONLY, 
    //    L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet"
    //    );

    // std::wstringstream  white_list_path;
    // white_list_path <<  L"HKEY_LOCAL_MACHINE\\SOFTWARE\\" << rv.product << L"\\*";

    // Allow READ access to Ghostscript spacific registry keys.
    // policy.AddRule(
    //    sandbox::TargetPolicy::SUBSYS_REGISTRY, 
    //    sandbox::TargetPolicy::REG_ALLOW_READONLY, 
    //    white_list_path.str().c_str()
    //    );
    // *******************************************************************************************************

    // Read GS_LIB and whitelist path - as per dwdll.c
    wchar_t gs_key[256];
    wchar_t dotversion[16];
    _snwprintf(dotversion, 16, L"%d.%02d", 
                        (int)(rv.revision / 100),
                        (int)(rv.revision % 100));
    _snwprintf(gs_key, 256, L"SOFTWARE\\%hs\\%s", 
                        rv.product, 
                        dotversion);

    // Allow READ access to directory on lib path. Find this by looking
    // up the registry key HKLM/Software/GPL Ghostscript/9.02/GS_LIB.
    // The value is stored is a ";" seperated path.
    HKEY hKey;
    DWORD result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, gs_key, 0, KEY_READ, &hKey);
    if (ERROR_SUCCESS == result) {
        std::wstring gs_lib_path;
        GetStringRegKey(hKey, L"GS_LIB", gs_lib_path, L"");

        wchar_t* part = wcstok(&gs_lib_path[0], L";", 0);
        while(part != NULL) {
            wchar_t lib_path[MAX_PATH];
            _snwprintf(lib_path, MAX_PATH, L"%s\\*", part);

            // Whitelist the LIB dir.
            (void) policy.GetConfig()->AllowFileAccess(
                sandbox::FileSemantics::kAllowReadonly,
                lib_path
            );

            part = wcstok(NULL, L";", 0);
        }

        RegCloseKey(hKey);
    } 

    // Allow READ and WRITE access to default temp directory.
    {
        wchar_t temp_dir[MAX_PATH];
        GetTempPath(MAX_PATH - 1, temp_dir);
        wchar_t dir_rule[MAX_PATH];
        _snwprintf(dir_rule, MAX_PATH - 1, L"%s*", temp_dir);
        (void) policy.GetConfig()->AllowFileAccess(
            sandbox::FileSemantics::kAllowAny,
            dir_rule
        );
    }


    // Allow READ access to C:\Windows\Fonts directory.
    {
        wchar_t win_dir[MAX_PATH];
        GetWindowsDirectory(win_dir, MAX_PATH - 1);
        wchar_t dir_rule[MAX_PATH];
        _snwprintf(dir_rule, MAX_PATH - 1, L"%s\\Fonts\\*", win_dir);
        (void) policy.GetConfig()->AllowFileAccess(
            sandbox::FileSemantics::kAllowReadonly,
            dir_rule
        );
    }

    // Allow WRITE access to OutputFile target directory
    BOOL has_outfile = FALSE;
    BOOL test_enabled = FALSE;
    BOOL help_requested = FALSE;
    int i;
    for (i = 0; i < argc; ++i) {
        wchar_t *p;

        if (wcscmp(argv[i], L"-h") == 0) {
            printf("Ghost Trap: GPL Ghostscript running in the Google Chromium Sandbox.\n");
            printf("Ghost Trap: Version %s\n", GHOST_TRAP_VERSION);
            printf("Ghost Trap: %s\n", GHOST_TRAP_COPYRIGHT);
            printf("\n");
            help_requested = TRUE;
            continue;
        }

        if ((p = wcsstr(nargv[i], PARAM_OUTPUT_FILE)) != NULL) {
            p += wcslen(PARAM_OUTPUT_FILE);
            AllowAccessToFile(policy, p, TRUE);
            has_outfile = TRUE;
        }

        if ((p = wcsstr(nargv[i], L"--test-sandbox")) != NULL) {
            test_enabled = TRUE;
        }

        // Sandbox Testing - whitelist test data
        if ((p = wcsstr(nargv[i], PARAM_FAIL_TEST)) != NULL) {
            p += wcslen(PARAM_FAIL_TEST);

            /* Setup Test 1 for failure
             * Allow write access to C:\Windows\Temp folder
             */
            if(wcscmp(p, L"1") == 0 && test_enabled) {
                wchar_t win_dir[MAX_PATH];
                GetWindowsDirectory(win_dir, MAX_PATH - 1);
                wchar_t dir_rule[MAX_PATH];
                _snwprintf(dir_rule, MAX_PATH - 1, L"%s\\Temp\\*", win_dir);
                (void) policy.GetConfig()->AllowFileAccess(
                    sandbox::FileSemantics::kAllowAny,
                    dir_rule
                );
            }

            /* Setup Test 2 for failure
             * Allow read-only access to C:\Windows\notepad.exe
             */
            if(wcscmp(p, L"2") == 0 && test_enabled) {
                wchar_t win_dir[MAX_PATH];
                GetWindowsDirectory(win_dir, MAX_PATH - 1);
                wchar_t dir_rule[MAX_PATH];
                _snwprintf(dir_rule, MAX_PATH - 1, L"%s\\notepad.exe", win_dir);
                (void) policy.GetConfig()->AllowFileAccess(
                    sandbox::FileSemantics::kAllowReadonly,
                    dir_rule
                );
            }

            /* Setup Test 3 for failure
             * Allow read access to registry key HKCU\Environment
             * 
             * Note: Registry sandboxing was removed from the Chromium sandbox. Commenting code for prosterity.
             */
            // if(wcscmp(p, L"3") == 0 && test_enabled) {
            //     policy.AddRule(
            //         sandbox::TargetPolicy::SUBSYS_REGISTRY, 
            //         sandbox::TargetPolicy::REG_ALLOW_READONLY, 
            //         L"HKEY_CURRENT_USER\\Environment"
            //     );
            // }
        }
    }

    // If no OutputFile, add READ/WRITE access to current working directory?
    if (!has_outfile && !test_enabled && !help_requested) {
        fprintf(stderr, "Ghost Trap: An OutputFile with an absolute path is required.\n");
    }

    // Add access to every input file.  Input files are identified with the same
    // classifier used to expand them - there may be several, and they may be
    // given via -f/-F, "--" or @ forms.  The access granted is whatever
    // AllowAccessToFile() grants; nargv already holds the absolute path, so step
    // past any preserved switch prefix.
    //
    // ClassifyInputArgs() is lexical only, so re-running it here is safe.
    // Whether a token was rewritten is read off nargv, not re-derived with a
    // second filesystem query.  Both clauses below are needed:
    // input_prefix[i] >= 0 excludes tokens the OutputFile branch rewrote.
    {
        int *input_prefix = (int *) calloc(argc, sizeof(int));
        if (input_prefix != NULL) {
            ClassifyInputArgs(argc, argv, input_prefix);
            for (i = 0; i < argc; ++i) {
                if (input_prefix[i] >= 0 && nargv[i] != argv[i]) {
                    AllowAccessToFile(policy, nargv[i] + input_prefix[i], FALSE);
                }
            }
            free(input_prefix);
        } else {
            fprintf(stderr, "Ghost Trap: Failed to initialise due to malloc failure\n");
        }
    }
}


/*
 * For convendience, we load the DLL in the child before we sandbox. At this point we're
 * not running any user supplied code so this should be a safe operation.
 *
 * IMPORTANT: This code does not run in the sandbox. Take care when adding new code!
 */
static int PreSandboxedInit(int argc, wchar_t* argv[]) {
    if (!LoadGSDLL(&global_gsdll)) {
         return 1;
    }
    return 0;
}


/*
 * Convert wchar to unicode.  Similar to function in gp_wutf8.c in Ghostscript,
 * however without the buffer length calculation problem on generation.
 */
static int wchar_to_utf8(char *out, const wchar_t *in) {
    unsigned int i;
    unsigned int len = 1;

    if (out) {
        while ((i = (unsigned int)*in++)) {
            if (i < 0x80) {
                *out++ = (char)i;
                len++;
            } else if (i < 0x800) {
                *out++ = 0xC0 | ( i>> 6        );
                *out++ = 0x80 | ( i      & 0x3F);
                len += 2;
            } else /* if (i < 0x10000) */ {
                *out++ = 0xE0 | ( i>>12        );
                *out++ = 0x80 | ((i>> 6) & 0x3F);
                *out++ = 0x80 | ( i      & 0x3F);
                len += 3;
            }
        }
        *out = 0;
    } else {
        while ((i = (unsigned int)*in++)) {
            if (i < 0x80) {
                len++;
            } else if (i < 0x800) {
                len += 2;
            } else /* if (i < 0x10000) */ {
                len += 3;
            }
        }
    }
    return len;
}

/*
 * This is an internal method to do a few simple checks to make sure our sandbox is working.  
 * The build script will use this to verify that everything is working as expected.
 * A non-zero exit code indicates a possible error that should be looked at.
 *
 * For the moment our tests are:
 *    1 - Check that we can't write a file we should not - i.e. file in c:\Windows\Temp
 *    2 - Check that we can't read files from the general os filesystem (c:\Windows\notepad.exe)
 *    3 - Check that we can't read general registry entries (HKEY_CURRENT_USER\\Environment)
 *
 * The tests can be setup to fail (whitelisted parameters) by adding the --fail-test flag.
 * e.g. 
 *    --fail-test=1 [will fail test 1]
 *    --fail-test=2 [will fail test 2]
 */
static int TestSandbox() {
    /*
     * If C:\Windows\Temp\test.txt is whitelisted via -sOutputFile or the last param
     * then this will fail
    */
    std::ofstream output("C:\\Windows\\Temp\\test.txt");
    if (output.is_open()) {        
        // We shouldn't be able to read this file
        return 61;
    }

    /* 
     * Test 2
     * If C:\Windows\\notepad.exe is the last parameter, this will succeed as we 
     * are whitelisting the input file.
    */ 
    std::ifstream input("C:\\Windows\\notepad.exe", std::ios::binary);
    if (input.is_open()) {
        // Oh... we can read it! Oh no.... we can read something we were not expecting!!!
        return 62;
    }

    /*
     * Test 3
     * Check that we can't read the registry
     */
    HKEY hKey;
    DWORD Ret;
    wchar_t skey[256] = L"Environment";

    Ret = RegOpenKeyEx(HKEY_CURRENT_USER, skey, 0, KEY_READ, &hKey);
    if (Ret == ERROR_SUCCESS) {
        return 63;
    }

    return 0;
}

/*
 * The main method (sandboxed). Here we do the heavy lifting in the sandbox.  i.e.
 * We hand the hard work off to the Ghostscript DLL :-)
 */
static int SandboxedMain(int argc, wchar_t* argv[]) {
    // If -h, print out Ghost Trap information as well.
    for (int i = 0; i < argc; ++i) {
        // Used for developer testing only (not documented in usage)
        if (wcscmp(argv[i], L"--test-sandbox") == 0) {
            return TestSandbox();
        }
    }

    wchar_t **full_path_argv = ExpandPathsInArgs(argc, argv, FALSE);
  
    /* Duplicate wide args as utf8 */
    char **nargv;
    int nargc = 0;
    int i;

    nargv = (char **) calloc(argc + 1, sizeof(char *));
    if (nargv == NULL) {
        goto error;
    }

    for (i = 0; i < argc; ++i) {

        // GhostTrap is a secured version, so it's only appropriate to ensure 
        // -dSAFER is always on by default. Always append (after arg 0).
        if (i == 1) {
            nargv[nargc] = _strdup("-dSAFER");
            ++nargc;
        }

        wchar_t *current_argv = full_path_argv[i];

        nargv[nargc] = (char *) malloc(wchar_to_utf8(NULL, current_argv));
        if (nargv[nargc] == NULL) {
            goto error;
        }
        wchar_to_utf8(nargv[nargc], current_argv);
        ++nargc;
    }

    // Run GS code (via DLL)
    int code;
    int code1;

    code = global_gsdll.new_instance(&global_gsinstance, NULL);
    if (code < 0) {
        return 22;
    }

    code = global_gsdll.init_with_args(global_gsinstance, nargc, nargv);

    code1 = global_gsdll.exit(global_gsinstance);

    if ((code == 0) || (code == gs_error_Quit)) {
        code = code1;
    }

    if (0) {
error:
        fprintf(stderr, "Ghost Trap: Failed to initialise due to malloc failure\n");
        code = -1;
    }

    if (nargv) {
        for (i = 0; i < argc; ++i) {
            free(nargv[i]);
        }
        free(nargv);
    }

    if (!((code == 0) || (code == gs_error_Quit))) {
        return abs(code);
    }
    return 0;
}


/*
 * The main() function. We simply set up and pass execution across to sandbox_procmgmt.
 */
int wmain(int argc, wchar_t* argv[]) {
    return RunConsoleAppInSandbox(ApplyPolicy, PreSandboxedInit, SandboxedMain, argc, argv);
}
