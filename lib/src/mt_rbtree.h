/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _MT_LIB_RBTREE_HEAD_H_
#define _MT_LIB_RBTREE_HEAD_H_

#include "mt_main.h"

enum mt_rbtree_color {
  MT_RBTREE_RED = 0,
  MT_RBTREE_BLACK = 1,
};

/* This is a key-value rbtree node,
 * the key is unique and used for ordering. */
struct mt_rbtree_node {
  uint64_t key;                  /* unique key */
  uint64_t value;                /* stored value */
  enum mt_rbtree_color color;    /* node color, red or black */
  struct mt_rbtree_node* left;   /* left child */
  struct mt_rbtree_node* right;  /* right child */
  struct mt_rbtree_node* parent; /* parent node, NULL for root node */
};

struct mt_rbtree {
  struct mt_rbtree_node* root; /* the root node */
  int size;                    /* record number of nodes */
  int soc_id;                  /* for numa memory allocation */
};

struct mt_rbtree* mt_rbtree_init(int soc_id);
int mt_rbtree_uinit(struct mt_rbtree* tree);
int mt_rbtree_add(struct mt_rbtree* tree, uint64_t key, uint64_t value);
int mt_rbtree_del(struct mt_rbtree* tree, uint64_t key);
struct mt_rbtree_node* mt_rbtree_find(struct mt_rbtree* tree, uint64_t key);

/* for debug use */
void mt_rbtree_print(struct mt_rbtree* tree);
bool mt_rbtree_is_balanced(struct mt_rbtree* tree);

#endif