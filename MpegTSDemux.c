/*
 * Unpack data from Mpeg2 Transport Streams.
 * See earlier code "modc/MpegTS.h" for starters.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */
#include <inttypes.h>

#include "CTI.h"
#include "MpegTSDemux.h"
#include "MpegTS.h"
#include "SourceSink.h"
#include "Cfg.h"

static const char *stream_type_strings[256] = {
  [0x0f] = "AAC",
  [0x1b] = "H.264",
};


static const char *stream_type_string(int index)
{
  const char * result = stream_type_strings[index];
  if (result == NULL) {
    result = "unknown";
  }
  return result;
}


static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input MpegTSDemux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  // [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
};

//enum { /* OUTPUT_... */ };
static Output MpegTSDemux_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


/*
 * "Stream", maybe a "Packetized elementary stream", 
 *
 *    http://en.wikipedia.org/wiki/Packetized_elementary_stream
 *
 *  but I don't understand the layers that well yet.
 */
typedef struct _Stream {
  int pid;
  ArrayU8 *data;
  int seq;
  struct _Stream *next;
} Stream;

static Stream * Stream_new(pid)
{
  Stream *s = Mem_calloc(1, sizeof(*s));
  s->pid = pid;
  printf("\n[new pid %d]\n", pid);
  return s;
}

static int packetCounter = 0;

typedef struct {
  Instance i;
  String input;
  Source *source;
  ArrayU8 *chunk;
  int needData;

  int enable;			/* Set this to start processing. */
  int filter_pid;
  
  int pmt_id;			/* Program Map Table ID */

  uint64_t offset;

  //int use_feedback;
  //int pending_feedback;
  //int feedback_threshold;

  int retry;
  int exit_on_eof;

  Stream *streams;

} MpegTSDemux_private;


static int set_input(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;
  if (priv->source) {
    Source_free(&priv->source);
  }

  if (value[0] == '$') {
    value = getenv(value+1);
    if (!value) {
      fprintf(stderr, "env var %s is not set\n", value+1);
      return 1;
    }
  }

  String_set(&priv->input, value);
  priv->source = Source_new(sl(priv->input));

  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->needData = 1;

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "MpegTSDemux: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("MpegTSDemux enable set to %d\n", priv->enable);

  return 0;
}


static int set_filter_pid(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;

  priv->filter_pid = atoi(value);

  printf("MpegTSDemux filter_pid set to %d\n", priv->filter_pid);

  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "filter_pid", set_filter_pid, 0L, 0L },
  { "exit_on_eof", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, exit_on_eof) },
  // { "use_feedback", set_use_feedback, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void print_packet(uint8_t * packet)
{
  int i = 0;
  printf("   ");
  while (i < 188) {
    printf(" %02x", packet[i]);
    i++;
    if (i % 16 == 0) {
      printf("\n   ");
    }
  }

  if (i % 16 != 0) {
    printf("\n");
  }
}

static void Streams_add(MpegTSDemux_private *priv, uint8_t *packet)
{
  int payloadLen = 188 - 4;
  int payloadOffset = 188 - payloadLen;
  int pid = MpegTS_PID(packet);
  int afLen = 0;
  int isPUS = 0;

  if (priv->filter_pid && priv->filter_pid != pid) {
    // cfg.verbosity = 0;
  }

  if (priv->streams == NULL) {
    priv->streams = Stream_new(pid);
  }
  
  Stream *s = priv->streams;

  while (s) {
    if (1 || cfg.verbosity) { 
      // printf("packet %d, pid %d s->pid=%d\n", packetCounter, pid, s->pid);
    }

    if (pid == s->pid) {
      printf("\npacket %d, pid %d\n", packetCounter, pid);
      print_packet(packet);

      if (MpegTS_sync_byte(packet) != 0x47) {
	printf("invalid packet at offset %" PRIu64 "\n", priv->offset);
	return;
      }
    
      if (MpegTS_TEI(packet)) {
	printf("transport error at offset %" PRIu64 "\n", priv->offset);
	return;
      }

      if (MpegTS_PUS(packet)) {
	printf("  payload unit start (PES or PSI)\n");
	isPUS = 1;
	if (s->data) {
	  /* Flush, free, reallocate. */
	  if (s->data->len) {
	    char name[32];
	    FILE *f;
	    sprintf(name, "%05d-%04d.%04d", packetCounter, s->pid, s->seq);
	    f = fopen(name, "wb");
	    if (f) {
	      int n = fwrite(s->data->data, s->data->len, 1, f);
	      if (n != 1) { perror(name); }
	      fclose(f);
	      f = NULL;
	    }

	    if (cfg.verbosity) { 
	      /* NOTE: This is peeking down into the PES layer, and is
		 only written to handle one specific example case. */
	      if (pid > 256) { 
		printf("  [PES] previous payload PTS: %d%d%d%d (%d) %d",
		       (s->data->data[9] >> 7) & 1,
		       (s->data->data[9] >> 6) & 1,
		       (s->data->data[9] >> 5) & 1,
		       (s->data->data[9] >> 4) & 1,
		       (s->data->data[9] >> 1) & 7,
		       (s->data->data[9] >> 0) & 1);

		printf(" %" PRIu64 "", MpegTS_PTS(s->data->data + 9));
		printf("\n");
	      }

	      if (pid == 258) { 
		printf("  [PES] previous payload DTS: %d%d%d%d (%d) %d",
		       (s->data->data[14] >> 7) & 1,
		       (s->data->data[14] >> 6) & 1,
		       (s->data->data[14] >> 5) & 1,
		       (s->data->data[14] >> 4) & 1,
		       (s->data->data[14] >> 1) & 7,
		       (s->data->data[14] >> 0) & 1);

		printf(" %" PRIu64, MpegTS_PTS(s->data->data + 14));
		printf("\n");
	      }
	    }


	  } /* if (s->data->len) */
	  s->seq += 1;
	  ArrayU8_cleanup(&s->data);
	} /* if (s->data) */
	s->data = ArrayU8_new();
      }

      if (MpegTS_TP(packet)) {
	if (cfg.verbosity) printf("  transport priority bit set\n");
      }

      if (MpegTS_SC(packet)) {
	int bits = MpegTS_SC(packet);
	if (cfg.verbosity) printf("  scrambling control enabled (bits %d%d)\n",
				  (bits>>1), (bits&1));
      }

      if (MpegTS_AFE(packet)) {
	afLen = MpegTS_AF_len(packet);
	// int optionOffset = 2;
	payloadLen = 188 - 4 - 1 - afLen;
	payloadOffset = 188 - payloadLen;
	if (cfg.verbosity) printf("  AF present, length: (1+) %d\n", afLen);

	if (afLen == 0) {
	  /* I've observed this while analyzing the Apple TS dumps.  I
	     treat it such that the remaining payload is 183 byte, so
	     the AF length field occupies the one byte, and the
	     additional flags are left out!  Which implies they are
	     optional, at least in this case, which is not indicated by
	     the Wikipedia page. */
	  goto xx;
	}

	if (MpegTS_AF_DI(packet)) {
	  if (cfg.verbosity) printf("  discontinuity indicated\n");
	}
	if (MpegTS_AF_RAI(packet)) {
	  if (cfg.verbosity) printf("  random access indicator (new video/audio sequence)\n");
	}
	if (MpegTS_AF_ESPI(packet)) {
	  if (cfg.verbosity) printf("  high priority \n");
	}
	if (MpegTS_AF_PCRI(packet)) {
	  if (0 && cfg.verbosity) printf("  PCR present:  %02x %02x %02x %02x %01x  %02x  %03x\n",
		 /* 33 bits, 90KHz */
		 packet[6],
		 packet[7],
		 packet[8],
		 packet[9],
		 packet[10] >> 7,
		 /* 6 bits "reserved", see iso13818-1.pdf Table 2-6 */
		 (packet[10] >> 1) & 0x3f,
		 /* 9 bits, 27MHz */
		 ((packet[10] & 1) << 8) | packet[11]);

	  if (cfg.verbosity) {
	    printf("  PCR present:  [%02x] 90kHz:%d",
		   /* 33 bits, 90KHz */
		   packet[6],
		   (packet[6] << 25) | (packet[7] << 17) | (packet[8] << 9) | (packet[9] << 1) | (packet[10] >> 7));
	    /* 6 bits "reserved", see iso13818-1.pdf Table 2-6 */
	    printf(" res:%d", (packet[10] >> 1) & 0x3f);
	    /* 9 bits, 27MHz */
	    printf(" [%02x, %02x] 27MHz:%d\n", packet[10], packet[11], ((packet[10] & 1) << 8) | packet[11]);
	  }
	}
	if (MpegTS_AF_OPCRI(packet)) {
	  if (cfg.verbosity) printf("  OPCR present\n");
	}
	if (MpegTS_AF_SP(packet)) {
	  if (cfg.verbosity) printf("  splice point\n");
	}
	if (MpegTS_AF_TP(packet)) {
	  if (cfg.verbosity) printf("  transport private data\n");
	}
	if (MpegTS_AF_AFEXT(packet)) {
	  if (cfg.verbosity) printf("  adapdation field extension\n");
	}

      xx:;

      }	/* AFE... */


      if (MpegTS_PDE(packet)) {
	if (cfg.verbosity) printf("  payload data present, length %d\n", payloadLen);
	if (!s->data) {
	  s->data = ArrayU8_new();
	}
	ArrayU8_append(s->data, ArrayU8_temp_const(packet+payloadOffset, payloadLen));
      }
      else {
	if (cfg.verbosity) printf("  no payload data\n");
      }

      int cc = MpegTS_CC(packet);
      printf("  continuity counter=%d\n", cc);

      /* http://en.wikipedia.org/wiki/Program_Specific_Information */
      if (pid == 0) {
	printf("  PSI: Program Association Table\n");
	int pointer_filler_bytes = MpegTS_PSI_PTRFIELD(packet);
	printf("  (%d pointer filler bytes)\n", pointer_filler_bytes);
	uint8_t * table_header = MpegTS_PSI_TABLEHDR(packet);
	
	int table_id = MpegTS_PSI_TABLE_ID(table_header);
	printf("  Table ID %d\n", table_id);
	printf("  Section syntax indicator: %d\n", MpegTS_PSI_TABLE_SSI(table_header));
	printf("  Private bit: %d\n", MpegTS_PSI_TABLE_PB(table_header));
	printf("  Reserved bits: %d\n", MpegTS_PSI_TABLE_RBITS(table_header));
	printf("  Section length unused bits: %d\n", MpegTS_PSI_TABLE_SUBITS(table_header));
	int slen = MpegTS_PSI_TABLE_SLEN(table_header);
	printf("  Section length: %d\n", slen);

	if (MpegTS_PSI_TABLE_SSI(table_header)) {
	  uint8_t * tss = MpegTS_PSI_TSS(table_header);
	  printf("    Table ID extension: %d\n", MpegTS_PSI_TSS_ID_EXT(tss));
	  printf("    Reserved bits: %d\n", MpegTS_PSI_TSS_RBITS(tss));
	  printf("    Version number: %d\n", MpegTS_PSI_TSS_VERSION(tss));
	  printf("    current/next: %d\n", MpegTS_PSI_TSS_CURRNEXT(tss));
	  printf("    Section number: %d\n", MpegTS_PSI_TSS_SECNO(tss));
	  printf("    Last section number: %d\n", MpegTS_PSI_TSS_LASTSECNO(tss));
	  
	  uint8_t * pasd = MpegTS_PAT_PASD(tss);
	  uint32_t crc;
	  int remain = 
	    slen 
	    - 5 /* PSI_TSS */
	    - sizeof(crc);
	  while (remain > 0) {
	    printf("      Program number: %d\n", MpegTS_PAT_PASD_PROGNUM(pasd));
	    printf("      Reserved bits: %d\n", MpegTS_PAT_PASD_RBITS(pasd));
	    priv->pmt_id = MpegTS_PAT_PASD_PMTID(pasd);
	    printf("      PMT ID: %d\n", priv->pmt_id);
	    pasd += 4;
	    remain -= 4;
	  }
	  crc = (pasd[0]<<24)|(pasd[1]<<16)|(pasd[2]<<8)|(pasd[3]<<0);
	  printf("      crc: (supplied:calculated) 0x%08x:0x????????\n", crc);
	}
      }

      else if (priv->pmt_id != 0 && pid == priv->pmt_id) {
	printf("  PSI: Program Map Table\n");
	int pointer_filler_bytes = MpegTS_PSI_PTRFIELD(packet);
	printf("  (%d pointer filler bytes)\n", pointer_filler_bytes);
	uint8_t * table_header = MpegTS_PSI_TABLEHDR(packet);
	
	int table_id = MpegTS_PSI_TABLE_ID(table_header);
	printf("  Table ID %d\n", table_id);
	printf("  Section syntax indicator: %d\n", MpegTS_PSI_TABLE_SSI(table_header));
	printf("  Private bit: %d\n", MpegTS_PSI_TABLE_PB(table_header));
	printf("  Reserved bits: %d\n", MpegTS_PSI_TABLE_RBITS(table_header));
	printf("  Section length unused bits: %d\n", MpegTS_PSI_TABLE_SUBITS(table_header));
	int slen = MpegTS_PSI_TABLE_SLEN(table_header);
	printf("  Section length: %d\n", slen);

	if (MpegTS_PSI_TABLE_SSI(table_header)) {
	  uint8_t * tss = MpegTS_PSI_TSS(table_header);
	  printf("    Table ID extension: %d\n", MpegTS_PSI_TSS_ID_EXT(tss));
	  printf("    Reserved bits: %d\n", MpegTS_PSI_TSS_RBITS(tss));
	  printf("    Version number: %d\n", MpegTS_PSI_TSS_VERSION(tss));
	  printf("    current/next: %d\n", MpegTS_PSI_TSS_CURRNEXT(tss));
	  printf("    Section number: %d\n", MpegTS_PSI_TSS_SECNO(tss));
	  printf("    Last section number: %d\n", MpegTS_PSI_TSS_LASTSECNO(tss));
	  
	  uint8_t * pmsd = MpegTS_PMT_PMSD(tss);
	  printf("      [--Program Map Specific data]\n");
	  printf("      Reserved bits: 0x%x\n", MpegTS_PMT_PMSD_RBITS(pmsd));
	  printf("      PCR PID: %d\n", MpegTS_PMT_PMSD_PCRPID(pmsd));
	  printf("      Reserved bits 2: 0x%x\n", MpegTS_PMT_PMSD_RBITS2(pmsd));
	  printf("      Unused bits: %d\n", MpegTS_PMT_PMSD_UNUSED(pmsd));
	  printf("      Program descriptors length: %d\n", MpegTS_PMT_PMSD_PROGINFOLEN(pmsd));

	  uint8_t * essd = MpegTS_PMT_ESSD(pmsd);
	  uint32_t crc;
	  int remain = 
	    slen 
	    - 5 /* PSI_TSS */
	    - 4 /* PMT_PMSD */
	    - MpegTS_PMT_PMSD_PROGINFOLEN(pmsd)
	    - sizeof(crc);
	  while (remain > 0) {
	    printf("      [--Elementary stream specific data]\n");
	    printf("      Stream type: 0x%02x (%s)\n", 
		   MpegTS_ESSD_STREAMTYPE(essd),
		   stream_type_string(MpegTS_ESSD_STREAMTYPE(essd)));
	    printf("      Reserved bits: %d\n", MpegTS_ESSD_RBITS1(essd));
	    printf("      Elementary PID: %d\n", MpegTS_ESSD_ELEMENTARY_PID(essd));
	    printf("      Reserved bits2: %d\n", MpegTS_ESSD_RBITS2(essd));
	    printf("      Unused: %d\n", MpegTS_ESSD_UNUSED(essd));
	    printf("      Stream descriptors length: %d\n", MpegTS_ESSD_DESCRIPTORSLENGTH(essd));
	    int n = 5 + MpegTS_ESSD_DESCRIPTORSLENGTH(essd);
	    remain -= n;
	    essd += n;
	  }
	  
	  crc = (essd[0]<<24)|(essd[1]<<16)|(essd[2]<<8)|(essd[3]<<0);
	  printf("      crc: (supplied:calculated) 0x%08x:0x????????\n", crc);
	}
      }
      

      break;
    }

    if (!s->next) {
      s->next = Stream_new(pid);
      /* loop will roll around and break above on pid match */
    }
    s = s->next;
  }


  {
    char filename[256];
    sprintf(filename, "tmp/%05d-ts%04d-%03d%s%s", packetCounter, pid, afLen, afLen ? "-AF" : "", isPUS ? "-PUS" : "");
    FILE *f = fopen(filename, "wb");
    if (f) {
      int n = fwrite(packet, 188, 1, f);
      if (!n) perror("fwrite");
      fclose(f);
    }
  }
}


static void parse_chunk(MpegTSDemux_private *priv)
{
  uint8_t *packet;

  while (priv->offset < (priv->chunk->len - 188)) {
    packet = priv->chunk->data + priv->offset;

    Streams_add(priv, packet);

    priv->offset += 188;
    packetCounter += 1;
  }

}


static void MpegTSDemux_tick(Instance *pi)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;
  Handler_message *hm;
  int wait_flag;

  if (!priv->enable || !priv->source) {
    wait_flag = 1;
  }
  else {
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->enable || !priv->source) {
    return;
  }

  if (priv->needData) {
    ArrayU8 *newChunk;
    newChunk = Source_read(priv->source);

    //int vsave = cfg.verbosity; cfg.verbosity = 1;
    if (newChunk && cfg.verbosity) { 
      fprintf(stderr, "got %d bytes\n", newChunk->len);
    }
    //cfg.verbosity = vsave;

    if (!newChunk) {
      /* FIXME: EOF on a local file should be restartable.  Maybe
	 socket sources should be restartable, too. */
      Source_close_current(priv->source);
      fprintf(stderr, "%s: source finished.\n", __func__);
      if (priv->retry) {
	fprintf(stderr, "%s: retrying.\n", __func__);
	sleep(1);
	Source_free(&priv->source);
	priv->source = Source_new(sl(priv->input));
      }
      else if (priv->exit_on_eof) {
	exit(0);
      }
      else {
	priv->enable = 0;
      }
      return;
    }

    ArrayU8_append(priv->chunk, newChunk);
    ArrayU8_cleanup(&newChunk);
    parse_chunk(priv);
    // if (cfg.verbosity) { fprintf(stderr, "needData = 0\n"); }
    // priv->needData = 0;
  }

  /* trim consumed data from chunk, reset "current" variables. */
  ArrayU8_trim_left(priv->chunk, priv->offset);
  priv->offset = 0;
  pi->counter += 1;
}

static void MpegTSDemux_instance_init(Instance *pi)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;
  priv->filter_pid = -1;
  
}


static Template MpegTSDemux_template = {
  .label = "MpegTSDemux",
  .priv_size = sizeof(MpegTSDemux_private),
  .inputs = MpegTSDemux_inputs,
  .num_inputs = table_size(MpegTSDemux_inputs),
  .outputs = MpegTSDemux_outputs,
  .num_outputs = table_size(MpegTSDemux_outputs),
  .tick = MpegTSDemux_tick,
  .instance_init = MpegTSDemux_instance_init,
};

void MpegTSDemux_init(void)
{
  Template_register(&MpegTSDemux_template);
}
