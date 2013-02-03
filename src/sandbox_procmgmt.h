/*
 * Copyright (c) 2012-2013 PaperCut Software International Pty. Ltd.
 * http://www.papercut.com/
 *
 * Author: Chris Dance <chris.dance@papercut.com>
 *
 * $Id: $
 *
 * Dual License: GPL or MIT
 *
 * ----
 * Library code to help wrap a Windows command-line (console) application is the
 * Google Chromium Sandbox.  This code was originally develop for the Ghost Trap
 * open source project, however may also serve as a useful library for sandboxing 
 * any console application.
 * 
 * This sandboxing library wraps the Google Chromium Sandbox sub-project and
 * provides the required infastructure required to support most Windows console
 * applications.  The sandbox works by applying a policy in a parent process,
 * then exec'ing a child to run restricted code.  Communication between the parent
 * and child to facilitate the trasfer of STDIN/STDOUT/STDERR, is via named pipes.
 *
 * The general approch for wrapping an console application would be as follows:
 *
 *   1) Rename your wmain method to sandboxed_wmain.
 * 
 *   2) Add a new wmain method simply wrapping RunConsoleAppInSandbox as follows:
 *
 *        int wmain(int argc, wchar_t* argv[]) {
 *            return RunConsoleAppInSandbox(
 *                                      ApplyPolicy, 
 *                                      NULL, 
 *                                      SandboxedMain, 
 *                                      argc, argv);
 *        }
 *
 *   3) Define an ApplyPolicy function white-listing any files/resources your 
 *      application can safely access.  Using procmon.exe from SysInternals
 *      can help here.  
 * 
 * Optional: If your application needs to run code in the child process before
 *           full sandboxing takes place, define a pre_sandbox_init function.
 *
 * An Example:
 *
 *    #include "sandbox_procmgmt.h"
 *
 *    int sandboxed_main(int argc, wchar_t* argv[]) {
 *        char buffer[10];
 *
 *        printf("Exploit my buffer: ");
 *        scanf("%s", buffer);
 * 
 *        return 0;
 *    }
 *
 *    int wmain(int argc, wchar_t* argv[]) {
 *        return RunConsoleAppInSandbox(NULL, NULL, sandboxed_main, argc, argv);
 *    }   
 *   
 * 
 */

#include <sandbox/win/src/sandbox.h>

typedef void (*SandboxPolicyFn)(sandbox::TargetPolicy *policy, int argc, wchar_t* argv[]);
typedef int (*ConsoleWMainFn)(int argc, wchar_t* argv[]);

int RunConsoleAppInSandbox(SandboxPolicyFn policy_provider, 
                           ConsoleWMainFn pre_sandbox_init, 
                           ConsoleWMainFn sandboxed_wmain, 
                           int argc, wchar_t* argv[]);
