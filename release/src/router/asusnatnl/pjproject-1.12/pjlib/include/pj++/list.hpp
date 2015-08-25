/* $Id: list.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
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
#ifndef __PJPP_LIST_HPP__
#define __PJPP_LIST_HPP__

#include <pj/list.h>
#include <pj++/pool.hpp>


//
// Linked-list.
//
// Note:
//  List_Node must have public member next and prev. Normally
//  it will be declared like:
//
//  struct my_node
//  {
//      PJ_DECL_LIST_MEMBER(struct my_node);
//      ..
//  };
//
//
template <class List_Node>
class Pj_List : public Pj_Object
{
public:
    //
    // List const_iterator.
    //
    class const_iterator
    {
    public:
	const_iterator() 
            : node_(NULL) 
        {}
	const_iterator(const List_Node *nd) 
            : node_((List_Node*)nd) 
        {}
	const List_Node * operator *() 
        { 
            return node_; 
        }
	const List_Node * operator -> () 
        { 
            return node_; 
        }
	const_iterator operator++() 
        { 
            return const_iterator((const List_Node *)node_->next); 
        }
	bool operator==(const const_iterator &rhs) 
        { 
            return node_ == rhs.node_; 
        }
	bool operator!=(const const_iterator &rhs) 
        { 
            return node_ != rhs.node_; 
        }

    protected:
	List_Node *node_;
    };

    //
    // List iterator.
    //
    class iterator : public const_iterator
    {
    public:
	iterator() 
        {}
	iterator(List_Node *nd) 
            : const_iterator(nd) 
        {}
	List_Node * operator *() 
        { 
            return node_; 
        }
	List_Node * operator -> () 
        { 
            return node_; 
        }
	iterator operator++() 
        { 
            return iterator((List_Node*)node_->next); 
        }
	bool operator==(const iterator &rhs) 
        { 
            return node_ == rhs.node_; 
        }
	bool operator!=(const iterator &rhs) 
        { 
            return node_ != rhs.node_; 
        }
    };

    //
    // Default constructor.
    //
    Pj_List() 
    { 
        pj_list_init(&root_); 
        if (0) compiletest(); 
    }

    //
    // You can cast Pj_List to pj_list
    //
    operator pj_list&()
    {
	return (pj_list&)root_;
    }
    operator const pj_list&()
    {
	return (const pj_list&)root_;
    }

    //
    // You can cast Pj_List to pj_list* too
    //
    operator pj_list*()
    {
	return (pj_list*)&root_;
    }
    operator const pj_list*()
    {
	return (const pj_list*)&root_;
    }

    //
    // Check if list is empty.
    // 
    bool empty() const
    {
	return pj_list_empty(&root_);
    }

    //
    // Get first element.
    //
    iterator begin()
    {
	return iterator(root_.next);
    }

    //
    // Get first element.
    //
    const_iterator begin() const
    {
	return const_iterator(root_.next);
    }

    //
    // Get end-of-element
    //
    const_iterator end() const
    {
	return const_iterator((List_Node*)&root_);
    }

    //
    // Get end-of-element
    //
    iterator end()
    {
	return iterator((List_Node*)&root_);
    }

    //
    // Insert node.
    //
    void insert_before (iterator &pos, List_Node *node)
    {
	pj_list_insert_before( *pos, node );
    }

    //
    // Insert node.
    //
    void insert_after(iterator &pos, List_Node *node)
    {
	pj_list_insert_after(*pos, node);
    }

    //
    // Merge list.
    //
    void merge_first(List_Node *list2)
    {
	pj_list_merge_first(&root_, list2);
    }

    //
    // Merge list.
    //
    void merge_last(Pj_List *list)
    {
	pj_list_merge_last(&root_, &list->root_);
    }

    //
    // Insert list.
    //
    void insert_nodes_before(iterator &pos, Pj_List *list2)
    {
	pj_list_insert_nodes_before(*pos, &list2->root_);
    }

    //
    // Insert list.
    //
    void insert_nodes_after(iterator &pos, Pj_List *list2)
    {
	pj_list_insert_nodes_after(*pos, &list2->root_);
    }

    //
    // Erase an element.
    //
    void erase(iterator &it)
    {
	pj_list_erase(*it);
    }

    //
    // Get first element.
    //
    List_Node *front()
    {
	return root_.next;
    }

    //
    // Get first element.
    //
    const List_Node *front() const
    {
	return root_.next;
    }

    //
    // Remove first element.
    //
    void pop_front()
    {
	pj_list_erase(root_.next);
    }

    //
    // Get last element.
    //
    List_Node *back()
    {
	return root_.prev;
    }

    //
    // Get last element.
    //
    const List_Node *back() const
    {
	return root_.prev;
    }

    //
    // Remove last element.
    //
    void pop_back()
    {
	pj_list_erase(root_.prev);
    }

    //
    // Find a node.
    //
    iterator find(List_Node *node)
    {
	List_Node *n = pj_list_find_node(&root_, node);
	return n ? iterator(n) : end();
    }

    //
    // Find a node.
    //
    const_iterator find(List_Node *node) const
    {
	List_Node *n = pj_list_find_node(&root_, node);
	return n ? const_iterator(n) : end();
    }

    //
    // Insert a node in the back.
    //
    void push_back(List_Node *node)
    {
	pj_list_insert_after(root_.prev, node);
    }

    //
    // Insert a node in the front.
    //
    void push_front(List_Node *node)
    {
	pj_list_insert_before(root_.next, node);
    }

    //
    // Remove all elements.
    //
    void clear()
    {
	root_.next = &root_;
	root_.prev = &root_;
    }

private:
    struct RootNode
    {
	PJ_DECL_LIST_MEMBER(List_Node);
    } root_;

    void compiletest()
    {
	// If you see error in this line, 
	// it's because List_Node is not derived from Pj_List_Node.
	List_Node *n = (List_Node*)0;
	n = (List_Node *)n->next; n = (List_Node *)n->prev;
    }
};


#endif	/* __PJPP_LIST_HPP__ */

