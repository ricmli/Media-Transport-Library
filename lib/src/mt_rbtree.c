/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "mt_rbtree.h"

static int rbtree_rotate_left(struct mt_rbtree_node* pivot) {
  struct mt_rbtree_node* right = pivot->right;
  if (!right) {
    err("no right child, cannot rotate left");
    return -EIO;
  }
  struct mt_rbtree_node* parent = pivot->parent;

  pivot->right = right->left;
  if (right->left) {
    right->left->parent = pivot;
  }
  right->left = pivot;
  pivot->parent = right;
  right->parent = parent;
  if (parent) {
    if (parent->right == pivot)
      parent->right = right;
    else
      parent->left = right;
  }

  return 0;
}

static int rbtree_rotate_right(struct mt_rbtree_node* pivot) {
  struct mt_rbtree_node* left = pivot->left;
  if (!left) {
    err("no left child, cannot rotate right");
    return -EIO;
  }
  struct mt_rbtree_node* parent = pivot->parent;

  pivot->left = left->right;
  if (left->right) {
    left->right->parent = pivot;
  }
  left->right = pivot;
  pivot->parent = left;
  left->parent = parent;
  if (parent) {
    if (parent->left == pivot)
      parent->left = left;
    else
      parent->right = left;
  }

  return 0;
}

static inline void rbtree_node_swap_color(struct mt_rbtree_node* x,
                                          struct mt_rbtree_node* y) {
  enum mt_rbtree_color temp = x->color;
  x->color = y->color;
  y->color = temp;
}

static int rbtree_add_fix(struct mt_rbtree_node* node) {
  struct mt_rbtree_node* parent = node->parent;
  if (!parent) { /* root node */
    node->color = MT_RBTREE_BLACK;
    return 0;
  }
  if (parent->color == MT_RBTREE_BLACK) return 0;
  bool node_left = parent->left == node;

  struct mt_rbtree_node* grandparent = parent->parent;
  if (!grandparent) return 0; /* parent is root node */
  bool parent_left = grandparent->left == parent;
  struct mt_rbtree_node* uncle = parent_left ? grandparent->right : grandparent->left;

  if (uncle && uncle->color == MT_RBTREE_RED) {
    uncle->color = MT_RBTREE_BLACK;
    parent->color = MT_RBTREE_BLACK;
    grandparent->color = MT_RBTREE_RED;
    rbtree_add_fix(grandparent);
  } else { /* uncle is balck (null is also black) */
    if (parent_left) {
      if (node_left) { /* ll case */
        rbtree_rotate_right(grandparent);
        rbtree_node_swap_color(parent, grandparent);
      } else { /* lr case */
        rbtree_rotate_left(parent);
        rbtree_add_fix(parent);
      }
    } else {
      if (node_left) { /* rl case */
        rbtree_rotate_right(parent);
        rbtree_add_fix(parent);
      } else { /* rr case */
        rbtree_rotate_left(grandparent);
        rbtree_node_swap_color(parent, grandparent);
      }
    }
  }

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
        warn("%s, duplicate value %" PRIu64 " for key %" PRIu64 "\n", __func__, value,
             key);
      } else {
        err("%s, conflict for key %" PRIu64 ", old value: %" PRIu64
            ", new value: %" PRIu64 "\n",
            __func__, key, tmp->value, value);
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

  rbtree_add_fix(node);

  return 0;
}

static int rbtree_del_fix(struct mt_rbtree* tree, struct mt_rbtree_node* parent,
                          bool left) {
  struct mt_rbtree_node* sibling = NULL;
}

/* delete the node which has only one or no child */
static int rbtree_del_none_twins(struct mt_rbtree* tree, struct mt_rbtree_node* node) {
  bool need_fix = true;
  struct mt_rbtree_node* parent = node->parent;
  bool left = parent ? parent->left == node : false;

  if (!node->left && !node->right) { /* no child */
    if (!parent) {
      /* root */
      tree->root = NULL;
      need_fix = false;
    } else {
      if (left) {
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
      if (left) {
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
      if (left) {
        parent->left = node->left;
      } else {
        parent->right = node->left;
      }
      node->left->parent = parent;
      if (node->color == MT_RBTREE_RED) need_fix = false;
    }
    mt_rte_free(node);
  }

  if (need_fix) {
    rbtree_del_fix(tree, parent, left);
  }

  return 0;
}

int mt_rbtree_del(struct mt_rbtree* tree, uint64_t key) {
  struct mt_rbtree_node* node = mt_rbtree_find(tree, key);
  if (!node) {
    err("%s, node not found\n", __func__);
    return -EIO;
  }

  struct mt_rbtree_node* parent = node->parent;
  bool left = parent ? parent->left == node : false;

  if (node->left && node->right) { /* have both children */
    /* find successor node of inorder traversal, replace */
    struct mt_rbtree_node* successor = node->right;
    while (successor->left) {
      successor = successor->left;
    }
    node->key = successor->key;
    node->value = successor->value;
    /* the successor should have null left child */
    rbtree_del_none_twins(tree, successor);
  } else {
    rbtree_del_none_twins(tree, node);
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