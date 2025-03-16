/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2015             *
 * by the Xiph.Org Foundation https://xiph.org/                     *
 *                                                                  *
 ********************************************************************

 function: basic shared codebook operations

 ********************************************************************/

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <ogg/ogg.h>
#include "os.h"
#include "misc.h"
#include "vorbis/codec.h"
#include "codebook.h"
#include "scales.h"
#include <ogg/logger.h>

/**** pack/unpack helpers ******************************************/

int ov_ilog(ogg_uint32_t v){
  int ret;
  log_message("[TRACE] Hash: 91699f91453a64b311a4f691ac5f389c, File: vorbis/lib/sharedbook.c, Func: ov_ilog, Line: 32, Col: 13\n");
  for(ret=0;v;ret++)v>>=1;
  return ret;
}

/* 32 bit float (not IEEE; nonnormalized mantissa +
   biased exponent) : neeeeeee eeemmmmm mmmmmmmm mmmmmmmm
   Why not IEEE?  It's just not that important here. */

#define VQ_FEXP 10
#define VQ_FMAN 21
#define VQ_FEXP_BIAS 768 /* bias toward values smaller than 1. */

/* doesn't currently guard under/overflow */
long _float32_pack(float val){
  int sign=0;
  long exp;
  long mant;
  if(val<0){
    sign=0x80000000;
    val= -val;
  }
  exp= floor(log(val)/log(2.f)+.001); /* +epsilon */
  mant=rint(ldexp(val,(VQ_FMAN-1)-exp));
  exp=(exp+VQ_FEXP_BIAS)<<VQ_FMAN;

  return(sign|exp|mant);
}

float _float32_unpack(long val){
  double mant=val&0x1fffff;
  int    sign=val&0x80000000;
  long   exp =(val&0x7fe00000L)>>VQ_FMAN;
  if(sign)mant= -mant;
  exp=exp-(VQ_FMAN-1)-VQ_FEXP_BIAS;
  /* clamp excessive exponent values */
  log_message("[TRACE] Hash: 2b90001ff1e4da4cc129778b84d18822, File: vorbis/lib/sharedbook.c, Func: _float32_unpack, Line: 67, Col: 7\n");
  if (exp>63){
    exp=63;
  }
  log_message("[TRACE] Hash: 4642d29250939386de3975fa3c2b195a, File: vorbis/lib/sharedbook.c, Func: _float32_unpack, Line: 70, Col: 7\n");
  if (exp<-63){
    exp=-63;
  }
  return(ldexp(mant,exp));
}

/* given a list of word lengths, generate a list of codewords.  Works
   for length ordered or unordered, always assigns the lowest valued
   codewords first.  Extended to handle unused entries (length 0) */
ogg_uint32_t *_make_words(char *l,long n,long sparsecount){
  long i,j,count=0;
  ogg_uint32_t marker[33];
  log_message("[TRACE] Hash: 0cd377d918b1af5c09b5d2f914366cd9, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 82, Col: 32\n");
  ogg_uint32_t *r=_ogg_malloc((sparsecount?sparsecount:n)*sizeof(*r));
  memset(marker,0,sizeof(marker));

  log_message("[TRACE] Hash: 4a538121d84dd792b094d078ed57b5b5, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 85, Col: 11\n");
  for(i=0;i<n;i++){
    long length=l[i];
    if(length>0){
      ogg_uint32_t entry=marker[length];

      /* when we claim a node for an entry, we also claim the nodes
         below it (pruning off the imagined tree that may have dangled
         from it) as well as blocking the use of any nodes directly
         above for leaves */

      /* update ourself */
      log_message("[TRACE] Hash: de4c9eef4419780401ff18803196e778, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 96, Col: 23\n");
      log_message("[TRACE] Hash: ef686e3435f0ec83539ed32e9e92891f, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 96, Col: 10\n");
      if(length<32 && (entry>>length)){
        /* error condition; the lengths must specify an overpopulated tree */
        _ogg_free(r);
        return(NULL);
      }
      r[count++]=entry;

      /* Look to see if the next shorter marker points to the node
         above. if so, update it and repeat.  */
      {
        log_message("[TRACE] Hash: 8897362ab931a4e09902cd6bc07e0a63, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 106, Col: 22\n");
        for(j=length;j>0;j--){

          if(marker[j]&1){
            /* have to jump branches */
            log_message("[TRACE] Hash: de80b6ae43490990ab021da5c28050ad, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 110, Col: 16\n");
            if(j==1)
              marker[1]++;
            else
              marker[j]=marker[j-1]<<1;
            break; /* invariant says next upper marker would already
                      have been moved if it was on the same path */
          }
          marker[j]++;
        }
      }

      /* prune the tree; the implicit invariant says all the longer
         markers were dangling from our just-taken node.  Dangle them
         from our *new* node. */
      log_message("[TRACE] Hash: 6a312d3313b0d7f59c9d7fd42b83e05f, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 124, Col: 22\n");
      for(j=length+1;j<33;j++)
        log_message("[TRACE] Hash: 509b7de0312e39979f07ace8e0690bf0, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 125, Col: 12\n");
        if((marker[j]>>1) == entry){
          entry=marker[j];
          marker[j]=marker[j-1]<<1;
        }else
          break;
    }else
      log_message("[TRACE] Hash: a59705b90e0994861c6562d833be67c4, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 131, Col: 10\n");
      if(sparsecount==0)count++;
  }

  /* any underpopulated tree must be rejected. */
  /* Single-entry codebooks are a retconned extension to the spec.
     They have a single codeword '0' of length 1 that results in an
     underpopulated tree.  Shield that case from the underformed tree check. */
  log_message("[TRACE] Hash: 87dc71e374e1d9f82fc0d1fee3e749da, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 138, Col: 8\n");
  if(!(count==1 && marker[2]==2)){
    log_message("[TRACE] Hash: 5d87855b1486a213ab9a44d6e22e2c0a, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 139, Col: 13\n");
    for(i=1;i<33;i++)
      log_message("[TRACE] Hash: 1dd80dfcee2d743822c61654b64db067, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 140, Col: 10\n");
      if(marker[i] & (0xffffffffUL>>(32-i))){
        _ogg_free(r);
        return(NULL);
      }
  }

  /* bitreverse the words because our bitwise packer/unpacker is LSb
     endian */
  log_message("[TRACE] Hash: 38f0e51c4672be5489922c59d1621685, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 148, Col: 19\n");
  for(i=0,count=0;i<n;i++){
    ogg_uint32_t temp=0;
    for(j=0;j<l[i];j++){
      temp<<=1;
      temp|=(r[count]>>j)&1;
    }

    log_message("[TRACE] Hash: 61bfa3ceeec6b44f7895e8e1e60a1341, File: vorbis/lib/sharedbook.c, Func: _make_words, Line: 155, Col: 8\n");
    if(sparsecount){
      if(l[i])
        r[count++]=temp;
    }else
      r[count++]=temp;
  }

  return(r);
}

/* there might be a straightforward one-line way to do the below
   that's portable and totally safe against roundoff, but I haven't
   thought of it.  Therefore, we opt on the side of caution */
long _book_maptype1_quantvals(const static_codebook *b){
  long vals;
  log_message("[TRACE] Hash: 1d9f41ae18bdbfab2a665b057f223f6b, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 170, Col: 6\n");
  if(b->entries<1){
    return(0);
  }
  vals=floor(pow((float)b->entries,1.f/b->dim));

  /* the above *should* be reliable, but we'll not assume that FP is
     ever reliable when bitstream sync is at stake; verify via integer
     means that vals really is the greatest value of dim for which
     vals^b->bim <= b->entries */
  /* treat the above as an initial guess */
  log_message("[TRACE] Hash: 41ed539475282667e5ecbdf79c8f9290, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 180, Col: 6\n");
  if(vals<1){
    vals=1;
  }
  while(1){
    long acc=1;
    long acc1=1;
    int i;
    log_message("[TRACE] Hash: 8d323a9e2608a2bc84d2b67aff55d321, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 187, Col: 13\n");
    for(i=0;i<b->dim;i++){
      log_message("[TRACE] Hash: 2a57bffeed7d7b530986893bae2cf288, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 188, Col: 10\n");
      if(b->entries/vals<acc)break;
      acc*=vals;
      if(LONG_MAX/(vals+1)<acc1)acc1=LONG_MAX;
      else acc1*=vals+1;
    }
    log_message("[TRACE] Hash: 0ad2f18a4b3188fffaa1cb1bf657f621, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 193, Col: 8\n");
    log_message("[TRACE] Hash: 7b32829b6eb650df80e2be6cfe68b0b0, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 193, Col: 40\n");
    log_message("[TRACE] Hash: a6a018446805f10f9170130b7e8773c2, File: vorbis/lib/sharedbook.c, Func: _book_maptype1_quantvals, Line: 193, Col: 21\n");
    if(i>=b->dim && acc<=b->entries && acc1>b->entries){
      return(vals);
    }else{
      if(i<b->dim || acc>b->entries){
        vals--;
      }else{
        vals++;
      }
    }
  }
}

/* unpack the quantized list of values for encode/decode ***********/
/* we need to deal with two map types: in map type 1, the values are
   generated algorithmically (each column of the vector counts through
   the values in the quant vector). in map type 2, all the values came
   in in an explicit list.  Both value lists must be unpacked */
float *_book_unquantize(const static_codebook *b,int n,int *sparsemap){
  long j,k,count=0;
  log_message("[TRACE] Hash: a6c2ac5c7c9a5f95ab0fd8f8769355c4, File: vorbis/lib/sharedbook.c, Func: _book_unquantize, Line: 212, Col: 23\n");
  if(b->maptype==1 || b->maptype==2){
    int quantvals;
    float mindel=_float32_unpack(b->q_min);
    float delta=_float32_unpack(b->q_delta);
    float *r=_ogg_calloc(n*b->dim,sizeof(*r));

    /* maptype 1 and 2 both use a quantized value vector, but
       different sizes */
    log_message("[TRACE] Hash: 26aedfa1810da458687a1c5cf77c9715, File: vorbis/lib/sharedbook.c, Func: _book_unquantize, Line: 220, Col: 12\n");
    switch(b->maptype){
    case 1:
      /* most of the time, entries%dimensions == 0, but we need to be
         well defined.  We define that the possible vales at each
         scalar is values == entries/dim.  If entries%dim != 0, we'll
         have 'too few' values (values*dim<entries), which means that
         we'll have 'left over' entries; left over entries use zeroed
         values (and are wasted).  So don't generate codebooks like
         that */
      quantvals=_book_maptype1_quantvals(b);
      for(j=0;j<b->entries;j++){
        if((sparsemap && b->lengthlist[j]) || !sparsemap){
          float last=0.f;
          int indexdiv=1;
          for(k=0;k<b->dim;k++){
            int index= (j/indexdiv)%quantvals;
            float val=b->quantlist[index];
            val=fabs(val)*delta+mindel+last;
            if(b->q_sequencep)last=val;
            if(sparsemap)
              r[sparsemap[count]*b->dim+k]=val;
            else
              r[count*b->dim+k]=val;
            indexdiv*=quantvals;
          }
          count++;
        }

      }
      break;
    case 2:
      for(j=0;j<b->entries;j++){
        if((sparsemap && b->lengthlist[j]) || !sparsemap){
          float last=0.f;

          for(k=0;k<b->dim;k++){
            float val=b->quantlist[j*b->dim+k];
            val=fabs(val)*delta+mindel+last;
            if(b->q_sequencep)last=val;
            if(sparsemap)
              r[sparsemap[count]*b->dim+k]=val;
            else
              r[count*b->dim+k]=val;
          }
          count++;
        }
      }
      break;
    }

    return(r);
  }
  return(NULL);
}

void vorbis_staticbook_destroy(static_codebook *b){
  log_message("[TRACE] Hash: 589237b067f47185ec5791463b245fa7, File: vorbis/lib/sharedbook.c, Func: vorbis_staticbook_destroy, Line: 276, Col: 6\n");
  if(b->allocedp){
    if(b->quantlist)_ogg_free(b->quantlist);
    log_message("[TRACE] Hash: 0c0bf1bbdd670ed29ed6c235837baa28, File: vorbis/lib/sharedbook.c, Func: vorbis_staticbook_destroy, Line: 278, Col: 8\n");
    if(b->lengthlist)_ogg_free(b->lengthlist);
    memset(b,0,sizeof(*b));
    _ogg_free(b);
  } /* otherwise, it is in static memory */
}

void vorbis_book_clear(codebook *b){
  /* static book is not cleared; we're likely called on the lookup and
     the static codebook belongs to the info struct */
  if(b->valuelist)_ogg_free(b->valuelist);
  log_message("[TRACE] Hash: bcb0c15ff25e18c0cce5a6a604277b0d, File: vorbis/lib/sharedbook.c, Func: vorbis_book_clear, Line: 288, Col: 6\n");
  if(b->codelist)_ogg_free(b->codelist);

  log_message("[TRACE] Hash: 359aa47b4ae52092285ecbedd0121d7f, File: vorbis/lib/sharedbook.c, Func: vorbis_book_clear, Line: 290, Col: 6\n");
  if(b->dec_index)_ogg_free(b->dec_index);
  log_message("[TRACE] Hash: 0d358a24c5362355f80301aa2ca2a52a, File: vorbis/lib/sharedbook.c, Func: vorbis_book_clear, Line: 291, Col: 6\n");
  if(b->dec_codelengths)_ogg_free(b->dec_codelengths);
  log_message("[TRACE] Hash: 1f2ed841fb6ec3ea8578280f1b9085db, File: vorbis/lib/sharedbook.c, Func: vorbis_book_clear, Line: 292, Col: 6\n");
  if(b->dec_firsttable)_ogg_free(b->dec_firsttable);

  memset(b,0,sizeof(*b));
}

int vorbis_book_init_encode(codebook *c,const static_codebook *s){

  memset(c,0,sizeof(*c));
  c->c=s;
  c->entries=s->entries;
  c->used_entries=s->entries;
  c->dim=s->dim;
  c->codelist=_make_words(s->lengthlist,s->entries,0);
  /* c->valuelist=_book_unquantize(s,s->entries,NULL); */
  c->quantvals=_book_maptype1_quantvals(s);
  c->minval=(int)rint(_float32_unpack(s->q_min));
  c->delta=(int)rint(_float32_unpack(s->q_delta));

  return(0);
}

static ogg_uint32_t bitreverse(ogg_uint32_t x){
  x=    ((x>>16)&0x0000ffffUL) | ((x<<16)&0xffff0000UL);
  x=    ((x>> 8)&0x00ff00ffUL) | ((x<< 8)&0xff00ff00UL);
  x=    ((x>> 4)&0x0f0f0f0fUL) | ((x<< 4)&0xf0f0f0f0UL);
  x=    ((x>> 2)&0x33333333UL) | ((x<< 2)&0xccccccccUL);
  return((x>> 1)&0x55555555UL) | ((x<< 1)&0xaaaaaaaaUL);
}

static int sort32a(const void *a,const void *b){
  return ( **(ogg_uint32_t **)a>**(ogg_uint32_t **)b)-
    ( **(ogg_uint32_t **)a<**(ogg_uint32_t **)b);
}

/* decode codebook arrangement is more heavily optimized than encode */
int vorbis_book_init_decode(codebook *c,const static_codebook *s){
  int i,j,n=0,tabn;
  int *sortindex;

  memset(c,0,sizeof(*c));

  /* count actually used entries and find max length */
  log_message("[TRACE] Hash: fd8619667aed1885ddbf5853acdbc9a1, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 334, Col: 11\n");
  for(i=0;i<s->entries;i++)
    if(s->lengthlist[i]>0)
      n++;

  c->entries=s->entries;
  c->used_entries=n;
  c->dim=s->dim;

  log_message("[TRACE] Hash: 050a9d65e0c5660f2e53cf72de0f3821, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 342, Col: 6\n");
  if(n>0){
    /* two different remappings go on here.

    First, we collapse the likely sparse codebook down only to
    actually represented values/words.  This collapsing needs to be
    indexed as map-valueless books are used to encode original entry
    positions as integers.

    Second, we reorder all vectors, including the entry index above,
    by sorted bitreversed codeword to allow treeless decode. */

    /* perform sort */
    ogg_uint32_t *codes=_make_words(s->lengthlist,s->entries,c->used_entries);
    ogg_uint32_t **codep=alloca(sizeof(*codep)*n);

    log_message("[TRACE] Hash: 1124158747e30cffc5ccd86a7aa7684b, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 357, Col: 8\n");
    if(codes==NULL)goto err_out;

    log_message("[TRACE] Hash: 1710866f2bd823ae9e7a776eb6b5a74d, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 359, Col: 13\n");
    for(i=0;i<n;i++){
      codes[i]=bitreverse(codes[i]);
      codep[i]=codes+i;
    }

    qsort(codep,n,sizeof(*codep),sort32a);

    sortindex=alloca(n*sizeof(*sortindex));
    c->codelist=_ogg_malloc(n*sizeof(*c->codelist));
    /* the index is a reverse index */
    log_message("[TRACE] Hash: 717ac5e9c5635ee61a13064e69d8cd41, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 369, Col: 13\n");
    for(i=0;i<n;i++){
      int position=codep[i]-codes;
      sortindex[position]=i;
    }

    log_message("[TRACE] Hash: d504ed517d6594c76deb8384442ac43d, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 374, Col: 13\n");
    for(i=0;i<n;i++)
      c->codelist[sortindex[i]]=codes[i];
    _ogg_free(codes);

    c->valuelist=_book_unquantize(s,n,sortindex);
    c->dec_index=_ogg_malloc(n*sizeof(*c->dec_index));

    log_message("[TRACE] Hash: 967ddb94d1147672b1bce236e3b5fcd8, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 381, Col: 17\n");
    for(n=0,i=0;i<s->entries;i++)
      if(s->lengthlist[i]>0)
        c->dec_index[sortindex[n++]]=i;

    c->dec_codelengths=_ogg_malloc(n*sizeof(*c->dec_codelengths));
    c->dec_maxlength=0;
    log_message("[TRACE] Hash: 34e61a349cc24bcb4d814d79d217333e, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 387, Col: 17\n");
    for(n=0,i=0;i<s->entries;i++)
      if(s->lengthlist[i]>0){
        c->dec_codelengths[sortindex[n++]]=s->lengthlist[i];
        log_message("[TRACE] Hash: 9f4065b0f46fca827897699483ada7fe, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 390, Col: 12\n");
        if(s->lengthlist[i]>c->dec_maxlength)
          c->dec_maxlength=s->lengthlist[i];
      }

    log_message("[TRACE] Hash: 5cd65109a9d0e0119f7a08125a6d0f05, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 394, Col: 8\n");
    if(n==1 && c->dec_maxlength==1){
      /* special case the 'single entry codebook' with a single bit
       fastpath table (that always returns entry 0 )in order to use
       unmodified decode paths. */
      c->dec_firsttablen=1;
      c->dec_firsttable=_ogg_calloc(2,sizeof(*c->dec_firsttable));
      c->dec_firsttable[0]=c->dec_firsttable[1]=1;

    }else{
      c->dec_firsttablen=ov_ilog(c->used_entries)-4; /* this is magic */
      log_message("[TRACE] Hash: 5ea68d0b33e458199e96a14332185129, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 404, Col: 10\n");
      if(c->dec_firsttablen<5)c->dec_firsttablen=5;
      log_message("[TRACE] Hash: 5b87e00e2d80d037e819e0a0be34a6be, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 405, Col: 10\n");
      if(c->dec_firsttablen>8)c->dec_firsttablen=8;

      tabn=1<<c->dec_firsttablen;
      c->dec_firsttable=_ogg_calloc(tabn,sizeof(*c->dec_firsttable));

      log_message("[TRACE] Hash: a9aa291f929a2bbc2888a810e4b62d35, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 410, Col: 15\n");
      for(i=0;i<n;i++){
        log_message("[TRACE] Hash: 12e1d1ca4b123918f674401058de0ef2, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 411, Col: 12\n");
        if(c->dec_codelengths[i]<=c->dec_firsttablen){
          ogg_uint32_t orig=bitreverse(c->codelist[i]);
          for(j=0;j<(1<<(c->dec_firsttablen-c->dec_codelengths[i]));j++)
            c->dec_firsttable[orig|(j<<c->dec_codelengths[i])]=i+1;
        }
      }

      /* now fill in 'unused' entries in the firsttable with hi/lo search
         hints for the non-direct-hits */
      {
        ogg_uint32_t mask=0xfffffffeUL<<(31-c->dec_firsttablen);
        long lo=0,hi=0;

        log_message("[TRACE] Hash: 7eb7b2680e5a88fafe89263f949c677a, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 424, Col: 17\n");
        for(i=0;i<tabn;i++){
          ogg_uint32_t word=((ogg_uint32_t)i<<(32-c->dec_firsttablen));
          if(c->dec_firsttable[bitreverse(word)]==0){
            log_message("[TRACE] Hash: 903f56ead4491ba73e81df1e69c9e44e, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 427, Col: 31\n");
            log_message("[TRACE] Hash: af2c1fd2dbd9d46c8c9a208d4eb97ad2, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 427, Col: 19\n");
            while((lo+1)<n && c->codelist[lo+1]<=word)lo++;
            log_message("[TRACE] Hash: 273d92034462b112176ce0809d6dbea2, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 428, Col: 31\n");
            log_message("[TRACE] Hash: 9ff03bfff00d7cdc0f1ab3ca057e13e3, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 428, Col: 23\n");
            while(    hi<n && word>=(c->codelist[hi]&mask))hi++;

            /* we only actually have 15 bits per hint to play with here.
               In order to overflow gracefully (nothing breaks, efficiency
               just drops), encode as the difference from the extremes. */
            {
              unsigned long loval=lo;
              unsigned long hival=n-hi;

              log_message("[TRACE] Hash: 3ff73331cd59cbe4d205153f3728661d, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 437, Col: 18\n");
              if(loval>0x7fff)loval=0x7fff;
              log_message("[TRACE] Hash: 73e96ae525ffa46c4e4bd7648c1c46ab, File: vorbis/lib/sharedbook.c, Func: vorbis_book_init_decode, Line: 438, Col: 18\n");
              if(hival>0x7fff)hival=0x7fff;
              c->dec_firsttable[bitreverse(word)]=
                0x80000000UL | (loval<<15) | hival;
            }
          }
        }
      }
    }
  }

  return(0);
 err_out:
  vorbis_book_clear(c);
  return(-1);
}

long vorbis_book_codeword(codebook *book,int entry){
  if(book->c) /* only use with encode; decode optimizations are
                 allowed to break this */
    return book->codelist[entry];
  return -1;
}

long vorbis_book_codelen(codebook *book,int entry){
  if(book->c) /* only use with encode; decode optimizations are
                 allowed to break this */
    return book->c->lengthlist[entry];
  return -1;
}

#ifdef _V_SELFTEST

/* Unit tests of the dequantizer; this stuff will be OK
   cross-platform, I simply want to be sure that special mapping cases
   actually work properly; a bug could go unnoticed for a while */

#include <stdio.h>

/* cases:

   no mapping
   full, explicit mapping
   algorithmic mapping

   nonsequential
   sequential
*/

static long full_quantlist1[]={0,1,2,3,    4,5,6,7, 8,3,6,1};
static long partial_quantlist1[]={0,7,2};

/* no mapping */
static_codebook test1={
  4,16,
  NULL,
  0,
  0,0,0,0,
  NULL,
  0
};
static float *test1_result=NULL;

/* linear, full mapping, nonsequential */
static_codebook test2={
  4,3,
  NULL,
  2,
  -533200896,1611661312,4,0,
  full_quantlist1,
  0
};
static float test2_result[]={-3,-2,-1,0, 1,2,3,4, 5,0,3,-2};

/* linear, full mapping, sequential */
static_codebook test3={
  4,3,
  NULL,
  2,
  -533200896,1611661312,4,1,
  full_quantlist1,
  0
};
static float test3_result[]={-3,-5,-6,-6, 1,3,6,10, 5,5,8,6};

/* linear, algorithmic mapping, nonsequential */
static_codebook test4={
  3,27,
  NULL,
  1,
  -533200896,1611661312,4,0,
  partial_quantlist1,
  0
};
static float test4_result[]={-3,-3,-3, 4,-3,-3, -1,-3,-3,
                              -3, 4,-3, 4, 4,-3, -1, 4,-3,
                              -3,-1,-3, 4,-1,-3, -1,-1,-3,
                              -3,-3, 4, 4,-3, 4, -1,-3, 4,
                              -3, 4, 4, 4, 4, 4, -1, 4, 4,
                              -3,-1, 4, 4,-1, 4, -1,-1, 4,
                              -3,-3,-1, 4,-3,-1, -1,-3,-1,
                              -3, 4,-1, 4, 4,-1, -1, 4,-1,
                              -3,-1,-1, 4,-1,-1, -1,-1,-1};

/* linear, algorithmic mapping, sequential */
static_codebook test5={
  3,27,
  NULL,
  1,
  -533200896,1611661312,4,1,
  partial_quantlist1,
  0
};
static float test5_result[]={-3,-6,-9, 4, 1,-2, -1,-4,-7,
                              -3, 1,-2, 4, 8, 5, -1, 3, 0,
                              -3,-4,-7, 4, 3, 0, -1,-2,-5,
                              -3,-6,-2, 4, 1, 5, -1,-4, 0,
                              -3, 1, 5, 4, 8,12, -1, 3, 7,
                              -3,-4, 0, 4, 3, 7, -1,-2, 2,
                              -3,-6,-7, 4, 1, 0, -1,-4,-5,
                              -3, 1, 0, 4, 8, 7, -1, 3, 2,
                              -3,-4,-5, 4, 3, 2, -1,-2,-3};

void run_test(static_codebook *b,float *comp){
  float *out=_book_unquantize(b,b->entries,NULL);
  int i;

  if(comp){
    if(!out){
      fprintf(stderr,"_book_unquantize incorrectly returned NULL\n");
      exit(1);
    }

    for(i=0;i<b->entries*b->dim;i++)
      if(fabs(out[i]-comp[i])>.0001){
        fprintf(stderr,"disagreement in unquantized and reference data:\n"
                "position %d, %g != %g\n",i,out[i],comp[i]);
        exit(1);
      }

  }else{
    if(out){
      fprintf(stderr,"_book_unquantize returned a value array: \n"
              " correct result should have been NULL\n");
      exit(1);
    }
  }
  _ogg_free(out);
}

int main(){
  /* run the nine dequant tests, and compare to the hand-rolled results */
  fprintf(stderr,"Dequant test 1... ");
  run_test(&test1,test1_result);
  fprintf(stderr,"OK\nDequant test 2... ");
  run_test(&test2,test2_result);
  fprintf(stderr,"OK\nDequant test 3... ");
  run_test(&test3,test3_result);
  fprintf(stderr,"OK\nDequant test 4... ");
  run_test(&test4,test4_result);
  fprintf(stderr,"OK\nDequant test 5... ");
  run_test(&test5,test5_result);
  fprintf(stderr,"OK\n\n");

  return(0);
}

#endif
