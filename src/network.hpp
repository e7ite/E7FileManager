#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/types/span.h>
#include <glibmm/ustring.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

// Contains information about a network endpoint. Can be extended to hold
// data from different systems.
struct NetworkAddressInfo {
  addrinfo *info_node = nullptr;
  int test_data = 0;  // Should not be used other than for testing.

  explicit NetworkAddressInfo(addrinfo *posix_info_node)
      : info_node(posix_info_node) {}
  explicit NetworkAddressInfo(int test_data) : test_data(test_data) {}

  NetworkAddressInfo(const NetworkAddressInfo &) = delete;
  NetworkAddressInfo &operator=(const NetworkAddressInfo &) = delete;

  NetworkAddressInfo(NetworkAddressInfo &&);
  NetworkAddressInfo &operator=(NetworkAddressInfo &&);

  ~NetworkAddressInfo();
};

// Interface that can be used to query the system's networking API.
// Implementation should wrap the system's networking API calls and transform
// their results to function with this interface.
class NetworkInterface {
 public:
  virtual ~NetworkInterface() = default;

  virtual absl::StatusOr<std::vector<NetworkAddressInfo>>
  GetAvailableAddressesForEndpoint(std::string_view endpoint_name,
                                   std::string_view service) = 0;
  virtual int CreateSocket(const NetworkAddressInfo &endpoint_info) = 0;
  virtual int ConnectSocketToEndpoint(
      int sockfd, const NetworkAddressInfo &endpoint_info) = 0;
  virtual int CloseSocket(int fd) = 0;
  virtual absl::StatusOr<size_t> SendData(int sockfd, const void *buf,
                                          size_t size) = 0;
  virtual absl::StatusOr<size_t> RecvData(int sockfd, void *buf,
                                          size_t size) = 0;
};

class POSIXNetworkInterface : public NetworkInterface {
 public:
  absl::StatusOr<std::vector<NetworkAddressInfo>>
  GetAvailableAddressesForEndpoint(std::string_view endpoint_name,
                                   std::string_view service) override;
  int CreateSocket(const NetworkAddressInfo &endpoint_info) override;
  int ConnectSocketToEndpoint(int sockfd,
                              const NetworkAddressInfo &endpoint_info) override;
  int CloseSocket(int fd) override;
  absl::StatusOr<size_t> SendData(int sockfd, const void *buf,
                                  size_t size) override;
  absl::StatusOr<size_t> RecvData(int sockfd, void *buf, size_t size) override;
};

// Represents am established network connection to a network endpoint and to
// have two-way communcation with it. Does not handle any protocol-specific
// communication, just handles sending and receiving data from it. These can be
// created for each connection to a network endpoint.
class NetworkConnection {
 public:
  ~NetworkConnection();

  NetworkConnection(const NetworkConnection &) = delete;
  NetworkConnection &operator=(const NetworkConnection &) = delete;

  NetworkConnection(NetworkConnection &&);
  NetworkConnection &operator=(NetworkConnection &&);

  // Establishes a network connection, which can be used to send and receive
  // data via the Send() and Recv() methods. Will take ownership of the network
  // interface.
  static absl::StatusOr<NetworkConnection> Create(
      NetworkInterface &network_interface, std::string_view host_name,
      short port);

  // Can be used to send a blob of data to the network endpoint. Returns
  // absl::OkStatus() if all the bytes in bytes_to_send were sent successfully,
  // and returns a specific error if a failure has occurred.
  absl::Status Send(absl::Span<const char> bytes_to_send);

  // Can be used to receive a blob of data from the network endpoint. Returns
  // absl::OkStatus() if all the bytes were retrieved successfully, else returns
  // a specific error if a failure has occurred.
  absl::StatusOr<std::vector<char>> Recv();

 private:
  NetworkConnection(NetworkInterface &network_interface, int socket_fd,
                    std::string host_name, short port);

  std::unique_ptr<NetworkInterface> connection_interface_;
  int socket_fd_ = -1;
  std::string host_name_;
  short port_ = -1;
};

#endif