//
//  urt_datatypes.hpp
//  Ultimate Rides
//
//  Created by Ray Hunter on 22/01/2017.
//  Copyright © 2017 Atomic Rabbit Ltd. All rights reserved.
//

#ifndef urt_datatypes_h
#define urt_datatypes_h

//
//  Routing had failed because we've segmented the data and one of the segments is missing.
//
#define ROUTING_FAILED_SEGMENTATION "ROUTING_FAILED_SEGMENTATION"


struct EdgeArrayEntryApp
{
    EdgeArrayEntryApp(){}
    NodeID target;
    int weight : 29;
    bool shortcut : 1;
    bool forward : 1;
    bool backward : 1;
    union
    {
        NodeID middleNodeId : 31;  // If shortcut
        GeometryID geometryId;     // If not shortcut
    };
};

typedef std::vector<EdgeArrayEntryApp> EdgeArray;

#endif /* urt_datatypes_h */
