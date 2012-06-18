/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef VSF_STRLIST_H
#define VSF_STRLIST_H

/* Forward declarations */
struct mystr;
struct mystr_list_node;

struct mystr_list
{
  unsigned int PRIVATE_HANDS_OFF_alloc_len;
  unsigned int PRIVATE_HANDS_OFF_list_len;
  struct mystr_list_node* PRIVATE_HANDS_OFF_p_nodes;
};

#define INIT_STRLIST \
  { 0, 0, (void*)0 }

void str_list_free(struct mystr_list* p_list);

void str_list_add(struct mystr_list* p_list, const struct mystr* p_str,
                  const struct mystr* p_sort_key_str);
void str_list_sort(struct mystr_list* p_list, int reverse);

int str_list_get_length(const struct mystr_list* p_list);
int str_list_contains_str(const struct mystr_list* p_list,
                          const struct mystr* p_str);

const struct mystr* str_list_get_pstr(const struct mystr_list* p_list,
                                      unsigned int indexx);

#endif /* VSF_STRLIST_H */

