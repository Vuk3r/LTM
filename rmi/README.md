# Basic RMI (Remote Method Invocation) in C

This is a basic implementation of Remote Method Invocation (RMI) in C using TCP/IP sockets.

## Overview

The program consists of:
- **Server** (`server.c`): Provides remote methods that can be invoked by clients
- **Client** (`client.c`): Command-line client that connects to the server and invokes remote methods
- **GUI Client** (`client_gui.c`): GTK-based graphical user interface for the RMI client
- **Shared Header** (`rmi.h`): Contains common definitions, data structures, and serialization functions

## Available Remote Methods

1. **Add** (Method ID: 1) - Adds two numbers
2. **Subtract** (Method ID: 2) - Subtracts second number from first
3. **Multiply** (Method ID: 3) - Multiplies two numbers
4. **Divide** (Method ID: 4) - Divides first number by second

## Compilation

Use the provided Makefile:

```bash
make
```

This will compile the server, command-line client, and GUI client executables.

**Note**: The GUI client requires GTK+3.0 (or GTK+2.0) development libraries. Install them with:
```bash
# Ubuntu/Debian
sudo apt-get install libgtk-3-dev

# Fedora/RHEL
sudo dnf install gtk3-devel
```

To clean compiled files:

```bash
make clean
```

## Usage

### 1. Start the Server

In one terminal, run:

```bash
./server
```

The server will listen on port 8080 and wait for client connections.

### 2. Run the Client

You have two options for the client:

#### Option A: GUI Client (Recommended)

Run the graphical interface:

```bash
./client_gui
```

The GUI provides:
- Server IP and port configuration
- Connect/Disconnect buttons
- Method selection dropdown (Add, Subtract, Multiply, Divide)
- Input fields for two arguments
- Calculate button to invoke remote methods
- Result display
- Status indicator

**Usage:**
1. Enter the server IP (default: 192.168.1.79) and port (default: 8080)
2. Click "Connect" to establish connection
3. Select a method from the dropdown
4. Enter two numbers in the argument fields
5. Click "Calculate" to invoke the remote method
6. View the result in the result label

#### Option B: Command-Line Client

In another terminal, you can run the client in two modes:

**Interactive Mode (no arguments):**

```bash
./client
```

Then follow the prompts to select a method and enter arguments.

**Command Line Mode:**

```bash
./client <method_id> <arg1> <arg2>
```

Example:
```bash
./client 1 10 5    # Add: 10 + 5 = 15
./client 2 10 5    # Subtract: 10 - 5 = 5
./client 3 10 5    # Multiply: 10 * 5 = 50
./client 4 10 5    # Divide: 10 / 5 = 2
```

## How It Works

1. **Serialization**: Request and response structures are serialized into strings using pipe-delimited format
2. **Network Communication**: Client and server communicate via TCP sockets
3. **Method Invocation**: Client sends method ID and arguments, server processes and returns result
4. **Error Handling**: Server validates operations (e.g., division by zero) and returns error messages

## Architecture

- **Request Structure**: Contains method ID and two double-precision arguments
- **Response Structure**: Contains success flag, result value, and error message
- **Stub Function**: `invoke_remote_method()` acts as a client-side stub that handles communication

## Notes

- The server handles one client at a time (sequential processing)
- To stop the server, use Ctrl+C or have a client send METHOD_EXIT (99)
- Default port is 8080 (defined in `rmi.h`)

