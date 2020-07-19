/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NSS_NLIST_H__
#define __NSS_NLIST_H__

/**
 * @addtogroup libnss_nl
 * @{
 */

/**
 * @file nss_nlist.h
 * 	This file declares the NSS NL list API(s) for user space
 */

/**
 * @brief list node
 */
struct nss_nlist {
	struct nss_nlist *next;	/**< next node */
	struct nss_nlist *prev;	/**< previous node */
};

/**
 * @brief initialize the list node
 *
 * @param node[IN] list node
 */
static inline void nss_nlist_init(struct nss_nlist *node)
{
	node->next = node->prev = node;
}

/**
 * @brief get the previous of the node
 *
 * @param node[IN] node
 *
 * @return previous node or head node
 */
static inline struct nss_nlist *nss_nlist_prev(struct nss_nlist *node)
{
	return node->prev;
}

/**
 * @brief get next of the node
 *
 * @param node[IN] node
 *
 * @return next node or head node
 */
static inline struct nss_nlist *nss_nlist_next(struct nss_nlist *node)
{
	return node->next;
}

/**
 * @brief initialize the head node
 *
 * @param head[IN] head of list
 */
static inline void nss_nlist_init_head(struct nss_nlist *head)
{
	nss_nlist_init(head);
}

/**
 * @brief return the first node in the list
 *
 * @param head[IN] list head
 *
 * @return first node
 */
static inline struct nss_nlist *nss_nlist_first(struct nss_nlist *head)
{
	return nss_nlist_next(head);
}

/**
 * @brief return the last node in the list
 *
 * @param head[IN] list head
 *
 * @return last node
 */
static inline struct nss_nlist *nss_nlist_last(struct nss_nlist *head)
{
	return nss_nlist_prev(head);
}

/**
 * @brief check if the list is empty
 *
 * @param head[IN] list head
 *
 * @return true if empty
 */
static inline bool nss_nlist_isempty(struct nss_nlist *head)
{
	struct nss_nlist *first = nss_nlist_first(head);

	return first == head;
}

/**
 * @brief check if this the last node
 *
 * @param head[IN] head node
 * @param node[IN] node to check
 *
 * @return true if it is the last node
 */
static inline bool nss_nlist_islast(struct nss_nlist *head, struct nss_nlist *node)
{
	struct nss_nlist *last = nss_nlist_last(head);

	return last == node;
}

/**
 * @brief add node to head of the list
 *
 * @param head[IN] list head
 * @param node[IN] node to add
 */
static inline void nss_nlist_add_head(struct nss_nlist *head, struct nss_nlist *node)
{
	struct nss_nlist *first = nss_nlist_first(head);

	node->prev = head;
	node->next = first;

	first->prev = node;
	head->next = node;

}

/**
 * @brief add node to tail of the list
 *
 * @param head[IN] list head
 * @param node[IN] node to add
 */
static inline void nss_nlist_add_tail(struct nss_nlist *head, struct nss_nlist *node)
{
	struct nss_nlist *last = nss_nlist_last(head);

	node->next = head;
	node->prev = last;

	last->next = node;
	head->prev = node;
}

/**
 * @brief unlink the node from the list
 *
 * @param node[IN] node to unlink
 */
static inline void nss_nlist_unlink(struct nss_nlist *node)
{
	struct nss_nlist *prev = nss_nlist_prev(node);
	struct nss_nlist *next = nss_nlist_next(node);

	prev->next = next;
	next->prev = prev;

	nss_nlist_init(node);
}

/**
 * @brief list node iterator
 *
 * @param _tmp[IN] a temp node for assignment
 * @param _head[IN] head node to start
 */
#define nss_nlist_iterate(_tmp, _head)			\
	for ((_tmp) = nss_nlist_first((_head));		\
		!nss_nlist_islast((_head), (_tmp));	\
		(_tmp) = nss_nlist_next((_tmp))
/**}@*/
#endif /* !__NSS_NLIST_H__*/
