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

package ext

import (
	"io"

	"gvisor.dev/gvisor/pkg/sentry/context"
	"gvisor.dev/gvisor/pkg/sentry/usermem"
)

// symlink represents a symlink inode.
type symlink struct {
	inode  Inode
	target string // immutable
}

// newSymlink is the symlink constructor. It reads out the symlink target from
// the inode (however it might have been stored).
func newSymlink(ctx context.Context, dev io.ReadSeeker, blkSize uint64, inode Inode) (*symlink, error) {
	var file *symlink
	var link []byte

	// If the symlink target is lesser than 60 bytes, its stores in inode.Data().
	// Otherwise either extents or block maps will be used to store the link.
	size := inode.diskInode.Size()
	if size < 60 {
		link = inode.diskInode.Data()[:size]
	} else {
		// Create a regular file out of this inode and read out the target.
		regFile, err := newRegularFile(dev, blkSize, inode)
		if err != nil {
			return nil, err
		}

		link = make([]byte, size)
		ioSeq := usermem.BytesIOSequence(link)

		_, err = regFile.impl.Read(ctx, dev, blkSize, ioSeq, 0)
		if err != nil {
			return nil, err
		}
	}

	file = &symlink{inode: inode, target: string(link)}
	file.inode.impl = file
	return file, nil
}
