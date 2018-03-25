/*
 * Copyright (C) 2017 Jianhui Zhao <jianhuizhao329@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libubox/ulog.h>

#include "utils.h"
#include "term.h"
#include "config.h"

struct avl_tree term_tree;

struct terminal *find_term(const char *mac)
{
	struct terminal *term;

	return avl_find_element(&term_tree, mac, term, avl);
}

void del_term(struct terminal *term)
{
	deny_termianl(term->mac);
	avl_delete(&term_tree, &term->avl);
	uloop_timeout_cancel(&term->timeout);
	free(term);
}

void del_term_by_mac(const char *mac)
{
	struct terminal *term = find_term(mac);

	if (term)
		del_term(term);
}

static void term_timeout_cb(struct uloop_timeout *t)
{
	struct terminal *term = container_of(t, struct terminal, timeout);

	if (term->flag & TERM_FLAG_AUTHED) {
		ULOG_INFO("terminal timeout: %s\n", term->mac);
		term->flag |= TERM_FLAG_TIMEOUT;
		return;
	}

	ULOG_INFO("terminal auth timeout: %s\n", term->mac);
	del_term(term);
}

struct terminal *term_new(const char *mac, const char *ip, const char *token)
{
	struct terminal *term;

	term = calloc(1, sizeof(struct terminal));
	if (!term) {
		ULOG_ERR("term_new failed: No mem\n");
		return NULL;
	}

	ULOG_INFO("New terminal:%s %s\n", mac, ip);

	strncpy(term->token, token, sizeof(term->token) - 1);
	strncpy(term->ip, ip, sizeof(term->ip) - 1);
	term->avl.key = strcpy(term->mac, mac);
	term->timeout.cb = term_timeout_cb;
	uloop_timeout_set(&term->timeout, 1000 * 60);
	avl_insert(&term_tree, &term->avl);
	return term;
}

void auth_term_by_mac(const char *mac)
{
	struct terminal *term = find_term(mac);
	struct config *conf = get_config();

	if (term) {
		ULOG_INFO("Auth terminal:%s\n", mac);
		term->flag |= TERM_FLAG_AUTHED;
		term->auth_time = time(NULL);
		allow_termianl(mac, false);
		uloop_timeout_set(&term->timeout, conf->checkinterval * conf->clienttimeout * 1000);
	}
}

static int avl_strcmp(const void *k1, const void *k2, void *ptr)
{
	return strcasecmp(k1, k2);
}

int term_init()
{
	avl_init(&term_tree, avl_strcmp, false, NULL);
	return 0;
}

void term_deinit()
{
	struct terminal *term, *ptr;

	avl_for_each_element_safe(&term_tree, term, avl, ptr) {
		del_term(term);
	}
}