# AGENTS.md - Entrelacs Project Documentation

## Project Overview
The Entrelacs System is a prototyping platform for arrow-based computing.

Key features:
- Arrow Space: A persistent (journalized) memory space for storing and managing Arrows
- Transient Arrows: Temporary working arrows that are lazily assimilated into the Arrow Space
- Machine: A state machine inspired by the CaEK abstract machine for processing arrows
- Sessions: Contexts where agents interact with the system
- Context Management: Hierarchical contexts for organizing knowledge

## Building the Project

### Prerequisites
- C compiler (GCC or Clang)
- Make utility
- Standard C libraries

### Build Instructions

The project uses a makefile for building. Available build commands:

```bash
make all          # Compile binaries and test
make run          # Compile and test everything (type Ctrl+D when in the shell)
make help         # See other build commands
make clean        # Clean build artifacts
cd repl && make   # Build a REPL cli
```

## Built Files

| File | Description |
|------|-------------|
| `bin/libentrelacs.a` | Static library for the entrelacs "core" |
| `bin/libentrelacs.so` | Dynamic library |
| `bin/entrelacsd` | Entrelacs server with HTTP API |
| `repl/bin/entrelacs` | REPL CLI |
| `tests/testshell` | Minimalist REPL |

## Code Structure

The project is organized into several directories:

- **entrelacs/**: Core header files and public API
- **mem/**: Memory management and allocation
- **space/**: Space and cell management, arrow operations
- **machine/**: Machine context, session management, and transient arrows
- **server/**: Server and web-related components
- **web-terminal/**: Web-based terminal interface
- **repl/**: Read-Eval-Print Loop functionality
- **log/**: Logging functionality
- **sha1/**: SHA1 hashing implementation
- **test/**: Test files and utilities

Key Files for Understanding are:

1. **entrelacs/entrelacs.h**: Main public API
2. **space/space.h**: Arrow Space API
3. **machine/transient.h**: Transient Arrow API
4. **machine/machine.h**: Machine API
5. **machine/context.h**: Context API
6. **machine/session.h**: Session API
7. **space/cell.h**: Cell structure and operations
8. **mem/mem0.h**: Memory system

## Key Concepts

### Arrows

Arrows are the fundamental computational units in Entrelacs, forming a deduplicated binary graph structure.

#### Core Principles

1. **Binary Relationship**: Every arrow is defined by exactly two arrow references:
   - **tail**: Left arrow reference
   - **head**: Right arrow reference

2. **Deduplicated Storage**: Each unique arrow definition (tail, head, type, data) exists exactly once in the Arrow Space, regardless of how many times it's referenced.

3. **Graph Structure**: Arrows form a directed acyclic graph where:
   - Internal nodes are PAIR arrows (tail→head relationships)
   - Leaf nodes are ATOM arrows (containing actual data)
   - EVE is the simplest atom (address 0, empty content)

#### Key Properties

1. **Definition-Based Identity**: Arrows are identified by their complete definition, not by creation context or location
2. **Automatic Deduplication**: The system ensures each unique arrow definition exists only once
3. **Persistent References**: All references to equivalent arrow definitions resolve to the same physical arrow
4. **Recursive Composition**: Complex structures are built by combining simpler arrows through binary relationships

#### Technical Implementation

- **Arrow Space**: A deduplicated storage system where each unique arrow definition has exactly one representation
- **Hash-Based Lookup**: Arrow definitions are hashed to detect and prevent duplicates
- **Reference Counting**: Tracks how many other arrows reference each arrow
- **Garbage Collection**: Unreferenced arrows are eventually removed when no longer needed

#### Computational Implications

1. **Efficient Storage**: Deduplication minimizes memory usage for shared substructures
2. **Consistent Identity**: Equivalent arrow definitions always resolve to the same instance
3. **Graph Traversal**: Computation involves navigating this deduplicated arrow graph
4. **Pattern Matching**: The deduplicated nature enables efficient pattern recognition

#### Arrow Serialization Types

One definition of arrow but several Serialization types

- **PAIR**: Arrows consisting of tail and head arrow references (internal nodes)
- **ATOM**: Atomic arrows containing data (leaf nodes):
  - **EVE**: The root/empty arrow (address 0) - simplest atom
  - **SMALL**: 1-11 bytes of data
  - **TAG**: 12-99 bytes of data
  - **BLOB**: 100+ bytes of data

### Arrow Space

The Arrow Space is a deduplicated persistent storage system for arrows. Key characteristics:

- **Deduplicated Storage**: Each unique arrow definition exists exactly once
- **Hash-Based Indexing**: Arrow definitions are hashed for efficient lookup and deduplication
- **Persistent Identity**: Arrows maintain consistent identities across sessions
- **Reference Counting**: Tracks arrow usage for garbage collection

#### Key Operations

- **Assimilation**: Adding arrows to the space with automatic deduplication
- **Rooting**: Marking arrows as persistent to prevent garbage collection
- **GC (Garbage Collection)**: Removing arrows with zero references
- **Browsing**: Navigating the arrow graph through tail/head relationships
- **Resolution**: Finding existing arrows by their definition (with automatic deduplication)

### Transient Arrows

Transient arrows (xs_*) are temporary working arrows that:
- Are allocated in a growing pool (freed at each commit)
- Use pointer-based references to other arrows
- Are lazily assimilated into the Arrow Space with automatic deduplication
- Provide efficient working space for arrow manipulation before persistence

#### Key Characteristics

- **Temporary Nature**: Exist only within a transaction/commit cycle
- **Pointer-Based**: Use direct memory pointers instead of Arrow Space addresses
- **Lazy Assimilation**: Only persisted to Arrow Space when needed
- **Deduplication Aware**: Resolve to existing arrows when assimilated
- **Efficient Operations**: Enable fast arrow manipulation without immediate persistence overhead

### Machine

The Entrelacs Machine is a state machine that:
- Processes λ-calculus programs in A-normal form (A(CS) language)
- Uses continuations for control flow
- Handles context switching and session management

### Sessions

Sessions provide contexts for agent interactions:
- System sessions: Admin-level operations
- User sessions: User-specific operations
- Session arrows are rooted as `/${session}+/${agent}+${session-uuid}`

### Contexts

Contexts are hierarchical referentials where:
- Arrows can be rooted and unrooted
- Variables can be set, got, and reset
- Links can be created and browsed
- Context paths define the hierarchy (e.g., `///World+Europa+France`)

## Coding Conventions

### File Organization

- Header files use the `.h` extension
- Source files use the `.c` extension
- Each major component has its own directory
- Header files include function declarations and type definitions
- Source files include implementations

### Naming Conventions

- Namespace Prefixes:
  - `xl_`: Arrow Space operations
  - `xs_`: Transient arrow operations
  - `mem_`: Memory operations
  - `cell_`: Cell operations
- Function names: `snake_case` (e.g., `xl_init`, `xs_context_root`)
- Type names: `CamelCase` or `snake_case` depending on context
- Variable names: `snake_case`
- Constants: `UPPER_CASE`

### Code Style

- Indentation: Uses spaces (check .editorconfig for exact settings)
- Braces: Typically on their own lines for function definitions
- Comments: Used to explain complex logic and function purposes
- Debug macros: `ONDEBUG()`, `DEBUGPRINTF()`, etc.

### Memory Management

The project uses a sophisticated memory system:

1. **mem0**: Low-level persistent memory with journaling
2. **mem**: Higher-level memory operations
3. **geoalloc**: Geometrically growing RAM areas
4. **Cell structure**: 24-byte cells containing arrow data

## API Documentation

### Arrow Space API (xl_*)

Key functions in `space/space.h`:

- `xl_init()` / `xl_destroy()`: Initialize/destroy Arrow Space
- `xl_pair()`, `xl_atom()`: Create arrows
- `xl_root()` / `xl_unroot()`: Root management
- `xl_open()` / `xl_close()` / `xl_commit()`: Transaction management
- `xl_childrenOf()`: Browse arrow children

### Transient Arrow API (xs_*)

Key functions in `machine/transient.h`:

- `xs_init()`: Initialize transient system
- `xs_pair()`, `xs_atom()`: Create transient arrows
- `xs_resolve()`: Resolve to Arrow Space
- `xs_assimilate()`: Assimilate to Arrow Space
- `xs_context_*` functions: Context management

### Machine API

Key functions in `machine/machine.h`:

- `xs_run()`: Run the Entrelacs Machine
- `xs_eval()`: Evaluate a program
- `xs_operator()` / `xs_continuation()`: C-implemented operators

### Session API

Key functions in `machine/session.h`:

- `xs_session_open()`: Create a session
- `xs_session_commit()`: Commit session changes
- `xs_session_close()`: Close a session
- `xs_session_connect()`: Authenticate a session

### Context API

Key functions in `machine/context.h`:

- `xs_context_root()` / `xs_context_unroot()`: Root management
- `xs_context_get()` / `xs_context_set()`: Variable management
- `xs_context_link()` / `xs_context_unlink()`: Link management
- `xs_context_list()`: List context contents

## Testing

The project includes comprehensive testing:

### Test Files

- **testdraft.c**: Draft testing functionality
- **testmachine.c**: Machine component testing
- **testscript.c**: Script testing
- **testshell.c**: Shell testing
- **testspace.c**: Space component testing
- **testuri.c**: URI testing
- **utest_hash.c**: Hash function testing

### Running Tests

```bash
make run  # Run all tests
```

Or run specific tests:

```bash
cd test
gcc -o test_output testfile.c ../path/to/dependencies
./test_output
```

### Test Macros

The project uses test macros from `stupid_test.h`:

```c
#define test_title(T) fprintf(stderr, "\n=========== TEST %s ===========\n", (test_title = T))
#define test_ok()  fprintf(stderr, "          TEST %s SUCCESS\n", test_title)
#define test_done() fprintf(stderr, "\n\n\nALL TESTS FROM " __FILE__ " DONE\n\n\n")
```

## Memory System

### Cell Structure

Cells are 24-byte structures containing:

```c
typedef union u_cell {
    struct u_full {
        char data[22];
        unsigned char pebble;  // More counter
        unsigned char type;     // Cell type ID
    } full;
    // ... various specialized structures
} Cell;
```

### Cell Types

- `CELLTYPE_EMPTY`: Empty cell
- `CELLTYPE_PAIR`: Pair arrow
- `CELLTYPE_SMALL`: Small atom (1-11 bytes)
- `CELLTYPE_TAG`: Tag atom (12-99 bytes)
- `CELLTYPE_BLOB`: Blob atom (100+ bytes)
- `CELLTYPE_SLICE`: Binary string segment
- `CELLTYPE_LAST`: Last binary string segment
- `CELLTYPE_SYNC`: Synchronization
- `CELLTYPE_CHILDREN`: Children segment

### Memory Addressing

- Uses twin prime numbers for space size
- Open addressing with probing
- Journalized writes for persistence
- Geometric allocation for dynamic memory

## Persistence

The system uses persistent storage:

- **Persistence file**: `~/.entrelacs/entrelacs.dat`
- **Journal file**: `~/.entrelacs/entrelacs.journal`
- **Blob directory**: `~/.entrelacs/blob/`
- **Environment variable**: `ENTRELACS` for custom paths

## Logging

The project includes a comprehensive logging system:

- **Log levels**: OFF, FATAL, ERROR, WARN, INFO, TRACE, DEBUG
- **Facilities**: General, mem0, mem, space, transient, machine, session, server
- **Macros**: `FATALPRINTF()`, `ERRORPRINTF()`, `DEBUGPRINTF()`, etc.

## Server Components

The server includes:

- **Mongoose web server**: HTTP server implementation
- **URL handling**: Arrow URL management
- **Session management**: Web-based session handling
- **REST API**: For arrow operations

Scripts to query the server.

| File | Description |
|------|-------------|
| `web-terminal/cli.sh` | Simple script to query the server (depends on curl CLI) |
| `web-terminal/fill-demo.sh` | Fill up the demo server with some knowledge via cli.sh |

## Web Interface

The web-terminal provides:

- HTML/CSS/JavaScript web interface
- Server communication via HTTP
- Arrow visualization and manipulation
- Session management

## REPL (Read-Eval-Print Loop)

The REPL provides:

- Interactive command-line interface
- Line editing with linenoise
- Program evaluation
- Session management

## Development Workflow

### Editing Code

1. Follow existing coding conventions
2. Use appropriate prefixes for functions
3. Add comments for complex logic
4. Use debug macros for debugging

### Adding Features

1. Implement in appropriate component directory
2. Add corresponding header declarations
3. Update public API if needed
4. Add tests for new functionality

### Debugging

1. Use `DEBUGPRINTF()` macros
2. Check log files
3. Use `cell_show()` for arrow inspection
4. Use `mem_show()` for memory inspection
