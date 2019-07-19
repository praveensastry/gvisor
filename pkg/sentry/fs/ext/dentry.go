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
	"gvisor.dev/gvisor/pkg/sentry/vfs"
)

// Dentry implements vfs.DentryImpl.
type Dentry struct {
	vfsd vfs.Dentry

	// inode is the inode represented by this Dentry. Multiple Dentries may
	// share a single non-directory Inode (with hard links). inode is
	// immutable.
	inode *Inode

	// dentryEntry (ugh) links Dentries into their parent directory.childList.
	dentryEntry
}

// Compiles only if Dentry implements vfs.DentryImpl.
var _ vfs.DentryImpl = (*Dentry)(nil)

// newDentry is the Dentry constructor.
func newDentry(in *Inode) *Dentry {
	d := &Dentry{
		inode: in,
	}
	d.vfsd.Init(d)
	return d
}

// IncRef implements vfs.DentryImpl.IncRef.
func (d *Dentry) IncRef(vfsfs *vfs.Filesystem) {
	d.inode.incRef()
}

// TryIncRef implements vfs.DentryImpl.TryIncRef.
func (d *Dentry) TryIncRef(vfsfs *vfs.Filesystem) bool {
	return d.inode.tryIncRef()
}

// DecRef implements vfs.DentryImpl.DecRef.
func (d *Dentry) DecRef(vfsfs *vfs.Filesystem) {
	d.inode.decRef(vfsfs.Impl().(*Filesystem))
}
