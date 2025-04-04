# Windows IPC Job Dispatcher

## Overview

This project demonstrates a Windows-based system programming solution that utilizes inter-process communication (IPC) and process management through the Windows API. The dispatcher distributes incoming jobs to a pool of specialized worker processes based on job type. Each worker simulates job processing by sleeping for the specified duration and signals the dispatcher upon completion.

## Features

- **Dynamic Worker Pool Management**: Creates and manages worker processes based on a configurable distribution
- **IPC via Anonymous Pipes**: Establishes two-way communication channels between the dispatcher and workers
- **Non-Blocking Operation**: Uses Windows API functions for efficient job monitoring without blocking
- **Job-Type Specialization**: Distributes work to specialized worker processes based on job type
- **Graceful Termination**: Properly signals and shuts down all worker processes when jobs are complete

## Technical Implementation

### IPC Mechanism

Instead of POSIX-specific functions like `fork()`, `pipe()`, and `select()`, this Windows implementation uses:

- `CreatePipe()` - Creates anonymous pipes for IPC channels
- `CreateProcessA()` - Spawns worker processes with inherited pipe handles
- `PeekNamedPipe()` - Allows non-blocking checks for worker completion signals
- `ReadFile()`/`WriteFile()` - Transmits job data and completion signals

### Worker Process Behavior

Each worker process:
1. Inherits communication pipes from the dispatcher
2. Waits for job assignments (duration in seconds)
3. Simulates work by sleeping for the specified duration
4. Signals completion by writing the job duration back to the dispatcher
5. Terminates when it receives a duration of 0

### Dispatcher Behavior

The dispatcher process:
1. Creates worker processes based on provided configuration
2. Distributes incoming jobs to appropriate workers based on job type
3. Monitors worker output pipes for completion signals
4. Manages worker availability and job queuing
5. Sends termination signals (0) to all workers when all jobs are processed

## Usage

### Compilation

```bash
# Using Visual Studio Developer Command Prompt
cl /EHsc /W4 dispatcher.cpp /Fe:dispatcher.exe
cl /EHsc /W4 worker.cpp /Fe:worker.exe
```

### Execution

```bash
# Run with worker configuration (numbers represent workers per job type)
dispatcher.exe 1 3 1 1 1
```

### Input Format

The program accepts jobs from standard input with the following format:
```
<job_type> <duration>
```

Where:
- `job_type` is an integer from 1 to 5
- `duration` is the processing time in seconds

Example input:
```
1 5
2 3
3 7
4 2
5 4
```

## Requirements

- Windows operating system
- Visual C++ compiler
- Windows SDK

## Architecture Diagram

```
                                 ┌─────────────┐
                                 │  Dispatcher │
                                 └─────────────┘
                                        │
                  ┌──────────┬──────────┼──────────┬──────────┐
                  │          │          │          │          │
          ┌───────▼──┐┌─────▼─────┐┌───▼───┐┌─────▼─────┐┌───▼───┐
          │Worker(1) ││Worker(2)  ││Worker ││Worker(4)  ││Worker │
          │  Type 1  ││  Type 2   ││Type 3 ││  Type 4   ││Type 5 │
          └──────────┘└───────────┘└───────┘└───────────┘└───────┘
```

## Implementation Details

### Pipe Communication

Each worker has two pipes:
- Read pipe: Receives job assignments from the dispatcher
- Write pipe: Sends completion signals back to the dispatcher

### Error Handling

The implementation includes robust error handling for:
- Pipe creation failures
- Process creation issues
- Read/write operation errors
- Resource cleanup

### Performance Considerations

- Non-blocking I/O is implemented to prevent the dispatcher from stalling
- The dispatcher efficiently polls for worker completion signals
- Resources are properly managed to prevent leaks

## Future Improvements

- Add logging capabilities for better debugging
- Implement job priorities and preemption
- Add support for more complex job parameters
- Create a graphical interface for monitoring system state
- Implement worker health monitoring and automatic restart
