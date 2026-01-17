# Reliable UDP (RUDP) - Custom Transport Protocol Implementation

A comprehensive implementation of a reliable transport protocol built on top of UDP, demonstrating advanced networking concepts including sliding window protocol, flow control, and bidirectional communication.

## 🎯 Project Overview

This project implements a **Reliable User Datagram Protocol (RUDP)** from scratch in C, providing TCP-like reliability guarantees over UDP. The implementation supports two modes of operation:
- **File Transfer Mode**: High-performance reliable file transfer with integrity verification
- **Chat Mode**: Real-time bidirectional messaging with guaranteed delivery

## 🔑 Key Concepts Demonstrated

### 1. **Transport Layer Protocol Design**
- Custom packet header structure with sequence numbers, acknowledgments, and control flags
- Three-way handshake for connection establishment (SYN, SYN-ACK, ACK)
- Four-way connection termination (FIN, ACK, FIN, ACK)
- Properly structured protocol state machine

### 2. **Sliding Window Protocol (Go-Back-N)**
- Implemented sender-side sliding window with configurable size (10 packets)
- Efficient in-flight packet tracking and management
- Window-based flow control to prevent receiver buffer overflow
- Dynamic window adjustment based on receiver capacity

### 3. **Reliability Mechanisms**
- **Selective Acknowledgment**: Cumulative ACKs with selective retransmission
- **Timeout and Retransmission**: RTO-based packet retransmission (500ms default)
- **Out-of-Order Packet Buffering**: Receiver buffers out-of-order packets for in-sequence delivery
- **Duplicate Detection**: Prevents duplicate message delivery in chat mode

### 4. **Flow Control**
- Receiver advertises available buffer space in every ACK
- Sender respects receiver window to prevent buffer overflow
- Dynamic buffer management (20KB receiver buffer)
- Prevents network congestion through proper window management

### 5. **Multiplexed I/O**
- Non-blocking socket operations with `select()` for event-driven programming
- Concurrent handling of user input and network packets in chat mode
- Efficient timeout management without busy-waiting

### 6. **Packet Loss Simulation**
- Built-in configurable packet loss simulation for testing (0.0-1.0 probability)
- Validates reliability mechanisms under adverse network conditions
- Demonstrates protocol robustness

### 7. **Data Integrity**
- MD5 checksum calculation for received files using OpenSSL
- File integrity verification post-transfer
- Compatible with both OpenSSL and LibreSSL

## 🚀 Unique Implementation Features

### 1. **Dual-Mode Architecture**
The protocol intelligently switches between file transfer and chat modes, optimizing behavior for each use case:
- File mode prioritizes throughput and silent operation
- Chat mode provides interactive user experience with real-time feedback

### 2. **Advanced Logging System**
- Environment-variable controlled verbose logging (`RUDP_LOG=1`)
- Microsecond-precision timestamps for performance analysis
- Separate log files for client and server
- Detailed protocol event logging (SYN, ACK, DATA, RETX, DROP, TIMEOUT)

### 3. **Message Queue for Chat**
- Circular queue (10 messages) for buffering outgoing chat messages
- Prevents message loss when waiting for acknowledgments
- Automatic retransmission on timeout

### 4. **Smart Flow Control**
- Byte-level granular flow control (not just packet-based)
- Real-time buffer occupancy tracking
- Window size communicated in every ACK packet
- Prevents sender from overwhelming receiver

### 5. **Robust Connection Termination**
- Graceful shutdown with proper 4-way handshake
- Timeout-based retransmission of FIN packets
- Fallback mechanism after max attempts (prevents hanging)
- Separate handling for chat and file transfer termination

### 6. **Out-of-Order Packet Handling**
- Sophisticated receiver buffer implementation
- Stores packets arriving before expected sequence
- Delivers data in-order to application layer
- Efficient memory management with circular buffer

### 7. **Clean Separation of Concerns**
- Protocol logic separated from application logic
- Reusable header structure (`sham.h`)
- Mode-specific output (silent file transfer, verbose chat)
- Extensible architecture for future enhancements

## 📁 Project Structure

```
RUDP/
├── sham.h          # Protocol definitions, packet structures, function declarations
├── client.c        # Client implementation (602 lines)
├── server.c        # Server implementation (746 lines)
├── Makefile        # Build automation
└── README.md       # This file
```

### Header File (`sham.h`)
- Protocol constants (packet size, window size, RTO)
- Packet and header structures with proper alignment
- Window slot structure for tracking in-flight packets
- Receive buffer structure for out-of-order handling
- Message queue structure for chat mode

## 🛠️ Technical Skills Showcased

### Systems Programming
- **Memory Management**: Proper allocation, deallocation, and buffer management
- **Pointer Arithmetic**: Efficient data structure manipulation
- **System Calls**: Socket programming, file I/O, process control
- **Error Handling**: Comprehensive error checking and recovery

### Network Programming
- **Socket API**: UDP socket creation, binding, sendto/recvfrom
- **Network Byte Order**: Proper handling with htons/ntohs
- **Non-blocking I/O**: fcntl, select, O_NONBLOCK
- **Timeout Management**: SO_RCVTIMEO socket options

### Protocol Design
- **State Machines**: Connection establishment and teardown
- **Reliability**: ARQ (Automatic Repeat Request) mechanisms
- **Flow Control**: Window-based congestion prevention
- **Packet Framing**: Custom header design with flags and metadata

### Concurrent Programming
- **Multiplexing**: select() for handling multiple I/O sources
- **Event-Driven Architecture**: Non-blocking operations
- **Timers**: gettimeofday, timersub for timeout calculation

### Security & Cryptography
- **OpenSSL Integration**: EVP API for MD5 computation
- **Cross-platform Compatibility**: Supporting multiple OpenSSL versions
- **Data Integrity**: Checksum verification

## 📊 Performance Characteristics

- **Window Size**: 10 packets (configurable)
- **Packet Size**: 1024 bytes
- **RTO**: 500ms (retransmission timeout)
- **Receiver Buffer**: 20KB (20480 bytes)
- **Throughput**: Optimized with pipelined transmission
- **Latency**: Minimized with immediate ACKs

## 🔧 Building and Running

### Prerequisites
```bash
# Install OpenSSL development libraries
sudo apt-get install libssl-dev  # Ubuntu/Debian
sudo yum install openssl-devel   # CentOS/RHEL
```

### Build
```bash
make                # Build both client and server
make debug          # Build with debug symbols
make clean          # Clean build artifacts
```

### Usage

#### File Transfer Mode
```bash
# Server
./server <port> [loss_rate]
./server 8080 0.1

# Client
./client <server_ip> <server_port> <input_file> <output_name> [loss_rate]
./client 127.0.0.1 8080 test.txt received.txt 0.1
```

#### Chat Mode
```bash
# Server
./server <port> --chat [loss_rate]
./server 8080 --chat 0.05

# Client
./client <server_ip> <server_port> --chat [loss_rate]
./client 127.0.0.1 8080 --chat 0.05
```

#### Enable Verbose Logging
```bash
# Set environment variable before running
export RUDP_LOG=1
./client 127.0.0.1 8080 file.txt output.txt

# Check logs
cat client_log.txt
cat server_log.txt
```

### Chat Commands
- Type messages and press Enter to send
- Type `/quit` to gracefully exit chat session

## 🧪 Testing Features

### Packet Loss Simulation
Test reliability under adverse conditions:
```bash
# 10% packet loss
./server 8080 0.1
./client 127.0.0.1 8080 test.txt out.txt 0.1

# 30% packet loss (extreme conditions)
./server 8080 0.3
./client 127.0.0.1 8080 test.txt out.txt 0.3
```

### Integrity Verification
After file transfer, server automatically computes and displays MD5:
```bash
MD5: 098f6bcd4621d373cade4e832627b4f6
```

### Protocol Event Logging
With `RUDP_LOG=1`, observe detailed protocol behavior:
```
[2026-01-17 10:30:45.123456] [LOG] SND SYN SEQ=1000
[2026-01-17 10:30:45.125678] [LOG] RCV SYN-ACK SEQ=2000 ACK=1001
[2026-01-17 10:30:45.127890] [LOG] SND DATA SEQ=1 LEN=1024
[2026-01-17 10:30:45.650123] [LOG] TIMEOUT SEQ=1
[2026-01-17 10:30:45.650456] [LOG] RETX DATA SEQ=1 LEN=1024
```

## 🎓 Learning Outcomes

This project demonstrates deep understanding of:

1. **Transport Layer Protocols**: How TCP-like reliability can be implemented over UDP
2. **Protocol State Machines**: Managing connection lifecycle and packet states
3. **Performance Optimization**: Pipelining, windowing, and efficient buffer management
4. **Error Recovery**: Timeout detection, retransmission strategies, and duplicate handling
5. **Real-world Networking**: Handling packet loss, reordering, and network delays
6. **Systems Design**: Clean architecture, separation of concerns, and extensibility

## 🔍 What Makes This Implementation Stand Out

### 1. **Production-Quality Code**
- Comprehensive error handling at every system call
- Memory leak prevention with proper cleanup
- Graceful degradation under error conditions
- Cross-platform compatibility (Linux tested)

### 2. **Thoughtful Design Decisions**
- Mode-specific behavior (silent file transfer vs interactive chat)
- Configurable packet loss for testing
- Optional verbose logging without code changes
- Clean termination under all scenarios

### 3. **Advanced Features**
- Out-of-order packet buffering (not found in basic implementations)
- Byte-granular flow control (more sophisticated than packet-based)
- Retransmission on timeout with attempt counting
- Message queue for chat mode reliability

### 4. **Educational Value**
- Clear demonstration of sliding window protocol
- Practical implementation of textbook concepts
- Real-world networking challenges addressed
- Extensible codebase for further experimentation

## 📝 Potential Extensions

- **Congestion Control**: Add slow-start and congestion avoidance (TCP Reno/Tahoe)
- **Selective ACK (SACK)**: Implement selective acknowledgment for efficiency
- **Adaptive RTO**: Dynamic timeout calculation based on RTT measurements
- **Connection Pooling**: Support multiple simultaneous clients
- **Encryption**: Add TLS/SSL for secure communication
- **Compression**: Integrate zlib for data compression
- **IPv6 Support**: Extend to support IPv6 addressing

## 📜 License

This project is educational and open for learning and experimentation.

## 👤 Author

**Rishabh**
- Demonstrated expertise in: Systems Programming, Network Protocols, C Programming, Unix/Linux Development
- Strong understanding of: TCP/IP Stack, Reliable Data Transfer, Flow Control, Error Recovery

---

*This project showcases advanced networking knowledge and production-quality systems programming skills suitable for roles in systems engineering, network protocol development, and distributed systems.*
