//
//  Boundary.h
//  OSRM
//
//  Created by Ray Hunter on 12/08/2015.
//
//

#ifndef __OSRM__Boundary__
#define __OSRM__Boundary__

#include <stdio.h>
#include <memory>
#include <vector>
#include <fstream>
#include <iostream>

struct coord {
    int x; /*!< X-Value */
    int y; /*!< Y-Value */
};

struct rect {struct coord l,h;};

class Boundary : public std::enable_shared_from_this<Boundary>
{
public:
    Boundary( std::ifstream &densityIn );
    bool ContainsCoord( const struct coord *c );
    bool CoordinateIsInTown( const struct coord &c, const double townDensity );
    long long totalArea;
    unsigned roadStartsInBoundary;
    double Density();
    std::string isoCode;
    double targetDensityForCountry;
    
private:
    struct rect enclosingRect;
    typedef std::pair<struct rect, std::vector<struct coord> > OuterWay;
    std::vector<OuterWay> outerWays;
    std::vector< std::shared_ptr< Boundary > > childBoundaries;
};


std::ostream& operator >> (std::ostream& os, const Boundary& obj);

#endif /* defined(__OSRM__Boundary__) */