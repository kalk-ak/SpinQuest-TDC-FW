# High-Throughput DAQ Server & FPGA Simulator

A high performance Data Acquisition system designed to simulate a receive and log particle accelerator spill data from multiple FPGA boards. The project includes a server and a multi-threaded FPGA simulator for testing high bandwidth network ingestion.

## 🚀 Features

- **Multi-Protocol Support**: Seamlessly switch between **TCP**, **UDP**, and **UNIX Domain Sockets**.
- **Concurrent Architecture**:
  - **TCP**: Spawns a dedicated thread per connection for isolated stream processing.
  - **UDP**: Utilizes `SO_REUSEPORT` for kernel-level load balancing, allowing multiple worker threads to listen on the same port without mutex contention.
- **RAII Resource Management**: Custom `UniqueFD` wrapper ensures no file descriptor leaks, even during crashes.
- **High Bandwidth Simulation**: Simulate multiple FPGA boards with configurable clock frequencies and spill durations.
- **Automatic Dependencies**: All libraries (`CLI11`, `spdlog`) are managed automatically via CMake's `FetchContent`.

---

## 🛠️ Installation & Building

### Prerequisites

- **CMake** (v3.23 or higher)
- **C++17** compatible compiler (GCC 9+, Clang 10+)
- **Linux** (Recommended for `SO_REUSEPORT` and UNIX socket support)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/kalk-ak/SpinQuest-TDC-FW/tree/master/
cd kaleb-projects

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)
```

This will generate two executables in the `build/` directory:

1. `daq_server`: The ingestion engine.
2. `fake_fpga`: The board simulator.

---

## 🖥️ Usage: DAQ Server

The server listens for incoming spillb bursts of 64-bit data packets framed by specific preambles.

### Basic Command

```bash
./build/daq_server --ip 127.0.0.1 --port 6767 --network tcp --out ./data
```

### Options

| Flag            | Description                                   | Default        |
| :-------------- | :-------------------------------------------- | :------------- |
| `-i, --ip`      | IP Address to bind to (or file path for UNIX) | `127.0.0.1`    |
| `-p, --port`    | Port number                                   | `6767`         |
| `-n, --network` | Protocol: `tcp` or `udp`                      | `tcp`          |
| `-d, --domain`  | Domain type: `ipv4`, `ipv6`, or `unix`        | `ipv4`         |
| `-o, --out`     | Directory to save binary `.dat` files         | `./spill_data` |
| `-w, --workers` | (UDP only) Number of concurrent listeners     | `4`            |
| `-v, --verbose` | Enable debug logging                          | `false`        |

---

## ⚡ Usage: Fake FPGA Simulator

The simulator mimics real hardware by sending a start preamble, a stream of random 64-bit words at a fixed frequency, and an end preamble.

### Basic Command

```bash
# Simulate 10 boards sending data at 4.0 MHz to a local server
./build/fake_fpga --boards 10 --freq 4.0 --network tcp
```

### Options

| Flag            | Description                                       | Default     |
| :-------------- | :------------------------------------------------ | :---------- |
| `-i, --ip`      | Server IP Address                                 | `127.0.0.1` |
| `-p, --port`    | Server Port                                       | `6767`      |
| `-n, --network` | Protocol: `tcp` or `udp`                          | `tcp`       |
| `-f, --freq`    | FPGA Clock Frequency in MHz (0 = unlimited)       | `4.0`       |
| `-b, --boards`  | Number of concurrent boards to simulate           | `1`         |
| `--names`       | Path to a text file containing unique board names | (Numeric)   |
| `-v, --verbose` | Enable debug logging                              | `false`     |

---

## 🔬 System Architecture

### UDP Load Balancing

Unlike traditional UDP servers that use a single socket and a bottlenecked dispatcher, this system uses `SO_REUSEPORT`.

1. Multiple **Datagram Workers** bind to the same port.
2. The Linux kernel hashes incoming packets (Source IP + Port) to a specific worker.
3. This ensures that all packets from **Board A** always go to **Worker 1**, allowing for **lock-free** file writes and maximum multi-core utilization.

### Data Format

The server expects 64-bit (8-byte) words:

- **Start Preamble**: `0xAAAAAAAABBBBBBBB`
- **End Preamble**: `0xDEADBEEFDEADBEEF`
- **Payload**: Randomly generated 64-bit integers.

---

## 📂 Project Structure

- `src/`: Implementation files.
- `include/`: Header files and class definitions.
- `build/`: Compilation artifacts (created after build).
- `CMakeLists.txt`: Build configuration and dependency management.
