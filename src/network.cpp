#include "network.hpp"

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/types/span.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <vector>

NetworkAddressInfo::NetworkAddressInfo(NetworkAddressInfo &&address_info) {
  this->info_node = address_info.info_node;
  address_info.info_node = nullptr;
}

NetworkAddressInfo &NetworkAddressInfo::operator=(
    NetworkAddressInfo &&address_info) {
  this->info_node = address_info.info_node;
  address_info.info_node = nullptr;
  return *this;
}

NetworkAddressInfo::~NetworkAddressInfo() {
  if (this->info_node != nullptr) freeaddrinfo(this->info_node);
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
  absl::StatusOr<std::vector<NetworkAddressInfo>> available_addresses =
      net_interface.GetAvailableAddressesForEndpoint(host_name,
                                                     std::to_string(port));
  if (!available_addresses.ok()) return available_addresses.status();

  int socket_fd;
  for (const NetworkAddressInfo &address_info : *available_addresses) {
    if ((socket_fd = net_interface.CreateSocket(address_info)) == -1) continue;

    if (!net_interface.ConnectSocketToEndpoint(socket_fd, address_info)) {
      return NetworkConnection(net_interface, socket_fd, std::string(host_name),
                               port);
    }

    net_interface.CloseSocket(socket_fd);
  }

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