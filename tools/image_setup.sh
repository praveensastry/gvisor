#!/bin/bash

# Copyright 2019 The gVisor Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script runs remotely. It should ensure that build dependencies are
# installed, bazel is installed, etc. It will be run only once per project.
#
# This script should be idempotent. In general, it will not run multiple times,
# but it may be run as part of Kokoro workflows when it has already run.

set -xeo pipefail

# Install all essential build tools.
apt-get -y install make git-core build-essential linux-headers-$(uname -r) pkg-config

# Install a recent go toolchain.
if ! [[ -d /usr/local/go ]]; then
    wget https://dl.google.com/go/go1.12.linux-amd64.tar.gz
    tar -xvf go1.12.linux-amd64.tar.gz
    mv go /usr/local
fi

# Include in our path from here.
export PATH=$PATH:/usr/local/go/bin

#########
# Bazel #
#########

# Install bazel dependencies.
apt-get update && apt-get install -y openjdk-8-jdk-headless

# Install bazel repositories.
echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
curl https://bazel.build/bazel-release.pub.gpg | apt-key add -
apt-get update && apt-get -y install bazel

##########
# Docker #
##########

# Install Docker dependencies.
apt-get install -y apt-transport-https ca-certificates curl gnupg-agent software-properties-common

# Install the Docker packages.
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | apt-key add -
add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
apt-get update && apt-get install -y docker-ce docker-ce-cli containerd.io

##################
# CRI/Containerd #
##################

# Helper for Go packages below.
install_helper() {
  PACKAGE="${1}"
  TAG="${2}"
  GOPATH="${3}"

  # Clone the repository.
  mkdir -p "${GOPATH}"/src/$(dirname "${PACKAGE}") && \
     git clone https://"${PACKAGE}" "${GOPATH}"/src/"${PACKAGE}"

  # Checkout and build the repository.
  (cd "${GOPATH}"/src/"${PACKAGE}" && \
      git checkout "${TAG}" && \
      GOPATH="${GOPATH}" make && \
      GOPATH="${GOPATH}" make install)
}

# Install dependencies for the crictl tests.
apt-get install -y btrfs-tools libseccomp-dev

# Install containerd & cri-tools.
GOPATH=$(mktemp -d --tmpdir gopathXXXXX)
install_helper github.com/containerd/containerd v1.2.2 "${GOPATH}"
install_helper github.com/kubernetes-sigs/cri-tools v1.11.0 "${GOPATH}"

# Install gvisor-containerd-shim.
declare -r base="https://storage.googleapis.com/cri-containerd-staging/gvisor-containerd-shim"
declare -r latest=$(mktemp --tmpdir gvisor-containerd-shim-latest.XXXXXX)
declare -r shim_path=$(mktemp --tmpdir gvisor-containerd-shim.XXXXXX)
wget --no-verbose "${base}"/latest -O ${latest}
wget --no-verbose "${base}"/gvisor-containerd-shim-$(cat ${latest}) -O ${shim_path}
chmod +x ${shim_path}
mv ${shim_path} /usr/local/bin

# Configure containerd-shim.
declare -r shim_config_path=/etc/containerd
declare -r shim_config_tmp_path=$(mktemp --tmpdir gvisor-containerd-shim.XXXXXX.toml)
mkdir -p ${shim_config_path}
cat > ${shim_config_tmp_path} <<-EOF
    runc_shim = "/usr/local/bin/containerd-shim"

[runsc_config]
    debug = "true"
    debug-log = "/tmp/runsc-logs/"
    strace = "true"
    file-access = "shared"
EOF
mv ${shim_config_tmp_path} ${shim_config_path}

# Configure CNI.
(cd "${GOPATH}" && GOPATH="${GOPATH}" \
    src/github.com/containerd/containerd/script/setup/install-cni)

# Cleanup the above.
rm -rf "${GOPATH}"
rm -rf "${latest}"
rm -rf "${shim_path}"
rm -rf "${shim_config_tmp_path}"

##########
# Kokoro #
##########

declare -r ssh_public_key="ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDg7L/ZaEauETWrPklUTky3kvxqQfe2Ax/2CsSqhNIGNMnK/8d79CHlmY9+dE1FFQ/RzKNCaltgy7XcN/fCYiCZr5jm2ZtnLuGNOTzupMNhaYiPL419qmL+5rZXt4/dWTrsHbFRACxT8j51PcRMO5wgbL0Bg2XXimbx8kDFaurL2gqduQYqlu4lxWCaJqOL71WogcimeL63Nq/yeH5PJPWpqE4P9VUQSwAzBWFK/hLeds/AiP3MgVS65qHBnhq0JsHy8JQsqjZbG7Iidt/Ll0+gqzEbi62gDIcczG4KC0iOVzDDP/1BxDtt1lKeA23ll769Fcm3rJyoBMYxjvdw1TDx sabujp"

# Install dependencies.
apt-get install -y rsync coreutils

# We need a kbuilder user.
if useradd -c "kbuilder user" -m -s /bin/bash kbuilder; then
    # User was added successfully; we add the relevant SSH keys here.
    mkdir -p ~kbuilder/.ssh
    echo "${ssh_public_key}" > ~kbuilder/.ssh/authorized_keys
    chmod 0600 ~kbuilder/.ssh/authorized_keys
fi
