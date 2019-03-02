/*
    红黑树的性质：
    1、每个节点要么是红，要么是黑。
    2、根节点是黑。
    3、每个叶子节点，即NIL节点是黑。
    4、如果一个节点是红，那么其两个叶子节点是黑。
    5、对于任一结点而言，其到叶结点树尾端NIL指针的每一条路径都包含相同数目的黑结点。
*/
#include<inttypes.h>

#define BLACK 0
#define RED 1

typedef struct rbtree_node_s {
    uint64_t key;
    struct rbtree_node_s *left;
    struct rbtree_node_s *right;
    struct rbtree_node_s *parent;
    char color;
} rbtree_node_t;

typedef struct rbtree_node_t rbtree_t;

static void
_rbtree_left_rotate(rbtree_node_t *node) {
    rbtree_node_t *tmp = node->right;
    node->right = tmp->left;
    
    if (NULL != tmp->left) {
        tmp->left->parent = node;
    }

    if (NULL != node->parent) {
        if (node == node->parent->left) {
            node->parent->left = tmp;
        } else {
            node->parent->right = tmp;
        }
        tmp->parent = node->parent;
    } else {
        tmp->parent = tmp;
    }
    
    tmp->left = node;
    node->parent = tmp;
}

static void
_rbtree_right_rotate(rbtree_node_t *node) {
    rbtree_node_t *tmp = node->left;
    node->left = tmp->right;
    
    if (NULL != tmp->right) {
        tmp->right->parent = node;
    }
    
    if (NULL != node->parent) {
        if (node == node->parent->left) {
            node->parent->left = tmp;
        } else {
            node->parent->right = tmp;
        }
        tmp->parent = node->parent;
    } else {
        tmp->parent = tmp;
    }
    
    tmp->right = node;
    node->parent = tmp;
}

void
rbtree_init(rbtree_t *tree) {
    tree->color = BLACK;
    tree->left = NULL;
    tree->right = NULL;
    tree->parent = tree;
    tree->key = 0;
}