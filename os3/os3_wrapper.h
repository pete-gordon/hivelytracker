#ifndef __OS3_WRAPPER_H__
#define __OS3_WRAPPER_H__

typedef signed char    int8;
typedef unsigned char  uint8;
typedef short          int16;
typedef unsigned short uint16;
typedef long           int32;
typedef unsigned long  uint32;
typedef float          float64;
typedef void *         APTR;

// argb to pens
#define RPTAG_APenColor RPTAG_APen
#define RPTAG_BPenColor RPTAG_BPen

#define REQIMAGE_QUESTION 0
#define REQIMAGE_WARNING 1
#define REQIMAGE_ERROR 2

struct Node * GetHead(struct List *);
struct Node * GetSucc(struct Node *);
struct Node * GetTail(struct List *);
  
#endif /* __OS3_WRAPPER_H__ */
