/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License").
 * Unless the License provides otherwise, you may not use, modify, copy,
 * publish, distribute, disclose or transmit this software or the related
 * documents without Intel's prior written permission.
 *
 * This software and the related documents are provided as is, with no
 * express or implied warranties, other than those that are expressly stated
 * in the License.
 *
 */

#ifndef _ST_LIB_AVX512_H_
#define _ST_LIB_AVX512_H_

#include "st_main.h"

int st20_rfc4175_422be10_to_yuv422p10le_avx512(struct st20_rfc4175_422_10_pg2_be* pg,
                                               uint16_t* y, uint16_t* b, uint16_t* r,
                                               uint32_t w, uint32_t h);

int st20_rfc4175_422be10_to_422le10_avx512(struct st20_rfc4175_422_10_pg2_be* pg_be,
                                           struct st20_rfc4175_422_10_pg2_le* pg_le,
                                           uint32_t w, uint32_t h);

int st20_rfc4175_422be10_to_422le8_avx512(struct st20_rfc4175_422_10_pg2_be* pg_10,
                                          struct st20_rfc4175_422_8_pg2_le* pg_8,
                                          uint32_t w, uint32_t h);

int st20_rfc4175_422le10_to_v210_avx512(uint8_t* pg_le, uint8_t* pg_v210, uint32_t w,
                                        uint32_t h);

int st20_rfc4175_422be10_to_v210_avx512(uint8_t* pg_be, uint8_t* pg_v210, uint32_t w,
                                        uint32_t h);
#endif