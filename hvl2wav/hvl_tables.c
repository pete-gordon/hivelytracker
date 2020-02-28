#include <math.h>

#include "types.h"

#include "hvl_replay.h"
#include "hvl_tables.h"

static const uint16 lentab[45] = { 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 3, 7, 0xf, 0x1f, 0x3f, 0x7f,
    0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
    0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
    (0x280*3)-1 };

const int16 vib_tab[64] =
{
  0,24,49,74,97,120,141,161,180,197,212,224,235,244,250,253,255,
  253,250,244,235,224,212,197,180,161,141,120,97,74,49,24,
  0,-24,-49,-74,-97,-120,-141,-161,-180,-197,-212,-224,-235,-244,-250,-253,-255,
  -253,-250,-244,-235,-224,-212,-197,-180,-161,-141,-120,-97,-74,-49,-24
};

const uint16 period_tab[61] =
{
  0x0000, 0x0D60, 0x0CA0, 0x0BE8, 0x0B40, 0x0A98, 0x0A00, 0x0970,
  0x08E8, 0x0868, 0x07F0, 0x0780, 0x0714, 0x06B0, 0x0650, 0x05F4,
  0x05A0, 0x054C, 0x0500, 0x04B8, 0x0474, 0x0434, 0x03F8, 0x03C0,
  0x038A, 0x0358, 0x0328, 0x02FA, 0x02D0, 0x02A6, 0x0280, 0x025C,
  0x023A, 0x021A, 0x01FC, 0x01E0, 0x01C5, 0x01AC, 0x0194, 0x017D,
  0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0,
  0x00E2, 0x00D6, 0x00CA, 0x00BE, 0x00B4, 0x00AA, 0x00A0, 0x0097,
  0x008F, 0x0087, 0x007F, 0x0078, 0x0071
};

const int32 stereopan_left[5]  = { 128,  96,  64,  32,   0 };
const int32 stereopan_right[5] = { 128, 160, 193, 225, 255 };


static void hvl_GenPanningTables( void )
{
  uint32 i;
  float64 aa, ab;

  // Sine based panning table
  aa = (3.14159265f*2.0f)/4.0f;   // Quarter of the way through the sinewave == top peak
  ab = 0.0f;                      // Start of the climb from zero

  for( i=0; i<256; i++ )
  {
    panning_left[i]  = (uint32)(sin(aa)*255.0f);
    panning_right[i] = (uint32)(sin(ab)*255.0f);

    aa += (3.14159265*2.0f/4.0f)/256.0f;
    ab += (3.14159265*2.0f/4.0f)/256.0f;
  }
  panning_left[255] = 0;
  panning_right[0] = 0;
}

static void hvl_GenSawtooth( int8 *buf, uint32 len )
{
  uint32 i;
  int32  val, add;

  add = 256 / (len-1);
  val = -128;

  for( i=0; i<len; i++, val += add )
    *buf++ = (int8)val;
}

static void hvl_GenTriangle( int8 *buf, uint32 len )
{
  uint32 i;
  int32  d2, d5, d1, d4;
  int32  val;
  int8   *buf2;

  d2  = len;
  d5  = len >> 2;
  d1  = 128/d5;
  d4  = -(d2 >> 1);
  val = 0;

  for( i=0; i<d5; i++ )
  {
    *buf++ = val;
    val += d1;
  }
  *buf++ = 0x7f;

  if( d5 != 1 )
  {
    val = 128;
    for( i=0; i<d5-1; i++ )
    {
      val -= d1;
      *buf++ = val;
    }
  }

  buf2 = buf + d4;
  for( i=0; i<d5*2; i++ )
  {
    int8 c;

    c = *buf2++;
    if( c == 0x7f )
      c = 0x80;
    else
      c = -c;

    *buf++ = c;
  }
}

static void hvl_GenSquare( int8 *buf )
{
  uint32 i, j;

  for( i=1; i<=0x20; i++ )
  {
    for( j=0; j<(0x40-i)*2; j++ )
      *buf++ = 0x80;
    for( j=0; j<i*2; j++ )
      *buf++ = 0x7f;
  }
}

static inline float64 clip( float64 x )
{
  if( x > 127.f )
    x = 127.f;
  else if( x < -128.f )
    x = -128.f;
  return x;
}

static void hvl_GenFilterWaves( int8 *buf, int8 *lowbuf, int8 *highbuf )
{
  float64 freq;
  uint32  temp;

  for( temp=0, freq=8.f; temp<31; temp++, freq+=3.f )
  {
    uint32 wv;
    int8   *a0 = buf;

    for( wv=0; wv<6+6+0x20+1; wv++ )
    {
      float64 fre, high, mid, low;
      uint32  j;

      mid = 0.f;
      low = 0.f;
      fre = freq * 1.25f / 100.0f;

      for( j=0; j<=lentab[wv]; j++ )
      {
        high  = a0[j] - mid - low;
        high  = clip( high );
        mid  += high * fre;
        mid   = clip( mid );
        low  += mid * fre;
        low   = clip( low );
        *lowbuf++  = (int8)low;
        *highbuf++ = (int8)high;
      }

      a0 += lentab[wv]+1;
    }
  }
}

static void hvl_GenWhiteNoise( int8 *buf, uint32 len )
{
  uint32 ays;

  ays = 0x41595321;

  do {
    uint16 ax, bx;
    int8 s;

    s = ays;

    if( ays & 0x100 )
    {
      s = 0x80;

      if( (int32)(ays & 0xffff) >= 0 )
        s = 0x7f;
    }

    *buf++ = s;
    len--;

    ays = (ays >> 5) | (ays << 27);
    ays = (ays & 0xffffff00) | ((ays & 0xff) ^ 0x9a);
    bx  = ays;
    ays = (ays << 2) | (ays >> 30);
    ax  = ays;
    bx  += ax;
    ax  ^= bx;
    ays  = (ays & 0xffff0000) | ax;
    ays  = (ays >> 3) | (ays << 29);
  } while( len );
}

void hvl_GenTables( void )
{
  hvl_GenPanningTables();
  hvl_GenSawtooth( &waves[WO_SAWTOOTH_04], 0x04 );
  hvl_GenSawtooth( &waves[WO_SAWTOOTH_08], 0x08 );
  hvl_GenSawtooth( &waves[WO_SAWTOOTH_10], 0x10 );
  hvl_GenSawtooth( &waves[WO_SAWTOOTH_20], 0x20 );
  hvl_GenSawtooth( &waves[WO_SAWTOOTH_40], 0x40 );
  hvl_GenSawtooth( &waves[WO_SAWTOOTH_80], 0x80 );
  hvl_GenTriangle( &waves[WO_TRIANGLE_04], 0x04 );
  hvl_GenTriangle( &waves[WO_TRIANGLE_08], 0x08 );
  hvl_GenTriangle( &waves[WO_TRIANGLE_10], 0x10 );
  hvl_GenTriangle( &waves[WO_TRIANGLE_20], 0x20 );
  hvl_GenTriangle( &waves[WO_TRIANGLE_40], 0x40 );
  hvl_GenTriangle( &waves[WO_TRIANGLE_80], 0x80 );
  hvl_GenSquare( &waves[WO_SQUARES] );
  hvl_GenWhiteNoise( &waves[WO_WHITENOISE], WHITENOISELEN );
  hvl_GenFilterWaves( &waves[WO_TRIANGLE_04], &waves[WO_LOWPASSES], &waves[WO_HIGHPASSES] );
}
