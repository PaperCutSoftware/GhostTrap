/*
 * Copyright (c) 2012-2019 PaperCut Software International Pty. Ltd.
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
#define GHOST_TRAP_VERSION      "1.3.9.27"
#define GHOST_TRAP_COPYRIGHT    "Copyright (c) 2012-2019 PaperCut Software International Pty. Ltd."

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
static void AllowAccessToFile(scoped_refptr<sandbox::TargetPolicy> policy, wchar_t *file, BOOL parent_dir) {

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

        policy->AddRule(
            sandbox::TargetPolicy::SUBSYS_FILES, 
            sandbox::TargetPolicy::FILES_ALLOW_ANY, 
            path_rule
            );
    }
}


/*
 * Look through the standard Ghostscript cmd-line arguments looking for paths.
 * Expand relative and normalize all paths.  This is required as at the time of 
 * writing, any open file request in the Google Chrome sandbox must be an absolute
 * path.  This function will pass back a new **argv instance any any expanded
 * paths will be malloced buffers.
 * 
 * TODO: The input file argument is assumed to be the last argument. This is not
 * necessarily so via the -f option.
 *
 * FIXME: What about other paths such as defining a custom libary files or fonts?
 */
static wchar_t ** ExpandPathsInArgs(int argc, wchar_t *argv[]) {
    wchar_t **full_path_argv = (wchar_t **) calloc(argc, sizeof(full_path_argv[0]));

    if (full_path_argv == NULL) {
        fprintf(stderr, "Ghost Trap: Failed to initialise due to malloc failure\n");
        return argv;
    }

    wchar_t new_arg[MAX_PATH + 20];

    for (int i = 0; i < argc; ++i) {
        wchar_t *current_argv = argv[i];

        full_path_argv[i] = current_argv;
        // Convert relative paths to absolute if found.
        wchar_t *p;
        if ((p = wcsstr(argv[i], L"OutputFile=")) != NULL) {
            p += 11;

            wchar_t full_path[MAX_PATH];

            if (GetFullPathName(p, MAX_PATH, full_path, NULL) > 0) {

                full_path_argv[i] = (wchar_t *) malloc(sizeof(new_arg));
                _snwprintf(full_path_argv[i], sizeof(new_arg), L"-sOutputFile=%s", full_path);
            }
        }

        // Make the input file absolute if required. We assume this is the last argument.
        // FIXME: Assumption not quite correct with -f option.
        if ((i + 1) == argc) {
            wchar_t *last_arg = argv[argc - 1];
            if (*last_arg != L'-') {

                if (GetFullPathName(last_arg, MAX_PATH, new_arg, NULL) > 0) {

                    full_path_argv[i] = (wchar_t *) malloc(sizeof(new_arg));
                    _snwprintf(full_path_argv[i], sizeof(new_arg), L"%s", new_arg);

                }
            }
        }
    }

    return full_path_argv;
}


/*
 * This function is passed into RunConsoleAppInSandbox an is called to apply the sandbox's
 * access policies.  
 * 
 * IMPORTANT: This code does not run in the sandbox (runs in the parent process). Take care!
 */
static void ApplyPolicy(scoped_refptr<sandbox::TargetPolicy> policy, int argc, wchar_t* argv[]) {
    // Fix up and expand paths in the args
    wchar_t **nargv = ExpandPathsInArgs(argc, argv);

    /*
     * These items need to be white listed:
     *   - READ access on the directories on GS_LIB.
     *   - READ access to Ghostscript spacific registry keys.
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

    policy->AddRule(
        sandbox::TargetPolicy::SUBSYS_REGISTRY,
        sandbox::TargetPolicy::REG_ALLOW_READONLY, 
        L"HKEY_CURRENT_USER"
        );

    // Allow READ access to OS keys (e.g. Locale lookup) 
    policy->AddRule(
        sandbox::TargetPolicy::SUBSYS_REGISTRY, 
        sandbox::TargetPolicy::REG_ALLOW_READONLY, 
        L"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet"
        );

    std::wstringstream  white_list_path;
    white_list_path <<  L"HKEY_LOCAL_MACHINE\\SOFTWARE\\" << rv.product << L"\\*";

    // Allow READ access to Ghostscript spacific registry keys.
    policy->AddRule(
        sandbox::TargetPolicy::SUBSYS_REGISTRY, 
        sandbox::TargetPolicy::REG_ALLOW_READONLY, 
        white_list_path.str().c_str()
        );

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

        wchar_t* part = wcstok(&gs_lib_path[0], L";");
        while(part != NULL) {
            wchar_t lib_path[MAX_PATH];
            _snwprintf(lib_path, MAX_PATH, L"%s\\*", part);

            // Whitelist the LIB dir.
            policy->AddRule(
                sandbox::TargetPolicy::SUBSYS_FILES, 
                sandbox::TargetPolicy::FILES_ALLOW_READONLY, 
                lib_path
                );

            part = wcstok(NULL, L";");
        }

        RegCloseKey(hKey);
    } 

    // Allow READ and WRITE access to default temp directory.
    {
        wchar_t temp_dir[MAX_PATH];
        GetTempPath(MAX_PATH - 1, temp_dir);
        wchar_t dir_rule[MAX_PATH];
        _snwprintf(dir_rule, MAX_PATH - 1, L"%s*", temp_dir);
        policy->AddRule(
            sandbox::TargetPolicy::SUBSYS_FILES, 
            sandbox::TargetPolicy::FILES_ALLOW_ANY, 
            dir_rule
            );
    }


    // Allow READ access to C:\Windows\Fonts directory.
    {
        wchar_t win_dir[MAX_PATH];
        GetWindowsDirectory(win_dir, MAX_PATH - 1);
        wchar_t dir_rule[MAX_PATH];
        _snwprintf(dir_rule, MAX_PATH - 1, L"%s\\Fonts\\*", win_dir);
        policy->AddRule(
            sandbox::TargetPolicy::SUBSYS_FILES, 
            sandbox::TargetPolicy::FILES_ALLOW_READONLY, 
            dir_rule
            );
    }

    // Allow WRITE access to OutputFile target directory
    BOOL has_outfile = FALSE;
    BOOL test_enabled = FALSE;
    int i;
    for (i = 0; i < argc; ++i) {
        wchar_t *p;

        if ((p = wcsstr(nargv[i], L"OutputFile=")) != NULL) {
            p += 11;
            AllowAccessToFile(policy, p, TRUE);
            has_outfile = TRUE;
        }

        if ((p = wcsstr(nargv[i], L"--test-sandbox")) != NULL) {
            test_enabled = TRUE;
        }

        // Sandbox Testing - whitelist test data
        if ((p = wcsstr(nargv[i], L"--fail-test=")) != NULL) {
            p += 9;

            /* Setup Test 1 for failure
             * Allow write access to C:\Windows\Temp folder
             */
            if(wcscmp(p, L"1") == 0 && test_enabled) {
                wchar_t win_dir[MAX_PATH];
                GetWindowsDirectory(win_dir, MAX_PATH - 1);
                wchar_t dir_rule[MAX_PATH];
                _snwprintf(dir_rule, MAX_PATH - 1, L"%s\\Temp\\*", win_dir);
                policy->AddRule(
                    sandbox::TargetPolicy::SUBSYS_FILES, 
                    sandbox::TargetPolicy::FILES_ALLOW_ANY, 
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
                policy->AddRule(
                    sandbox::TargetPolicy::SUBSYS_FILES, 
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY, 
                    dir_rule
                );
            }

            /* Setup Test 3 for failure
             * Allow read access to registry key HKCU\Environment
             */
            if(wcscmp(p, L"3") == 0 && test_enabled) {
                // 
                policy->AddRule(
                    sandbox::TargetPolicy::SUBSYS_REGISTRY, 
                    sandbox::TargetPolicy::REG_ALLOW_READONLY, 
                    L"HKEY_CURRENT_USER\\Environment"
                );
            }
        }
    }

    // If no OutputFile, add READ/WRITE access to current working directory?
    if (!has_outfile) {
        fprintf(stderr, "Ghost Trap: An OutputFile with an absolute path is required.\n");
    }

    // Add READ/WRITE access to the provided input file (assume this will be the last arg).
    wchar_t *last_arg = nargv[argc - 1];
    if (*last_arg != L'-') {
       AllowAccessToFile(policy, last_arg, FALSE);
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
        if (wcscmp(argv[i], L"-h") == 0) {
            printf("Ghost Trap: GPL Ghostscript running in with the Google Chromium Sandbox.\n");
            printf("Ghost Trap: Version %s\n", GHOST_TRAP_VERSION);
            printf("Ghost Trap: %s\n", GHOST_TRAP_COPYRIGHT);
            printf("\n");
            break;
        }
        // Used for developer testing only (not documented in usage)
        if (wcscmp(argv[i], L"--test-sandbox") == 0) {
            return TestSandbox();
        }
    }

    wchar_t **full_path_argv = ExpandPathsInArgs(argc, argv);
  
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