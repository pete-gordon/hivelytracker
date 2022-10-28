
This replayer was basically ripped out of HivelyTracker and bodged into a
standalone replayer. Its not necessarily optimal, i haven't had a chance
to comment anything.

There are no documents for the HVL module format, I just knocked it together
as I wrote hivelytracker. This replayer is the documentation.


Functions:

void hvl_InitReplayer( void )

  This calculates all waves and stuff. Call it before any other replayer
  funcs.


struct hvl_tune *hvl_LoadTune( TEXT *name, uint32 freq, uint32 defstereo )

  Specify the filename and mixing frequency, and it will load the tune into
  RAM, and return a handle. W00t!
  
  defstereo is the stereo seperation for playing AHX tunes (HVL tunes override
  this setting and ignore it). It can be:
  
  0 = 0%  (mono)
  1 = 25%
  2 = 50%
  3 = 75%
  4 = 100% (paula)


void hvl_FreeTune( struct hvl_tune *ht )

  Frees the tune again.


BOOL hvl_InitSubsong( struct hvl_tune *ht, uint32 subsong )

  Initialises a subsong ready for playing


void hvl_DecodeFrame( struct hvl_tune *ht, int8 *buf1, int8 *buf2, int32 bufmod )

  Decodes a frame of HVL music. Works the same as the ahx replayer library
  on OS4Depot.



Enjoy.
Peter Gordon (pete@gordon.plus)
