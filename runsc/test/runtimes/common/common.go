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

// Package common executes functions for proctor binaries.
package common

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
)

var (
	list    = flag.Bool("list", false, "list all available tests")
	test    = flag.String("test", "", "run a single test from the list of available tests")
	version = flag.Bool("v", false, "print out the version of node that is installed")
)

type testRunner interface {
	ListTests() ([]string, error)
	RunTest(test string)
}

// LaunchFunc parses flags passed by a proctor binary and calls the requested behavior.
func LaunchFunc(tr testRunner) {
	flag.Parse()

	if *list && *test != "" {
		flag.PrintDefaults()
		os.Exit(1)
	}
	if *list {
		tests, err := tr.ListTests()
		if err != nil {
			log.Fatalf("Failed to list tests: %v", err)
		}
		for _, test := range tests {
			fmt.Println(test)
		}
		return
	}
	if *version {
		fmt.Println(os.Getenv("LANG_NAME"), " version: ", os.Getenv("LANG_VER"), " is installed.")
		return
	}
	if *test != "" {
		tr.RunTest(*test)
		return
	}
	runAllTests(tr)
}

// TestExec executes a single test passed by a proctor binary.
func TestExec(runner string, args []string) {
	cmd := exec.Command(runner, args...)
	cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
	if err := cmd.Run(); err != nil {
		log.Fatalf("Failed to run: %v", err)
	}
}

func runAllTests(tr testRunner) {
	tests, err := tr.ListTests()
	if err != nil {
		log.Fatalf("Failed to list tests: %v", err)
	}
	for _, test := range tests {
		tr.RunTest(test)
	}
}
