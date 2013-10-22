#include <system_includes.h>

struct Node * GetHead(struct List *list)
{
  if ((!list) || (!list->lh_Head)) return NULL;
  if (list->lh_Head->ln_Succ == NULL) return NULL;
  return list->lh_Head;
}

struct Node * GetSucc(struct Node *node)
{
  if (node->ln_Succ->ln_Succ == NULL) return NULL;
  return node->ln_Succ;
}

struct Node * GetTail(struct List *list)
{
  if ((!list) || (!list->lh_TailPred) || (!list->lh_TailPred->ln_Pred)) return NULL;
  return list->lh_TailPred;
}
