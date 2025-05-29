#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <regex>

// Error handling function
void error_exit(const std::string &message)
{
    perror(message.c_str());
    exit(EXIT_FAILURE);
}

enum class IpType
{
    IPv4,
    IPv6,
    Any
};

sockaddr_storage parse(const std::string &ip_str, const uint16_t port,
                       IpType type)
{
    addrinfo hints{};
    switch (type)
    {
    case IpType::IPv4:
        hints.ai_family = AF_INET; // IPv4
        break;
    case IpType::IPv6:
        hints.ai_family = AF_INET6; // IPv6
        break;
    default:
        hints.ai_family = AF_UNSPEC; // Any address family
        break;
    }

    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP; // TCP

    addrinfo *address_result;
    int errcode = getaddrinfo(ip_str.c_str(), nullptr, &hints, &address_result);
    if (errcode != 0)
    {
        throw std::runtime_error("getaddrinfo(): " +
                                 std::string(gai_strerror(errcode)));
    }

    std::cout << "getaddrinfo() succeeded for " << ip_str << ":" << port << std::endl;

    // Convert to sockaddr_storage
    sockaddr_storage address;
    if (address_result->ai_family == AF_INET)
    {
        sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(&address);
        addr_in->sin_family = AF_INET;
        addr_in->sin_port = htons(port);
        memcpy(&addr_in->sin_addr, &reinterpret_cast<sockaddr_in *>(address_result->ai_addr)->sin_addr, sizeof(in_addr));
    }
    else if (address_result->ai_family == AF_INET6)
    {
        sockaddr_in6 *addr_in6 = reinterpret_cast<sockaddr_in6 *>(&address);
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_port = htons(port);
        memcpy(&addr_in6->sin6_addr, &reinterpret_cast<sockaddr_in6 *>(address_result->ai_addr)->sin6_addr, sizeof(in6_addr));
    }
    else
    {
        throw std::runtime_error("Unsupported address family");
    }

    freeaddrinfo(address_result);
    return address;
}

void send_msg(int socket, const std::string &msg)
{
    ssize_t bytes_to_send = msg.length();
    ssize_t bytes_sent = 0;
    while (bytes_to_send > 0)
    {
        ssize_t result = send(socket, msg.c_str() + bytes_sent, bytes_to_send, 0);
        if (result < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                // Interrupted or temporary failure, retry
                continue;
            }
            throw std::runtime_error("send() failed");
        }
        bytes_sent += result;
        bytes_to_send -= result;

        std::cout << "Sent: " << bytes_sent << "." << std::endl;
        if (bytes_to_send > 0)
        {
            std::cout << "Remaining bytes to send: " << bytes_to_send << "/" << msg.size() << "." << std::endl;
        }
    }

    std::cout << "All bytes sent successfully." << std::endl;
}

const static std::string CLRF = "\r\n";

static std::string ADD_CLRF(const std::string &str)
{
    return str + CLRF;
}

static std::string WAIT_FOR_MSG(int socket)
{
    std::string response;
    do
    {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                // Interrupted or temporary failure, retry
                continue;
            }
            throw std::runtime_error("recv() failed");
        }

        if (bytes_received == 0)
        {
            throw std::runtime_error("Server disconnected unexpectedly");
        }

        std::string chunk(buffer, bytes_received);
        response += chunk;

        std::cout << "Received: " << bytes_received << " bytes." << std::endl;
        std::cout << "Current response: " << response << std::endl;
    } while (response.find(CLRF) == std::string::npos);

    std::cout << "Full response received: " << '\"' << response << '\"' << std::endl;

    // Remove trailing CLRF
    if (response.size() >= CLRF.size() && response.compare(response.size() - CLRF.size(), CLRF.size(), CLRF) == 0)
    {
        response.erase(response.size() - CLRF.size());
    }
    else
    {
        throw std::runtime_error("Response does not end with expected CLRF");
    }

    return response;
}

std::pair<int, int> connect_to_server(const std::string &server_ip, int port, IpType type, int recv_buf_size = 1)
{
    // Get addr info
    auto address = parse(server_ip, port, type);

    // Determine address family
    int family = AF_INET;
    if (((sockaddr *)&address)->sa_family == AF_INET6)
    {
        family = AF_INET6;
    }

    // Create socket
    int client_fd = socket(family, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        throw std::runtime_error("Client socket creation failed: " + std::string(strerror(errno)));
    }

    // Set buff as small as possible
    std::cout << "Client: Setting send buffer size to: " << recv_buf_size << std::endl;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size)) < 0)
    {
        close(client_fd);
        throw std::runtime_error("setsockopt(SO_RCVBUF) failed: " + std::string(strerror(errno)));
    }

    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) < 0)
    {
        throw std::runtime_error("getsockopt(SO_RCVBUF) failed");
    }
    std::cout << "Client: Receiving buffer size set to: " << optval << std::endl;

    if (optval > 10000)
    {
        throw std::runtime_error("Client: Receiving buffer size is too large. This test is pointless for you. You can try to set it so the whole SCORING message won't fit in it.");
    }

    // Connect to server
    socklen_t addrlen = (family == AF_INET6) ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    if (connect(client_fd, (struct sockaddr *)&address, addrlen) < 0)
    {
        close(client_fd);
        throw std::runtime_error("Client connection failed: " + std::string(strerror(errno)));
    }

    std::cout << "Client: Connected to server" << std::endl;
    return std::make_pair(client_fd, optval);
}

bool compare_coeffs(const std::string &a, const std::string &b)
{
    std::regex coeff_regex("^COEFF( -?\\d+(\\.\\d{0,7})?)+$");
    if (!std::regex_match(a, coeff_regex))
    {
        throw std::invalid_argument("Invalid COEFF message format: " + a);
    }

    if (!std::regex_match(b, coeff_regex))
    {
        throw std::invalid_argument("Invalid COEFF message format: " + b);
    }

    std::string a_coeffs = a.substr(6); // Remove "COEFF "
    std::string b_coeffs = b.substr(6); // Remove "COEFF "

    std::istringstream a_stream(a_coeffs);
    std::istringstream b_stream(b_coeffs);
    std::string a_coeff, b_coeff;
    while (std::getline(a_stream, a_coeff, ' ') && std::getline(b_stream, b_coeff, ' '))
    {
        double a_value = std::stod(a_coeff);
        double b_value = std::stod(b_coeff);
        if (std::abs(a_value - b_value) > 1e-7) // Allow for small floating-point precision errors
        {
            std::cout << "Coefficient mismatch: " << a_value << " != " << b_value << std::endl;
            return false;
        }
    }

    return true; // All coefficients match
}

// Client function
void run_client(const int client_fd, const int recv_buff_size, const std::string &coeff_response)
{
    std::cout << "Client: Connected to server" << std::endl;

    std::cout << "Client: Sending HELLO message" << std::endl;
    send_msg(client_fd, ADD_CLRF("HELLO player"));

    std::cout << "Client: Waiting for server response..." << std::endl;
    auto response = WAIT_FOR_MSG(client_fd);

    std::cout << "Client: Received response." << std::endl;
    std::cout << "ACT:\"" << response << "\"" << std::endl;
    std::cout << "EXP:\"" << coeff_response << "\"" << std::endl;

    if (!compare_coeffs(response, coeff_response))
    {
        throw std::runtime_error("Client: Response does not match expected coefficient response.");
    }

    std::cout << "Client: Response matches expected coefficient response." << std::endl;

    std::cout << "Client: Sending PUT message" << std::endl;
    send_msg(client_fd, ADD_CLRF("PUT 1 1"));

    // We sent the first and only put, so we expect the server to send scorings and disconnect (also STATE as the response).
    // However as we set really small receiving buffer it won't be able, so it should disconnect.
    std::cout << "Client: Waiting for server response..." << std::endl;

    size_t sleep_time = 2;
    std::cout << "Sleeping for " << sleep_time << " s." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(sleep_time));

    std::cout << "Server should have disconnected now." << std::endl;

    size_t total_received_bytes = 0;
    do
    {
        char buffer[1024];
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received == 0)
        {
            std::cout << "Client: Server has disconnected." << std::endl;
            return;
        }
        else if (bytes_received < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                // Interrupted or temporary failure, retry
                std::cout << "Client: recv() interrupted or temporary failure, retrying..." << std::endl;
            }
            else
            {
                throw std::runtime_error("recv() failed: " + std::string(strerror(errno)));
            }
        }
        else
        {
            std::cout << "Client: Received " << bytes_received << " bytes." << std::endl;
            total_received_bytes += static_cast<size_t>(bytes_received);
            std::cout << "Client: Total received bytes: " << total_received_bytes << "/" << recv_buff_size << std::endl;
        }
    } while (total_received_bytes <= recv_buff_size);

    throw std::runtime_error("Client: Received more data than expected. Server should have disconnected by now.");
}

int main(int argc, char *argv[])
{
    std::cout << "Starting TCP force-close test..." << std::endl;

    // Print args
    std::cout << "Arguments: ";
    for (int i = 0; i < argc; ++i)
    {
        std::cout << argv[i] << "\n";
    }

    std::cout << "\n";

    // Get server address and port from command line arguments if provided
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " [server_ip] [port] [IPv4|IPv6|Any] [COEFF response]" << std::endl;
        return 1;
    }

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    IpType type = IpType::Any;

    std::string type_str = argv[3];
    if (type_str == "IPv4")
    {
        type = IpType::IPv4;
    }
    else if (type_str == "IPv6")
    {
        type = IpType::IPv6;
    }
    else if (type_str == "Any")
    {
        type = IpType::Any;
    }
    else
    {
        std::cerr << "Unknown IP type: " << type_str << std::endl;
        return 1;
    }

    std::string coeff_response = argv[4];

    std::cout << "Using server IP: " << ip << ", Port: " << port << std::endl;

    // Run client in main thread
    auto [socket, recv_buf_size] = connect_to_server(ip, port, type);

    try
    {
        run_client(socket, recv_buf_size, coeff_response);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        close(socket);
        return 1;
    }

    // TODO: close(socket);

    std::cout << "Test completed." << std::endl;
    return 0;
}
