#include "util/types.h"
#include "util/list.h"

void
list_pushback(struct list_node* head, struct list_node* node)
{
  node->next = head;
  node->prev = head->prev;
  head->prev->next = node;
  head->prev = node;
}

void
list_pushfront(struct list_node* head, struct list_node* node)
{
  node->prev = head;
  node->next = head->next;
  head->next->prev = node;
  head->next = node;
}

void
list_remove(struct list_node* node)
{
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = node->next = NULL;
}