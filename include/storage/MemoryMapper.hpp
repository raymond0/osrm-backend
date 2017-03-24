//
//  MemoryMapper.hpp
//  Ultimate Rides
//
//  Created by Ray Hunter on 16/03/2015.
//  Copyright (c) 2015 Atomic Rabbit Ltd. All rights reserved.
//

#ifndef Ultimate_Rides_MemoryMapper_hpp
#define Ultimate_Rides_MemoryMapper_hpp

#include <stdio.h>

struct memory_mapped_info
{
    int mmfile;
    char *mmptr;
    size_t len;
};


bool MemoryMap( const char *filename, size_t size, struct memory_mapped_info &mminfo );
void MemoryUnmap( struct memory_mapped_info &mminfo );

#endif
