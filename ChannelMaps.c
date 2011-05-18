#include "Template.h"
#include "ChannelMaps.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *channel;
    const char *frequency_HZ;
} Channel;

Channel NTSC_Cable_channels[] = {
  {"2", "55250000"},
  {"3", "61250000"},
  {"4", "67250000"},
  {"5", "77250000"},
  {"6", "83250000"},
  {"7", "175250000"},
  {"8", "181250000"},
  {"9", "187250000"},
  {"10", "193250000"},
  {"11", "199250000"},
  {"12", "205250000"},
  {"13", "211250000"},
  {"14", "121250000"},
  {"15", "127250000"},
  {"16", "133250000"},
  {"17", "139250000"},
  {"18", "145250000"},
  {"19", "151250000"},
  {"20", "157250000"},
  {"21", "163250000"},
  {"22", "169250000"},
  {"23", "217250000"},
  {"24", "223250000"},
  {"25", "229250000"},
  {"26", "235250000"},
  {"27", "241250000"},
  {"28", "247250000"},
  {"29", "253250000"},
  {"30", "259250000"},
  {"31", "265250000"},
  {"32", "271250000"},
  {"33", "277250000"},
  {"34", "283250000"},
  {"35", "289250000"},
  {"36", "295250000"},
  {"37", "301250000"},
  {"38", "307250000"},
  {"39", "313250000"},
  {"40", "319250000"},
  {"41", "325250000"},
  {"42", "331250000"},
  {"43", "337250000"},
  {"44", "343250000"},
  {"45", "349250000"},
  {"46", "355250000"},
  {"47", "361250000"},
  {"48", "367250000"},
  {"49", "373250000"},
  {"50", "379250000"},
  {"51", "385250000"},
  {"52", "391250000"},
  {"53", "397250000"},
  {"54", "403250000"},
  {"55", "409250000"},
  {"56", "415250000"},
  {"57", "421250000"},
  {"58", "427250000"},
  {"59", "433250000"},
  {"60", "439250000"},
  {"61", "445250000"},
  {"62", "451250000"},
  {"63", "457250000"},
  {"64", "463250000"},
  {"65", "469250000"},
  {"66", "475250000"},
  {"67", "481250000"},
  {"68", "487250000"},
  {"69", "493250000"},
  {"70", "499250000"},
  {"71", "505250000"},
  {"72", "511250000"},
  {"73", "517250000"},
  {"74", "523250000"},
  {"75", "529250000"},
  {"76", "535250000"},
  {"77", "541250000"},
  {"78", "547250000"},
  {"79", "553250000"},
  {"80", "559250000"},
  {"81", "565250000"},
  {"82", "571250000"},
  {"83", "577250000"},
};

static int NTSC_Cable_count = table_size(NTSC_Cable_channels);

static struct {
  const char *map_name;
  Channel *channel_map;
  int *channel_map_size;	/* I have to use a pointer here, because the counts aren't constant at compile time. */
} all_maps[] = {
  { .map_name = "NTSC_Cable", .channel_map = NTSC_Cable_channels, .channel_map_size = &NTSC_Cable_count },
};

const char *ChannelMaps_channel_to_frequency(const char *map, const char *channel)
{
  int i, j;
  int num_tables = table_size(all_maps);
  printf("num_tables=%d, %s %s\n", num_tables, map, channel);
  const char *frequency_HZ = 0L;

  for (i=0; i < num_tables; i++) {
    if (streq(map, all_maps[i].map_name)) {
      int n = *(all_maps[i].channel_map_size);
      for (j=0; j < n; j++) {
	if (streq(channel, all_maps[i].channel_map[j].channel)) {
	  frequency_HZ = all_maps[i].channel_map[j].frequency_HZ;
	  goto out;
	}
      }
      break;
    }
  }

 out:
  return frequency_HZ;
}