
#ifndef __SDL_WRAPPER_H__
#define __SDL_WRAPPER_H__

/* Map amiga types to SDL types */
typedef SDL_bool BOOL;
typedef Uint32   uint32;
typedef Sint32   int32;
typedef Uint16   uint16;
typedef Sint16   int16;
typedef Uint8    uint8;
typedef Sint8    int8;
typedef char     TEXT;
typedef char *   STRPTR;
typedef void *   APTR;
typedef float    float32;
typedef double   float64;
typedef Sint32   LONG;

#define CONST const

#define TRUE SDL_TRUE
#define FALSE SDL_FALSE

#define ASOT_SEMAPHORE 1
#define ASOT_LIST 2
#define ASOT_NODE 3

struct SignalSemaphore
{
  SDL_mutex *mtx;
};

struct List
{
  struct Node *lh_Head;
  struct Node *lh_Tail;
  struct Node *lh_TailPred;
};

#define NT_USER 0
struct Node
{
  struct Node *ln_Succ;
  struct Node *ln_Pred;
  STRPTR       ln_Name;
  int8         ln_Pri;
  uint8        ln_Type;
};

struct ExecIFace
{
  APTR (*AllocVecTags)(uint32, ...);
  void (*FreeVec)(void *);
  APTR (*AllocSysObjectTags)(uint32, ...);
  void (*FreeSysObject)(uint32, APTR);
  void (*ObtainSemaphore)(struct SignalSemaphore *);
  void (*ReleaseSemaphore)(struct SignalSemaphore *);
  void (*CopyMem)(APTR src, APTR dest, uint32 size);
  struct Node * (*GetHead)(struct List *);
  struct Node * (*GetSucc)(struct Node *);
  void (*AddTail)(struct List *, struct Node *);
  void (*Remove)(struct Node *);
  APTR (*AllocPooled)(APTR, uint32);
  void (*FreePooled)(APTR, APTR, uint32);
  struct Node *(*RemHead)(struct List *);
  struct Node *(*GetTail)(struct List *);
  struct Node *(*RemTail)(struct List *);
};


extern struct ExecIFace *IExec;

#define SRFTYPE SDL_HWSURFACE
//#define SRFTYPE SDL_SWSURFACE

#define IEQUALIFIER_RBUTTON 1
#define IEQUALIFIER_LCOMMAND 2
#define IEQUALIFIER_CONTROL 4
#define IEQUALIFIER_LSHIFT  8
#define IEQUALIFIER_RSHIFT  16
#define IEQUALIFIER_LALT    32

#define SELECTDOWN SDL_BUTTON_LEFT
#define SELECTUP   SDL_BUTTON_LEFT|0x100
#define MENUDOWN   SDL_BUTTON_RIGHT
#define MENUUP     SDL_BUTTON_RIGHT|0x100

#define IDCMP_MOUSEMOVE 1
#define IDCMP_MOUSEBUTTONS 2
#define IDCMP_RAWKEY 4
#define IDCMP_EXTENDEDMOUSE 8

#define IMSGCODE_INTUIWHEELDATA 1

struct IntuiMessage
{
  uint16 Qualifier;
  uint16 Class;
  uint16 Code;
  int32  MouseX;
  int32  MouseY;
  void * IAddress;
};

struct IntuiWheelData
{
  int16 WheelY;
};

uint16 sdl_keysym_to_amiga_rawkey(SDLKey keysym);

#define TAG_DONE 0

#define REQIMAGE_QUESTION 0
#define REQIMAGE_WARNING 1
#define REQIMAGE_ERROR 2

#endif /* __SDL_WRAPPER_H__ */
