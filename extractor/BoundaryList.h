//
//  BoundaryList.h
//  OSRM
//
//  Created by Ray Hunter on 12/08/2015.
//
//

#ifndef __OSRM__BoundaryList__
#define __OSRM__BoundaryList__

#include <stdio.h>
#include <fstream>
#include <iostream>
#include "Boundary.h"
#include <memory>
#include "osrm/coordinate.hpp"

class BoundaryList
{
public:
    void ReadDensityTree( std::ifstream &densityIn );
    std::shared_ptr< Boundary > SmallestBoundaryForFixedPointCoordinate( const FixedPointCoordinate &fpc ) const;
    
private:
    std::shared_ptr< Boundary > SmallestBoundaryForCoordinate( const struct coord &c ) const;
    std::vector< std::shared_ptr< Boundary > > countries;
};

#endif /* defined(__OSRM__BoundaryList__) */
