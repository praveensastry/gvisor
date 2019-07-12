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

#include "test/syscalls/linux/socket_bind_to_device.h"

#include <arpa/inet.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gtest/gtest.h"
#include "test/syscalls/linux/ip_socket_test_util.h"
#include "test/syscalls/linux/socket_test_util.h"
#include "test/util/test_util.h"
#include "test/util/uid_util.h"

namespace gvisor {
namespace testing {

using std::string;
using std::vector;

std::unordered_set<string> get_interface_names() {
  struct if_nameindex* interfaces = if_nameindex();
  if (interfaces == nullptr) {
    return {};
  }
  std::unordered_set<string> names;
  for (auto interface = interfaces;
       interface->if_index != 0 || interface->if_name != nullptr; interface++) {
    names.insert(interface->if_name);
  }
  if_freenameindex(interfaces);
  return names;
}

Tunnel::Tunnel() {
  fd_ = open("/dev/net/tun", O_RDWR);
  if (fd_ < 0) {
    return;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN;

  int err = ioctl(fd_, (TUNSETIFF), (void*)&ifr);
  if (err < 0) {
    close(fd_);
    fd_ = -1;
  }
  name_ = ifr.ifr_name;
}

// Tests the creation of a seires of sockets with varying SO_BINDTODEVICE and
// SO_REUSEPORT options.  Uses consecutive ethernet devices named "eth1",
// "eth2", etc until they run out and then creates tunnel devices as needed.
TEST_P(BindToDeviceParamTest, BindToDevice) {
  auto test_case = ::testing::get<1>(GetParam());
  auto test_name = test_case.name;
  auto test_actions = test_case.actions;

  auto interface_names = get_interface_names();
  // devices maps from the device id in the test case to the name of the device.
  std::unordered_map<int, string> devices;
  int next_unused_eth = 1;
  std::vector<std::unique_ptr<Tunnel>> tunnels;
  for (const auto& action : test_actions) {
    if (action.device != 0 && devices.find(action.device) == devices.end()) {
      // Need to pick a new device.
      devices[action.device] = string("eth") + std::to_string(next_unused_eth);
      next_unused_eth++;

      if (interface_names.find(devices[action.device]) ==
          interface_names.end()) {
        // Need a tunnel.
        tunnels.push_back(NewTunnel());
        // Skip if failed to get a tunnel.
        SKIP_IF(tunnels.back()->GetName().empty());
        devices[action.device] = tunnels.back()->GetName();
      }
    }
  }

  SCOPED_TRACE(::testing::get<0>(GetParam()).description + ", " + test_name);

  int action_index = 0;
  // sockets_to_close is a map from action index to the socket that was created.
  std::unordered_map<int,
                     std::unique_ptr<gvisor::testing::FileDescriptor>>
      sockets_to_close;
  // All the actions will use the same port, whichever we are assigned.
  in_port_t port = htons(0);
  for (const auto& action : test_actions) {
    SCOPED_TRACE(std::string("Action index: ") + std::to_string(action_index));
    if (action.release) {
      // Close the socket that was made in a previous action.  The release_row
      // indicates which socket to close based on index into the list of
      // actions.
      sockets_to_close.erase(action.release_row);
      continue;
    }

    // Make the socket.
    sockets_to_close[action_index] = ASSERT_NO_ERRNO_AND_VALUE(NewSocket());
    auto socket_fd = sockets_to_close[action_index]->get();
    action_index++;

    // If reuse is indicated, do that.
    if (action.reuse) {
      int reuse = 1;
      EXPECT_THAT(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuse,
                             sizeof(reuse)),
                  SyscallSucceedsWithValue(0));
    }

    // If the device is non-zero, bind to that device.
    if (action.device != 0) {
      string device_name = devices[action.device];
      EXPECT_THAT(setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE,
                             device_name.c_str(), device_name.size() + 1),
                  SyscallSucceedsWithValue(0));
      char getDevice[100];
      socklen_t get_device_size = 100;
      EXPECT_THAT(getsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, getDevice,
                             &get_device_size),
                  SyscallSucceedsWithValue(0));
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = port;
    if (action.want == 0) {
      ASSERT_THAT(
          bind(socket_fd, reinterpret_cast<const struct sockaddr*>(&addr),
               sizeof(addr)),
          SyscallSucceeds());
    } else {
      ASSERT_THAT(
          bind(socket_fd, reinterpret_cast<const struct sockaddr*>(&addr),
               sizeof(addr)),
          SyscallFailsWithErrno(action.want));
    }

    if (port == 0) {
      // We don't yet know what port we'll be using so we need to fetch it and
      // remember it for future commands.
      socklen_t addr_size = sizeof(addr);
      ASSERT_THAT(
          getsockname(socket_fd, reinterpret_cast<struct sockaddr*>(&addr),
                      &addr_size),
          SyscallSucceeds());
      port = addr.sin_port;
    }
  }
}

// Tests getsockopt of the default value.
TEST_P(BindToDeviceTest, GetsockoptDefault) {
  char name_buffer[IFNAMSIZ * 2];
  socklen_t name_buffer_size;

  // Read the default SO_BINDTODEVICE.
  for (int i = 0; i <= sizeof(name_buffer); i++) {
    memset(name_buffer, 'a', sizeof(name_buffer));
    name_buffer_size = i;
    EXPECT_THAT(getsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                           name_buffer, &name_buffer_size),
                SyscallSucceedsWithValue(0));
    EXPECT_EQ(name_buffer_size, 0);
  }
}

// Tests setsockopt of invalid value.
TEST_P(BindToDeviceTest, SetsockoptInvalid) {
  char name_buffer[IFNAMSIZ * 2];
  socklen_t name_buffer_size;

  // Set an invalid device name.
  memset(name_buffer, 'a', 5);
  name_buffer_size = 5;
  EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                         name_buffer, name_buffer_size),
              SyscallFailsWithErrno(ENODEV));
}

// Tests setsockopt of a valid device name but not null-terminated.
TEST_P(BindToDeviceTest, SetsockoptValidNoNull) {
  char name_buffer[IFNAMSIZ * 2];
  socklen_t name_buffer_size;

  memset(name_buffer, 'a', sizeof(name_buffer));
  strncpy(name_buffer, GetInterfaceName().c_str(), GetInterfaceName().size());
  for (int i = 1; i <= sizeof(name_buffer); i++) {
    name_buffer_size = i;
    SCOPED_TRACE(std::string("Buffer size: ") + std::to_string(i));
    // It should only work if the size provided is exactly right.
    if (name_buffer_size == GetInterfaceName().size()) {
      EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                             name_buffer, name_buffer_size),
                  SyscallSucceeds());
    } else {
      EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                             name_buffer, name_buffer_size),
                  SyscallFailsWithErrno(ENODEV));
    }
  }
}

// Tests setsockopt of a valid device name that is null-terminated.
TEST_P(BindToDeviceTest, SetsockoptValid) {
  char name_buffer[IFNAMSIZ * 2];
  socklen_t name_buffer_size;

  // Set a valid device name with a string that is null-terminated.
  memset(name_buffer, 'a', sizeof(name_buffer));
  strncpy(name_buffer, GetInterfaceName().c_str(), sizeof(name_buffer));
  for (int i = 1; i <= sizeof(name_buffer); i++) {
    name_buffer_size = i;
    SCOPED_TRACE(std::string("Buffer size: ") + std::to_string(i));
    // It should only work if the size provided is at least the right size.
    if (name_buffer_size >= GetInterfaceName().size()) {
      EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                             name_buffer, name_buffer_size),
                  SyscallSucceeds());
    } else {
      EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                             name_buffer, name_buffer_size),
                  SyscallFailsWithErrno(ENODEV));
    }
  }
}

// Tests that setsockopt of an invalid device name doesn't unset the prevoius
// valid setsockopt.
TEST_P(BindToDeviceTest, SetsockoptValidThenInvalid) {
  char name_buffer[IFNAMSIZ * 2];
  socklen_t name_buffer_size;

  // Write successfully.
  strcpy(name_buffer, GetInterfaceName().c_str());
  EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                         name_buffer, sizeof(name_buffer)),
              SyscallSucceeds());

  // Read it back successfully.
  memset(name_buffer, 'a', sizeof(name_buffer));
  name_buffer_size = sizeof(name_buffer);
  strcpy(name_buffer, GetInterfaceName().c_str());
  EXPECT_THAT(getsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                         name_buffer, &name_buffer_size),
              SyscallSucceeds());
  EXPECT_EQ(name_buffer_size, GetInterfaceName().size() + 1);
  EXPECT_STREQ(name_buffer, GetInterfaceName().c_str());

  // Write unsuccessfully.
  memset(name_buffer, 'a', sizeof(name_buffer));
  name_buffer_size = 5;
  EXPECT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                         name_buffer, sizeof(name_buffer)),
              SyscallFailsWithErrno(ENODEV));

  // Read it back successfully, it's unchanged.
  memset(name_buffer, 'a', sizeof(name_buffer));
  name_buffer_size = sizeof(name_buffer);
  strcpy(name_buffer, GetInterfaceName().c_str());
  EXPECT_THAT(getsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                         name_buffer, &name_buffer_size),
              SyscallSucceeds());
  EXPECT_EQ(name_buffer_size, GetInterfaceName().size() + 1);
  EXPECT_STREQ(name_buffer, GetInterfaceName().c_str());
}

// Tests getsockopt with different buffer sizes.
TEST_P(BindToDeviceTest, GetsockoptDevice) {
  char name_buffer[IFNAMSIZ * 2];
  socklen_t name_buffer_size;

  // Write successfully.
  strcpy(name_buffer, GetInterfaceName().c_str());
  ASSERT_THAT(setsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                         name_buffer, sizeof(name_buffer)),
              SyscallSucceeds());

  // Read it back at various buffer sizes.
  for (int i = 0; i <= sizeof(name_buffer); i++) {
    memset(name_buffer, 'a', sizeof(name_buffer));
    name_buffer_size = i;
    SCOPED_TRACE(std::string("Buffer size: ") + std::to_string(i));
    // Linux only allows a buffer at least IFNAMSIZ, even if less would suffice
    // for this interface name.
    if (name_buffer_size >= IFNAMSIZ) {
      EXPECT_THAT(getsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                             name_buffer, &name_buffer_size),
                  SyscallSucceeds());
      EXPECT_EQ(name_buffer_size, GetInterfaceName().size() + 1);
      EXPECT_STREQ(name_buffer, GetInterfaceName().c_str());
    } else {
      EXPECT_THAT(getsockopt(GetSocketFd(), SOL_SOCKET, SO_BINDTODEVICE,
                             name_buffer, &name_buffer_size),
                  SyscallFailsWithErrno(EINVAL));
      EXPECT_EQ(name_buffer_size, i);
    }
  }
}

}  // namespace testing
}  // namespace gvisor
