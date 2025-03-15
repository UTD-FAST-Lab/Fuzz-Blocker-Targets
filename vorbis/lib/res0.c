/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2010             *
 * by the Xiph.Org Foundation https://xiph.org/                     *
 *                                                                  *
 ********************************************************************

 function: residue backend 0, 1 and 2 implementation

 ********************************************************************/

/* Slow, slow, slow, simpleminded and did I mention it was slow?  The
   encode/decode loops are coded for clarity and performance is not
   yet even a nagging little idea lurking in the shadows.  Oh and BTW,
   it's slow. */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ogg/ogg.h>
#include "vorbis/codec.h"
#include "codec_internal.h"
#include "registry.h"
#include "codebook.h"
#include "misc.h"
#include "os.h"

#if defined(TRAIN_RES) || defined (TRAIN_RESAUX)
#include <stdio.h>
#endif

typedef struct {
  vorbis_info_residue0 *info;

  int         parts;
  int         stages;
  codebook   *fullbooks;
  codebook   *phrasebook;
  codebook ***partbooks;

  int         partvals;
  int       **decodemap;

  long      postbits;
  long      phrasebits;
  long      frames;

#if defined(TRAIN_RES) || defined(TRAIN_RESAUX)
  int        train_seq;
  long      *training_data[8][64];
  float      training_max[8][64];
  float      training_min[8][64];
  float     tmin;
  float     tmax;
  int       submap;
#endif

} vorbis_look_residue0;

void res0_free_info(vorbis_info_residue *i){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)i;
  printf("[TRACE] Hash: 43402c8ed8ca756bc21ea6bf965865ae, File: vorbis/lib/res0.c, Func: res0_free_info, Line: 67, Col: 6, Branch: if(info){\n");
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

void res0_free_look(vorbis_look_residue *i){
  int j;
  printf("[TRACE] Hash: f9f82474a81140919becf213ae574e8b, File: vorbis/lib/res0.c, Func: res0_free_look, Line: 75, Col: 6, Branch: if(i){\n");
  if(i){

    vorbis_look_residue0 *look=(vorbis_look_residue0 *)i;

#ifdef TRAIN_RES
    {
      int j,k,l;
      for(j=0;j<look->parts;j++){
        /*fprintf(stderr,"partition %d: ",j);*/
        for(k=0;k<8;k++)
          if(look->training_data[k][j]){
            char buffer[80];
            FILE *of;
            codebook *statebook=look->partbooks[j][k];

            /* long and short into the same bucket by current convention */
            sprintf(buffer,"res_sub%d_part%d_pass%d.vqd",look->submap,j,k);
            of=fopen(buffer,"a");

            for(l=0;l<statebook->entries;l++)
              fprintf(of,"%d:%ld\n",l,look->training_data[k][j][l]);

            fclose(of);

            /*fprintf(stderr,"%d(%.2f|%.2f) ",k,
              look->training_min[k][j],look->training_max[k][j]);*/

            _ogg_free(look->training_data[k][j]);
            look->training_data[k][j]=NULL;
          }
        /*fprintf(stderr,"\n");*/
      }
    }
    fprintf(stderr,"min/max residue: %g::%g\n",look->tmin,look->tmax);

    /*fprintf(stderr,"residue bit usage %f:%f (%f total)\n",
            (float)look->phrasebits/look->frames,
            (float)look->postbits/look->frames,
            (float)(look->postbits+look->phrasebits)/look->frames);*/
#endif


    /*vorbis_info_residue0 *info=look->info;

    fprintf(stderr,
            "%ld frames encoded in %ld phrasebits and %ld residue bits "
            "(%g/frame) \n",look->frames,look->phrasebits,
            look->resbitsflat,
            (look->phrasebits+look->resbitsflat)/(float)look->frames);

    for(j=0;j<look->parts;j++){
      long acc=0;
      fprintf(stderr,"\t[%d] == ",j);
      for(k=0;k<look->stages;k++)
        if((info->secondstages[j]>>k)&1){
          fprintf(stderr,"%ld,",look->resbits[j][k]);
          acc+=look->resbits[j][k];
        }

      fprintf(stderr,":: (%ld vals) %1.2fbits/sample\n",look->resvals[j],
              acc?(float)acc/(look->resvals[j]*info->grouping):0);
    }
    fprintf(stderr,"\n");*/

    printf("[TRACE] Hash: 3bd7aa82be07ceda7c97752f9633c5fb, File: vorbis/lib/res0.c, Func: res0_free_look, Line: 139, Col: 13, Branch: for(j=0;j<look->parts;j++)\n");
    for(j=0;j<look->parts;j++)
      printf("[TRACE] Hash: ed60a620f491c8bb42acc02b4b947e02, File: vorbis/lib/res0.c, Func: res0_free_look, Line: 140, Col: 10, Branch: if(look->partbooks[j])_ogg_free(look->partbooks[j]);\n");
      if(look->partbooks[j])_ogg_free(look->partbooks[j]);
    _ogg_free(look->partbooks);
    printf("[TRACE] Hash: 49fdc61ca1b615e298a65591dd9843f2, File: vorbis/lib/res0.c, Func: res0_free_look, Line: 142, Col: 13, Branch: for(j=0;j<look->partvals;j++)\n");
    for(j=0;j<look->partvals;j++)
      _ogg_free(look->decodemap[j]);
    _ogg_free(look->decodemap);

    memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

static int icount(unsigned int v){
  int ret=0;
  while(v){
    ret+=v&1;
    v>>=1;
  }
  return(ret);
}


void res0_pack(vorbis_info_residue *vr,oggpack_buffer *opb){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)vr;
  int j,acc=0;
  oggpack_write(opb,info->begin,24);
  oggpack_write(opb,info->end,24);

  oggpack_write(opb,info->grouping-1,24);  /* residue vectors to group and
                                             code with a partitioned book */
  oggpack_write(opb,info->partitions-1,6); /* possible partition choices */
  oggpack_write(opb,info->groupbook,8);  /* group huffman book */

  /* secondstages is a bitmask; as encoding progresses pass by pass, a
     bitmask of one indicates this partition class has bits to write
     this pass */
  for(j=0;j<info->partitions;j++){
    if(ov_ilog(info->secondstages[j])>3){
      /* yes, this is a minor hack due to not thinking ahead */
      oggpack_write(opb,info->secondstages[j],3);
      oggpack_write(opb,1,1);
      oggpack_write(opb,info->secondstages[j]>>3,5);
    }else
      oggpack_write(opb,info->secondstages[j],4); /* trailing zero */
    acc+=icount(info->secondstages[j]);
  }
  for(j=0;j<acc;j++)
    oggpack_write(opb,info->booklist[j],8);

}

/* vorbis_info is for range checking */
vorbis_info_residue *res0_unpack(vorbis_info *vi,oggpack_buffer *opb){
  int j,acc=0;
  vorbis_info_residue0 *info=_ogg_calloc(1,sizeof(*info));
  codec_setup_info     *ci=vi->codec_setup;

  info->begin=oggpack_read(opb,24);
  info->end=oggpack_read(opb,24);
  info->grouping=oggpack_read(opb,24)+1;
  info->partitions=oggpack_read(opb,6)+1;
  info->groupbook=oggpack_read(opb,8);

  /* check for premature EOP */
  printf("[TRACE] Hash: 52abbdbcfd5c83b523c0c3813aa082a2, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 203, Col: 6, Branch: if(info->groupbook<0)goto errout;\n");
  if(info->groupbook<0)goto errout;

  printf("[TRACE] Hash: 4042115573e91e5d122071baa24eeccc, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 205, Col: 11, Branch: for(j=0;j<info->partitions;j++){\n");
  for(j=0;j<info->partitions;j++){
    int cascade=oggpack_read(opb,3);
    int cflag=oggpack_read(opb,1);
    printf("[TRACE] Hash: c44b9e40275aae75d044a70a4490ab68, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 208, Col: 8, Branch: if(cflag<0) goto errout;\n");
    if(cflag<0) goto errout;
    printf("[TRACE] Hash: c8191a4ffb88354ef1532712e14489f7, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 209, Col: 8, Branch: if(cflag){\n");
    if(cflag){
      int c=oggpack_read(opb,5);
      if(c<0) goto errout;
      cascade|=(c<<3);
    }
    info->secondstages[j]=cascade;

    acc+=icount(cascade);
  }
  printf("[TRACE] Hash: ef4c628d0c0ae2d99a673283f9b71b77, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 218, Col: 11, Branch: for(j=0;j<acc;j++){\n");
  for(j=0;j<acc;j++){
    int book=oggpack_read(opb,8);
    printf("[TRACE] Hash: 5d4deca693457416995b738ab50751bc, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 220, Col: 8, Branch: if(book<0) goto errout;\n");
    if(book<0) goto errout;
    info->booklist[j]=book;
  }

  printf("[TRACE] Hash: e63317f3a0901c082c371dfcb7bb8ea0, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 224, Col: 6, Branch: if(info->groupbook>=ci->books)goto errout;\n");
  if(info->groupbook>=ci->books)goto errout;
  printf("[TRACE] Hash: e7fb6ddd5b4c6bd5a8c1c7e707544e69, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 225, Col: 11, Branch: for(j=0;j<acc;j++){\n");
  for(j=0;j<acc;j++){
    printf("[TRACE] Hash: f16f66561ae29d97638376df356e6952, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 226, Col: 8, Branch: if(info->booklist[j]>=ci->books)goto errout;\n");
    if(info->booklist[j]>=ci->books)goto errout;
    printf("[TRACE] Hash: 1ce7934092f7a98e0d3cf1f178934da1, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 227, Col: 8, Branch: if(ci->book_param[info->booklist[j]]->maptype==0)goto errout;\n");
    if(ci->book_param[info->booklist[j]]->maptype==0)goto errout;
  }

  /* verify the phrasebook is not specifying an impossible or
     inconsistent partitioning scheme. */
  /* modify the phrasebook ranging check from r16327; an early beta
     encoder had a bug where it used an oversized phrasebook by
     accident.  These files should continue to be playable, but don't
     allow an exploit */
  {
    int entries = ci->book_param[info->groupbook]->entries;
    int dim = ci->book_param[info->groupbook]->dim;
    int partvals = 1;
    printf("[TRACE] Hash: ffd98cc064ded07d5da7bdb3084b45e7, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 240, Col: 9, Branch: if (dim<1) goto errout;\n");
    if (dim<1) goto errout;
    printf("[TRACE] Hash: 7257cd41cc1622971ceb73bf117a6134, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 241, Col: 11, Branch: while(dim>0){\n");
    while(dim>0){
      partvals *= info->partitions;
      printf("[TRACE] Hash: 1486a71a97da387175f9d7d2f9026c3f, File: vorbis/lib/res0.c, Func: res0_unpack, Line: 243, Col: 10, Branch: if(partvals > entries) goto errout;\n");
      if(partvals > entries) goto errout;
      dim--;
    }
    info->partvals = partvals;
  }

  return(info);
 errout:
  res0_free_info(info);
  return(NULL);
}

vorbis_look_residue *res0_look(vorbis_dsp_state *vd,
                               vorbis_info_residue *vr){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)vr;
  vorbis_look_residue0 *look=_ogg_calloc(1,sizeof(*look));
  codec_setup_info     *ci=vd->vi->codec_setup;

  int j,k,acc=0;
  int dim;
  int maxstage=0;
  look->info=info;

  look->parts=info->partitions;
  look->fullbooks=ci->fullbooks;
  look->phrasebook=ci->fullbooks+info->groupbook;
  dim=look->phrasebook->dim;

  look->partbooks=_ogg_calloc(look->parts,sizeof(*look->partbooks));

  printf("[TRACE] Hash: 0cd1704a84d8f3feaf37d116dd2c5e52, File: vorbis/lib/res0.c, Func: res0_look, Line: 273, Col: 11, Branch: for(j=0;j<look->parts;j++){\n");
  for(j=0;j<look->parts;j++){
    int stages=ov_ilog(info->secondstages[j]);
    printf("[TRACE] Hash: f9cf69199d60291c4158a2aec36601b0, File: vorbis/lib/res0.c, Func: res0_look, Line: 275, Col: 8, Branch: if(stages){\n");
    if(stages){
      printf("[TRACE] Hash: 372182f62008c2a04555be5b1cb898e8, File: vorbis/lib/res0.c, Func: res0_look, Line: 276, Col: 10, Branch: if(stages>maxstage)maxstage=stages;\n");
      if(stages>maxstage)maxstage=stages;
      look->partbooks[j]=_ogg_calloc(stages,sizeof(*look->partbooks[j]));
      for(k=0;k<stages;k++)
        if(info->secondstages[j]&(1<<k)){
          look->partbooks[j][k]=ci->fullbooks+info->booklist[acc++];
#ifdef TRAIN_RES
          look->training_data[k][j]=_ogg_calloc(look->partbooks[j][k]->entries,
                                           sizeof(***look->training_data));
#endif
        }
    }
  }

  look->partvals=1;
  for(j=0;j<dim;j++)
      look->partvals*=look->parts;

  look->stages=maxstage;
  look->decodemap=_ogg_malloc(look->partvals*sizeof(*look->decodemap));
  printf("[TRACE] Hash: 4f37df7c614575ada1a53281d11ad9d3, File: vorbis/lib/res0.c, Func: res0_look, Line: 295, Col: 11, Branch: for(j=0;j<look->partvals;j++){\n");
  for(j=0;j<look->partvals;j++){
    long val=j;
    long mult=look->partvals/look->parts;
    look->decodemap[j]=_ogg_malloc(dim*sizeof(*look->decodemap[j]));
    for(k=0;k<dim;k++){
      long deco=val/mult;
      val-=deco*mult;
      mult/=look->parts;
      look->decodemap[j][k]=deco;
    }
  }
#if defined(TRAIN_RES) || defined (TRAIN_RESAUX)
  {
    static int train_seq=0;
    look->train_seq=train_seq++;
  }
#endif
  return(look);
}

/* break an abstraction and copy some code for performance purposes */
static int local_book_besterror(codebook *book,int *a){
  int dim=book->dim;
  int i,j,o;
  int minval=book->minval;
  int del=book->delta;
  int qv=book->quantvals;
  int ze=(qv>>1);
  int index=0;
  /* assumes integer/centered encoder codebook maptype 1 no more than dim 8 */
  int p[8]={0,0,0,0,0,0,0,0};

  if(del!=1){
    for(i=0,o=dim;i<dim;i++){
      int v = (a[--o]-minval+(del>>1))/del;
      int m = (v<ze ? ((ze-v)<<1)-1 : ((v-ze)<<1));
      index = index*qv+ (m<0?0:(m>=qv?qv-1:m));
      p[o]=v*del+minval;
    }
  }else{
    for(i=0,o=dim;i<dim;i++){
      int v = a[--o]-minval;
      int m = (v<ze ? ((ze-v)<<1)-1 : ((v-ze)<<1));
      index = index*qv+ (m<0?0:(m>=qv?qv-1:m));
      p[o]=v*del+minval;
    }
  }

  if(book->c->lengthlist[index]<=0){
    const static_codebook *c=book->c;
    int best=-1;
    /* assumes integer/centered encoder codebook maptype 1 no more than dim 8 */
    int e[8]={0,0,0,0,0,0,0,0};
    int maxval = book->minval + book->delta*(book->quantvals-1);
    for(i=0;i<book->entries;i++){
      if(c->lengthlist[i]>0){
        int this=0;
        for(j=0;j<dim;j++){
          int val=(e[j]-a[j]);
          this+=val*val;
        }
        if(best==-1 || this<best){
          memcpy(p,e,sizeof(p));
          best=this;
          index=i;
        }
      }
      /* assumes the value patterning created by the tools in vq/ */
      j=0;
      while(e[j]>=maxval)
        e[j++]=0;
      if(e[j]>=0)
        e[j]+=book->delta;
      e[j]= -e[j];
    }
  }

  if(index>-1){
    for(i=0;i<dim;i++)
      *a++ -= p[i];
  }

  return(index);
}

#ifdef TRAIN_RES
static int _encodepart(oggpack_buffer *opb,int *vec, int n,
                       codebook *book,long *acc){
#else
static int _encodepart(oggpack_buffer *opb,int *vec, int n,
                       codebook *book){
#endif
  int i,bits=0;
  int dim=book->dim;
  int step=n/dim;

  for(i=0;i<step;i++){
    int entry=local_book_besterror(book,vec+i*dim);

#ifdef TRAIN_RES
    if(entry>=0)
      acc[entry]++;
#endif

    bits+=vorbis_book_encode(book,entry,opb);

  }

  return(bits);
}

static long **_01class(vorbis_block *vb,vorbis_look_residue *vl,
                       int **in,int ch){
  long i,j,k;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int possible_partitions=info->partitions;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  long **partword=_vorbis_block_alloc(vb,ch*sizeof(*partword));
  float scale=100./samples_per_partition;

  /* we find the partition type for each partition of each
     channel.  We'll go back and do the interleaved encoding in a
     bit.  For now, clarity */

  for(i=0;i<ch;i++){
    partword[i]=_vorbis_block_alloc(vb,n/samples_per_partition*sizeof(*partword[i]));
    memset(partword[i],0,n/samples_per_partition*sizeof(*partword[i]));
  }

  for(i=0;i<partvals;i++){
    int offset=i*samples_per_partition+info->begin;
    for(j=0;j<ch;j++){
      int max=0;
      int ent=0;
      for(k=0;k<samples_per_partition;k++){
        if(abs(in[j][offset+k])>max)max=abs(in[j][offset+k]);
        ent+=abs(in[j][offset+k]);
      }
      ent*=scale;

      for(k=0;k<possible_partitions-1;k++)
        if(max<=info->classmetric1[k] &&
           (info->classmetric2[k]<0 || ent<info->classmetric2[k]))
          break;

      partword[j][i]=k;
    }
  }

#ifdef TRAIN_RESAUX
  {
    FILE *of;
    char buffer[80];

    for(i=0;i<ch;i++){
      sprintf(buffer,"resaux_%d.vqd",look->train_seq);
      of=fopen(buffer,"a");
      for(j=0;j<partvals;j++)
        fprintf(of,"%ld, ",partword[i][j]);
      fprintf(of,"\n");
      fclose(of);
    }
  }
#endif
  look->frames++;

  return(partword);
}

/* designed for stereo or other modes where the partition size is an
   integer multiple of the number of channels encoded in the current
   submap */
static long **_2class(vorbis_block *vb,vorbis_look_residue *vl,int **in,
                      int ch){
  long i,j,k,l;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int possible_partitions=info->partitions;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  long **partword=_vorbis_block_alloc(vb,sizeof(*partword));

#if defined(TRAIN_RES) || defined (TRAIN_RESAUX)
  FILE *of;
  char buffer[80];
#endif

  partword[0]=_vorbis_block_alloc(vb,partvals*sizeof(*partword[0]));
  memset(partword[0],0,partvals*sizeof(*partword[0]));

  for(i=0,l=info->begin/ch;i<partvals;i++){
    int magmax=0;
    int angmax=0;
    for(j=0;j<samples_per_partition;j+=ch){
      if(abs(in[0][l])>magmax)magmax=abs(in[0][l]);
      for(k=1;k<ch;k++)
        if(abs(in[k][l])>angmax)angmax=abs(in[k][l]);
      l++;
    }

    for(j=0;j<possible_partitions-1;j++)
      if(magmax<=info->classmetric1[j] &&
         angmax<=info->classmetric2[j])
        break;

    partword[0][i]=j;

  }

#ifdef TRAIN_RESAUX
  sprintf(buffer,"resaux_%d.vqd",look->train_seq);
  of=fopen(buffer,"a");
  for(i=0;i<partvals;i++)
    fprintf(of,"%ld, ",partword[0][i]);
  fprintf(of,"\n");
  fclose(of);
#endif

  look->frames++;

  return(partword);
}

static int _01forward(oggpack_buffer *opb,
                      vorbis_look_residue *vl,
                      int **in,int ch,
                      long **partword,
#ifdef TRAIN_RES
                      int (*encode)(oggpack_buffer *,int *,int,
                                    codebook *,long *),
                      int submap
#else
                      int (*encode)(oggpack_buffer *,int *,int,
                                    codebook *)
#endif
){
  long i,j,k,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

#ifdef TRAIN_RES
  look->submap=submap;
#endif

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int possible_partitions=info->partitions;
  int partitions_per_word=look->phrasebook->dim;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  long resbits[128];
  long resvals[128];

#ifdef TRAIN_RES
  for(i=0;i<ch;i++)
    for(j=info->begin;j<info->end;j++){
      if(in[i][j]>look->tmax)look->tmax=in[i][j];
      if(in[i][j]<look->tmin)look->tmin=in[i][j];
    }
#endif

  memset(resbits,0,sizeof(resbits));
  memset(resvals,0,sizeof(resvals));

  /* we code the partition words for each channel, then the residual
     words for a partition per channel until we've written all the
     residual words for that partition word.  Then write the next
     partition channel words... */

  for(s=0;s<look->stages;s++){

    for(i=0;i<partvals;){

      /* first we encode a partition codeword for each channel */
      if(s==0){
        for(j=0;j<ch;j++){
          long val=partword[j][i];
          for(k=1;k<partitions_per_word;k++){
            val*=possible_partitions;
            if(i+k<partvals)
              val+=partword[j][i+k];
          }

          /* training hack */
          if(val<look->phrasebook->entries)
            look->phrasebits+=vorbis_book_encode(look->phrasebook,val,opb);
#if 0 /*def TRAIN_RES*/
          else
            fprintf(stderr,"!");
#endif

        }
      }

      /* now we encode interleaved residual values for the partitions */
      for(k=0;k<partitions_per_word && i<partvals;k++,i++){
        long offset=i*samples_per_partition+info->begin;

        for(j=0;j<ch;j++){
          if(s==0)resvals[partword[j][i]]+=samples_per_partition;
          if(info->secondstages[partword[j][i]]&(1<<s)){
            codebook *statebook=look->partbooks[partword[j][i]][s];
            if(statebook){
              int ret;
#ifdef TRAIN_RES
              long *accumulator=NULL;
              accumulator=look->training_data[s][partword[j][i]];
              {
                int l;
                int *samples=in[j]+offset;
                for(l=0;l<samples_per_partition;l++){
                  if(samples[l]<look->training_min[s][partword[j][i]])
                    look->training_min[s][partword[j][i]]=samples[l];
                  if(samples[l]>look->training_max[s][partword[j][i]])
                    look->training_max[s][partword[j][i]]=samples[l];
                }
              }
              ret=encode(opb,in[j]+offset,samples_per_partition,
                         statebook,accumulator);
#else
              ret=encode(opb,in[j]+offset,samples_per_partition,
                         statebook);
#endif

              look->postbits+=ret;
              resbits[partword[j][i]]+=ret;
            }
          }
        }
      }
    }
  }

  return(0);
}

/* a truncated packet here just means 'stop working'; it's not an error */
static int _01inverse(vorbis_block *vb,vorbis_look_residue *vl,
                      float **in,int ch,
                      long (*decodepart)(codebook *, float *,
                                         oggpack_buffer *,int)){

  long i,j,k,l,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int partitions_per_word=look->phrasebook->dim;
  int max=vb->pcmend>>1;
  int end=(info->end<max?info->end:max);
  int n=end-info->begin;

  if(n>0){
    int partvals=n/samples_per_partition;
    int partwords=(partvals+partitions_per_word-1)/partitions_per_word;
    int ***partword=alloca(ch*sizeof(*partword));

    for(j=0;j<ch;j++)
      partword[j]=_vorbis_block_alloc(vb,partwords*sizeof(*partword[j]));

    for(s=0;s<look->stages;s++){

      /* each loop decodes on partition codeword containing
         partitions_per_word partitions */
      for(i=0,l=0;i<partvals;l++){
        if(s==0){
          /* fetch the partition word for each channel */
          for(j=0;j<ch;j++){
            int temp=vorbis_book_decode(look->phrasebook,&vb->opb);

            if(temp==-1 || temp>=info->partvals)goto eopbreak;
            partword[j][l]=look->decodemap[temp];
            if(partword[j][l]==NULL)goto errout;
          }
        }

        /* now we decode residual values for the partitions */
        for(k=0;k<partitions_per_word && i<partvals;k++,i++)
          for(j=0;j<ch;j++){
            long offset=info->begin+i*samples_per_partition;
            if(info->secondstages[partword[j][l][k]]&(1<<s)){
              codebook *stagebook=look->partbooks[partword[j][l][k]][s];
              if(stagebook){
                if(decodepart(stagebook,in[j]+offset,&vb->opb,
                              samples_per_partition)==-1)goto eopbreak;
              }
            }
          }
      }
    }
  }
 errout:
 eopbreak:
  return(0);
}

int res0_inverse(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    printf("[TRACE] Hash: ca9d790f923a245afcb61975b45aa224, File: vorbis/lib/res0.c, Func: res0_inverse, Line: 707, Col: 8, Branch: if(nonzero[i])\n");
    if(nonzero[i])
      in[used++]=in[i];
  printf("[TRACE] Hash: 1f3b9727f10e001191efcca464b95ccf, File: vorbis/lib/res0.c, Func: res0_inverse, Line: 709, Col: 6, Branch: if(used)\n");
  if(used)
    return(_01inverse(vb,vl,in,used,vorbis_book_decodevs_add));
  else
    return(0);
}

int res1_forward(oggpack_buffer *opb,vorbis_block *vb,vorbis_look_residue *vl,
                 int **in,int *nonzero,int ch, long **partword, int submap){
  int i,used=0;
  (void)vb;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];

  if(used){
#ifdef TRAIN_RES
    return _01forward(opb,vl,in,used,partword,_encodepart,submap);
#else
    (void)submap;
    return _01forward(opb,vl,in,used,partword,_encodepart);
#endif
  }else{
    return(0);
  }
}

long **res1_class(vorbis_block *vb,vorbis_look_residue *vl,
                  int **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_01class(vb,vl,in,used));
  else
    return(0);
}

int res1_inverse(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,int *nonzero,int ch){
  int i,used=0;
  printf("[TRACE] Hash: 832320c501ec6bfd24dc7e21c60d2730, File: vorbis/lib/res0.c, Func: res1_inverse, Line: 750, Col: 11, Branch: for(i=0;i<ch;i++)\n");
  for(i=0;i<ch;i++)
    printf("[TRACE] Hash: ca2fe141f1a97055210fcc1f80d41c1e, File: vorbis/lib/res0.c, Func: res1_inverse, Line: 751, Col: 8, Branch: if(nonzero[i])\n");
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_01inverse(vb,vl,in,used,vorbis_book_decodev_add));
  else
    return(0);
}

long **res2_class(vorbis_block *vb,vorbis_look_residue *vl,
                  int **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])used++;
  if(used)
    return(_2class(vb,vl,in,ch));
  else
    return(0);
}

/* res2 is slightly more different; all the channels are interleaved
   into a single vector and encoded. */

int res2_forward(oggpack_buffer *opb,
                 vorbis_block *vb,vorbis_look_residue *vl,
                 int **in,int *nonzero,int ch, long **partword,int submap){
  long i,j,k,n=vb->pcmend/2,used=0;

  /* don't duplicate the code; use a working vector hack for now and
     reshape ourselves into a single channel res1 */
  /* ugly; reallocs for each coupling pass :-( */
  int *work=_vorbis_block_alloc(vb,ch*n*sizeof(*work));
  for(i=0;i<ch;i++){
    int *pcm=in[i];
    if(nonzero[i])used++;
    for(j=0,k=i;j<n;j++,k+=ch)
      work[k]=pcm[j];
  }

  if(used){
#ifdef TRAIN_RES
    return _01forward(opb,vl,&work,1,partword,_encodepart,submap);
#else
    (void)submap;
    return _01forward(opb,vl,&work,1,partword,_encodepart);
#endif
  }else{
    return(0);
  }
}

/* duplicate code here as speed is somewhat more important */
int res2_inverse(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,int *nonzero,int ch){
  long i,k,l,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int partitions_per_word=look->phrasebook->dim;
  int max=(vb->pcmend*ch)>>1;
  printf("[TRACE] Hash: 125d7870f25e535eabea382addd0002b, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 812, Col: 12, Branch: int end=(info->end<max?info->end:max);\n");
  int end=(info->end<max?info->end:max);
  int n=end-info->begin;

  printf("[TRACE] Hash: c43e7eee640e5202a93706c567f5d788, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 815, Col: 6, Branch: if(n>0){\n");
  if(n>0){
    int partvals=n/samples_per_partition;
    int partwords=(partvals+partitions_per_word-1)/partitions_per_word;
    int **partword=_vorbis_block_alloc(vb,partwords*sizeof(*partword));

    for(i=0;i<ch;i++)if(nonzero[i])break;
    if(i==ch)return(0); /* no nonzero vectors */

    printf("[TRACE] Hash: 98f45eecf39799e98ffac09b5ef7d50c, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 823, Col: 13, Branch: for(s=0;s<look->stages;s++){\n");
    for(s=0;s<look->stages;s++){
      printf("[TRACE] Hash: 0eff2615af72df5492df04409d9d30ea, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 824, Col: 19, Branch: "for(i=0\n");
      for(i=0,l=0;i<partvals;l++){

        if(s==0){
          /* fetch the partition word */
          int temp=vorbis_book_decode(look->phrasebook,&vb->opb);
          printf("[TRACE] Hash: 39c944ed25d582d6ae08086bb0250ea1, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 829, Col: 26, Branch: if(temp==-1 || temp>=info->partvals)goto eopbreak;\n");
          printf("[TRACE] Hash: 5c114258f3bfc0487d172bb9574d073a, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 829, Col: 14, Branch: if(temp==-1 || temp>=info->partvals)goto eopbreak;\n");
          if(temp==-1 || temp>=info->partvals)goto eopbreak;
          partword[l]=look->decodemap[temp];
          printf("[TRACE] Hash: f91f6b8f5e7c7f43c676aed3fec82c5c, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 831, Col: 14, Branch: if(partword[l]==NULL)goto errout;\n");
          if(partword[l]==NULL)goto errout;
        }

        /* now we decode residual values for the partitions */
        printf("[TRACE] Hash: c844f44def94ae808597984e44cc8466, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 835, Col: 42, Branch: "for(k=0;k<partitions_per_word && i<partvals;k++\n");
        for(k=0;k<partitions_per_word && i<partvals;k++,i++)
          printf("[TRACE] Hash: 2725300afcb7909f3f212dee6545f76f, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 836, Col: 14, Branch: if(info->secondstages[partword[l][k]]&(1<<s)){\n");
          if(info->secondstages[partword[l][k]]&(1<<s)){
            codebook *stagebook=look->partbooks[partword[l][k]][s];

            printf("[TRACE] Hash: 1b69d4ca01a17960153dfbcff177c6b9, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 839, Col: 16, Branch: if(stagebook){\n");
            if(stagebook){
              printf("[TRACE] Hash: e5e3913c079a48ac37a36b96dd31d20c, File: vorbis/lib/res0.c, Func: res2_inverse, Line: 840, Col: 18, Branch: "if(vorbis_book_decodevv_add(stagebook\n");
              if(vorbis_book_decodevv_add(stagebook,in,
                                          i*samples_per_partition+info->begin,ch,
                                          &vb->opb,samples_per_partition)==-1)
                goto eopbreak;
            }
          }
      }
    }
  }
 errout:
 eopbreak:
  return(0);
}


const vorbis_func_residue residue0_exportbundle={
  NULL,
  &res0_unpack,
  &res0_look,
  &res0_free_info,
  &res0_free_look,
  NULL,
  NULL,
  &res0_inverse
};

const vorbis_func_residue residue1_exportbundle={
  &res0_pack,
  &res0_unpack,
  &res0_look,
  &res0_free_info,
  &res0_free_look,
  &res1_class,
  &res1_forward,
  &res1_inverse
};

const vorbis_func_residue residue2_exportbundle={
  &res0_pack,
  &res0_unpack,
  &res0_look,
  &res0_free_info,
  &res0_free_look,
  &res2_class,
  &res2_forward,
  &res2_inverse
};
