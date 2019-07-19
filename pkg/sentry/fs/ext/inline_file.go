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

// inlineFile is a type of regular file. All the data here is stored in the
// inode.Data() array.
type inlineFile struct {
	regFile regularFile
}

// Compiles only if inlineFile implements fileReader.
var _ fileReader = (*inlineFile)(nil)

// Read implements fileReader.Read.
func (f *inlineFile) Read(ctx context.Context, dev io.ReadSeeker, blkSize uint64, dst usermem.IOSequence, offset int64) (int64, error) {
	n, err := dst.CopyOut(ctx, f.regFile.inode.diskInode.Data()[offset:])
	return int64(n), err
}

// newInlineFile is the inlineFile constructor.
func newInlineFile(regFile regularFile) *inlineFile {
	file := &inlineFile{regFile: regFile}
	file.regFile.impl = file
	return file
}
