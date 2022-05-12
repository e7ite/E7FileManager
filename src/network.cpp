#include "network.hpp"

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/types/span.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <regex>
#include <string>
#include <string_view>
#include <vector>

bool IsHTTPAddress(const Glib::ustring &address) {
  static std::regex kHttpRegexMatcher(
      R"(http:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*))");

  bool was_match;
  try {
    was_match = std::regex_match(std::string(address), kHttpRegexMatcher);
  } catch (const std::regex_error &error) {
    return false;
  }
  return was_match;
}

NetworkAddressInfo::NetworkAddressInfo(addrinfo *posix_linked_list)
    : deleter_(+[](addrinfo *posix_linked_list) {
        if (posix_linked_list != nullptr) freeaddrinfo(posix_linked_list);
      }) {
  for (addrinfo *info_node = posix_linked_list; info_node != nullptr;
       info_node = info_node->ai_next)
    this->info_nodes_.push_back(NetworkAddressInfoNode(info_node));
}

NetworkAddressInfo::NetworkAddressInfo(std::initializer_list<int> test_data) {
  for (int test_data_node : test_data)
    this->info_nodes_.push_back(NetworkAddressInfoNode(test_data_node));
}

NetworkAddressInfoNode::NetworkAddressInfoNode(addrinfo *posix_info_node)
    : posix_info_node_(posix_info_node) {}

NetworkAddressInfoNode::NetworkAddressInfoNode(int test_data)
    : test_info_node_(test_data) {}

NetworkAddressInfo::NetworkAddressInfo(NetworkAddressInfo &&address_info) {
  this->info_nodes_ = std::move(address_info.info_nodes_);
}

NetworkAddressInfo &NetworkAddressInfo::operator=(
    NetworkAddressInfo &&address_info) {
  this->info_nodes_ = std::move(address_info.info_nodes_);
  return *this;
}

NetworkAddressInfo::~NetworkAddressInfo() {
  // Deleter will not be empty if initialized by a POSIX linked list node.
  if (deleter_ != nullptr && !this->info_nodes_.empty())
    deleter_(this->info_nodes_.begin()->posix_info_node_);
}

absl::StatusOr<NetworkAddressInfo>
POSIXNetworkInterface::GetAvailableAddressesForEndpoint(
    std::string_view node, std::string_view service) {
  addrinfo hints;
  hints.ai_family = AF_UNSPEC;      // Use IPv4 or IPv6 protocol family/domain
  hints.ai_flags = 0;               // Do not narrow down any further with flags
  hints.ai_protocol = 0;            // Use any protocol for the socket
  hints.ai_socktype = SOCK_STREAM;  // Use TCP (connection-oriented) sockets

  addrinfo *matching_addresses;
  int status =
      ::getaddrinfo(node.data(), service.data(), &hints, &matching_addresses);
  if (status != 0)
    return absl::UnavailableError(
        absl::StrCat("::getaddrinfo(): ", gai_strerror(status)));

  return NetworkAddressInfo(/*posix_info_node=*/matching_addresses);
}

int POSIXNetworkInterface::CreateSocket(
    const NetworkAddressInfoNode &endpoint_info) {
  addrinfo *endpoint_node = endpoint_info.posix_info_node_;
  return ::socket(endpoint_node->ai_family, endpoint_node->ai_socktype,
                  endpoint_node->ai_protocol);
}

int POSIXNetworkInterface::ConnectSocketToEndpoint(
    int sockfd, const NetworkAddressInfoNode &endpoint_info) {
  addrinfo *endpoint_node = endpoint_info.posix_info_node_;
  return ::connect(sockfd, endpoint_node->ai_addr, endpoint_node->ai_addrlen);
}

int POSIXNetworkInterface::CloseSocket(int fd) { return ::close(fd); }

absl::StatusOr<size_t> POSIXNetworkInterface::SendData(int sockfd,
                                                       const void *buf,
                                                       size_t size) {
  size_t bytes_sent = ::send(sockfd, buf, size, 0);
  if (bytes_sent == -1)
    return absl::DataLossError(absl::StrCat("::send(): ", strerror(errno)));
  return bytes_sent;
}

absl::StatusOr<size_t> POSIXNetworkInterface::RecvData(int sockfd, void *buf,
                                                       size_t size) {
  ssize_t bytes_received = ::recv(sockfd, buf, size, 0);
  if (bytes_received == -1)
    return absl::DataLossError(absl::StrCat("::recv(): ", strerror(errno)));
  return bytes_received;
}

NetworkConnection::NetworkConnection(NetworkInterface &network_interface,
                                     int socket_fd, std::string host_name,
                                     short port)
    : connection_interface_(&network_interface),
      socket_fd_(socket_fd),
      host_name_(std::move(host_name)),
      port_(port) {}

NetworkConnection::~NetworkConnection() {
  if (this->connection_interface_ != nullptr && this->socket_fd_ != -1)
    this->connection_interface_->CloseSocket(this->socket_fd_);
}

NetworkConnection::NetworkConnection(NetworkConnection &&connection) {
  this->port_ = connection.port_;
  this->socket_fd_ = connection.socket_fd_;
  this->connection_interface_ = std::move(connection.connection_interface_);
  this->host_name_ = connection.host_name_;
}

NetworkConnection &NetworkConnection::operator=(
    NetworkConnection &&connection) {
  this->port_ = connection.port_;
  this->socket_fd_ = connection.socket_fd_;
  this->connection_interface_ = std::move(connection.connection_interface_);
  this->host_name_ = connection.host_name_;
  return *this;
}

absl::StatusOr<NetworkConnection> NetworkConnection::Create(
    NetworkInterface &net_interface, std::string_view host_name, short port) {
  absl::StatusOr<NetworkAddressInfo> available_addresses =
      net_interface.GetAvailableAddressesForEndpoint(host_name,
                                                     std::to_string(port));
  if (!available_addresses.ok()) return available_addresses.status();

  int socket_fd;
  for (const NetworkAddressInfoNode &address_info : *available_addresses) {
    if ((socket_fd = net_interface.CreateSocket(address_info)) == -1) continue;

    if (!net_interface.ConnectSocketToEndpoint(socket_fd, address_info)) {
      return NetworkConnection(net_interface, socket_fd, std::string(host_name),
                               port);
    }

    net_interface.CloseSocket(socket_fd);
  }

  delete &net_interface;
  return absl::InternalError("Failed to create an endpoint for communication!");
}

absl::Status NetworkConnection::Send(absl::Span<const char> bytes_to_send) {
  if (socket_fd_ == -1)
    return absl::InternalError("Socket not connected to any endpoint!");
  if (bytes_to_send.empty())
    return absl::InvalidArgumentError("Bytes to send cannot be empty!");

  size_t total_bytes_sent = 0;
  while (1) {
    absl::StatusOr<size_t> bytes_sent = connection_interface_->SendData(
        socket_fd_, bytes_to_send.begin() + total_bytes_sent,
        bytes_to_send.size() - total_bytes_sent);
    if (!bytes_sent.ok())
      return absl::DataLossError(absl::StrCat(bytes_sent.status().message(),
                                              ": Only ", total_bytes_sent,
                                              " were sent to endpoint!"));
    total_bytes_sent += *bytes_sent;
    if (total_bytes_sent == bytes_to_send.size()) break;
  }

  return absl::OkStatus();
}

absl::StatusOr<std::vector<char>> NetworkConnection::Recv() {
  if (socket_fd_ == -1)
    return absl::InternalError("Socket not connected to any endpoint!");

  std::vector<char> result;
  std::vector<char> buf(/*size=*/50, /*allocator=*/0);
  while (1) {
    absl::StatusOr<size_t> bytes_received =
        this->connection_interface_->RecvData(this->socket_fd_, buf.data(),
                                              buf.size());
    if (!bytes_received.ok()) return bytes_received.status();
    if (*bytes_received == 0) break;

    result.insert(result.end(), buf.begin(), buf.begin() + *bytes_received);
  }

  return result;
}