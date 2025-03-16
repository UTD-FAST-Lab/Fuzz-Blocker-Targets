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

 function: single-block PCM synthesis

 ********************************************************************/

#include <stdio.h>
#include <ogg/ogg.h>
#include "vorbis/codec.h"
#include "codec_internal.h"
#include "registry.h"
#include "misc.h"
#include "os.h"

int vorbis_synthesis(vorbis_block *vb,ogg_packet *op){
  puts("[TRACE] Hash: 766cb977492e107067d4330ea4d75a7d, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 26, Col: 29\n");
  vorbis_dsp_state     *vd= vb ? vb->vd : 0;
  puts("[TRACE] Hash: af18226a23942780c99b1ee373271c0e, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 27, Col: 28\n");
  private_state        *b= vd ? vd->backend_state : 0;
  puts("[TRACE] Hash: 43d6bd01c043cd4250019a87ae4526da, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 28, Col: 29\n");
  vorbis_info          *vi= vd ? vd->vi : 0;
  puts("[TRACE] Hash: 134e1883fea51dc1028782342bf284f9, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 29, Col: 29\n");
  codec_setup_info     *ci= vi ? vi->codec_setup : 0;
  puts("[TRACE] Hash: 0c0d9f797b5ed1f566bd36e5fd1186f1, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 30, Col: 29\n");
  oggpack_buffer       *opb=vb ? &vb->opb : 0;
  int                   type,mode,i;

  puts("[TRACE] Hash: 308961a1d94a981309fedddf748cefbb, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 33, Col: 14\n");
  puts("[TRACE] Hash: 7f9ba270a9993babeb69ba023bef5937, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 33, Col: 20\n");
  puts("[TRACE] Hash: 88090241eed79b650e41f4332395b5b1, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 33, Col: 7\n");
  puts("[TRACE] Hash: afff9918dfa9c9bb1b2fcaeb49275b60, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 33, Col: 34\n");
  puts("[TRACE] Hash: cb7fcb9759fc6218aa8e8aaa07e0e5c4, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 33, Col: 27\n");
  if (!vd || !b || !vi || !ci || !opb) {
    return OV_EBADPACKET;
  }

  /* first things first.  Make sure decode is ready */
  _vorbis_block_ripcord(vb);
  oggpack_readinit(opb,op->packet,op->bytes);

  /* Check the packet type */
  if(oggpack_read(opb,1)!=0){
    /* Oops.  This is not an audio data packet */
    return(OV_ENOTAUDIO);
  }

  /* read our mode and pre/post windowsize */
  mode=oggpack_read(opb,b->modebits);
  puts("[TRACE] Hash: 3cad27026f1b2dde6ad8ae428ff780fc, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 49, Col: 6\n");
  if(mode==-1){
    return(OV_EBADPACKET);
  }

  vb->mode=mode;
  puts("[TRACE] Hash: 09c7339e114e5af790cc47d8d80719b6, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 54, Col: 6\n");
  if(!ci->mode_param[mode]){
    return(OV_EBADPACKET);
  }

  vb->W=ci->mode_param[mode]->blockflag;
  puts("[TRACE] Hash: e549bd773295459cfd277bbae02c644b, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 59, Col: 6\n");
  if(vb->W){

    /* this doesn;t get mapped through mode selection as it's used
       only for window selection */
    vb->lW=oggpack_read(opb,1);
    vb->nW=oggpack_read(opb,1);
    puts("[TRACE] Hash: 98ce2b9f8a451da078d4276c29b16605, File: vorbis/lib/synthesis.c, Func: vorbis_synthesis, Line: 65, Col: 8\n");
    if(vb->nW==-1){
      return(OV_EBADPACKET);
    }
  }else{
    vb->lW=0;
    vb->nW=0;
  }

  /* more setup */
  vb->granulepos=op->granulepos;
  vb->sequence=op->packetno;
  vb->eofflag=op->e_o_s;

  /* alloc pcm passback storage */
  vb->pcmend=ci->blocksizes[vb->W];
  vb->pcm=_vorbis_block_alloc(vb,sizeof(*vb->pcm)*vi->channels);
  for(i=0;i<vi->channels;i++)
    vb->pcm[i]=_vorbis_block_alloc(vb,vb->pcmend*sizeof(*vb->pcm[i]));

  /* unpack_header enforces range checking */
  type=ci->map_type[ci->mode_param[mode]->mapping];

  return(_mapping_P[type]->inverse(vb,ci->map_param[ci->mode_param[mode]->
                                                   mapping]));
}

/* used to track pcm position without actually performing decode.
   Useful for sequential 'fast forward' */
int vorbis_synthesis_trackonly(vorbis_block *vb,ogg_packet *op){
  vorbis_dsp_state     *vd=vb->vd;
  private_state        *b=vd->backend_state;
  vorbis_info          *vi=vd->vi;
  codec_setup_info     *ci=vi->codec_setup;
  oggpack_buffer       *opb=&vb->opb;
  int                   mode;

  /* first things first.  Make sure decode is ready */
  _vorbis_block_ripcord(vb);
  oggpack_readinit(opb,op->packet,op->bytes);

  /* Check the packet type */
  if(oggpack_read(opb,1)!=0){
    /* Oops.  This is not an audio data packet */
    return(OV_ENOTAUDIO);
  }

  /* read our mode and pre/post windowsize */
  mode=oggpack_read(opb,b->modebits);
  if(mode==-1)return(OV_EBADPACKET);

  vb->mode=mode;
  if(!ci->mode_param[mode]){
    return(OV_EBADPACKET);
  }

  vb->W=ci->mode_param[mode]->blockflag;
  if(vb->W){
    vb->lW=oggpack_read(opb,1);
    vb->nW=oggpack_read(opb,1);
    if(vb->nW==-1)   return(OV_EBADPACKET);
  }else{
    vb->lW=0;
    vb->nW=0;
  }

  /* more setup */
  vb->granulepos=op->granulepos;
  vb->sequence=op->packetno;
  vb->eofflag=op->e_o_s;

  /* no pcm */
  vb->pcmend=0;
  vb->pcm=NULL;

  return(0);
}

long vorbis_packet_blocksize(vorbis_info *vi,ogg_packet *op){
  codec_setup_info     *ci=vi->codec_setup;
  oggpack_buffer       opb;
  int                  mode;

  if(ci==NULL || ci->modes<=0){
    /* codec setup not properly intialized */
    return(OV_EFAULT);
  }

  oggpack_readinit(&opb,op->packet,op->bytes);

  /* Check the packet type */
  if(oggpack_read(&opb,1)!=0){
    /* Oops.  This is not an audio data packet */
    return(OV_ENOTAUDIO);
  }

  /* read our mode and pre/post windowsize */
  mode=oggpack_read(&opb,ov_ilog(ci->modes-1));
  if(mode==-1 || !ci->mode_param[mode])return(OV_EBADPACKET);
  return(ci->blocksizes[ci->mode_param[mode]->blockflag]);
}

int vorbis_synthesis_halfrate(vorbis_info *vi,int flag){
  /* set / clear half-sample-rate mode */
  codec_setup_info     *ci=vi->codec_setup;

  /* right now, our MDCT can't handle < 64 sample windows. */
  if(ci->blocksizes[0]<=64 && flag)return -1;
  ci->halfrate_flag=(flag?1:0);
  return 0;
}

int vorbis_synthesis_halfrate_p(vorbis_info *vi){
  codec_setup_info     *ci=vi->codec_setup;
  return ci->halfrate_flag;
}
