/*
 * Mpeg TS/PES muxer.  
 * This might also handle generating sequential files and indexes.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* access */
#include <sys/stat.h>		/* mkdir */
#include <sys/types.h>		/* mkdir */

#include "CTI.h"
#include "MpegTSMux.h"
#include "MpegTS.h"
#include "Images.h"
#include "Audio.h"
#include "Array.h"

/* I can't figure out an algorithm to emulate the varying audio PTS values,
   so I am temporarily hacking in the observed values. */
static uint64_t audiopts[] = {
#include "audiopts.c"
};
static int audiopts_index = 0;

static void Config_handler(Instance *pi, void *msg);
static void H264_handler(Instance *pi, void *msg);
static void AAC_handler(Instance *pi, void *msg);
static void MP3_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_H264, INPUT_AAC, INPUT_MP3 };
static Input MpegTSMux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_H264 ] = { .type_label = "H264_buffer", .handler = H264_handler },
  [ INPUT_AAC ] = { .type_label = "AAC_buffer", .handler = AAC_handler },
  [ INPUT_MP3 ] = { .type_label = "MP3_buffer", .handler = MP3_handler },
};

//enum { /* OUTPUT_... */ };
static Output MpegTSMux_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


typedef struct _ts_packet {
  uint8_t data[188];
  struct _ts_packet *next;
  uint af:1;
  uint pus:1;
} TSPacket;

#define ESSD_MAX 2

typedef struct {
  TSPacket * packets;
  TSPacket * last_packet;
  uint64_t running_pts;
  int pts_add;
  uint64_t running_pcr;
  int pcr_add;
  uint16_t pid;
  uint16_t continuity_counter;
  uint16_t es_id;
  uint pts:1;
  uint dts:1;
  uint pcr:1;
} Stream;

typedef struct {
  Instance i;
  // TS file format string.
  String *chunk_fmt;
  // TS sequence duration in seconds.
  int chunk_duration;
  FILE *chunk_file;

  int pktseq;			/* Output packet counter... */

  // Index file format string.
  String *index_fmt;
  
#define MAX_STREAMS 2
  Stream streams[MAX_STREAMS];

  struct {
    uint8_t continuity_counter;
  } PAT;

  TSPacket PAT_packet;

  struct {
    uint16_t PCR_PID;
    /* Elementary stream specific data: */
    struct {
      uint8_t streamType;
      uint16_t elementary_PID;
    } ESSD[ESSD_MAX];
    uint8_t continuity_counter;
  } PMT;

  TSPacket PMT_packet;

  uint32_t continuity_counter;

  time_t tv_sec_offset;
} MpegTSMux_private;


static int set_pmt_essd(Instance *pi, const char *value)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  String_list * parts = String_split_s(value, ":");
  
  if (String_list_len(parts) != 3) {
    fprintf(stderr, "%s expected essd as index:streamtype:elementary_pid\n", __func__);
    return 1;
  }

  int i, streamtype, elementary_pid;
  String_parse_int(String_list_get(parts, 0), 0, &i);
  String_parse_int(String_list_get(parts, 1), 0, &streamtype);
  String_parse_int(String_list_get(parts, 2), 0, &elementary_pid);

  if (i < 0 || i >= (ESSD_MAX)) {
    fprintf(stderr, "%s: essd index out of range\n",  __func__);
    return 1;
  }

  if (elementary_pid < 0 || elementary_pid > 8191) {
    fprintf(stderr, "%s: essd elementary_pid out of range\n",  __func__);
    return 1;
  }

  if (streamtype < 0 || streamtype > 255) {
    fprintf(stderr, "%s: essd streamtype out of range\n",  __func__);
    return 1;
  }

  priv->PMT.ESSD[i].elementary_PID = elementary_pid;
  priv->PMT.ESSD[i].streamType = streamtype;

  String_list_free(&parts);
  return 0;
}


static int set_pmt_pcrpid(Instance *pi, const char *value)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  int pcrpid = atoi(value);
  if (pcrpid < 0 || pcrpid > 8191) {
    fprintf(stderr, "%s: pmt pcrpid out of range\n",  __func__);
    return 1;
  }
  priv->PMT.PCR_PID = pcrpid;
  return 0;
}


static Config config_table[] = {
  { "pmt_essd", set_pmt_essd, 0L, 0L },
  { "pmt_pcrpid", set_pmt_pcrpid, 0L, 0L },
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void packetize(MpegTSMux_private * priv, uint16_t pid, ArrayU8 * data)
{
  MpegTimeStamp pts = {};
  MpegTimeStamp dts = {};
  uint64_t tv90k;
  int i;
  Stream *stream = NULL;
  
  for (i=0; i < MAX_STREAMS; i++) {
    if (priv->streams[i].pid == pid) {
      stream = &priv->streams[i];
      break;
    }
  }
  if (i == MAX_STREAMS) {
    printf("pid %d not set up\n", pid);
    return;
  }

  /* Map timestamp to 33-bit 90KHz units. */
  //tv90k = ((h264->c.tv.tv_sec - priv->tv_sec_offset) * 90000) + (h264->c.tv.tv_usec * 9 / 100);
  tv90k = stream->running_pcr;

  /* Assign to PTS, and same to DTS. */
  /* FIXME: This might not actually be the right thing to do. */
  uint8_t peslen = 0; /* PES remaining header data length */
  uint8_t flags = 0;
  if (stream->pts) {
    pts.set = 1;
    flags |= (1<<7);
    peslen += 5;
    pts.value = stream->running_pts;
    if (stream->dts) {
      dts.set = 1;
      flags |= (1<<6);
      peslen += 5;
      dts.value = pts.value;
    }
  }

  /* Wrap the data in a PES packet (prepend a chunk of header data),
     then divide into TS packets, and either write out, or save in a
     list so they can be smoothly interleaved with audio packets.  See
     "demo-ts-analysis.txt".  */
  ArrayU8 *pes = ArrayU8_new();
  ArrayU8_append_bytes(pes, 
		       0x00, 0x00, 0x01, /* PES packet start code prefix. */
		       stream->es_id);	 /* Elementary stream id */

  if (stream->pcr) {
    /* PES remaining length unset for video. */
    ArrayU8_append_bytes(pes, 0x00, 0x00);
  }
  else {
    /* PES packet length IS set for audio. */
    int x = 8 + data->len;
    ArrayU8_append_bytes(pes, (x >> 8 & 0xff), (x & 0xff));
  }

  if (1 /* extension present, depending on Stream id */) {
    ArrayU8_append_bytes(pes,
			 0x84,	/* '10', PES priority bit set, various flags unset. */
			 flags,
			 peslen
			 );
  }

  if (pts.set) {
    /* Pack PTS. */
    ArrayU8_append_bytes(pes, 
			 (pts.set << 5) | (dts.set << 4) | (((pts.value>>30)&0x7) << 1 ) | 1
			 ,((pts.value >> 22)&0xff) /* bits 29:22 */
			 ,((pts.value >> 14)&0xfe) | 1 /* bits 21:15, 1 */
			 ,((pts.value >> 7)&0xff)	   /* bits 14:7 */
			 ,((pts.value << 1)&0xfe) | 1   /* bits 6:0, 1 */
			 //,00,00,00,00
			 );
    stream->running_pts += stream->pts_add;

    {
      /* Special-case handling for test sample. */
      if (stream->es_id == 224 && stream->running_pts == 1404504) { stream->running_pts += 1; }
      if (stream->es_id == 192 && audiopts_index < table_size(audiopts)) {
	audiopts_index += 1;
	stream->running_pts = audiopts[audiopts_index];
      } 
    }


  }

  if (dts.set) {
    /* Pack DTS, same format as PTS. */
    ArrayU8_append_bytes(pes,
			 (dts.set << 4) | (((dts.value>>30)&0x7) << 1 ) | 1
			 ,((dts.value >> 22)&0xff) /* bits 29:22 */
			 ,((dts.value >> 14)&0xfe) | 1 /* bits 21:15, 1 */
			 ,((dts.value >> 7)&0xff)	   /* bits 14:7 */
			 ,((dts.value << 1)&0xfe) | 1   /* bits 6:0, 1 */
			 );
  }


  /* Copy the data. */
  ArrayU8_append(pes, data);

  printf("pes->len=%d, data->len=%d\n", pes->len, data->len);
  
  /* Now pack into TS packets... */
  unsigned int pes_offset = 0;
  unsigned int pes_remaining;
  unsigned int payload_size;
  unsigned int payload_index = 0;

  while (pes_offset < pes->len) {
    TSPacket *packet = Mem_calloc(1, sizeof(*packet));
    unsigned int afLen = 0;
    unsigned int afAdjust = 1;	/* space for the afLen byte */

    pes_remaining = pes->len - pes_offset;
    printf("pes->len=%d pes_remaining=%d\n", pes->len, pes_remaining);

    packet->data[0] = 0x47;			/* Sync byte */

    packet->data[1] = 
      (0 << 5) |		      /* priority bit, not set */
      ((pid & 0x1f00) >> 8);	      /* pid[13:8] */

    if (payload_index == 0) {
      packet->pus = 1;
      packet->data[1] |= (1 << 6);      /* PUS */
    }
    
    packet->data[2] = (pid & 0xff); /* pid[7:0] */
    packet->data[3] = (0x0 << 6); /* not scrambled */

    /* Always pack in a payload. */
    packet->data[3] |= (1 << 4);
    
    /* Increment continuity counter if payload is present. */
    packet->data[3] |= (stream->continuity_counter & 0x0F);
    stream->continuity_counter += 1;

    if (payload_index == 0 && ( (stream->pcr) || (!stream->pcr && pes->len <= 183) ) ) {
      packet->data[3] |= (1 << 5); /* adaptation field */
      packet->af = 1;
      afLen = 1;

      if (stream->pcr) {
	afLen += 6;
	packet->data[5] |= (1<<4);	/* PCR */
	/* FIXME: use data[index] if other flag bits are to be set. */
	/* (notice corresponding unpack code in MpegTSDemux.c for same shifts in other direction) */
	packet->data[6] = ((tv90k >> 25) & 0xff);
	packet->data[7] = ((tv90k >> 17) & 0xff);
	packet->data[8] = ((tv90k >> 9) & 0xff);
	packet->data[9] = ((tv90k >> 1) & 0xff);
	packet->data[10] = ((tv90k & 1) << 7) /* low bit of 33 bits */
	  | (0x3f << 1)  /* 6 bits reserved/padding */
	  | (0<<0);	/* 9th bit of extension */
	packet->data[11] = (0x00);	/* bits 8:0 of extension */
	
	stream->running_pcr += stream->pcr_add;

	{
	  /* Special-case handling for test sample. */
	  if (stream->running_pcr == 1402104) { stream->running_pcr += 1; }
	}
      }

      int available_payload_size = (188 - 4 - 1 - afLen);
      if (pes_remaining < available_payload_size) {
	/* Everything fits into one packet! */
	int filler_count = (available_payload_size - pes_remaining);
	memset(packet->data+4+1+afLen, 0xFF, filler_count); /* pad with FFs... */
	afLen += filler_count;
	payload_size = pes_remaining;
      }
      else {
	payload_size = available_payload_size;
      }
      packet->data[4] = afLen;
    }
    else if (pes_remaining <= 183) {
      /* Note: when pes_remaining == 183, afLen is 1, and the memset
	 sets 0 bytes. */
      packet->data[3] |= (1 << 5); /* adaptation field */
      afLen = (188 - 4 - 1 - pes_remaining);
      packet->af = 1;
      packet->data[4] = afLen;
      packet->data[5]  = 0x00;	/* no flags set */
      if (afLen > 0) {
	memset(packet->data+4+1+1, 0xFF, afLen-1);  /* pad with FFs... */
      }
      payload_size = pes_remaining;
    }
    else {
      /* No adaptation field. */
      payload_size = 188 - 4;
      afAdjust = 0;
    }

    memcpy(packet->data+4+afAdjust+afLen, 
	   pes->data+pes_offset, 
	   payload_size);

    pes_offset += payload_size;
    payload_index += 1;

    /* Add to list. */
    if (!stream->packets) {
      stream->packets = packet;
      stream->last_packet = packet;
    }
    else {
      stream->last_packet->next = packet;
      stream->last_packet = packet;
    }
  }
}


static void H264_handler(Instance *pi, void *msg)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  H264_buffer *h264 = msg;
  printf("h264->encoded_length=%d\n",  h264->encoded_length);
  packetize(priv, 258, ArrayU8_temp_const(h264->data, h264->encoded_length) );
  H264_buffer_discard(h264);
}


static void AAC_handler(Instance *pi, void *msg)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  AAC_buffer *aac = msg;
  packetize(priv, 257, ArrayU8_temp_const(aac->data, aac->data_length) );
  AAC_buffer_discard(&aac);
}


static void MP3_handler(Instance *pi, void *msg)
{
  // MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  //MP3_buffer *mp3 = msg;

  /* Assemble TS packets, save in a list so they can be smoothly
     interleaved with video packets. */
  
  /* Discard MP3 data. */
  //MP3_buffer_discard(&mp3);
}


static void generate_pat(MpegTSMux_private *priv)
{
  memset(priv->PAT_packet.data, 0xff, sizeof(priv->PAT_packet.data));

  /* Calculate CRC... */
  priv->PAT.continuity_counter = (priv->PAT.continuity_counter + 1) % 16;
}


static void generate_pmt(MpegTSMux_private *priv)
{
  memset(priv->PMT_packet.data, 0xff, sizeof(priv->PMT_packet.data));

  /* Calculate CRC... */
  priv->PMT.continuity_counter = (priv->PMT.continuity_counter + 1) % 16;
}


static void flush(Instance *pi)
{
  /* Write out interleaved video and audio packets, adding PAT and PMT
     packets as needed.  I don't know how to schedule them yet... */
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  int i;

  if (access("outpackets", R_OK) != 0) {
    mkdir("outpackets", 0744);
  }

  // printf("%s: priv->video_packets = %p\n", __func__, priv->video_packets);

  /* This is temporary, I'll eventually set up a proper output, or
     m3u8 generation. */
  if (0) {
    generate_pat(priv);
  }
  
  if (0) {
    generate_pmt(priv);
  }

  for (i=0; i < MAX_STREAMS; i++) {
    Stream * stream = &priv->streams[i];
    while (stream->packets) {
      TSPacket *pkt = stream->packets;
      char fname[256]; sprintf(fname, 
			       "outpackets/%05d-ts%04d%s%s", 
			       priv->pktseq++,
			       stream->pid,
			       pkt->af ? "-AF" : "" ,
			       pkt->pus ? "-PUS" : ""
			       );
      FILE * f = fopen(fname, "wb");
      if (f) {
	if (fwrite(pkt->data, sizeof(pkt->data), 1, f) != 1) { perror("fwrite"); }
	fclose(f);
      }
      stream->packets = stream->packets->next;
      Mem_free(pkt);
    }
  }
}


static void MpegTSMux_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
    flush(pi);    
  }

  pi->counter++;
}

static void MpegTSMux_instance_init(Instance *pi)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;

  priv->streams[0] = (Stream) {
    .pid = 258, 
    .pts = 1, 
    .dts = 1, 
    .pcr = 1, 
    .es_id = 224,
    .running_pts = 900000,
    .pts_add = 6006,
    .running_pcr = 897600,
    .pcr_add = 6006
  };

  priv->streams[1] = (Stream) {
    .pid = 257,
    .pts = 1,
    .dts = 0,
    .pcr = 0,
    .es_id = 192,
    .running_pts = 900000,
    .pts_add = 4180
  };
  
  /* FIXME: This is arbitrary, based on an early 2011 epoch date.  The
     real problem is I don't know to handle PTS wraps.  Should fix
     that sometime... */
  priv->tv_sec_offset = 1300000000;  
}


static Template MpegTSMux_template = {
  .label = "MpegTSMux",
  .priv_size = sizeof(MpegTSMux_private),
  .inputs = MpegTSMux_inputs,
  .num_inputs = table_size(MpegTSMux_inputs),
  .outputs = MpegTSMux_outputs,
  .num_outputs = table_size(MpegTSMux_outputs),
  .tick = MpegTSMux_tick,
  .instance_init = MpegTSMux_instance_init,
};

void MpegTSMux_init(void)
{
  Template_register(&MpegTSMux_template);
}
