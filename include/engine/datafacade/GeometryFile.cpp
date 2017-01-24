//
//  GeometryFile.cpp
//  Ultimate Rides
//
//  Created by Ray Hunter on 31/07/2015.
//  Copyright (c) 2015 Atomic Rabbit Ltd. All rights reserved.
//

#include "GeometryFile.h"

#include <fstream>
#include <iostream>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/filesystem.hpp>
#include <vector>

using namespace boost::filesystem;


GeometryFile::GeometryFile( const string &path ) : m_path( path )
{

}


bool GeometryFile::LoadGeometryFile( )
{
    m_geometryStream.open( m_path );
    
    m_geometryStream.read((char *)&(startOffset), sizeof(startOffset));
    m_geometryStream.read((char *)&(indicesCount), sizeof(indicesCount));
    
    m_indices_start = sizeof(startOffset) + sizeof(indicesCount);
    m_compressed_geometry_start = m_indices_start + (indicesCount * sizeof(unsigned)) + sizeof( unsigned );
    
    m_geometryStream.seekg( m_compressed_geometry_start - sizeof( unsigned ) );
    m_geometryStream.read((char *)&(number_of_compressed_geometries), sizeof(number_of_compressed_geometries));
    
    return true;
}


bool GeometryFile::CanResolveGeometry( EdgeID geomId )
{
    bool canResolve = ( startOffset <= geomId && geomId < startOffset + indicesCount );
    return canResolve;
}


void GeometryFile::GetUncompressedForwardGeometry(const EdgeID geomId, std::vector<NodeID> &result_nodes)
{
    assert ( startOffset <= geomId && geomId < startOffset + indicesCount );
    
    EdgeID localGeomId = geomId - startOffset;
    
    if (!m_geometryStream.good())
    {
        m_geometryStream.clear(std::ios::goodbit);
        cout << "Resetting stale filestream\n";
    }

    m_geometryStream.seekg( m_indices_start + ( sizeof( unsigned ) * localGeomId ) );
    unsigned indexPair[2];
    m_geometryStream.read((char *)&(indexPair), sizeof(indexPair) );
    
    const unsigned begin = indexPair[0];
    const unsigned end = indexPair[1];
    const unsigned length = end - begin;
    
    result_nodes.clear();
    
    m_geometryStream.seekg( m_compressed_geometry_start + ( sizeof( unsigned ) * begin ) );
    result_nodes.resize( length );
    m_geometryStream.read((char *)&(result_nodes[0]), length * sizeof(unsigned) );    
}


void GeometryFile::GetUncompressedReverseGeometry(const EdgeID id, std::vector<NodeID> &result_nodes)
{
    GetUncompressedForwardGeometry( id, result_nodes );
    std::reverse(result_nodes.begin(), result_nodes.end());
}
