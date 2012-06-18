#ifndef _SPLAY_TREE_H_
#define _SPLAY_TREE_H_

typedef struct tree_node {
    struct tree_node * left, * right;
    int key;
    int size;   /* maintained to be the number of nodes rooted here */

    void *data;
} splay_tree;


splay_tree * splaytree_splay (splay_tree *t, int key);
splay_tree * splaytree_insert(splay_tree *t, int key, void *data);
splay_tree * splaytree_delete(splay_tree *t, int key);
splay_tree * splaytree_size(splay_tree *t);

#define splaytree_size(x) (((x)==NULL) ? 0 : ((x)->size))
/* This macro returns the size of a node.  Unlike "x->size",     */
/* it works even if x=NULL.  The test could be avoided by using  */
/* a special version of NULL which was a real node with size 0.  */


#endif
