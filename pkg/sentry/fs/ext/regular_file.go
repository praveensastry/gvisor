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
)

// fileReader is used to abstact away the complexity of how the file data is
// stored under the hood. Provides a method to get a file reader which can be
// used to read file data without worrying about how it is organized on disk.
type fileReader interface {

	// getFileReader returns a Reader implementation which can be used to read a
	// file. It abstracts away the complexity of how the file is actually
	// organized on disk. The reader is initialized with the passed offset.
	//
	// This reader is not meant to be retained across Read operations as it needs
	// to be reinitialized with the correct offset for every Read.
	//
	// Precondition: Must hold the mutex of the filesystem containing dev while
	//               using the Reader.
	getFileReader(dev io.ReadSeeker, blkSize uint64, offset uint64) io.Reader
}

// regularFile represents a regular file's inode.
type regularFile struct {
	inode inode

	impl fileReader // immutable
}

// newRegularFile is the regularFile constructor. It figures out what kind of
// file this is and initializes the fileReader.
//
// Preconditions: Must hold the mutex of the filesystem containing dev.
func newRegularFile(dev io.ReadSeeker, blkSize uint64, inode inode) (*regularFile, error) {
	regFile := regularFile{
		inode: inode,
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

	file, err := newBlockMapFile(blkSize, regFile)
	if err != nil {
		return nil, err
	}
	file.regFile.inode.impl = &file.regFile
	return &file.regFile, nil
}

func (f *regularFile) blksUsed(blkSize uint64) uint64 {
	return (f.inode.diskInode.Size() + blkSize - 1) / blkSize
}
