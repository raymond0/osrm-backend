//
//  GeometryFile.h
//  Ultimate Rides
//
//  Created by Ray Hunter on 31/07/2015.
//  Copyright (c) 2015 Atomic Rabbit Ltd. All rights reserved.
//

#ifndef __Ultimate_Rides__GeometryFile__
#define __Ultimate_Rides__GeometryFile__

#include <stdio.h>
#include <string>
#include <fstream>
#include <vector>
#include "util/typedefs.hpp"

using namespace std;

class GeometryFile
{
public:
    GeometryFile( const string &path );
    bool LoadGeometryFile( );
    
    bool CanResolveGeometry( EdgeID geomId );
    void GetUncompressedForwardGeometry(const EdgeID id, std::vector<NodeID> &result_nodes);
    void GetUncompressedReverseGeometry(const EdgeID id, std::vector<NodeID> &result_nodes);

private:
    string m_path;
    EdgeID startOffset;
    unsigned int indicesCount;
    
    ifstream m_geometryStream;
    size_t m_indices_start;
    size_t m_compressed_geometry_start;
    unsigned number_of_compressed_geometries;
};

#endif /* defined(__Ultimate_Rides__GeometryFile__) */
