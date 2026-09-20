# 🔍 TCP Port Scanner v1.0

> A multithreaded TCP port scanner built in C++ using Object-Oriented Programming, RAII, and POSIX sockets.  
> Built as a 2nd Semester college project to apply core CS concepts in a real-world security tool.

---

## 📌 Overview

This tool takes a target IP address and a port range as input, then uses multithreading to attempt a TCP `connect()` on each port simultaneously. If the 3-way handshake succeeds, the port is flagged as **open**.

```
User enters IP + port range
        ↓
Ports checked in batches of 200 threads simultaneously
        ↓
connect() returns 0   →  PORT IS OPEN  ✅
connect() returns -1  →  Closed / Filtered by firewall  ❌
        ↓
All open ports sorted and printed
```

---

## ⚙️ Build & Run

**Requirements:** Linux, g++ with C++17 support

```bash
# Clone the repo
git clone https://github.com/Starguyplayzz/TCP-Port-Scanner.git
cd TCP-Port-Scanner

# Compile
g++ -std=c++17 -o scanner port_scanner.cpp -lpthread

# Run
./scanner
```

You'll be prompted to enter:
- Target IP address (e.g. `127.0.0.1`)
- Start port (e.g. `1`)
- End port (e.g. `1024`)

---

## 🏗️ Concepts & Implementation

| Concept | Implementation |
|---|---|
| **RAII** | `Socket` class acquires a socket in constructor, releases it in destructor — automatically, no manual `close()` needed |
| **Abstract Class** | `Scanner` base class with pure virtual `scan() = 0` — cannot be instantiated directly |
| **Inheritance** | `TCPScanner` inherits from `Scanner`, gets `targetIP`, `startPort`, `endPort` without redefining them |
| **Polymorphism** | `Scanner*` base class pointer holds a `TCPScanner` object — runtime dispatch calls the right `scan()` |
| **Multithreading** | `std::thread` spawns 200 threads per batch to check ports in parallel |
| **Mutex** | `std::mutex` + `std::lock_guard` prevents race conditions when multiple threads write to `openPorts` simultaneously |
| **POSIX Sockets** | `socket()`, `connect()`, `setsockopt()`, `inet_pton()`, `htons()` via Linux system headers |

---

## 🧱 Class Structure

```
Scanner  (Abstract Base Class)
│
│   # Protected:
│   targetIP, startPort, endPort
│   virtual scan() = 0
│
└── TCPScanner  (Derived Class)
        │
        │   # Private:
        │   vector<int> openPorts
        │   mutex mtx
        │   void checkPort(int port)   ← runs in each thread
        │
        └── void scan() override       ← spawns threads in batches
```

---

## 🔎 How `checkPort()` Works

Each thread calls `checkPort(port)` with a single port number:

1. **Create socket** — RAII `Socket` object opens it automatically
2. **Set timeout** — `setsockopt()` limits wait to 1 second per port (handles filtered/firewalled ports)
3. **Build address struct** — `htons()` converts port to network byte order, `inet_pton()` converts IP string to binary
4. **Attempt TCP connect** — `connect()` tries the full 3-way handshake
5. **Thread-safe write** — if port is open, `lock_guard` locks the mutex before writing to `openPorts`
6. **Auto-cleanup** — `Socket` destructor closes the socket, `lock_guard` destructor unlocks the mutex

---

## 📁 Project Structure

```
TCP-Port-Scanner/
├── port_scanner.cpp    # Full source code (heavily commented)
└── README.md
```

---

## 🧪 Example Output

```
========================================
       TCP Port Scanner  v1.0
       Built with C++ OOP + Sockets
========================================

Enter target IP address (e.g. 127.0.0.1): 127.0.0.1
Enter start port (e.g. 1):    1
Enter end port   (e.g. 1024): 1024

[*] Starting TCP Scan on: 127.0.0.1
[*] Port range: 1 → 1024
[*] Please wait, scanning...

========================================
           SCAN RESULTS
========================================
  [OPEN]  Port 22
  [OPEN]  Port 80
  [OPEN]  Port 443
========================================
Total open ports found: 3
========================================
```

---

## ⚠️ Legal Disclaimer

This tool was built for **educational purposes only.**  
Only scan systems you **own** or have **explicit written permission** to test.  
Unauthorized port scanning may be **illegal** in your country.  
The author is not responsible for any misuse of this tool.

---

## 👤 Author

**Rudra** — Computer Science Student | Cybersecurity Enthusiast  
2nd Semester Project · Built and tested on Linux (Kali)

---

## 🏷️ Tags

`cpp` `networking` `port-scanner` `cybersecurity` `multithreading` `oop` `raii` `posix-sockets` `linux` `college-project`
