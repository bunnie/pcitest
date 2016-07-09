/*
 * pcimem.c: Simple program to read/write from/to a pci device from userspace.
 *
 *  Copyright (C) 2010, Bill Farrow (bfarrow@beyondelectronics.us)
 *
 *  Based on the devmem2.c code
 *  Copyright (C) 2000, Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/times.h>

#define PRINT_ERROR \
	do { \
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
		__LINE__, __FILE__, errno, strerror(errno)); exit(1); \
	} while(0)

#define MAP_SIZE (512 * 1024 * 1024)
#define MAP_MASK (MAP_SIZE - 1)

struct tms *tms_buf;

void fill_random(uint32_t *buf, uint32_t len) {
  int i;

  for( i = 0; i < len; i++ ) {
    buf[i] = random();
  }
}

static char state1[32];

int main(int argc, char **argv) {
  int fd;
  void *map_base, *virt_addr;
  char *filename;
  off_t target = 0;
  uint32_t *shadow_ram;
  uint32_t *local_ram;
  clock_t lastime;
  double elapsed;

  //  initstate(time(NULL), state1, sizeof(state1));
  //  setstate(state1);
  
  shadow_ram = (uint32_t *) malloc(MAP_SIZE);
  if( shadow_ram == NULL ) {
    printf ("can't allocate shadow ram \n");
    return 1;
  }
  local_ram = (uint32_t *) malloc(MAP_SIZE);
  if( local_ram == NULL ) {
    printf( "can't allocate local ram\n");
    return 1;
  }
  
  if(argc < 2) {
    // pcimem /sys/bus/pci/devices/0001\:00\:07.0/resource0 0x100 w 0x00
    // argv[0]  [1]                                         [2]   [3] [4]
    fprintf(stderr, "\nUsage:\t%s { sys file } [ offset ]\n"
	    "\tsys file: sysfs file for the pci resource to act on\n"
	    "\toffset  : offset into pci memory region to act upon\n", 
	    argv[0]);
    exit(1);
  }
  filename = argv[1];
  if( argc > 2 )
    target = strtoul(argv[2], 0, 0);
  
  if((fd = open(filename, O_RDWR | O_SYNC)) == -1) PRINT_ERROR;

  map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_32BIT, fd, target & ~MAP_MASK);
  if(map_base == (void *) -1) PRINT_ERROR;
  printf("PCI Memory mapped to address 0x%08x.\n", map_base);
  fflush(stdout);

  virt_addr = map_base + (target & MAP_MASK);

  int i, errors;
  int cmp;
  lastime = times(tms_buf);
  i = 0;
  errors = 0;
  //  for( i = 0; i < 10; i++ ) {
  while(1) {
    printf( "iteration: %d, errors: %d\n", i, errors);
    fill_random(shadow_ram, MAP_SIZE / sizeof(uint32_t));
    lastime = times(tms_buf);
    memcpy( local_ram, virt_addr, MAP_SIZE );  // copy first to local
    elapsed = ((double) times(tms_buf) - (double) lastime) / (double) sysconf(_SC_CLK_TCK);
    printf( "PCI->local RAM elapsed: %lfs, bandwidth: %lfMiB\n", elapsed, ((double)MAP_SIZE / elapsed) / (1024 * 1024) );
    printf( "Initial compare: %d\n", memcmp( shadow_ram, local_ram, MAP_SIZE ) );
    
    lastime = times(tms_buf);
    memcpy( virt_addr, shadow_ram, MAP_SIZE );  // copy shadow to PCIe
    elapsed = ((double) times(tms_buf) - (double) lastime) / (double) sysconf(_SC_CLK_TCK);
    printf( "local RAM->PCI elapsed: %lfs, bandwidth: %lfMiB\n", elapsed, ((double)MAP_SIZE / elapsed) / (1024 * 1024) );
    
    memcpy( local_ram, virt_addr, MAP_SIZE );  // copy first to local
    cmp = memcmp( shadow_ram, local_ram, MAP_SIZE );
    printf( "After-copy compare: %d\n", cmp );
    if( cmp != 0 ) {
      printf( "****ERROR FOUND****\n" );
      errors++;
    }
    i++;
  }
    
  if(munmap(map_base, MAP_SIZE) == -1) PRINT_ERROR;
  free(shadow_ram);
  free(local_ram);
  close(fd);
  return 0;
}
