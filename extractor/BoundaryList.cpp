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
#include <limits.h>


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


typedef std::pair<std::string, double> DensityPair;

std::vector <DensityPair> countryDensities = {
    { "NL", 0.0000127158 },
    { "BE", 0.0000080000 },
    { "DE", 0.0000080000 },
    { "JP", 0.0000140105 },
    { "RU", 0.0000042000 }
};

const double defaultDensity = 0.0000058887;

double DensityForIsoCode( std::string &isoCode )
{
    if ( isoCode.length() == 0 )
    {
        printf( "Country ISO code missing...\n" );
        return defaultDensity;
    }
    
    for ( auto pair : countryDensities )
    {
        if ( pair.first.compare(isoCode) == 0 )
        {
            printf( "Country ISO code %s has density %f\n", isoCode.c_str(), pair.second );
            return pair.second;
        }
    }

    printf( "Country ISO code %s has default density %f\n", isoCode.c_str(), defaultDensity );
    return defaultDensity;
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
    
    for ( auto country : countries )
    {
        country->targetDensityForCountry = DensityForIsoCode( country->isoCode );
    }
}



bool BoundaryList::FixedPointCoordinateIsInTown( const FixedPointCoordinate &fpc ) const
{
    struct coord c = coordFromDecimal( fpc );
    
    for ( auto countryBoundary : countries )
    {
        if ( countryBoundary->CoordinateIsInTown(c, countryBoundary->targetDensityForCountry) )
        {
            return true;
        }
    }
    
    return false;
}
