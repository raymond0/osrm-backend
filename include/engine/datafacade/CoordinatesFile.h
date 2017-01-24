//
//  CoordinatesFile.h
//  Ultimate Rides
//
//  Created by Ray Hunter on 31/07/2015.
//  Copyright (c) 2015 Atomic Rabbit Ltd. All rights reserved.
//

#ifndef __Ultimate_Rides__CoordinatesFile__
#define __Ultimate_Rides__CoordinatesFile__

#include <stdio.h>
#include <string>
#include "util/typedefs.hpp"
#include "util/coordinate.hpp"

#include <fstream>
#include <iostream>



using namespace std;

class CoordinatesFile
{
public:
    CoordinatesFile( const string &path );
    ~CoordinatesFile();
    bool LoadCoordinatesFile( );
    
    bool CanResolveNode( unsigned geomId );
    osrm::util::Coordinate GetNodeCoords ( NodeID geomId );
    
    
private:
    ifstream *m_short_nodes_input_stream;
    string m_path;
    unsigned int startOffset;
    unsigned int numberOfCoordinates;
};

#endif /* defined(__Ultimate_Rides__CoordinatesFile__) */
