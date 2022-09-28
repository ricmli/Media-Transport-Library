#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Intel Corporation

set -e

find . -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' -exec clang-format -i {} \;