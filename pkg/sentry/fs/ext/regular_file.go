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

// fileReader is used to abstact away the complexity of how the file data is
// stored under the hood. Contains a method which maintains compatibility with
// vfs.FileDescriptionImpl.(P)Read.
type fileReader interface {

	// Read reads from the file into dst, starting at the given offset, and
	// returns the number of bytes read. Read is permitted to return partial
	// reads with a nil error when asked for more data than what exists.
	Read(ctx context.Context, dev io.ReadSeeker, blkSize uint64, dst usermem.IOSequence, offset int64) (int64, error)
}

// regularFile represents a regular file's inode.
type regularFile struct {
	inode Inode

	// blksUsed indicates how many physical blocks this inode uses.
	blksUsed uint64

	impl fileReader // immutable
}

// newRegularFile is the regularFile constructor. It figures out what kind of
// file this is and initializes the fileReader.
func newRegularFile(dev io.ReadSeeker, blkSize uint64, inode Inode) (*regularFile, error) {
	regFile := regularFile{
		inode:    inode,
		blksUsed: (inode.diskInode.Size() + blkSize - 1) / blkSize,
	}

	inodeFlags := inode.diskInode.Flags()

	if inodeFlags.Extents {
		file, err := newExtentFile(dev, blkSize, regFile)
		if err != nil {
			return nil, err
		}

		file.regFile.inode.impl = &file.regFile
		return &file.regFile, nil
	}

	if inodeFlags.Inline {
		if inode.diskInode.Size() > 60 {
			panic("ext fs: inline file larger than 60 bytes")
		}

		file := newInlineFile(regFile)
		file.regFile.inode.impl = &file.regFile
		return &file.regFile, nil
	}

	file, err := newBlockMapFile(dev, blkSize, regFile)
	if err != nil {
		return nil, err
	}
	file.regFile.inode.impl = &file.regFile
	return &file.regFile, nil
}
