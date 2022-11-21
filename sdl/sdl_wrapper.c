
#include <stdio.h>
#include <stdlib.h>
#include <system_includes.h>

static APTR _AllocVecTags(uint32 size, ...)
{
  return malloc(size);
}

static void _FreeVec(APTR ptr)
{
  if (ptr) free(ptr);
}

static APTR _AllocSysObjectTags(uint32 type, ...)
{
  APTR result = NULL;

  switch (type)
  {
    case ASOT_SEMAPHORE:
    {
      struct SignalSemaphore *sem = malloc(sizeof(struct SignalSemaphore));
      if (!sem) return NULL;
      sem->mtx = SDL_CreateMutex();
      if (!sem->mtx) { free(sem); return NULL; }
      result = sem;
      break;
    }

    case ASOT_LIST:
    {
      struct List *list = malloc(sizeof(struct List));
      if (!list) return NULL;
      list->lh_Head = (struct Node *)(&list->lh_Tail);
      list->lh_Tail = NULL;
      list->lh_TailPred = (struct Node *)(&list->lh_Head);
      result = list;
    }
  }

  return result;
}

static void _FreeSysObject(uint32 type, APTR ptr)
{
  switch (type)
  {
    case ASOT_SEMAPHORE:
    {
      struct SignalSemaphore *sem = (struct SignalSemaphore *)ptr;
      if (sem)
      {
        SDL_DestroyMutex(sem->mtx);
        free(sem);
      }
      break;
    }

    case ASOT_LIST:
    case ASOT_NODE:
      free(ptr);
      break;
  }
}

static void _ObtainSemaphore(struct SignalSemaphore *sem)
{
  SDL_LockMutex(sem->mtx);
}

static void _ReleaseSemaphore(struct SignalSemaphore *sem)
{
  SDL_UnlockMutex(sem->mtx);
}

static void _CopyMem(APTR src, APTR dest, uint32 size)
{
  memcpy(dest, src, size);
}

static struct Node *_GetHead(struct List *list)
{
  if ((!list) || (!list->lh_Head)) return NULL;
  if (list->lh_Head->ln_Succ == NULL) return NULL;
  return list->lh_Head;
}

static struct Node *_GetSucc(struct Node *node)
{
  if (node->ln_Succ->ln_Succ == NULL) return NULL;
  return node->ln_Succ;
}

static void _AddTail(struct List *list, struct Node *node)
{
  struct Node *tmp;

  tmp = list->lh_TailPred;
  list->lh_TailPred = node;
  node->ln_Succ = (struct Node *)&list->lh_Tail;
  node->ln_Pred = tmp;
  tmp->ln_Succ = node;
}

static void _Remove(struct Node *node)
{
  node->ln_Succ->ln_Pred = node->ln_Pred;
  node->ln_Pred->ln_Succ = node->ln_Succ;
}

static APTR _AllocPooled(APTR pool, uint32 size)
{
  return malloc(size);
}

static void _FreePooled(APTR pool, APTR ptr, uint32 size)
{
  free(ptr);
}

static struct Node *_RemHead(struct List *list)
{
  struct Node *node;

  if ((!list) || (!list->lh_Head) || (!list->lh_Head->ln_Succ)) return NULL;

  node = list->lh_Head;
  list->lh_Head = node->ln_Succ;
  node->ln_Succ->ln_Pred = (struct Node *)list;

  return node;
}

static struct Node *_GetTail(struct List *list)
{
  if ((!list) || (!list->lh_TailPred) || (!list->lh_TailPred->ln_Pred)) return NULL;
  return list->lh_TailPred;
}

static struct Node *_RemTail(struct List *list)
{
	struct Node *n = list->lh_TailPred;

	if (n->ln_Pred)
	{
		list->lh_TailPred = n->ln_Pred;
		list->lh_TailPred->ln_Succ = (struct Node *)(&(list->lh_Tail));
		return n;
	}
	return NULL;
}

static struct ExecIFace _exec = { .AllocVecTags       = _AllocVecTags,
                                  .FreeVec            = _FreeVec,
                                  .AllocSysObjectTags = _AllocSysObjectTags,
                                  .FreeSysObject      = _FreeSysObject,
                                  .ObtainSemaphore    = _ObtainSemaphore,
                                  .ReleaseSemaphore   = _ReleaseSemaphore,
                                  .CopyMem            = _CopyMem,
                                  .GetHead            = _GetHead,
                                  .GetSucc            = _GetSucc,
                                  .AddTail            = _AddTail,
                                  .Remove             = _Remove,
                                  .AllocPooled        = _AllocPooled,
                                  .FreePooled         = _FreePooled,
                                  .RemHead            = _RemHead,
                                  .GetTail            = _GetTail,
                                  .RemTail            = _RemTail };

struct ExecIFace *IExec = &_exec;



extern BOOL pref_rctrlplaypos;

uint16 sdl_keysym_to_amiga_rawkey(SDLKey keysym)
{
  /* Convert SDL key symbols to Amiga raw keys */
  switch (keysym)
  {
    case SDLK_F12:       return 13; // \ = drum pad mode (F12 on PC keyboards)
    case SDLK_BACKSLASH: return 48; // Blank next to Z (shut up) (backslash on PC keyboards)
    case SDLK_MINUS:     return 11; // Minus
    case SDLK_EQUALS:    return 12; // Equals
    case SDLK_F1:        return 80;
    case SDLK_F2:        return 81;
    case SDLK_F3:        return 82;
    case SDLK_F4:        return 83;
    case SDLK_F5:        return 84;
    case SDLK_F6:        return 85;
    case SDLK_F7:        return 86;
    case SDLK_F8:        return 87;
    case SDLK_F9:        return 88;
    case SDLK_F10:       return 89;
    case SDLK_ESCAPE:    return 69;
    case SDLK_MENU:      return 103;
    case SDLK_RSUPER:    return 103;
    case SDLK_RMETA:     return 103;
    case SDLK_RALT:      return 101;
#ifdef __linux__
    /* On ubuntu, Alt Gr seems to return this (313) instead of SDLK_RALT (307) */
    case 313:            return 101;
#endif
    case SDLK_KP_PLUS:   return 94;
    case SDLK_KP_MINUS:  return 74;
    case SDLK_LEFT:      return 79;
    case SDLK_RIGHT:     return 78;
    case SDLK_UP:        return 76;
    case SDLK_DOWN:      return 77;
    case SDLK_BACKSPACE: return 65;
    case SDLK_RETURN:    return 68;
    case SDLK_1:         return 1;
    case SDLK_2:         return 2;
    case SDLK_3:         return 3;
    case SDLK_4:         return 4;
    case SDLK_5:         return 5;
    case SDLK_6:         return 6;
    case SDLK_7:         return 7;
    case SDLK_8:         return 8;
    case SDLK_9:         return 9;
    case SDLK_0:         return 10;
    case SDLK_KP0:       return 0x0f;
    case SDLK_KP1:       return 0x1d;
    case SDLK_KP2:       return 0x1e;
    case SDLK_KP3:       return 0x1f;
    case SDLK_KP4:       return 0x2d;
    case SDLK_KP5:       return 0x2e;
    case SDLK_KP6:       return 0x2f;
    case SDLK_KP7:       return 0x3d;
    case SDLK_KP8:       return 0x3e;
    case SDLK_KP9:       return 0x3f;
    case SDLK_KP_PERIOD: return 60;
    case SDLK_q:         return 16;
    case SDLK_w:         return 17;
    case SDLK_e:         return 18;
    case SDLK_r:         return 19;
    case SDLK_t:         return 20;
    case SDLK_y:         return 21;
    case SDLK_u:         return 22;
    case SDLK_i:         return 23;
    case SDLK_o:         return 24;
    case SDLK_p:         return 25;
    case SDLK_LEFTBRACKET: return 26;
    case SDLK_RIGHTBRACKET: return 27;
    case SDLK_a:         return 32;
    case SDLK_s:         return 33;
    case SDLK_d:         return 34;
    case SDLK_f:         return 35;
    case SDLK_g:         return 36;
    case SDLK_h:         return 37;
    case SDLK_j:         return 38;
    case SDLK_k:         return 39;
    case SDLK_l:         return 40;
    case SDLK_SEMICOLON: return 41;
    case SDLK_QUOTE:     return 42;
    case SDLK_HASH:      return 43;
    case SDLK_z:         return 49;
    case SDLK_x:         return 50;
    case SDLK_c:         return 51;
    case SDLK_v:         return 52;
    case SDLK_b:         return 53;
    case SDLK_n:         return 54;
    case SDLK_m:         return 55;
    case SDLK_COMMA:     return 56;
    case SDLK_PERIOD:    return 57;
    case SDLK_SLASH:     return 58;
    case SDLK_SPACE:     return 64;
    case SDLK_TAB:       return 0x42;
    case SDLK_DELETE:    return 0x46;

    case SDLK_RCTRL:
      if (pref_rctrlplaypos)
        return 103; /* My linux laptop has no RSUPER or MENU, so use RCTRL */
      break;
      default:
          break;
  }
  return 0;
}

