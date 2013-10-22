typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef long int32;
typedef unsigned long uint32;
typedef float float64;

// argb to pens
#define RPTAG_APenColor RPTAG_APen
#define RPTAG_BPenColor RPTAG_BPen

/* Handy macros for checking what kind of object a FileInfoBlock 
  describes; Examine() / ExNext(). 
  -- Special versions of these macros for ExAll() are available 
    in the include file; dos/exall.h */ 

#define FIB_IS_FILE(fib)   ((fib)->fib_DirEntryType < 0) 
 
#define FIB_IS_DRAWER(fib)  ((fib)->fib_DirEntryType >= 0 && (fib)->fib_DirEntryType != ST_SOFTLINK) 
 
#define FIB_IS_LINK(fib)   ((fib)->fib_DirEntryType == ST_SOFTLINK || (fib)->fib_DirEntryType == ST_LINKDIR || (fib)->fib_DirEntryType == ST_LINKFILE) 
 
#define FIB_IS_SOFTLINK(fib) ((fib)->fib_DirEntryType == ST_SOFTLINK) 
 
#define FIB_IS_LINKDIR(fib)

