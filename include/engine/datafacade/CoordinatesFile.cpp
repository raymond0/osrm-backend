//
//  CoordinatesFile.cpp
//  Ultimate Rides
//
//  Created by Ray Hunter on 31/07/2015.
//  Copyright (c) 2015 Atomic Rabbit Ltd. All rights reserved.
//

#include "CoordinatesFile.h"

CoordinatesFile::CoordinatesFile( const string &path ) : m_path( path ), m_short_nodes_input_stream( NULL )
{
    
}


CoordinatesFile::~CoordinatesFile()
{
    if ( m_short_nodes_input_stream != NULL )
    {
        m_short_nodes_input_stream->close();
        delete m_short_nodes_input_stream;
    }
}


bool CoordinatesFile::LoadCoordinatesFile()
{
    m_short_nodes_input_stream = new ifstream(m_path, std::ios::binary);
    
    m_short_nodes_input_stream->read((char *)&startOffset, sizeof(unsigned));
    m_short_nodes_input_stream->read((char *)&numberOfCoordinates, sizeof(unsigned));
    
    return true;
}


bool CoordinatesFile::CanResolveNode( NodeID nodeId )
{
    return startOffset <= nodeId && nodeId < startOffset + numberOfCoordinates;
}


osrm::util::Coordinate CoordinatesFile::GetNodeCoords ( NodeID nodeId )
{
    assert( startOffset <= nodeId && nodeId < startOffset + numberOfCoordinates );
    
    nodeId -= startOffset;
    
    if (!m_short_nodes_input_stream->good())
    {
        m_short_nodes_input_stream->clear(std::ios::goodbit);
        cout << "Resetting stale filestream\n";
    }
    
    osrm::util::Coordinate fpc;
    m_short_nodes_input_stream->seekg( ( sizeof(unsigned) * 2 ) + (sizeof(osrm::util::Coordinate) * nodeId));
    m_short_nodes_input_stream->read((char *)&(fpc), sizeof(osrm::util::Coordinate));
    
    return fpc;
}

