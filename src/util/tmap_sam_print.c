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
#include "../io/tmap_file.h"
#include "../io/tmap_seq_io.h"
#include "../sw/tmap_sw.h"
#include "tmap_sam_print.h"

static char tmap_sam_rg_id[1024]="ID";

#define TMAP_SAM_PRINT_RG_HEADER_TAGS 12

// Notes: we could add all the tags as input
static void
tmap_sam_parse_rg(char *rg, const char *fo, const char *ks, const char *pg)
{
  int32_t i, j, len;
  // ID, CN, DS, DT, LB, PG, PI, PL, PU, SM
  int32_t tags_found[TMAP_SAM_PRINT_RG_HEADER_TAGS] = {0,0,0,0,0,0,0,0,0,0,0,0};
  char *tags_name[TMAP_SAM_PRINT_RG_HEADER_TAGS] = {"ID","CN","DS","DT","FO","KS","LB","PG","PI","PL","PU","SM"};
  char *tags_value[TMAP_SAM_PRINT_RG_HEADER_TAGS] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

  len = strlen(rg);

  // convert strings of "\t" to tab characters '\t'
  for(i=0;i<len-1;i++) {
      if(rg[i] == '\\' && rg[i+1] == 't') {
          rg[i] = '\t';
          // shift down
          for(j=i+1;j<len-1;j++) {
              rg[j] = rg[j+1];
          }
          len--;
          rg[len]='\0';
      }
  }

  // must have at least "@RG\t"
  if(len < 4
     || 0 != strncmp(rg, "@RG\t", 4)) {
      tmap_error("Malformed RG line", Exit, OutOfRange);
  }
  i = 3;
  
  while(i<len) {
      if('\t' == rg[i]) {
          int32_t tag_i = -1;
          i++; // move past the tab
          if(len <= i+2) { // must have "XX:" 
              tmap_error("Improper tag in the RG line", Exit, OutOfRange);
          }
          else if('I' == rg[i] && 'D' == rg[i+1]) {
              tag_i = 0;
              // copy over the id
              for(j=i+3;j<len;j++) {
                  if('\t' == rg[j]) break;
                  tmap_sam_rg_id[j-i-3] = rg[j];
              }
              if(j == i) tmap_error("Malformed RG line", Exit, OutOfRange);
              tmap_sam_rg_id[j-i]='\0'; // null terminator
          }
          else if('C' == rg[i] && 'N' == rg[i+1]) tag_i=1;
          else if('D' == rg[i] && 'S' == rg[i+1]) tag_i=2;
          else if('D' == rg[i] && 'T' == rg[i+1]) tag_i=3;
          else if('F' == rg[i] && 'O' == rg[i+1]) {
              tag_i=4;
              if(NULL != fo) {
                  tmap_error("FO tag not allowed in the RG line", Exit, OutOfRange);
              }
          }
          else if('K' != rg[i] && 'S' == rg[i+1]) {
              tag_i=5;
              if(NULL == ks) {
                  tmap_error("KS tag not allowed in the RG line", Exit, OutOfRange);
              }
          }
          else if('L' == rg[i] && 'B' == rg[i+1]) tag_i=6;
          else if('P' == rg[i] && 'G' == rg[i+1]) {
              tag_i=7;
              if(NULL != pg) {
                  tmap_error("PG tag not allowed in the RG line", Exit, OutOfRange);
              }
          }
          else if('P' == rg[i] && 'I' == rg[i+1]) tag_i=8;
          else if('P' == rg[i] && 'L' == rg[i+1]) tag_i=9;
          else if('P' == rg[i] && 'U' == rg[i+1]) tag_i=10;
          else if('S' == rg[i] && 'M' == rg[i+1]) tag_i=11;
          else {
              tmap_error("Improper tag in the RG line", Exit, OutOfRange);
          }
          tags_found[tag_i]++;
          if(1 == tags_found[tag_i]) {
              tags_value[tag_i] = tmap_malloc(sizeof(char) * (len + 1), "tags_value[tag_i]");
              for(j=i;j<len && '\t' != rg[j];j++) {
                  tags_value[tag_i][j-i] = rg[j];
              }
              if(j - i <= 3) {
                  tmap_file_fprintf(tmap_file_stderr, "\nFound an empty tag in the RG SAM header: %s\n", tags_name[tag_i]);
                  tmap_error(NULL, Exit, OutOfRange);
              }
              tags_value[tag_i][j-i] = '\0';
          }
      }
      i++;
  }

  strcpy(rg, "@RG");
  for(i=0;i<TMAP_SAM_PRINT_RG_HEADER_TAGS;i++) {
      if(1 < tags_found[i]) {
          tmap_file_fprintf(tmap_file_stderr, "\nFound multiple %s tags for the RG SAM header\n", tags_name[i]);
          tmap_error(NULL, Exit, OutOfRange);
      }
      else if(1 == tags_found[i]) {
          strcat(rg, "\t");
          strcat(rg, tags_value[i]);
          // copy over the tag
          free(tags_value[i]);
      }
  }
}

void
tmap_sam_print_header(tmap_file_t *fp, tmap_refseq_t *refseq, tmap_seq_io_t *seqio, char *sam_rg, char *flow_order, int32_t sam_sff_tags, int argc, char *argv[])
{
  int32_t i;
  // SAM header
  tmap_file_fprintf(fp, "@HD\tVN:%s\tSO:unsorted\n",
                    TMAP_SAM_PRINT_VERSION);
  for(i=0;i<refseq->num_annos;i++) {
      tmap_file_fprintf(fp, "@SQ\tSN:%s\tLN:%d\n",
                        refseq->annos[i].name->s, (int)refseq->annos[i].len);
  }
  // RG
  if(NULL != seqio && TMAP_SEQ_TYPE_SFF == seqio->type && 1 == sam_sff_tags) {
      if(NULL != flow_order) { // this should not happen, since it should be checked upstream
          tmap_error("flow order was specified when using an SFF", Exit, OutOfRange);
      }
      if(NULL != sam_rg) { // SAM RG is user-specified
          tmap_sam_parse_rg(sam_rg, 
                            seqio->io.sffio->gheader->flow->s,
                            seqio->io.sffio->gheader->key->s,
                            PACKAGE_NAME);
          tmap_file_fprintf(fp, "%s\n", sam_rg);
      }
      else {
          tmap_file_fprintf(fp, "@RG\tID:%s\tFO:%s\tKS:%s\tPG:%s\n",
                            tmap_sam_rg_id,
                            seqio->io.sffio->gheader->flow->s,
                            seqio->io.sffio->gheader->key->s,
                            PACKAGE_NAME);
      }
  }
  else {
      if(NULL != sam_rg) {
          tmap_sam_parse_rg(sam_rg, NULL, flow_order, PACKAGE_NAME);
          tmap_file_fprintf(fp, "%s\n", sam_rg);
      }
      else if(NULL != flow_order) {
          tmap_file_fprintf(fp, "@RG\tID:%s\tFO:%s\tPG:%s\n",
                            tmap_sam_rg_id,
                            flow_order,
                            PACKAGE_NAME);
      }
      else {
          tmap_file_fprintf(fp, "@RG\tID:%s\tPG:%s\n",
                            tmap_sam_rg_id,
                            PACKAGE_NAME);
      }
  }
  tmap_file_fprintf(fp, "@PG\tID:%s\tVN:%s\tCL:",
                    PACKAGE_NAME, PACKAGE_VERSION);
  for(i=0;i<argc;i++) {
      if(0 < i) tmap_file_fprintf(fp, " ");
      tmap_file_fprintf(fp, "%s", argv[i]);
  }
  tmap_file_fprintf(fp, "\n");
}

static inline void
tmap_sam_print_flowgram(tmap_file_t *fp, uint16_t *flowgram, int32_t length)
{
  int32_t i;
  tmap_file_fprintf(fp, "\tFZ:B:S");
  for(i=0;i<length;i++) {
      tmap_file_fprintf(fp, ",%u", flowgram[i]);
  }
}

inline void
tmap_sam_print_unmapped(tmap_file_t *fp, tmap_seq_t *seq, int32_t sam_sff_tags)
{
  uint16_t flag = 0x0004;
  tmap_string_t *name=NULL, *bases=NULL, *qualities=NULL;

  name = tmap_seq_get_name(seq);
  bases = tmap_seq_get_bases(seq);
  qualities = tmap_seq_get_qualities(seq);

  tmap_file_fprintf(fp, "%s\t%u\t%s\t%u\t%u\t*\t*\t0\t0\t%s\t%s",
                    name->s, flag, "*",
                    0, 0, 
                    bases->s, (0 == qualities->l) ? "*" : qualities->s);
  tmap_file_fprintf(fp, "\tRG:Z:%s\tPG:Z:%s",
                    tmap_sam_rg_id,
                    PACKAGE_NAME);
  if(TMAP_SEQ_TYPE_SFF == seq->type && 1 == sam_sff_tags) {
      tmap_sam_print_flowgram(fp, seq->data.sff->read->flowgram, seq->data.sff->gheader->flow_length);
  }
  tmap_file_fprintf(fp, "\n");
}

static inline tmap_string_t *
tmap_sam_md(tmap_refseq_t *refseq, char *read_bases, // read bases are characters
            uint32_t seqid, uint32_t pos, // seqid and pos are 0-based
            uint32_t *cigar, int32_t n_cigar, int32_t *nm)
{
  int32_t i, j;
  uint32_t ref_i, read_i, ref_start, ref_end;
  int32_t l = 0; // the length of the last md op
  uint8_t read_base, ref_base;
  tmap_string_t *md=NULL;
  uint8_t *target = NULL;;

  md = tmap_string_init(32);
  (*nm) = 0;

  ref_start = ref_end = pos + 1; // make one-based
  for(i=0;i<n_cigar;i++) { // go through each cigar operator
      int32_t op_len;
      op_len = cigar[i] >> 4;
      switch(cigar[i]&0xf) {
        case BAM_CMATCH:
        case BAM_CDEL:
        case BAM_CREF_SKIP:
          ref_end += op_len; break;
        default:
          break;
      }
  }
  ref_end--;
      
  target = tmap_malloc(sizeof(char) * (ref_end - ref_start + 1), "target");
  if(ref_end - ref_start + 1 != tmap_refseq_subseq(refseq, ref_start + refseq->annos[seqid].offset, ref_end - ref_start + 1, target)) {
      fprintf(stderr, "ref_start=%u ref_end=%u refseq->len=%u\n", ref_start, ref_end, (uint32_t)refseq->len);
      fprintf(stderr, "cigar=");
      for(i=0;i<n_cigar;i++) { // go through each cigar operator
          int32_t op_len;
          op_len = cigar[i] >> 4;
          fprintf(stderr, "%d%c", op_len, "MIDNSHP"[cigar[i]&0xf]);
          switch(cigar[i]&0xf) {
            case BAM_CMATCH:
            case BAM_CDEL:
            case BAM_CREF_SKIP:
            default:
              break;
          }
      }
      fprintf(stderr, "\n");
      tmap_error("bug encountered", Exit, OutOfRange);
  }

  // check if any IUPAC bases fall within the range
  if(0 < tmap_refseq_amb_bases(refseq, seqid+1, ref_start, ref_end)) {
      // modify them
      for(ref_i=ref_start;ref_i<=ref_end;ref_i++) {
          j = tmap_refseq_amb_bases(refseq, seqid+1, ref_i, ref_i); // Note: j is one-based
          if(0 < j) {
              target[ref_i-ref_start] = refseq->annos[seqid].amb_bases[j-1];
          }
      }
  }

  if(0 == n_cigar) {
      tmap_error("bug encountered", Exit, OutOfRange);
  }

  read_i = ref_i = 0;
  for(i=0;i<n_cigar;i++) { // go through each cigar operator
      int32_t op_len, op;

      op_len = cigar[i] >> 4;
      op = cigar[i] & 0xf;

      if(BAM_CMATCH == op) {
          for(j=0;j<op_len;j++) {
              if(refseq->len <= refseq->annos[seqid].offset + pos + ref_i) break; // out of boundary

              read_base = tmap_nt_char_to_int[(int)read_bases[read_i]]; 
              ref_base = target[ref_i];

              if(read_base == ref_base) { // a match
                  l++;
              }
              else {
                  tmap_string_lsprintf(md, md->l, "%d%c", l, tmap_iupac_int_to_char[ref_base]);
                  l = 0;
                  (*nm)++;
              }
              read_i++;
              ref_i++; 
          }
          if(j < op_len) break;
      }
      else if(BAM_CINS == op) {
          read_i += op_len;
          (*nm) += op_len;
      }
      else if(BAM_CDEL == op) {
          tmap_string_lsprintf(md, md->l, "%d^", l);
          for(j=0;j<op_len;j++) {
              if(refseq->len <= refseq->annos[seqid].offset + pos + ref_i) break; // out of boundary
              ref_base = target[ref_i];
              tmap_string_lsprintf(md, md->l, "%c", tmap_iupac_int_to_char[ref_base]);
              ref_i++;
          }
          if(j < op_len) break;
          (*nm) += op_len;
          l=0;
      }
      else if(BAM_CREF_SKIP == op) {
          ref_i += op_len;
      }
      else if(BAM_CSOFT_CLIP == op) {
          read_i += op_len;
      }
      else if(BAM_CHARD_CLIP == op) {
          // ignore
      }
      else if(BAM_CPAD == op) {
          // ignore
      }
      else {
          tmap_error("could not understand the cigar operator", Exit, OutOfRange);
      }
  }
  tmap_string_lsprintf(md, md->l, "%d", l);

  free(target);

  return md;
}

inline void
tmap_sam_print_mapped(tmap_file_t *fp, tmap_seq_t *seq, int32_t sam_sff_tags, tmap_refseq_t *refseq,
                      uint8_t strand, uint32_t seqid, uint32_t pos, 
                      uint8_t mapq, uint32_t *cigar, int32_t n_cigar,
                      int32_t score, int32_t ascore, int32_t algo_id, int32_t algo_stage,
                      const char *format, ...)
{
  va_list ap;
  int32_t i;
  tmap_string_t *name=NULL, *bases=NULL, *qualities=NULL;
  uint32_t *cigar_tmp = NULL, cigar_tmp_allocated = 0;
  tmap_string_t *md;
  int32_t nm;

  name = tmap_seq_get_name(seq);
  bases = tmap_seq_get_bases(seq);
  qualities = tmap_seq_get_qualities(seq);

  if(1 == strand) { // reverse for the output
      tmap_string_reverse_compliment(bases, 0);
      tmap_string_reverse(qualities);
  }

  if(0 == pos + 1) {
      tmap_error("position is out of range", Exit, OutOfRange);
  }

  tmap_file_fprintf(fp, "%s\t%u\t%s\t%u\t%u\t",
                    name->s, (1 == strand) ? 0x10 : 0, refseq->annos[seqid].name->s,
                    pos + 1,
                    mapq);
  cigar_tmp = cigar;
  // add the soft clipping of from an SFF
  /*
  if(TMAP_SEQ_TYPE_SFF == seq->type) {
      int32_t sff_soft_clip = seq->data.sff->gheader->key_length; // soft clip the key sequence
      if(0 < sff_soft_clip && 0 < n_cigar) {
          if(0 == strand) {  // forward strand sff soft clip
              if(4 == cigar[0]) { // add to an existing soft clip
                  cigar[0] = (((cigar[0]>>4) + sff_soft_clip) << 4) | 4;
              }
              else { // add a new soft clip to the front
                  cigar_tmp_allocated = 1;
                  cigar_tmp = tmap_calloc(n_cigar+1, sizeof(uint32_t), "cigar_tmp");
                  cigar_tmp[0] = (sff_soft_clip << 4) | 4; 
                  for(i=0;i<n_cigar;i++) {
                      cigar_tmp[i+1] = cigar[i];
                  }
                  n_cigar++;
              }
          }
          else {  // reverse strand sff soft clip
              if(4 == cigar[n_cigar-1]) { // add to an existing soft clip
                  cigar[n_cigar-1] = (((cigar[n_cigar-1]>>4) + sff_soft_clip) << 4) | 4;
              }
              else { // add a new soft clip to the end
                  cigar_tmp_allocated = 1;
                  cigar_tmp = tmap_calloc(n_cigar+1, sizeof(uint32_t), "cigar_tmp");
                  cigar_tmp[n_cigar] = (sff_soft_clip << 4) | 4; 
                  for(i=0;i<n_cigar;i++) {
                      cigar_tmp[i] = cigar[i];
                  }
                  n_cigar++;
              }
          }
      }
  }
  */

  // print out the cigar
  for(i=0;i<n_cigar;i++) {
      tmap_file_fprintf(fp, "%d%c",
                        cigar_tmp[i]>>4, "MIDNSHP"[cigar_tmp[i]&0xf]);
  }

  // bases and qualities
  tmap_file_fprintf(fp, "\t*\t0\t0\t%s\t%s",
                    bases->s, (0 == qualities->l) ? "*" : qualities->s);
  
  // RG and PG
  tmap_file_fprintf(fp, "\tRG:Z:%s\tPG:Z:%s",
                    tmap_sam_rg_id,
                    PACKAGE_NAME);

  // MD and NM
  md = tmap_sam_md(refseq, bases->s, seqid, pos, cigar_tmp, n_cigar, &nm);
  tmap_file_fprintf(fp, "\tMD:Z:%s\tNM:i:%d", md->s, nm);
  tmap_string_destroy(md);

  // AS
  tmap_file_fprintf(fp, "\tAS:i:%d", score);
  
  // FZ
  if(TMAP_SEQ_TYPE_SFF == seq->type && 1 == sam_sff_tags) {
      tmap_sam_print_flowgram(fp, seq->data.sff->read->flowgram, seq->data.sff->gheader->flow_length);
  }

  // XA
  if(0 < algo_stage) {
      tmap_file_fprintf(fp, "\tXA:Z:%s-%d", tmap_algo_id_to_name(algo_id), algo_stage);
  }
  
  // XZ
  if(TMAP_SEQ_TYPE_SFF == seq->type) {
      tmap_file_fprintf(fp, "\tXZ:i:%d", ascore);
  }

  // optional tags
  if(NULL != format) {
      va_start(ap, format);
      tmap_file_vfprintf(fp, format, ap);
      va_end(ap);
  }
  // new line
  tmap_file_fprintf(fp, "\n");
  if(1 == strand) { // reverse back
      tmap_string_reverse_compliment(bases, 0);
      tmap_string_reverse(qualities);
  }

  if(1 == cigar_tmp_allocated) {
      free(cigar_tmp);
  }
}

#ifdef HAVE_SAMTOOLS
// from bam_md.c in SAMtools
// modified not fill in the NM tag, and not to start the reference a c->pos
static void 
tmap_sam_md1_core(bam1_t *b, char *ref)
{
  uint8_t *seq = bam1_seq(b);
  uint32_t *cigar = bam1_cigar(b);
  bam1_core_t *c = &b->core;
  int i, x, y, u = 0;
  kstring_t *str;
  uint8_t *old_md, *old_nm;
  int32_t old_nm_i=-1, nm=0;

  str = (kstring_t*)calloc(1, sizeof(kstring_t));
  for (i = y = x = 0; i < c->n_cigar; ++i) {
      int j, l = cigar[i]>>4, op = cigar[i]&0xf;
      if (op == BAM_CMATCH) {
          for (j = 0; j < l; ++j) {
              int z = y + j;
              int c1 = bam1_seqi(seq, z), c2 = bam_nt16_table[(int)ref[x+j]];
              if (ref[x+j] == 0) break; // out of boundary
              if ((c1 == c2 && c1 != 15 && c2 != 15) || c1 == 0) { // a match
                  ++u;
              } else {
                  ksprintf(str, "%d", u);
                  kputc(ref[x+j], str);
                  u = 0; 
                  nm++;
              }
          }
          if (j < l) break;
          x += l; y += l;
      } else if (op == BAM_CDEL) {
          ksprintf(str, "%d", u);
          kputc('^', str);
          for (j = 0; j < l; ++j) {
              if (ref[x+j] == 0) break;
              kputc(ref[x+j], str);
          }
          u = 0;
          if (j < l) break;
          x += l; 
          nm += l;
      } else if (op == BAM_CINS || op == BAM_CSOFT_CLIP) {
          y += l;
          if (op == BAM_CINS) nm += l;
      } else if (op == BAM_CREF_SKIP) {
          x += l;
      }
  }
  ksprintf(str, "%d", u);

  // update MD
  old_md = bam_aux_get(b, "MD");
  if(NULL == old_md) {
      bam_aux_append(b, "MD", 'Z', str->l + 1, (uint8_t*)str->s);
  }
  else {
      int is_diff = 0;
      if(strlen((char*)old_md+1) == str->l) {
          for(i = 0; i < str->l; ++i) {
            if(toupper(old_md[i+1]) != toupper(str->s[i])) {
              break;
            }
          }
          if(i < str->l) {
              is_diff = 1;
          }
      } 
      else {
          is_diff = 1;
      }
      if(1 == is_diff) {
          bam_aux_del(b, old_md);
          bam_aux_append(b, "MD", 'Z', str->l + 1, (uint8_t*)str->s);
      }
  }

  // update NM
  old_nm = bam_aux_get(b, "NM");
  if(NULL != old_nm) {
      old_nm_i = bam_aux2i(old_nm);
      if(old_nm_i != nm) {
          bam_aux_del(b, old_nm);
          bam_aux_append(b, "NM", 'i', 4, (uint8_t*)&nm);
      }
  }

  free(str->s); free(str);
}

// from bam_md.c in SAMtools
void 
tmap_sam_md1(bam1_t *b, char *ref, int32_t len)
{
  int32_t i, j;
  char *ref_tmp = NULL;
  ref_tmp = tmap_malloc(sizeof(char) * (1 + len), "ref_tmp");
  for(i=j=0;i<len;i++) {
      if('-' != ref[i] && 'H' != ref[i]) {
          ref_tmp[j] = ref[i];
          j++;
      }
  }
  ref_tmp[j]='\0';
  tmap_sam_md1_core(b, ref_tmp);
  free(ref_tmp);
}

// soft-clipping is not supported
static inline int
tmap_sam_get_type(char ref, char read)
{
  if('-' == ref) { // insertion
      return TMAP_SW_FROM_I;
  }
  else if('-' == read) { // deletion
      return TMAP_SW_FROM_D;
  }
  else { // match/mismatch
      return TMAP_SW_FROM_M;
  }
}

void
tmap_sam_update_cigar_and_md(bam1_t *b, char *ref, char *read, int32_t len)
{
  int32_t i, n_cigar, last_type;
  uint32_t *cigar;
  int32_t diff;
  int32_t soft_clip_start_i, soft_clip_end_i;

  if(b->data_len - b->l_aux != bam1_aux(b) - b->data) {
      tmap_error("b->data_len - b->l_aux != bam1_aux(b) - b->data", Exit, OutOfRange);
  }

  // keep track of soft clipping
  n_cigar = soft_clip_start_i = soft_clip_end_i = 0;
  cigar = bam1_cigar(b);
  if(BAM_CSOFT_CLIP == TMAP_SW_CIGAR_OP(cigar[0])) {
      soft_clip_start_i = 1;
      n_cigar++;
  }
  if(1 < b->core.n_cigar && BAM_CSOFT_CLIP == TMAP_SW_CIGAR_OP(cigar[b->core.n_cigar-1])) {
      soft_clip_end_i = 1;
      n_cigar++;
  }
  cigar = NULL;

  // get the # of cigar operators
  last_type = tmap_sam_get_type(ref[0], read[0]);
  n_cigar++;
  for(i=1;i<len;i++) {
      int32_t cur_type = tmap_sam_get_type(ref[i], read[i]);
      if(cur_type != last_type) {
          n_cigar++;
      }
      last_type = cur_type;
  }

  // resize the data field if necessary
  if(n_cigar < b->core.n_cigar) {
      diff = sizeof(uint32_t) * (b->core.n_cigar - n_cigar);
      // shift down
      for(i=b->core.l_qname;i<b->data_len - diff;i++) {
          b->data[i] = b->data[i + diff];
      }
      b->data_len -= diff;
      b->core.n_cigar = n_cigar;
  }
  else if(b->core.n_cigar < n_cigar) {
      diff = sizeof(uint32_t) * (n_cigar - b->core.n_cigar);
      // realloc
      if(b->m_data <= (b->data_len + diff)) {
          b->m_data = b->data_len + diff + 1;
          tmap_roundup32(b->m_data);
          b->data = tmap_realloc(b->data, sizeof(uint8_t) * b->m_data, "b->data");
      }
      // shift up
      for(i=b->data_len-1;b->core.l_qname<=i;i--) {
          b->data[i + diff] = b->data[i];
      }
      b->data_len += diff;
      b->core.n_cigar = n_cigar;
  }
  if(b->data_len - b->l_aux != bam1_aux(b) - b->data) {
      tmap_error("b->data_len - b->l_aux != bam1_aux(b) - b->data", Exit, OutOfRange);
  }

  // create the cigar
  cigar = bam1_cigar(b);
  for(i=soft_clip_start_i;i<n_cigar-soft_clip_end_i;i++) {
      cigar[i] = 0;
  }
  n_cigar = soft_clip_start_i; // skip over soft clipping etc.
  last_type = tmap_sam_get_type(ref[0], read[0]);
  TMAP_SW_CIGAR_STORE(cigar[n_cigar], last_type, 1);
  for(i=1;i<len;i++) {
      int32_t cur_type = tmap_sam_get_type(ref[i], read[i]);
      if(cur_type == last_type) {
          // add to the cigar length
          TMAP_SW_CIGAR_ADD_LENGTH(cigar[n_cigar], 1);
      }
      else {
          // add to the cigar
          n_cigar++;
          TMAP_SW_CIGAR_STORE(cigar[n_cigar], cur_type, 1);
      }
      last_type = cur_type;
  }

  // Note: the md tag must be updated
  tmap_sam_md1(b, ref, len);
}
#endif
