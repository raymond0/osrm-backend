//
//  MemoryMapper.cpp
//  Ultimate Rides
//
//  Created by Ray Hunter on 16/03/2015.
//  Copyright (c) 2015 Atomic Rabbit Ltd. All rights reserved.
//

#include "storage/MemoryMapper.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <string.h>
#include <unistd.h>

bool MemoryMap( const char *filename, size_t size, struct memory_mapped_info &mminfo )
{
    int mapped_file = open( filename, O_RDONLY );
    
    if ( mapped_file == -1 )
    {
        char *serr = strerror( errno );
        printf( "memory map file open error: %s\n", serr );
        return false;
    }
    
    char *map_ptr = ( char * ) mmap(0,
                                    size,
                                    PROT_READ,
                                    MAP_PRIVATE | MAP_FILE,
                                    mapped_file,
                                    0);
    
    if ( map_ptr == MAP_FAILED || map_ptr == NULL )
    {
        close( mapped_file );
        char *serr = strerror( errno );
        printf( "mmap error: %s\n", serr );
        exit( EXIT_FAILURE );
    }
    
    mminfo.mmfile = mapped_file;
    mminfo.mmptr = map_ptr;
    mminfo.len = size;

    return true;
}


void MemoryUnmap( struct memory_mapped_info &mminfo )
{
    if ( munmap( mminfo.mmptr, mminfo.len ) == -1 )
    {
        char *serr = strerror( errno );
        printf( "memory unmap error: %s\n", serr );
    }
    
    if ( close( mminfo.mmfile ) == -1 )
    {
        char *serr = strerror( errno );
        printf( "memory unmap file close error: %s\n", serr );
    }
}
