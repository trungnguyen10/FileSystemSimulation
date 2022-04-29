//
// Software disk exerciser for LSU 4103 filesystem assignment.
// Written by Golden G. Richard III (@nolaforensix), 10/2017.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "softwaredisk.h"

char poetry[] = "Do not go gentle into that good night,"
                "Old age should burn and rave at close of day;"
                "Rage, rage against the dying of the light."
                ""
                "Though wise men at their end know dark is right,"
                "Because their words had forked no lightning they"
                "Do not go gentle into that good night."
                ""
                "Good men, the last wave by, crying how bright"
                "Their frail deeds might have danced in a green bay,"
                "Rage, rage against the dying of the light."
                ""
                "Wild men who caught and sang the sun in flight,"
                "And learn, too late, they grieved it on its way,"
                "Do not go gentle into that good night."
                ""
                "Grave men, near death, who see with blinding sight"
                "Blind eyes could blaze like meteors and be gay,"
                "Rage, rage against the dying of the light."
                ""
                "And you, my father, there on the sad height,"
                "Curse, bless, me now with your fierce tears, I pray."
                "Do not go gentle into that good night."
                "Rage, rage against the dying of the light.";

int main(int argc, char *argv[])
{
  char buf[SOFTWARE_DISK_BLOCK_SIZE]; // 512
  int i, j, ret;

  init_software_disk();
  printf("Size of software disk in blocks: %lu\n", software_disk_size());
  sd_print_error();
  printf("Writing a block of A's to block # 3.\n");
  memset(buf, 'A', SOFTWARE_DISK_BLOCK_SIZE);
  ret = write_sd_block(buf, 3);
  printf("Return value was %d.\n", ret);
  sd_print_error();
  printf("Reading block # 3.\n");
  bzero(buf, SOFTWARE_DISK_BLOCK_SIZE);
  ret = read_sd_block(buf, 3);
  printf("Return value was %d.\n", ret);
  sd_print_error();
  printf("Contents of block # 3:\n");
  for (j = 0; j < SOFTWARE_DISK_BLOCK_SIZE; j++)
  {
    printf("%c", buf[j]);
  }
  printf("\n");
  printf("Reading block # 7.\n");
  bzero(buf, SOFTWARE_DISK_BLOCK_SIZE);
  ret = read_sd_block(buf, 7);
  printf("Return value was %d.\n", ret);
  sd_print_error();
  printf("Contents of block # 7:\n");
  for (j = 0; j < SOFTWARE_DISK_BLOCK_SIZE; j++)
  {
    printf("%c", buf[j]);
  }
  printf("\n");
  printf("Trying to write a block of B's to block # %lu.\n", software_disk_size());
  memset(buf, 'B', SOFTWARE_DISK_BLOCK_SIZE);
  ret = write_sd_block(buf, software_disk_size());
  printf("Return value was %d.\n", ret);
  sd_print_error();
  printf("Writing some poetry to blocks #5-7.\n");
  for (i = 5; i <= 7; i++)
  {
    ret = write_sd_block(poetry + (i - 5) * 10, (unsigned long)i);
    printf("Return value was %d.\n", ret);
    sd_print_error();
  }
  printf("Reading blocks # 5-7.\n");
  for (i = 5; i <= 7; i++)
  {
    bzero(buf, SOFTWARE_DISK_BLOCK_SIZE);
    ret = read_sd_block(buf, (unsigned long)i);
    printf("Return value was %d.\n", ret);
    sd_print_error();
    printf("Contents of block # %d:\n", i);
    for (j = 0; j < SOFTWARE_DISK_BLOCK_SIZE; j++)
    {
      printf("%c", buf[j]);
    }
    printf("\n");
  }
}
