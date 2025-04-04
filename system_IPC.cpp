#include <windows.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <cstdlib>

// Structure to hold information about a worker process.
struct WorkerInfo {
    int worker_type;           // The job type this worker handles
    bool busy;                 // Busy flag: true if a job is in progress
    HANDLE hWrite;             // Parent's handle to write jobs to the worker (child's STDIN)
    HANDLE hRead;              // Parent's handle for reading responses from the worker (child's STDOUT)
    PROCESS_INFORMATION procInfo;  // Process information for later waiting and cleanup
};

// Structure for representing a job.
struct Job {
    int type;       // Job type [1 - 5]
    int duration;   // Duration in seconds (1-10) to simulate processing
};

//
// Worker process code: when the executable is invoked with "worker" argument,
// the process will run this loop. It reads an integer from its standard input,
// sleeps for that many seconds, and writes the integer back to standard output.
// A duration of 0 is treated as a termination command.
//
int WorkerMain(int workerType) {
    // Get inherited standard handles.
    HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD bytesRead = 0, bytesWritten = 0;
    int duration = 0;
    while (true) {
        // Read the job duration (4 bytes) from parent's pipe.
        BOOL success = ReadFile(hIn, &duration, sizeof(duration), &bytesRead, NULL);
        if (!success || bytesRead != sizeof(duration)) {
            // If reading fails, exit the loop.
            break;
        }
        // A duration of 0 indicates termination.
        if (duration == 0)
            break;

        // Simulate job processing (sleep in milliseconds).
        Sleep(duration * 1000);

        // Write the same integer back to parent to signal completion.
        success = WriteFile(hOut, &duration, sizeof(duration), &bytesWritten, NULL);
        if (!success || bytesWritten != sizeof(duration)) {
            break;
        }
    }
    return 0;
}

//
// Main (dispatcher) process code:
// 1. Creates worker processes based on a fixed configuration.
// 2. Dispatches jobs (from a simulated job queue) to workers handling matching job types.
// 3. Polls worker output pipes using PeekNamedPipe to determine which jobs are completed.
// 4. Sends a termination command (duration == 0) when all jobs are done.
//

int main(int argc, char* argv[])
{
    // If a command-line argument "worker" is provided, run as worker process.
    if (argc > 1 && std::string(argv[1]) == "worker") {
        int workerType = 0;
        if (argc > 2)
            workerType = std::atoi(argv[2]); // Worker type is passed as 2nd argument.
        std::cout << "Worker process started, handling job type " << workerType << ".\n";
        return WorkerMain(workerType);
    }

    // --- Main process (dispatcher) ---

    // Fixed worker configuration:
    // Worker type 1: 1 instance, type 2: 3 instances, type 3: 1, type 4: 1, type 5: 1.
    int worker_count_config[5] = {1, 3, 1, 1, 1};

    std::vector<WorkerInfo> workers; // Container to store all workers.

    // SECURITY_ATTRIBUTES to allow handle inheritance.
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;   // Allow inheriting handles in child processes.
    saAttr.lpSecurityDescriptor = NULL;

    // Get the full path of the current executable.
    char szExeName[MAX_PATH];
    GetModuleFileNameA(NULL, szExeName, MAX_PATH);

    // Create worker processes for each worker type.
    for (int i = 0; i < 5; i++) {  // for each worker type (1 through 5)
        for (int j = 0; j < worker_count_config[i]; j++) {
            WorkerInfo wi;
            wi.worker_type = i + 1;  // Worker type value.
            wi.busy = false;         // Initially, worker is idle.

            // Create the pipe for main-to-worker communication.
            // hChildStd_IN_R: read end inherited by worker (child's STDIN)
            // hChildStd_IN_W: write end kept by parent for sending jobs.
            HANDLE hChildStd_IN_R = NULL;
            HANDLE hChildStd_IN_W = NULL;
            if (!CreatePipe(&hChildStd_IN_R, &hChildStd_IN_W, &saAttr, 0)) {
                std::cerr << "StdIn CreatePipe failed.\n";
                return 1;
            }
            // Prevent the parent's write handle from being inherited.
            SetHandleInformation(hChildStd_IN_W, HANDLE_FLAG_INHERIT, 0);

            // Create the pipe for worker-to-main communication.
            // hChildStd_OUT_W: write end inherited by worker (child's STDOUT)
            // hChildStd_OUT_R: read end kept by parent for receiving responses.
            HANDLE hChildStd_OUT_R = NULL;
            HANDLE hChildStd_OUT_W = NULL;
            if (!CreatePipe(&hChildStd_OUT_R, &hChildStd_OUT_W, &saAttr, 0)) {
                std::cerr << "StdOut CreatePipe failed.\n";
                return 1;
            }
            // Prevent the parent's read handle from being inherited.
            SetHandleInformation(hChildStd_OUT_R, HANDLE_FLAG_INHERIT, 0);

            // Set up STARTUPINFO for the child process.
            STARTUPINFOA siStartInfo;
            ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
            siStartInfo.cb = sizeof(STARTUPINFOA);
            siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE); // optionally redirect STDERR
            siStartInfo.hStdOutput = hChildStd_OUT_W;               // child's STDOUT comes from our pipe
            siStartInfo.hStdInput  = hChildStd_IN_R;                // child's STDIN comes from our pipe
            siStartInfo.dwFlags |= STARTF_USESTDHANDLES;            

            // Build the command line for the worker process:
            // e.g.: "C:\path\to\windows_ipc.exe worker <worker_type>"
            std::stringstream ss;
            ss << "\"" << szExeName << "\" worker " << (i + 1);
            std::string cmdLineStr = ss.str();
            // Create a mutable C-string copy.
            char *cmdLine = new char[cmdLineStr.size() + 1];
            strcpy(cmdLine, cmdLineStr.c_str());

            PROCESS_INFORMATION piProcInfo;
            ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
            BOOL bSuccess = CreateProcessA(
                NULL,           // No module name (use command line)
                cmdLine,        // Command line string
                NULL,           // Process security attributes
                NULL,           // Primary thread security attributes
                TRUE,           // Inherit handles (so that the child gets pipe handles)
                0,              // No creation flags
                NULL,           // Use parent's environment
                NULL,           // Use parent's current directory
                &siStartInfo,   // STARTUPINFO pointer
                &piProcInfo     // PROCESS_INFORMATION pointer
            );
            delete[] cmdLine; // free allocated command line string

            if (!bSuccess) {
                std::cerr << "CreateProcess failed with error: " << GetLastError() << "\n";
                return 1;
            }

            // The parent no longer needs the child's end of the pipes.
            CloseHandle(hChildStd_IN_R);
            CloseHandle(hChildStd_OUT_W);

            // Save the parent's handles in the WorkerInfo structure.
            wi.hWrite = hChildStd_IN_W;  // Use to write a job (send duration)
            wi.hRead  = hChildStd_OUT_R;  // Use to read the response from worker
            wi.procInfo = piProcInfo;

            workers.push_back(wi);
        }
    }

    // Simulated job queue (could be replaced with actual input parsing).
    std::vector<Job> jobQueue = {
        {1, 3},
        {2, 5},
        {1, 2},
        {4, 7},
        {3, 1},
        {5, 1},
        {1, 5}
    };

    DWORD bytesWritten = 0, bytesRead = 0;
    // Main loop to dispatch jobs and read worker responses.
    while (!jobQueue.empty() || true) {
        // First, poll each worker's output pipe to check for a completed job.
        for (auto &w : workers) {
            if (w.busy) {
                DWORD avail = 0;
                // PeekNamedPipe checks if data is available without removing it.
                if (PeekNamedPipe(w.hRead, NULL, 0, NULL, &avail, NULL) && avail >= sizeof(int)) {
                    int finishedDuration = 0;
                    BOOL rSuccess = ReadFile(w.hRead, &finishedDuration, sizeof(int), &bytesRead, NULL);
                    if (rSuccess && bytesRead == sizeof(int)) {
                        std::cout << "Worker of type " << w.worker_type 
                                  << " completed job with duration " << finishedDuration << " seconds.\n";
                        w.busy = false; // Mark worker as idle.
                    }
                }
            }
        }

        // Dispatch jobs if available in the queue.
        if (!jobQueue.empty()) {
            Job job = jobQueue.front();
            bool dispatched = false;
            // Look for an idle worker of the correct job type.
            for (auto &w : workers) {
                if (w.worker_type == job.type && !w.busy) {
                    BOOL wSuccess = WriteFile(w.hWrite, &job.duration, sizeof(int), &bytesWritten, NULL);
                    if (!wSuccess || bytesWritten != sizeof(int)) {
                        std::cerr << "WriteFile to worker failed.\n";
                    } else {
                        std::cout << "Dispatched job of type " << job.type 
                                  << " with duration " << job.duration << " seconds.\n";
                        w.busy = true;  // Mark as busy since the job is in progress.
                        dispatched = true;
                        jobQueue.erase(jobQueue.begin());
                        break;
                    }
                }
            }
            // If no suitable worker is free, wait a short time.
            if (!dispatched) {
                Sleep(50);
            }
        } else {
            // If no jobs remain, check if all workers have finished pending jobs.
            bool allIdle = true;
            for (auto &w : workers) {
                if (w.busy) {
                    allIdle = false;
                    break;
                }
            }
            if (allIdle)
                break;
        }

        Sleep(10); // Small sleep to reduce busy waiting.
    }

    // Signal termination by sending a "0" duration to each worker.
    for (auto &w : workers) {
        int terminationSignal = 0;
        WriteFile(w.hWrite, &terminationSignal, sizeof(int), &bytesWritten, NULL);
    }

    // Wait for all worker processes to exit and clean up handles.
    for (auto &w : workers) {
        WaitForSingleObject(w.procInfo.hProcess, INFINITE);
        CloseHandle(w.procInfo.hProcess);
        CloseHandle(w.procInfo.hThread);
        CloseHandle(w.hWrite);
        CloseHandle(w.hRead);
    }

    std::cout << "All jobs completed and workers terminated.\n";
    return 0;
}