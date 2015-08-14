//
//  BoundaryList.cpp
//  OSRM
//
//  Created by Ray Hunter on 12/08/2015.
//
//

#include "BoundaryList.h"
#include <iostream>
#include <math.h>


static coord coordFromDecimal( FixedPointCoordinate fpc )
{
    double convertedLong = (((double)fpc.lon) / COORDINATE_PRECISION) * 6371000.0 * M_PI / 180;
    double convertedLat = log( tan( M_PI_4 + (((double)fpc.lat) / COORDINATE_PRECISION) * M_PI / 360 ) ) * 6371000.0;
    
    coord c;
    c.x = convertedLong;
    c.y = convertedLat;
    
    //cout << "Converted " << fpc << " to " << c.x << "," << c.y << "\n";
    
    return c;
}


void BoundaryList::ReadDensityTree( std::ifstream &densityIn )
{
    unsigned header = 0;
    unsigned nrCountries = 0;
    
    densityIn.read((char *) &header, sizeof(header));
    densityIn.read((char *) &nrCountries, sizeof(nrCountries));
    
    if ( header != 0xE0E0E0E0 )
    {
        printf( "ERROR: Density tree header was invalid" );
        exit( 1 );
    }
    
    for ( unsigned i = 0; i < nrCountries; i++ )
    {
        std::shared_ptr<Boundary> boundary ( new Boundary( densityIn ) );
        countries.emplace_back(boundary);
    }
}


std::shared_ptr< Boundary > BoundaryList::SmallestBoundaryForFixedPointCoordinate( const FixedPointCoordinate &fpc ) const
{
    struct coord c = coordFromDecimal( fpc );
    return SmallestBoundaryForCoordinate( c );
}


std::shared_ptr< Boundary > BoundaryList::SmallestBoundaryForCoordinate( const struct coord &c ) const
{
    std::shared_ptr< Boundary > smallestBoundary = nullptr;
    long long smallestArea = LLONG_MAX;
    
    for ( auto countryBoundary : countries )
    {
        auto countrySmallest = ( countryBoundary->SmallestBoundaryForCoordinate( c ) );
        
        if( countrySmallest == nullptr )
            continue;
        
        auto childArea = countrySmallest->totalArea;
        if ( childArea < smallestArea )
        {
            smallestBoundary = countrySmallest;
            smallestArea = childArea;
        }
    }
    
    return smallestBoundary;
    
}