 /*! \file mylist.c
     \brief Copy of the list implementation from the Linux kernel. Used to create and manipulate double-linked lists.
 */
#include <stdlib.h>

#include "mylist.h"

void init_agios_list_head(struct agios_list_head *list)
{
	list->next = list;
	list->prev = list;
}
void __agios_list_add(struct agios_list_head *new, struct agios_list_head *prev, struct agios_list_head *next)
{
        next->prev = new;
        new->next = next;
        new->prev = prev;
        prev->next = new;
}
void agios_list_add(struct agios_list_head *new, struct agios_list_head *head)
{
        __agios_list_add(new, head, head->next);
}
void agios_list_add_tail(struct agios_list_head *new, struct agios_list_head *head)
{
	__agios_list_add(new, head->prev, head);
}
void __agios_list_del(struct agios_list_head * prev, struct agios_list_head * next)
{
        next->prev = prev;
        prev->next = next;
}
void agios_list_del(struct agios_list_head *entry)
{
        __agios_list_del(entry->prev, entry->next);
        entry->next = entry;
        entry->prev = entry;
}
void __agios_list_del_entry(struct agios_list_head *entry)
{
        __agios_list_del(entry->prev, entry->next);
}
void agios_list_del_init(struct agios_list_head *entry)
{
        __agios_list_del_entry(entry);
        init_agios_list_head(entry);
}
bool agios_list_empty(const struct agios_list_head *head)
{
        return head->next == head;
}
