/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <config.h>

#ifdef HAVE_SAMTOOLS
#include <kstring.h>
#include <sam.h>
#include <bam.h>
#endif

#include "../util/tmap_alloc.h"
#include "../util/tmap_definitions.h"
#include "../util/tmap_string.h"
#include "tmap_sam.h"
#include "../io/tmap_sam_io.h"

#ifdef HAVE_SAMTOOLS
tmap_sam_t *
tmap_sam_init()
{
  return tmap_calloc(1, sizeof(tmap_sam_t), "sam");
}

void
tmap_sam_destroy(tmap_sam_t *sam)
{
  if(sam->name) tmap_string_destroy(sam->name);
  if(sam->seq) tmap_string_destroy(sam->seq);
  if(sam->qual) tmap_string_destroy(sam->qual);
  if(sam->b) bam_destroy1(sam->b);
  free(sam);
}

inline tmap_sam_t*
tmap_sam_clone(tmap_sam_t *sam)
{
  tmap_sam_t *ret = tmap_calloc(1, sizeof(tmap_sam_t), "ret");

  ret->name = tmap_string_clone(sam->name);
  ret->seq = tmap_string_clone(sam->seq);
  ret->qual = tmap_string_clone(sam->qual);
  ret->is_int = sam->is_int;

  return ret;
}

void
tmap_sam_reverse(tmap_sam_t *sam)
{
  tmap_string_reverse(sam->seq);
  tmap_string_reverse(sam->qual);
}

void
tmap_sam_reverse_compliment(tmap_sam_t *sam)
{
  tmap_string_reverse_compliment(sam->seq, sam->is_int);
  tmap_string_reverse(sam->qual);
}

void
tmap_sam_compliment(tmap_sam_t *sam)
{
  tmap_string_compliment(sam->seq, sam->is_int);
}

void
tmap_sam_to_int(tmap_sam_t *sam)
{
  int i;
  if(1 == sam->is_int) return;
  for(i=0;i<sam->seq->l;i++) {
      sam->seq->s[i] = tmap_nt_char_to_int[(int)sam->seq->s[i]];
  }
  sam->is_int = 1;
}

void
tmap_sam_to_char(tmap_sam_t *sam)
{
  int i;
  if(0 == sam->is_int) return;
  for(i=0;i<sam->seq->l;i++) {
      sam->seq->s[i] = "ACGTN"[(int)sam->seq->s[i]];
  }
  sam->is_int = 0;
}

inline tmap_string_t *
tmap_sam_get_bases(tmap_sam_t *sam)
{
    return sam->seq;
}

inline tmap_string_t *
tmap_sam_get_qualities(tmap_sam_t *sam)
{
    return sam->qual;
}

int32_t
tmap_sam_get_flow_order_int(tmap_sam_t *sam, uint8_t **flow_order)
{
  int32_t i, flow_order_len;
  char *fo = tmap_sam_io_get_rg_fo(sam->io);
  if(NULL == fo) return 0;
  flow_order_len = strlen(fo); // TODO: cache this
  (*flow_order) = tmap_malloc(sizeof(uint8_t) * flow_order_len, "flow_order");
  for(i=0;i<flow_order_len;i++) {
      (*flow_order)[i] = tmap_nt_char_to_int[(int)fo[i]];
  }
  return flow_order_len;
}

int32_t
tmap_sam_get_key_seq_int(tmap_sam_t *sam, uint8_t **key_seq)
{
  int32_t i, key_seq_len;
  char *ks = tmap_sam_io_get_rg_ks(sam->io);
  if(NULL == ks) return 0;
  key_seq_len = strlen(ks); // TODO: cache this
  (*key_seq) = tmap_malloc(sizeof(uint8_t) * key_seq_len, "key_seq");
  for(i=0;i<key_seq_len;i++) {
      (*key_seq)[i] = tmap_nt_char_to_int[(int)ks[i]];
  }
  return key_seq_len;
}

int32_t
tmap_sam_get_flowgram(tmap_sam_t *sam, uint16_t **flowgram, int32_t mem)
{
#ifdef bam_auxB2S
  int32_t flowgram_len = 0;
  uint8_t *tag = bam_aux_get(sam->b, "FZ");
  if(NULL == tag) return 0;
  if(0 < mem) free(*flowgram);
  *flowgram = bam_auxB2S(tag, &flowgram_len);
  return flowgram_len;
#else
  return 0;
#endif
}

#endif
