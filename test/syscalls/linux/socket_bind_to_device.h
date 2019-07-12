// Copyright 2019 The gVisor Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GVISOR_TEST_SYSCALLS_LINUX_SOCKET_BIND_TO_DEVICE_H_
#define GVISOR_TEST_SYSCALLS_LINUX_SOCKET_BIND_TO_DEVICE_H_

#include <string>

#include "test/syscalls/linux/ip_socket_test_util.h"
#include "test/syscalls/linux/socket_test_util.h"
#include "test/util/uid_util.h"

namespace gvisor {
namespace testing {

typedef struct TestAction {
  int device;
  bool reuse;
  bool release;
  int release_row;
  int want;
} TestAction;

typedef struct TestCase {
  std::string name;
  std::vector<TestAction> actions;
} TestCase;

// Test fixture for SO_BINDTODEVICE parameterized tests.
class BindToDeviceParamTest
    : public ::testing::TestWithParam<
          ::testing::tuple<SocketKind, struct TestCase>> {
 protected:
  void SetUp() override {
    printf("Testing case: %s, %s\n",
           ::testing::get<0>(GetParam()).description.c_str(),
           ::testing::get<1>(GetParam()).name.c_str());
    // Only root can use SO_BINDTODEVICE.
    SKIP_IF(!ASSERT_NO_ERRNO_AND_VALUE(IsRoot()));
    socket_factory = ::testing::get<0>(GetParam());
  }

  PosixErrorOr<std::unique_ptr<FileDescriptor>> NewSocket() const {
    return socket_factory.Create();
  }

 private:
  SocketKind socket_factory;
};

std::unordered_set<string> get_interface_names();
class Tunnel {
 public:
  // Creates a tunnel and returns the file descriptor of the tunnel and the
  // name.
  Tunnel();
  const string& GetName() const { return name_; }

  ~Tunnel() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

 private:
  int fd_;
  string name_;
};

static std::unique_ptr<Tunnel> NewTunnel() {
  return absl::make_unique<Tunnel>();
}

// Test fixture for SO_BINDTODEVICE tests.
class BindToDeviceTest : public ::testing::TestWithParam<SocketKind> {
 protected:
  void SetUp() override {
    printf("Testing case: %s\n", GetParam().description.c_str());
    // Only root can use SO_BINDTODEVICE.
    SKIP_IF(!ASSERT_NO_ERRNO_AND_VALUE(IsRoot()));

    interface_name_ = "eth1";
    auto interface_names = get_interface_names();
    if (interface_names.find(interface_name_) == interface_names.end()) {
      // Need a tunnel.
      tunnel_ = NewTunnel();
      // Skip if failed to get a tunnel.
      interface_name_ = tunnel_->GetName();
    }
    socket_ = ASSERT_NO_ERRNO_AND_VALUE(GetParam().Create());
    SKIP_IF(GetInterfaceName().empty());
  }

  std::string GetInterfaceName() const { return interface_name_; }

  int GetSocketFd() const { return socket_->get(); }

 private:
  std::unique_ptr<Tunnel> tunnel_;
  string interface_name_;
  std::unique_ptr<FileDescriptor> socket_;
};

}  // namespace testing
}  // namespace gvisor

#endif  // GVISOR_TEST_SYSCALLS_LINUX_SOCKET_BIND_TO_DEVICE_H_
