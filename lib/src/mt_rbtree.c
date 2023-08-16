/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "mt_rbtree.h"

static int rbtree_rotate_left(struct mt_rbtree* tree, struct mt_rbtree_node* pivot) {
  struct mt_rbtree_node* right = pivot->right;
  if (!right) {
    err("no right child, cannot rotate left");
    return -EIO;
  }

  pivot->right = right->left;
  if (right->left) {
    right->left->parent = pivot;
  }
  right->left = pivot;
  right->parent = pivot->parent;
  pivot->parent = right;

  return 0;
}

static int rbtree_rotate_right(struct mt_rbtree* tree, struct mt_rbtree_node* pivot) {
  struct mt_rbtree_node* left = pivot->left;
  if (!left) {
    err("no left child, cannot rotate right");
    return -EIO;
  }

  pivot->left = left->right;
  if (left->right) {
    left->right->parent = pivot;
  }
  left->right = pivot;
  left->parent = pivot->parent;
  pivot->parent = left;

  return 0;
}

int mt_rbtree_add(struct mt_rbtree* tree, uint64_t key, uint64_t value) {
  /* find the parent */
  struct mt_rbtree_node* tmp = tree->root;
  struct mt_rbtree_node* parent = NULL;
  bool left = false;
  while (tmp) {
    parent = tmp;
    if (key < tmp->key) {
      tmp = tmp->left;
      left = true;
    } else if (key > tmp->key) {
      tmp = tmp->right;
      left = false;
    } else { /* already has key node */
      if (tmp->value == value) {
        dbg("%s, duplicate value %" PRIu64 " for key %" PRIu64 "\n", __func__, value,
            key);
      } else {
        err("%s, conflict, old value: %" PRIu64 ", new value: %" PRIu64 "\n", __func__,
            tmp->value, value);
      }
      return -EIO;
    }
  }

  struct mt_rbtree_node* node = (struct mt_rbtree_node*)mt_rte_zmalloc_socket(
      sizeof(struct mt_rbtree_node), tree->soc_id);
  if (!node) {
    err("%s, failed to allocate memory for node\n", __func__);
    return -ENOMEM;
  }
  node->key = key;
  node->value = value;
  node->color = MT_RBTREE_RED;
  node->left = NULL;
  node->right = NULL;
  node->parent = NULL;

  if (!parent) { /* empty tree */
    tree->root = node;
    tree->root->color = MT_RBTREE_BLACK;
    tree->size++;
    return 0;
  }

  /* insert the node */
  if (left)
    parent->left = node;
  else
    parent->right = node;
  node->parent = parent;
  tree->size++;

  if (parent->color == MT_RBTREE_RED) {
    /* do something */
  }

  return 0;
}

int mt_rbtree_del(struct mt_rbtree* tree, uint64_t key) {
  struct mt_rbtree_node* node = mt_rbtree_find(tree, key);
  if (!node) {
    err("%s, node not found\n", __func__);
    return -EIO;
  }

  bool need_fix = true;
  struct mt_rbtree_node* parent = node->parent;

  if (!node->left && !node->right) { /* no children */
    if (!parent) {
      /* root */
      tree->root = NULL;
      need_fix = false;
    } else {
      if (parent->left == node) {
        parent->left = NULL;
      } else {
        parent->right = NULL;
      }
      if (node->color == MT_RBTREE_RED) need_fix = false;
    }
    mt_rte_free(node);
  } else if (!node->left) { /* only right child */
    if (!parent) {
      /* root */
      tree->root = node->right;
      tree->root->parent = NULL;
      /* may need change color*/
    } else {
      if (parent->left == node) {
        parent->left = node->right;
      } else {
        parent->right = node->right;
      }
      node->right->parent = parent;
      if (node->color == MT_RBTREE_RED) need_fix = false;
    }
    mt_rte_free(node);
  } else if (!node->right) { /* only left child */
    if (!parent) {
      /* root */
      tree->root = node->left;
      tree->root->parent = NULL;
      /* may need change color*/
    } else {
      if (parent->left == node) {
        parent->left = node->left;
      } else {
        parent->right = node->left;
      }
      node->left->parent = parent;
      if (node->color == MT_RBTREE_RED) need_fix = false;
    }
    mt_rte_free(node);
  } else { /* both children */
    struct mt_rbtree_node* tmp = node->right;
    while (tmp->left) {
      tmp = tmp->left;
    }
    node->key = tmp->key;
    node->value = tmp->value;
    parent = tmp->parent;
    if (parent->left == tmp) {
      parent->left = tmp->right;
    } else {
      parent->right = tmp->right;
    }
    if (tmp->right) {
      tmp->right->parent = parent;
    }
    mt_rte_free(tmp);
  }

  if (need_fix) {
    /* do something */
  }

  return 0;
}

struct mt_rbtree_node* mt_rbtree_find(struct mt_rbtree* tree, uint64_t key) {
  struct mt_rbtree_node* node = tree->root;

  while (node) {
    if (key < node->key) {
      node = node->left;
    } else if (key > node->key) {
      node = node->right;
    } else {
      return node;
    }
  }

  return NULL;
}

struct mt_rbtree* mt_rbtree_init(int soc_id) {
  struct mt_rbtree* tree =
      (struct mt_rbtree*)mt_rte_zmalloc_socket(sizeof(struct mt_rbtree), soc_id);
  if (!tree) return NULL;

  tree->root = NULL;
  tree->size = 0;
  tree->soc_id = soc_id;

  return tree;
}

static int rbtree_node_recursive_free(struct mt_rbtree_node* node) {
  if (!node) return 0;

  tree_node_recursive_free(node->left);
  tree_node_recursive_free(node->right);
  mt_rte_free(node);

  return 0;
}

int mt_rbtree_uinit(struct mt_rbtree* tree) {
  /* delete all nodes */
  rbtree_node_recursive_free(tree->root);
  mt_rte_free(tree);

  return 0;
}