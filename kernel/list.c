#include "type.h"
#include "list.h"

// 尾插
void
insert_list(struct list_node* head, struct list_node* node)
{
  node->next = head;
  node->prev = head->prev;
  head->prev->next = node;
  head->prev = node;
}

void
remove_from_list(struct list_node* node)
{
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = node->next = NULL;
}