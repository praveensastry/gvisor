// Copyright 2018 The gVisor Authors.
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

package tty

import (
	"gvisor.dev/gvisor/pkg/abi/linux"
	"gvisor.dev/gvisor/pkg/refs"
	"gvisor.dev/gvisor/pkg/sentry/arch"
	"gvisor.dev/gvisor/pkg/sentry/context"
	"gvisor.dev/gvisor/pkg/sentry/kernel"
	"gvisor.dev/gvisor/pkg/sentry/usermem"
)

// Terminal is a pseudoterminal.
//
// +stateify savable
type Terminal struct {
	refs.AtomicRefCount

	// n is the terminal index. It is immutable.
	n uint32

	// d is the containing directory. It is immutable.
	d *dirInodeOperations

	// ld is the line discipline of the terminal. It is immutable.
	ld *lineDiscipline

	// ktty contains the controlling process of this terminal.
	ktty *kernel.TTY
}

func newTerminal(ctx context.Context, d *dirInodeOperations, n uint32) *Terminal {
	termios := linux.DefaultSlaveTermios
	t := Terminal{
		d:    d,
		n:    n,
		ld:   newLineDiscipline(termios),
		ktty: &kernel.TTY{},
	}
	t.EnableLeakCheck("tty.Terminal")
	return &t
}

// setControllingTTY makes tm the controlling terminal of the calling thread
// group.
func (tm *Terminal) setControllingTTY(ctx context.Context, io usermem.IO, args arch.SyscallArguments, isMaster bool) error {
	task, ok := ctx.(*kernel.Task)
	if !ok {
		panic("setControllingTTY must be called from a task context")
	}

	tm.ktty.Mu.Lock()
	defer tm.ktty.Mu.Unlock()
	if err := task.ThreadGroup().SetControllingTTY(tm.ktty, args[2].Int(), isMaster); err != nil {
		return err
	}

	return nil
}

// releaseControllingTTY removes tm as the controlling terminal of the calling
// thread group.
func (tm *Terminal) releaseControllingTTY(ctx context.Context, io usermem.IO, args arch.SyscallArguments, isMaster bool) error {
	task, ok := ctx.(*kernel.Task)
	if !ok {
		panic("releaseControllingTTY must be called from a task context")
	}

	tm.ktty.Mu.Lock()
	defer tm.ktty.Mu.Unlock()
	if err := task.ThreadGroup().ReleaseControllingTTY(tm.ktty, isMaster); err != nil {
		return err
	}

	return nil
}

// foregroundProcessGroup gets the process group ID of tm's foreground process.
func (tm *Terminal) foregroundProcessGroup(ctx context.Context, io usermem.IO, args arch.SyscallArguments) (uintptr, error) {
	task, ok := ctx.(*kernel.Task)
	if !ok {
		panic("foregroundProcessGroup must be called from a task context")
	}

	tm.ktty.Mu.Lock()
	defer tm.ktty.Mu.Unlock()
	ret, err := task.ThreadGroup().ForegroundProcessGroup(tm.ktty)
	if err != nil {

		return 0, err
	}

	// Write it out to *arg.
	_, err = usermem.CopyObjectOut(ctx, io, args[2].Pointer(), int32(ret), usermem.IOOpts{
		AddressSpaceActive: true,
	})
	return 0, err
}

// foregroundProcessGroup sets tm's foreground process.
func (tm *Terminal) setForegroundProcessGroup(ctx context.Context, io usermem.IO, args arch.SyscallArguments) (uintptr, error) {
	task, ok := ctx.(*kernel.Task)
	if !ok {
		panic("setForegroundProcessGroup must be called from a task context")
	}

	// Read in the process group ID.
	var pgid int32
	if _, err := usermem.CopyObjectIn(ctx, io, args[2].Pointer(), &pgid, usermem.IOOpts{
		AddressSpaceActive: true,
	}); err != nil {
		return 0, err
	}

	tm.ktty.Mu.Lock()
	defer tm.ktty.Mu.Unlock()
	ret, err := task.ThreadGroup().SetForegroundProcessGroup(tm.ktty, kernel.ProcessGroupID(pgid))
	return uintptr(ret), err
}
