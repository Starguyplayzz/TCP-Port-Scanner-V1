// =====================================================================
//  TCP PORT SCANNER — C++ Project
//  Concepts used: OOP, Classes, Inheritance, RAII, Multithreading
//  Build command: g++ -std=c++17 -o scanner port_scanner.cpp -lpthread
//  Run command:   ./scanner
// =====================================================================

#include <iostream>       // For cout (printing to screen) and cin (taking input)
#include <string>         // For using std::string (text variables)
#include <vector>         // For using std::vector (a dynamic list/array)
#include <thread>         // For using std::thread (running things in parallel)
#include <mutex>          // For using std::mutex (preventing thread crashes)
#include <algorithm>      // For using std::sort (sorting our open ports list)

// These below are Linux-specific headers for socket (networking) programming
#include <sys/socket.h>   // Core socket functions: socket(), connect(), close()
#include <netinet/in.h>   // The sockaddr_in struct — holds IP address and port
#include <arpa/inet.h>    // inet_pton() — converts IP string like "192.168.1.1" to binary
#include <unistd.h>       // close() function to close a socket
#include <sys/time.h>     // timeval struct — used to set timeout on connections


// =====================================================================
//  CLASS 1: Socket
//  PURPOSE: RAII wrapper around a raw Linux socket
//
//  WHAT IS RAII?
//  RAII = Resource Acquisition Is Initialization
//  Simple meaning: when the object is CREATED, it opens the socket
//                  when the object is DESTROYED, it closes the socket
//  We never have to manually close sockets — C++ handles it for us!
// =====================================================================

class Socket {

private:
    // sockfd = socket file descriptor
    // In Linux, every socket is just a number (called file descriptor)
    // Think of it like a "ticket number" Linux gives us to track our socket
    int sockfd;

public:

    // CONSTRUCTOR — runs automatically when we write: Socket s;
    Socket() {
        // socket() is a Linux system call that creates a new TCP socket
        // AF_INET    = we are using IPv4 (like 192.168.x.x addresses)
        // SOCK_STREAM = we want TCP (reliable, connection-based)
        // 0          = use default protocol (TCP in this case)
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
    }

    // DESTRUCTOR — runs automatically when the Socket object goes out of scope
    // This is the RAII magic — we don't have to call close() ourselves ever!
    ~Socket() {
        if (sockfd >= 0) {
            close(sockfd); // Tell Linux: "we're done with this socket, free it"
        }
    }

    // get() — returns the raw socket number so other functions can use it
    int get() const {
        return sockfd;
    }

    // isValid() — returns true if socket was created successfully
    // If socket() fails, it returns -1, so we check for that
    bool isValid() const {
        return sockfd >= 0;
    }

    // We DELETE copy constructor and copy assignment
    // Reason: two objects should NOT share the same socket
    // If one closes it, the other would have a dangling (invalid) socket
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};


// =====================================================================
//  CLASS 2: Scanner (BASE CLASS — also called Abstract Class)
//  PURPOSE: A blueprint/template that all types of scanners must follow
//
//  WHY ABSTRACT?
//  Because we declared scan() as a "pure virtual function" (= 0)
//  This means: every class that inherits from Scanner MUST implement scan()
//  You cannot create a Scanner object directly — it's just a blueprint
// =====================================================================

class Scanner {

protected:
    // protected = accessible by this class AND its child classes
    // (unlike private which only this class can access)
    std::string targetIP;   // The IP address we want to scan
    int startPort;          // First port in the range
    int endPort;            // Last port in the range

public:

    // CONSTRUCTOR — initializes the scanner with IP and port range
    Scanner(std::string ip, int start, int end) {
        targetIP = ip;
        startPort = start;
        endPort = end;
    }

    // PURE VIRTUAL FUNCTION — this is what makes Scanner abstract
    // Any class inheriting from Scanner MUST override and implement this
    // The "= 0" at the end means: "no implementation here, child must do it"
    virtual void scan() = 0;

    // Virtual destructor — important rule when using inheritance + pointers
    // Without this, deleting a child object via base class pointer causes memory leak
    virtual ~Scanner() {}
};


// =====================================================================
//  CLASS 3: TCPScanner (DERIVED/CHILD CLASS)
//  PURPOSE: The actual scanner — inherits from Scanner and does the work
//
//  "public Scanner" means TCPScanner IS-A Scanner (inheritance)
//  It gets all the protected variables: targetIP, startPort, endPort
//  And it MUST implement scan() because Scanner forced it to
// =====================================================================

class TCPScanner : public Scanner {

private:

    // List to store all ports we find open during the scan
    std::vector<int> openPorts;

    // mutex = mutual exclusion lock
    // Since many threads run checkPort() at the same time,
    // we need this to make sure only ONE thread writes to openPorts at a time
    // Without this, two threads writing simultaneously = crash/data corruption
    std::mutex mtx;


    // ------------------------------------------------------------------
    // checkPort() — checks if ONE specific port is open
    // This function runs inside a separate THREAD for each port
    // ------------------------------------------------------------------
    void checkPort(int port) {

        // Step 1: Create a socket using our RAII Socket class
        // When this function ends, Socket destructor auto-closes the socket
        Socket sock;

        // If socket creation failed, skip this port entirely
        if (!sock.isValid()) return;


        // Step 2: Set a timeout so we don't wait forever on dead ports
        // Without timeout, a filtered port would make us wait indefinitely
        struct timeval timeout;
        timeout.tv_sec  = 1;  // Wait max 1 second per port
        timeout.tv_usec = 0;  // 0 microseconds extra

        // Apply the timeout to our socket
        // SOL_SOCKET  = we are setting socket-level option
        // SO_SNDTIMEO = timeout for sending data
        // SO_RCVTIMEO = timeout for receiving data
        setsockopt(sock.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));


        // Step 3: Set up the server address structure
        // sockaddr_in is a struct that bundles together: IP + port + address family
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET; // IPv4

        // htons() = host to network short
        // Converts port number from your computer's format to network standard format
        // Different computers store numbers differently (big-endian vs little-endian)
        // htons() ensures the port is in the correct format for the network
        serverAddr.sin_port = htons(port);

        // inet_pton() = "presentation to network"
        // Converts IP string like "192.168.1.1" into 4-byte binary format
        // AF_INET       = IPv4
        // targetIP.c_str() = converts our std::string to C-style char array
        inet_pton(AF_INET, targetIP.c_str(), &serverAddr.sin_addr);


        // Step 4: Try to connect to this port
        // connect() attempts a full TCP handshake with the target
        // Returns  0 = connection successful = PORT IS OPEN
        // Returns -1 = connection failed = port is closed or filtered
        int result = connect(
            sock.get(),                         // our socket
            (struct sockaddr*)&serverAddr,       // where to connect (IP + port)
            sizeof(serverAddr)                  // size of the address struct
        );

        // Step 5: If connection succeeded, this port is OPEN
        if (result == 0) {

            // lock_guard is RAII for mutex
            // It LOCKS the mutex here and UNLOCKS it automatically when it goes out of scope
            // This means only one thread can add to openPorts at a time — no crashes!
            std::lock_guard<std::mutex> lock(mtx);

            openPorts.push_back(port); // Add this open port to our list
        }

        // When function ends:
        // 1. lock_guard destructor runs → mutex unlocked
        // 2. Socket destructor runs → socket closed
        // ALL automatic — this is RAII in action!
    }


public:

    // CONSTRUCTOR
    // ": Scanner(ip, start, end)" calls the parent class constructor
    // This is how we pass the IP and port range up to the base class
    TCPScanner(std::string ip, int start, int end) : Scanner(ip, start, end) {}


    // scan() — overrides the pure virtual function from Scanner base class
    // "override" keyword tells compiler: yes, I am intentionally overriding this
    void scan() override {

        std::cout << "\n[*] Starting TCP Scan on: " << targetIP << std::endl;
        std::cout << "[*] Port range: " << startPort << " → " << endPort << std::endl;
        std::cout << "[*] Please wait, scanning...\n" << std::endl;

        // We scan in BATCHES of threads
        // Why not one thread per port? If endPort is 65535, we'd create 65535 threads
        // That would crash the system — OS has a limit on threads and file descriptors
        // Instead we do: 200 threads at a time, wait for them, then do next 200
        const int batchSize = 200;

        std::vector<std::thread> threads; // holds our active threads

        for (int port = startPort; port <= endPort; port++) {

            // Launch a new thread to check this port
            // &TCPScanner::checkPort = pointer to our checkPort function
            // this  = tells the thread which object's checkPort to call
            // port  = the port number to pass into checkPort
            threads.push_back(
                std::thread(&TCPScanner::checkPort, this, port)
            );

            // When batch is full OR we've reached the last port
            // wait for ALL threads in this batch to finish
            if ((int)threads.size() >= batchSize || port == endPort) {

                for (auto& t : threads) {
                    t.join(); // join() = "wait for this thread to finish"
                }

                threads.clear(); // empty the batch, ready for next round
            }
        }

        // Sort open ports from lowest to highest for clean output
        std::sort(openPorts.begin(), openPorts.end());

        // Print results
        std::cout << "\n========================================\n";
        std::cout << "           SCAN RESULTS                 \n";
        std::cout << "========================================\n";

        if (openPorts.empty()) {
            std::cout << "No open ports found in this range.\n";
        } else {
            for (int port : openPorts) {
                std::cout << "  [OPEN]  Port " << port << "\n";
            }
        }

        std::cout << "========================================\n";
        std::cout << "Total open ports found: " << openPorts.size() << "\n";
        std::cout << "========================================\n";
    }
};


// =====================================================================
//  MAIN FUNCTION — Entry point of the program
//  Execution always starts here
// =====================================================================

int main() {

    // Variables to store user input
    std::string ip;
    int startPort, endPort;

    // Display header
    std::cout << "========================================\n";
    std::cout << "       TCP Port Scanner  v1.0           \n";
    std::cout << "       Built with C++ OOP + Sockets     \n";
    std::cout << "========================================\n\n";

    // Take input from user
    std::cout << "Enter target IP address (e.g. 127.0.0.1): ";
    std::cin >> ip;

    std::cout << "Enter start port (e.g. 1):    ";
    std::cin >> startPort;

    std::cout << "Enter end port   (e.g. 1024): ";
    std::cin >> endPort;

    // Validate port range
    if (startPort < 1 || endPort > 65535 || startPort > endPort) {
        std::cout << "\n[ERROR] Invalid port range! Ports must be between 1 and 65535.\n";
        return 1; // Exit with error code 1
    }

    // Create a TCPScanner object
    // We use Scanner* (base class pointer) pointing to a TCPScanner object
    // This demonstrates POLYMORPHISM — the base class pointer can hold child objects
    Scanner* scanner = new TCPScanner(ip, startPort, endPort);

    // Call scan() — because of virtual functions, C++ knows to call TCPScanner::scan()
    // even though scanner is a Scanner* pointer. This is called DYNAMIC DISPATCH.
    scanner->scan();

    // Free the memory we allocated with new
    // This also calls TCPScanner's destructor automatically
    delete scanner;

    return 0; // 0 means program ended successfully

}
