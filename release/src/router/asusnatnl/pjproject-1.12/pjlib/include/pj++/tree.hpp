/* $Id: tree.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJPP_TREE_HPP__
#define __PJPP_TREE_HPP__

#include <pj/rbtree.h>

//
// Tree.
//
class PJ_Tree
{
public:
    typedef pj_rbtree_comp Comp;
    class iterator;
    class reverse_iterator;

    class Node : private pj_rbtree_node
    {
	friend class PJ_Tree;
	friend class iterator;
	friend class reverse_iterator;

    public:
	Node() {}
	explicit Node(void *data) { user_data = data; }
	void  set_user_data(void *data) { user_data = data; }
	void *get_user_data() const { return user_data; }
    };

    class iterator
    {
    public:
	iterator() {}
	iterator(const iterator &rhs) : tr_(rhs.tr_), nd_(rhs.nd_) {}
	iterator(pj_rbtree *tr, pj_rbtree_node *nd) : tr_(tr), nd_(nd) {}
	Node *operator*() { return (Node*)nd_; }
	bool operator==(const iterator &rhs) const { return tr_==rhs.tr_ && nd_==rhs.nd_; }
	iterator &operator=(const iterator &rhs) { tr_=rhs.tr_; nd_=rhs.nd_; return *this; }
	void operator++() { nd_=pj_rbtree_next(tr_, nd_); }
	void operator--() { nd_=pj_rbtree_prev(tr_, nd_); }
    protected:
	pj_rbtree *tr_;
	pj_rbtree_node *nd_;
    };

    class reverse_iterator : public iterator
    {
    public:
	reverse_iterator() {}
	reverse_iterator(const reverse_iterator &it) : iterator(it) {}
	reverse_iterator(pj_rbtree *t, pj_rbtree_node *n) : iterator(t, n) {}
	reverse_iterator &operator=(const reverse_iterator &rhs) { iterator::operator=(rhs); return *this; }
	Node *operator*() { return (Node*)nd_; }
	bool operator==(const reverse_iterator &rhs) const { return iterator::operator==(rhs); }
	void operator++() { nd_=pj_rbtree_prev(tr_, nd_); }
	void operator--() { nd_=pj_rbtree_next(tr_, nd_); }
    };

    explicit PJ_Tree(Comp *comp) { pj_rbtree_init(&t_, comp); }

    iterator begin()
    {
	return iterator(&t_, pj_rbtree_first(&t_));
    }

    iterator end()
    {
	return iterator(&t_, NULL);
    }

    reverse_iterator rbegin()
    {
	return reverse_iterator(&t_, pj_rbtree_last(&t_));
    }

    reverse_iterator rend()
    {
	return reverse_iterator(&t_, NULL);
    }

    bool insert(Node *node)
    {
	return pj_rbtree_insert(&t_, node)==0 ? true : false;
    }

    Node *find(const void *key)
    {
	return (Node*)pj_rbtree_find(&t_, key);
    }

    Node *erase(Node *node)
    {
	return (Node*)pj_rbtree_erase(&t_, node);
    }

    unsigned max_height(Node *node=NULL)
    {
	return pj_rbtree_max_height(&t_, node);
    }

    unsigned min_height(Node *node=NULL)
    {
	return pj_rbtree_min_height(&t_, node);
    }

private:
    pj_rbtree t_;
};

#endif	/* __PJPP_TREE_HPP__ */

