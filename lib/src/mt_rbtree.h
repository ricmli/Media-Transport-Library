/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _MT_LIB_RBTREE_HEAD_H_
#define _MT_LIB_RBTREE_HEAD_H_

#include "mt_main.h"

enum mt_rbtree_color { MT_RBTREE_RED = 0, MT_RBTREE_BLACK = 1 };

struct mt_rbtree_node {
  struct mt_rbtree_node* left;
  struct mt_rbtree_node* right;
  struct mt_rbtree_node* parent;
  enum mt_rbtree_color color;
  uint64_t key;
  uint64_t value;
};

struct mt_rbtree {
  struct mt_rbtree_node* root;
  int size;
  int soc_id;
};

int mt_rbtree_add(struct mt_rbtree* tree, uint64_t key, uint64_t value);

int mt_rbtree_del(struct mt_rbtree* tree, uint64_t key);

struct mt_rbtree_node* mt_rbtree_find(struct mt_rbtree* tree, uint64_t key);

struct mt_rbtree* mt_rbtree_init(int soc_id);

int mt_rbtree_uinit(struct mt_rbtree* tree);

#endif