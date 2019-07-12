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

#include <vector>

#include "test/syscalls/linux/ip_socket_test_util.h"
#include "test/syscalls/linux/socket_test_util.h"
#include "test/util/test_util.h"

namespace gvisor {
namespace testing {

TestAction NewTestAction(bool reuse, int device, bool release, int release_row,
                         int want) {
  TestAction test_action;
  test_action.reuse = reuse;
  test_action.device = device;
  test_action.release = release;
  test_action.release_row = release_row;
  test_action.want = want;
  return test_action;
}

TestAction NewReleaseAction(int release_row) {
  return NewTestAction(false, 0, true, release_row, 0);
}

TestAction NewBindAction(bool reuse, int device, int want) {
  return NewTestAction(reuse, device, false, 0, want);
}

TestCase NewTestCase(string name, std::vector<TestAction> actions) {
  TestCase test_case;
  test_case.name = name;
  test_case.actions = actions;
  return test_case;
}

std::vector<TestCase> GetTestCases() {
  return std::vector<TestCase>{
      NewTestCase(
          "bind twice with device fails",
          {
              NewBindAction(/* reuse */ false, /* device */ 3, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 3,
                            /* want */ EADDRINUSE),
          }),
      NewTestCase(
          "bind to device",
          {
              NewBindAction(/* reuse */ false, /* device */ 1, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 2, /* want */ 0),
          }),
      NewTestCase(
          "bind to device and then without device",
          {
              NewBindAction(/* reuse */ false, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 0,
                            /* want */ EADDRINUSE),
          }),
      NewTestCase(
          "bind without device",
          {
              NewBindAction(/* reuse */ false, /* device */ 0, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 123,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 123,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ false, /* device */ 0,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 0,
                            /* want */ EADDRINUSE),
          }),
      NewTestCase(
          "bind with device",
          {
              NewBindAction(/* reuse */ false, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 123,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 123,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ false, /* device */ 0,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 0,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 456, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 789, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 0,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 0,
                            /* want */ EADDRINUSE),
          }),
      NewTestCase(
          "bind with reuse",
          {
              NewBindAction(/* reuse */ true, /* device */ 0, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 123,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 0,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 0, /* want */ 0),
          }),
      NewTestCase(
          "binding with reuse and device",
          {
              NewBindAction(/* reuse */ true, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 123,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 0,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 456, /* want */ 0),
              NewBindAction(/* reuse */ true, /* device */ 0, /* want */ 0),
              NewBindAction(/* reuse */ true, /* device */ 789, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 999,
                            /* want */ EADDRINUSE),
          }),
      NewTestCase(
          "mixing reuse and not reuse by binding to device",
          {
              NewBindAction(/* reuse */ true, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 456, /* want */ 0),
              NewBindAction(/* reuse */ true, /* device */ 789, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 999, /* want */ 0),
          }),
      NewTestCase(
          "can't bind to 0 after mixing reuse and not reuse",
          {
              NewBindAction(/* reuse */ true, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 456, /* want */ 0),
              NewBindAction(/* reuse */ true, /* device */ 0,
                            /* want */ EADDRINUSE),
          }),
      NewTestCase(
          "bind and release",
          {
              NewBindAction(/* reuse */ true, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ true, /* device */ 0, /* want */ 0),
              NewBindAction(/* reuse */ false, /* device */ 345,
                            /* want */ EADDRINUSE),
              NewBindAction(/* reuse */ true, /* device */ 789, /* want */ 0),

              // Release the bind to device 0 and try again.
              NewReleaseAction(/* release_row */ 1),
              NewBindAction(/* reuse */ false, /* device */ 345, /* want */ 0),
          }),
      NewTestCase(
          "bind twice with reuse once",
          {
              NewBindAction(/* reuse */ false, /* device */ 123, /* want */ 0),
              NewBindAction(/* reuse */ true, /* device */ 0,
                            /* want */ EADDRINUSE),
          }),
  };
}

INSTANTIATE_TEST_SUITE_P(
    BindToDeviceTest, BindToDeviceParamTest,
    ::testing::Combine(::testing::Values(IPv4UDPUnboundSocket(0),
                                         IPv4TCPUnboundSocket(0)),
                       ::testing::ValuesIn(GetTestCases())));

INSTANTIATE_TEST_SUITE_P(BindToDeviceTest, BindToDeviceTest,
                         ::testing::Values(IPv4UDPUnboundSocket(0),
                                           IPv4TCPUnboundSocket(0)));

}  // namespace testing
}  // namespace gvisor
