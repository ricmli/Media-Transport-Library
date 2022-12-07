/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _LINUX_MTL_HEAD_H_
#define _LINUX_MTL_HEAD_H_

#include <mtl/st_pipeline_api.h>
#include <obs/obs-module.h>

#define TODO_OUTPUT

#define timeval2ns(tv) \
  (((uint64_t)tv.tv_sec * 1000000000) + ((uint64_t)tv.tv_usec * 1000))

#define blog(level, msg, ...) blog(level, "mtl-input: " msg, ##__VA_ARGS__)

enum st_frame_fmt obs_to_mtl_format(enum video_format fmt);
enum st_fps obs_to_mtl_fps(uint32_t fps_num, uint32_t fps_den);

#endif