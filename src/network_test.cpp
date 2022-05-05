#include "network.hpp"

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/types/span.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <vector>

namespace absl {
template <typename T>
void PrintTo(const ::absl::StatusOr<T>& data, std::ostream* os) {
  *os << data.status();
}
}  // namespace absl

namespace {

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

inline const ::absl::Status& GetStatus(const ::absl::Status& status) {
  return status;
}

template <typename T>
inline const ::absl::Status& GetStatus(const ::absl::StatusOr<T>& status) {
  return status.status();
}

// Monomorphic implementation of matcher IsOkAndHolds(m).  StatusOrType is a
// reference to StatusOr<T>.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  typedef
      typename std::remove_reference<StatusOrType>::type::value_type value_type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const value_type&>(
            std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream* os) const override {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      StatusOrType actual_value,
      ::testing::MatchResultListener* result_listener) const override {
    if (!actual_value.ok()) {
      *result_listener << "which has status " << actual_value.status();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        inner_matcher_.MatchAndExplain(*actual_value, &inner_listener);
    const std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *result_listener << "which contains value "
                       << ::testing::PrintToString(*actual_value) << ", "
                       << inner_explanation;
    }
    return matches;
  }

 private:
  const ::testing::Matcher<const value_type&> inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the
  // given type.  StatusOrType can be either StatusOr<T> or a
  // reference to StatusOr<T>.
  template <typename StatusOrType>
  operator ::testing::Matcher<StatusOrType>() const {  // NOLINT
    return ::testing::Matcher<StatusOrType>(
        new IsOkAndHoldsMatcherImpl<const StatusOrType&>(inner_matcher_));
  }

 private:
  const InnerMatcher inner_matcher_;
};

// Monomorphic implementation of matcher IsOk() for a given type T.
// T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoIsOkMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  void DescribeTo(std::ostream* os) const override { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK";
  }
  bool MatchAndExplain(T actual_value,
                       ::testing::MatchResultListener*) const override {
    return GetStatus(actual_value).ok();
  }
};

// Implements IsOk() as a polymorphic matcher.
class IsOkMatcher {
 public:
  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT
    return ::testing::Matcher<T>(new MonoIsOkMatcherImpl<T>());
  }
};

// Macros for testing the results of functions that return absl::Status or
// absl::StatusOr<T> (for any type T).
#define EXPECT_OK(expression) EXPECT_THAT(expression, IsOk())

// Returns a gMock matcher that matches a StatusOr<> whose status is
// OK and whose value matches the inner matcher.
template <typename InnerMatcher>
IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type> IsOkAndHolds(
    InnerMatcher&& inner_matcher) {
  return IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> which is OK.
inline IsOkMatcher IsOk() { return IsOkMatcher(); }

// Interface to mock server that can send and receive messages from the client.
// Accepts any number of bytes from the client and responds with the
// designated number of bytes specified when creating instance of class. Always
// accepts and receives bytes in random intervals.
class MockNetworkInterface : public NetworkInterface {
 public:
  MockNetworkInterface(size_t bytes_to_send_to_client)
      : bytes_remaining_to_send_(bytes_to_send_to_client) {}

  absl::StatusOr<std::vector<NetworkAddressInfo>>
  GetAvailableAddressesForEndpoint(std::string_view endpoint_name,
                                   std::string_view service) override {
    std::vector<NetworkAddressInfo> mock_address_information;
    if (endpoint_name == "meow.net") {
      mock_address_information.emplace_back(1);
      mock_address_information.emplace_back(2);
      mock_address_information.emplace_back(3);
      mock_address_information.emplace_back(4);
    } else if (endpoint_name == "poop.com") {
      mock_address_information.emplace_back(0);
      mock_address_information.emplace_back(5);
    }
    return mock_address_information;
  }

  int CreateSocket(const NetworkAddressInfo& endpoint_info) override {
    switch (endpoint_info.test_data) {
      case 2:
      case 3:
        return 25;
      default:
        return -1;
    }
    return -1;
  }

  int ConnectSocketToEndpoint(
      int sockfd, const NetworkAddressInfo& endpoint_info) override {
    switch (endpoint_info.test_data) {
      case 2:
      case 3:
        return 0;
      default:
        return -1;
    }
    return -1;
  }

  int CloseSocket(int fd) override {
    // Always succeed in closing the socket.
    return 0;
  }

  // Simulates successfully receiving a random amount of bytes from the client
  // to this mock server.
  absl::StatusOr<size_t> SendData(int sockfd, const void* buf,
                                  size_t size) override {
    size_t bytes_to_recv = rand() % size + 1;
    return bytes_to_recv;
  }

  // Simulates sending a random amount of bytes to the client.
  absl::StatusOr<size_t> RecvData(int sockfd, void* buf, size_t size) override {
    if (bytes_remaining_to_send_ == 0) return 0;

    size_t bytes_sent = std::min(rand() % bytes_remaining_to_send_ + 1, size);
    // Just act like a memset() in this case. No particular reason why.
    for (int i = 0; i < bytes_sent; i++) {
      reinterpret_cast<char*>(buf)[i] = 0;
    }
    bytes_remaining_to_send_ -= bytes_sent;
    return bytes_sent;
  }

 private:
  size_t bytes_remaining_to_send_;
};

TEST(NetworkConnectionTest, SucceedInCreatingAConnection) {
  size_t bytes_server_will_send = 10;
  const char* host = "meow.net";
  short port = 20;
  ASSERT_THAT(
      NetworkConnection::Create(
          *new MockNetworkInterface(bytes_server_will_send), host, port),
      IsOk());
}

TEST(NetworkConnectionTest, FailToCreateConnection) {
  size_t bytes_server_will_send = 10;
  const char* host = "poop.com";
  short port = 20;
  ASSERT_THAT(
      NetworkConnection::Create(
          *new MockNetworkInterface(bytes_server_will_send), host, port),
      Not(IsOk()));
}

TEST(NetworkConnectionTest, Send4Bytes) {
  size_t bytes_server_will_send = 10;
  const char* host = "meow.net";
  short port = 20;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  EXPECT_OK(connection->Send({0, 1, 2, 3}));
}

TEST(NetworkConnectionTest, Recv10Bytes) {
  size_t bytes_server_will_send = 10;
  const char* host = "meow.net";
  short port = 20;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  EXPECT_THAT(connection->Recv(), IsOkAndHolds(SizeIs(10)));
}

TEST(NetworkConnectionTest, Send4BytesAndRecv10Bytes) {
  size_t bytes_server_will_send = 10;
  const char* host = "meow.net";
  short port = 20;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  EXPECT_OK(connection->Send({0, 1, 2, 3}));
  EXPECT_THAT(connection->Recv(), IsOkAndHolds(SizeIs(10)));
}

TEST(NetworkConnectionTest, Send1MegaByte) {
  size_t bytes_server_will_send = 0;
  const char* host = "meow.net";
  short port = 20;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  size_t num_bytes_client_will_send = 1024;
  std::vector<char> bytes_to_send(num_bytes_client_will_send, /*allocator=*/0);
  EXPECT_OK(connection->Send(bytes_to_send));
}

TEST(NetworkConnectionTest, Recv1Megabyte) {
  size_t bytes_server_will_send = 1024;
  const char* host = "meow.net";
  short port = 4000;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  EXPECT_THAT(connection->Recv(), IsOkAndHolds(SizeIs(1024)));
}

TEST(NetworkConnectionTest, ErrorWhenSendingNoBytes) {
  size_t bytes_server_will_send = 0;
  const char* host = "meow.net";
  short port = 20;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  EXPECT_THAT(connection->Send({}), Not(IsOk()));
}

TEST(NetworkConnectionTest, DontFailWhenServerHasNoBytesToSend) {
  size_t bytes_server_will_send = 0;
  const char* host = "meow.net";
  short port = 20;
  absl::StatusOr<NetworkConnection> connection = NetworkConnection::Create(
      *new MockNetworkInterface(bytes_server_will_send), host, port);
  ASSERT_THAT(connection, IsOk());

  EXPECT_THAT(connection->Recv(), IsOkAndHolds(IsEmpty()));
}

TEST(IsHttpAddressTest, SucceedOnRegularHTTPAddress) {
  ASSERT_TRUE(IsHTTPAddress("http://google.com"));
  ASSERT_TRUE(IsHTTPAddress("http://google.com/"));
}

TEST(IsHttpAddressTest, FailOnRootDirectory) {
  ASSERT_FALSE(IsHTTPAddress("/"));
}

TEST(IsHttpAddressTest, FailOnMalformedURL) {
  ASSERT_FALSE(IsHTTPAddress("//"));
}

TEST(IsHttpAddressTest, FailOnEmptyDirectory) {
  ASSERT_FALSE(IsHTTPAddress(""));
}

TEST(IsHttpAddressTest, FailOnNestedDirectory) {
  ASSERT_FALSE(IsHTTPAddress("/dir"));
  ASSERT_FALSE(IsHTTPAddress("/dir/"));
}

TEST(IsHttpAddressTest, FailOnDoubleNestedDirectory) {
  ASSERT_FALSE(IsHTTPAddress("/dir/nesteddir"));
  ASSERT_FALSE(IsHTTPAddress("/dir/nesteddir/"));
}

TEST(IsHttpAddressTest, FailBecauseMissingHTTP) {
  ASSERT_FALSE(IsHTTPAddress("google.com"));
  ASSERT_FALSE(IsHTTPAddress("ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, FailOnHTTPS) {
  ASSERT_FALSE(IsHTTPAddress("https://google.com"));
  ASSERT_FALSE(IsHTTPAddress("https://ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, SucceedOnSchoolAddress) {
  ASSERT_TRUE(IsHTTPAddress("http://ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, SucceedOnIPAddress) {
  ASSERT_TRUE(IsHTTPAddress("http://10.0.0.12/"));
  ASSERT_TRUE(IsHTTPAddress("http://10.0.0.12"));
  ASSERT_TRUE(IsHTTPAddress("http://192.168.0.1/"));
  ASSERT_TRUE(IsHTTPAddress("http://192.168.0.1"));
}

TEST(IsHttpAddressTest, FailOnMissingLocation) {
  ASSERT_FALSE(IsHTTPAddress("http:"));
}

TEST(IsHttpAddressTest, FailOnMissingSecondForwardSlash) {
  ASSERT_FALSE(IsHTTPAddress("http:/google.com"));
  ASSERT_FALSE(IsHTTPAddress("http:/ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, FailOnMissingColon) {
  ASSERT_FALSE(IsHTTPAddress("http//google.com"));
  ASSERT_FALSE(IsHTTPAddress("http//ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, FailOnSlashesColon) {
  ASSERT_FALSE(IsHTTPAddress("http:google.com"));
  ASSERT_FALSE(IsHTTPAddress("http:ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, FailOnIncorrectProtocl) {
  ASSERT_FALSE(IsHTTPAddress("ssh://google.com"));
  ASSERT_FALSE(IsHTTPAddress("sftp://ecst.csuchico.edu"));
}

TEST(IsHttpAddressTest, SucceedOnHTTPAddressNestedDirectory) {
  ASSERT_TRUE(IsHTTPAddress("http://google.com/directory"));
  ASSERT_TRUE(IsHTTPAddress("http://ecst.csuchico.edu/directory"));
}

TEST(IsHttpAddressTest, SucceedOnHTTPAddressIndexFile) {
  ASSERT_TRUE(IsHTTPAddress("http://google.com/index.html"));
  ASSERT_TRUE(IsHTTPAddress("http://ecst.csuchico.edu/index.html"));
}

TEST(IsHttpAddressTest, SucceedOnHTTPSchoolInstructorDirectory) {
  ASSERT_TRUE(
      IsHTTPAddress("http://www.ecst.csuchico.edu/~sbsiewert/csci551/"));
  ASSERT_TRUE(IsHTTPAddress("http://www.ecst.csuchico.edu/~trhenry/"));
}

}  // namespace