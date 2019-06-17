/*
 * Copyright (c) 2012-2019 PaperCut Software International Pty. Ltd.
 * http://www.papercut.com/
 *
 * Author: Chris Dance <chris.dance@papercut.com>
 *
 * $Id: $
 *
 * Dual License: GPL or MIT
 *
 * GPL:
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
 * MIT:
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:

 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----
 * Library code to help wrap a Windows command-line (console) application is the
 * Google Chromium Sandbox.  This code was originally develop for the Ghost Trap
 * open source project, however may also serve as a useful library for sandboxing 
 * any console application.
 * 
 * See the header file, or Ghost Trap, for example and instructions.
 */

#include <windows.h>
#include <Aclapi.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

// Chromium Sandbox Header files
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/sandbox_factory.h"

#include "sandbox_procmgmt.h"



const wchar_t *STDOUT_PIPE_NAME = L"\\\\.\\pipe\\sandboxed-app-stdout-%d";
const wchar_t *STDERR_PIPE_NAME = L"\\\\.\\pipe\\sandboxed-app-stderr-%d";
const wchar_t *STDIN_PIPE_NAME  = L"\\\\.\\pipe\\sandboxed-app-stdin-%d";

/*
 * Consume std in/out from parent pipe.
 */
static DWORD ConsumeOutput(HANDLE h_in, FILE *f_out) {

    if (INVALID_HANDLE_VALUE == h_in) {
        return -1;
    }

    const int kSizeBuffer = 512;
    BYTE read_buffer[kSizeBuffer] = {0};

    DWORD last_error = 0;
    while (last_error == ERROR_SUCCESS 
            || last_error == ERROR_PIPE_LISTENING 
            || last_error == ERROR_NO_DATA) {

        DWORD read_data_length;
        if (::ReadFile(h_in,
            read_buffer,
            kSizeBuffer - 1,  // Max read size
            &read_data_length,
            NULL)) {  // Not overlapped

                fwrite(read_buffer, 1, read_data_length, f_out);
                fflush(f_out);
                last_error = ERROR_SUCCESS;
        } else {
            last_error = GetLastError();
            Sleep(100);
        }
    }
    
    ::CloseHandle(h_in);
    
    return 0;
}


/*
 * Thread for consuming the STDOUT provided by the child.
 */
static DWORD WINAPI ConsumeStdOut(void *param) {
    HANDLE stdout_pipe = (HANDLE) param;
    return ConsumeOutput(stdout_pipe, stdout);
}


/*
 * Thread for consuming the STDERR provided by the child.
 */
static DWORD WINAPI ConsumeStdErr(void *param) {
    HANDLE stderr_pipe = (HANDLE) param;
    return ConsumeOutput(stderr_pipe, stderr);
}


/*
 * Thread for providing the parent's STDIN to the child.
 */
static DWORD WINAPI ProvideStdIn(void *param) {
    HANDLE stdin_pipe = (HANDLE) param;

    CHAR read_buff[512];
    DWORD nBytesRead,nBytesWrote;
    // Get input from our console and send it to child through the pipe.
    HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
    while (true) {

        if (!ReadFile(h_stdin, read_buff, sizeof(read_buff), &nBytesRead, NULL)) {
            break;
        }

        if (nBytesRead > 0) {
            if (!WriteFile(stdin_pipe,  read_buff, nBytesRead, &nBytesWrote, NULL)) {
                DWORD last_error = GetLastError();
                if (ERROR_NO_DATA == last_error) {
                    break; // Pipe was closed (normal exit path).
                } else {
                    break;
                }
            }
            FlushFileBuffers(stdin_pipe);
        }
    }

    ::CloseHandle(stdin_pipe);
    return 0;
}


/*
 * The parent (unsandboxed process).  This function intializes the sandbox service broker,
 * sets up the pipes, applies the security policy, and then executes and monitors the child
 * process.
 */
static int RunParent(int argc, wchar_t* argv[], 
                        sandbox::BrokerServices* broker_service, 
                        SandboxPolicyFn policy_provider) {

    DWORD process_id = GetCurrentProcessId();
    sandbox::ResultCode result;
    
    // Start setting up the sandbox.
    if (0 != (result = broker_service->Init())) {
        fprintf(stderr, "Sandbox: Failed to initialize the sandbox - BrokerServices object\n");
        return 50;
    }

    // Apply our policy
    scoped_refptr<sandbox::TargetPolicy> targetPolicy = broker_service->CreatePolicy();

    // By default we'll apply full sandbox.  Your own policy is then applied over this.
    targetPolicy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);
    targetPolicy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS, sandbox::USER_LOCKDOWN);
    targetPolicy->SetAlternateDesktop(true);
    targetPolicy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

    // Apply any additional policy (e.g. a whitelist of files/registry/other) as provided.
    if (policy_provider) {
        policy_provider(targetPolicy, argc, argv);
    }

    PROCESS_INFORMATION pi;
    sandbox::ResultCode warning_result = sandbox::SBOX_ALL_OK;
    DWORD last_error = ERROR_SUCCESS;

    {
        wchar_t *orig_args = GetCommandLineW();
        int arg_max_len = wcslen(orig_args) + 50;
        wchar_t *args_plus_id = new wchar_t[arg_max_len];
        swprintf(args_plus_id, arg_max_len, L"%s %d", orig_args, process_id);
        args_plus_id[arg_max_len - 1] = L'\0';

        result = broker_service->SpawnTarget(argv[0], args_plus_id, targetPolicy, &warning_result, &last_error, &pi);
        delete[] args_plus_id;
    }
    
    targetPolicy = NULL;

    if (sandbox::SBOX_ALL_OK != result) {
        fprintf(stderr, "Sandbox: Failed to launch with the following result: %d\n", result);
        return 51;
    }

    // Setup pipe and consume STDOUT
    HANDLE stdout_thread;
    {
        wchar_t pipe_path[MAX_PATH];
        swprintf(pipe_path, MAX_PATH - 1, STDOUT_PIPE_NAME, process_id);

        HANDLE stdout_pipe = ::CreateNamedPipe(pipe_path,
                                                PIPE_ACCESS_INBOUND | WRITE_DAC,
                                                PIPE_TYPE_BYTE | PIPE_WAIT,
                                                1,  // Number of instances.
                                                512,  // Out buffer size.
                                                512,  // In buffer size.
                                                NMPWAIT_USE_DEFAULT_WAIT,
                                                NULL);
        // Set the security on 
        if (!sandbox::AddKnownSidToObject(stdout_pipe, SE_KERNEL_OBJECT, 
                                        WinWorldSid, 
                                        GRANT_ACCESS, FILE_ALL_ACCESS)) {
            fprintf(stderr, "Sandbox: Failed to set security on stdout pipe.\n");
            return 52;
        }

        DWORD thread_id;
        stdout_thread = ::CreateThread(NULL,  // Default security attributes
                        NULL,  // Default stack size
                        &ConsumeStdOut,
                        stdout_pipe,
                        0,  // No flags
                        &thread_id);
    }

    // Setup pipe and consume STDERR
    HANDLE stderr_thread;
    {
        wchar_t pipe_path[MAX_PATH];
        swprintf(pipe_path, MAX_PATH - 1, STDERR_PIPE_NAME, process_id);

        HANDLE stderr_pipe = ::CreateNamedPipe(pipe_path,
                                                PIPE_ACCESS_INBOUND | WRITE_DAC,
                                                PIPE_TYPE_BYTE | PIPE_WAIT,
                                                1,  // Number of instances.
                                                512,  // Out buffer size.
                                                512,  // In buffer size.
                                                NMPWAIT_USE_DEFAULT_WAIT,
                                                NULL);

        if (!sandbox::AddKnownSidToObject(stderr_pipe, SE_KERNEL_OBJECT, 
                                        WinCreatorOwnerSid, 
                                        GRANT_ACCESS, FILE_ALL_ACCESS)) {            
            fprintf(stderr, "Sandbox: Failed to set security on stderr pipe.\n");
            return 52;
        }

        DWORD thread_id;
        stderr_thread = ::CreateThread(NULL,  // Default security attributes
                        NULL,  // Default stack size
                        &ConsumeStdErr,
                        stderr_pipe,
                        0,  // No flags
                        &thread_id);
    }

    // Setup and provide STDIN
    wchar_t pipe_path[MAX_PATH];
    swprintf(pipe_path, MAX_PATH - 1, STDIN_PIPE_NAME, process_id);
    HANDLE stdin_pipe = ::CreateNamedPipe(pipe_path,
                                            PIPE_ACCESS_OUTBOUND | WRITE_DAC,
                                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                            1,  // Number of instances.
                                            512,  // Out buffer size.
                                            512,  // In buffer size.
                                            NMPWAIT_USE_DEFAULT_WAIT,
                                            NULL);

    if (!sandbox::AddKnownSidToObject(stdin_pipe, SE_KERNEL_OBJECT, 
                                        WinCreatorOwnerSid, 
                                        GRANT_ACCESS, FILE_ALL_ACCESS)) {    
        fprintf(stderr, "Sandbox: Failed to set security on stdin pipe.\n");
        return 52;
    }

    // Push STDIN
    DWORD thread_id;
    ::CreateThread(NULL,  // Default security attributes
        NULL,  // Default stack size
        &ProvideStdIn,
        stdin_pipe,
        0,  // No flags
        &thread_id);
    
    // All pipes are ready.  We can now resume the sandboxed child's execution.
    ::ResumeThread(pi.hThread);

    ::WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    if (!::GetExitCodeProcess(pi.hProcess, &exit_code)) {
        exit_code = 0xFFFF;  // Default exit code
    }

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    broker_service->WaitForAllTargets();

    // Wait for BOTH our consuming std(out|err) threads to finish.
    WaitForSingleObject(stdout_thread, 1000);
    WaitForSingleObject(stderr_thread, 1000);

    return exit_code;
}


/*
 * Fix standard stream as outlined here: http://support.microsoft.com/kb/105305
 */
static void ReattachStreamToPipe(FILE *stream, DWORD handle, char *mode) {

    int hCrt = _open_osfhandle(
             (long) GetStdHandle(handle),
             _O_BINARY
          );
    FILE *fpipe = _fdopen(hCrt, mode);
    *stream = *fpipe;
    _setmode(_fileno(stream), _O_BINARY);
    setvbuf(stream, NULL, _IONBF, 0 );
}


/*
 * The child process runs the sandboxed code. It attaches to the pipes created
 * by the parent to consume STDIN and provide STDOUT. The child process also
 * receives and extra argv argument which is used to determine its parent's
 * named pipes.
 */
int RunChild(ConsoleWMainFn pre_sandbox_init, 
                ConsoleWMainFn sandboxed_wmain,
                int argc, wchar_t* argv[]) {

    // ##### UNSAFE initialization code here, sandbox isn't active yet ######
    
    // The last argument on argv will be our parent process ID.  We use this to 
    // determine the path to our named pipes.
    int parent_id = _wtoi(argv[argc - 1]);

    // First we wire up our in/out to parent pipes.
    wchar_t stdin_pipe_path[MAX_PATH];
    swprintf(stdin_pipe_path, MAX_PATH - 1, STDIN_PIPE_NAME, parent_id);
    HANDLE stdin_pipe = ::CreateFile(stdin_pipe_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,  // Default security attributes.
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);  // No template

    if (INVALID_HANDLE_VALUE == stdin_pipe) {
        return 50;
    }
    ::SetStdHandle(STD_INPUT_HANDLE, stdin_pipe);

    wchar_t stdout_pipe_path[MAX_PATH];
    swprintf(stdout_pipe_path, MAX_PATH - 1, STDOUT_PIPE_NAME, parent_id);
    HANDLE stdout_pipe = ::CreateFile(stdout_pipe_path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,  // Default security attributes.
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);  // No template

    if (INVALID_HANDLE_VALUE == stdout_pipe) {
        return 50;
    }
    ::SetStdHandle(STD_OUTPUT_HANDLE, stdout_pipe);

    wchar_t stderr_pipe_path[MAX_PATH];
    swprintf(stderr_pipe_path, MAX_PATH - 1, STDERR_PIPE_NAME, parent_id);
    HANDLE stderr_pipe = ::CreateFile(stderr_pipe_path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,  // Default security attributes.
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);  // No template

    if (INVALID_HANDLE_VALUE == stderr_pipe) {
        return 50;
    }
    ::SetStdHandle(STD_ERROR_HANDLE, stderr_pipe);

    // Fix up C runtime output to console as per: http://support.microsoft.com/kb/105305
    ReattachStreamToPipe(stdin, STD_INPUT_HANDLE, (char*)"r");
    ReattachStreamToPipe(stdout, STD_OUTPUT_HANDLE, (char*)"w");
    ReattachStreamToPipe(stderr, STD_ERROR_HANDLE, (char*)"w");

    // Init Sandbox

    sandbox::TargetServices* target_service = sandbox::SandboxFactory::GetTargetServices();
    if (NULL == target_service) {
        fprintf(stderr, "Sandbox: Unable to setup sandbox service - GetTargetServices()\n");
        return 55;
    }

    if (sandbox::SBOX_ALL_OK != target_service->Init()) {
        fprintf(stderr, "Sandbox: Unable to setup sandbox service - Init()\n");
        return 56;
    }

    // The child has an extra arg passed in (to assist with unique pipe names) - ignore moving forward.
    int argc_less_id = argc - 1;

    // If we have pre-sandbox initalization code, run now.
    if (pre_sandbox_init) {
        int ret = pre_sandbox_init(argc_less_id, argv);
        if (ret != 0) {
            return ret;
        }
    }

    // Execution after this is now locked in the sandbox.
    target_service->LowerToken(); 

    // ##### END UNSAFE ######

    if (sandboxed_wmain == NULL) {
        fprintf(stderr, "Sandbox: No main method defined!\n");
        return 50;
    }

    // We're now in our sandbox, run our code.
    int exit_code = sandboxed_wmain(argc_less_id, argv);

    ::CloseHandle(stdin_pipe);
    ::CloseHandle(stdout_pipe);
    ::CloseHandle(stderr_pipe);

    return exit_code;
}


/*
 * Call this function from wmain. Pass in your policy function and your
 * sandboxed main method.
 */
int RunConsoleAppInSandbox(SandboxPolicyFn policy_provider, 
                            ConsoleWMainFn pre_sandbox_init, 
                            ConsoleWMainFn sandboxed_wmain, 
                            int argc, wchar_t* argv[]) {

    sandbox::BrokerServices* broker_service = sandbox::SandboxFactory::GetBrokerServices();

    // A non-NULL broker_service means that we are the unsandboxed parent process.
    if (NULL != broker_service) {
        return RunParent(argc, argv, broker_service, policy_provider);
    } else {
        return RunChild(pre_sandbox_init, sandboxed_wmain, argc, argv);
    }
}
