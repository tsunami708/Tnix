#pragma once

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - (u64) & ((type*)0)->member))

/*
container_of:
  ptr:    链表节点地址
  type:   内嵌链表节点的结构体
  member: 结构体中struct list_node成员名称

demo:
struct page{
  ...
  struct list_node page_node;
};

struct list_node* node = ... ;
struct page* p=container_of(node,struct page,page_node)
*/
struct list_node {
  struct list_node* next;
  struct list_node* prev;
};

/*静态定义一个链表头*/
#define INIT_LIST(name) struct list_node name = { .next = &name, .prev = &name }

static inline __attribute__((always_inline)) void
list_init(struct list_node* head)
{
  head->next = head->prev = head;
}
void list_pushback(struct list_node* head, struct list_node* node);
void list_pushfront(struct list_node* head, struct list_node* node);
void list_remove(struct list_node* node);