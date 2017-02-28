//
//  Boundary.cpp
//  OSRM
//
//  Created by Ray Hunter on 12/08/2015.
//
//

#include "tbb/tbb.h"
#include "extractor/Boundary.h"
#include <limits.h>

void
coord_extend_bbox(struct rect *dest, struct coord &additional)
{
    if (additional.x < dest->l.x)
        dest->l.x = additional.x;
    if (additional.x > dest->h.x)
        dest->h.x = additional.x;
    if (additional.y > dest->h.y)
        dest->h.y = additional.y;
    if (additional.y < dest->l.y)
        dest->l.y = additional.y;
}



Boundary::Boundary( std::ifstream &densityIn )
{
    unsigned header = 0;
    unsigned nrOuterWays = 0;
    unsigned nrChildBoundaries = 0;
    
    densityIn.read((char *) &header, sizeof(header));
    
    if ( header != 0xE9E9E9E9 )
    {
        printf( "ERROR: Boundary header was incorrect\n" );
        exit( 1 );
    }
    
    densityIn.read((char *) &nrOuterWays, sizeof(nrOuterWays));
    densityIn.read((char *) &nrChildBoundaries, sizeof(nrChildBoundaries));
    densityIn.read((char *) &totalArea, sizeof(totalArea));
    totalArea = llabs(totalArea);
    densityIn.read((char *) &roadStartsInBoundary, sizeof(roadStartsInBoundary));
    size_t strLen = 0;
    densityIn.read((char *) &strLen, sizeof(strLen));
    if ( strLen > 0 )
    {
        if ( strLen > 99 )
        {
            printf( "ERROR: iso code was over 99 bytes!!!\n" );
            exit( 1 );
        }
        
        char isoBuf[100];
        densityIn.read(isoBuf, strLen);
        isoBuf[strLen] = '\0';
        isoCode = std::string(isoBuf);
        
        //printf( "Read ISO code of length %ld: %s\n", strLen, isoBuf );
    }
    
    bool first = true;
    
    for ( unsigned i = 0; i < nrOuterWays; i++ )
    {
        unsigned wayHeader = 0;
        unsigned coordCount = 0;
        densityIn.read((char *) &wayHeader, sizeof(wayHeader));

        if ( wayHeader != 0xE8E8E8E8 )
        {
            printf( "ERROR: Coord header was incorrect\n" );
            exit( 1 );
        }

        densityIn.read((char *) &coordCount, sizeof(coordCount));
        
        std::vector<struct coord> outerWay;
        
        struct rect outerWayEnclosingRect;
        
        if ( coordCount < 3 )
        {
            printf( "ERROR: Boundary had %d coordinates\n", coordCount );
            continue;
        }
        
        for ( unsigned j = 0; j < coordCount; j++ )
        {
            struct coord newCoord;
            densityIn.read((char *) &newCoord.x, sizeof(newCoord.x));
            densityIn.read((char *) &newCoord.y, sizeof(newCoord.y));
            
            outerWay.emplace_back(newCoord);
            
            if ( j == 0 )
            {
                outerWayEnclosingRect.l.x = outerWayEnclosingRect.h.x = newCoord.x;
                outerWayEnclosingRect.l.y = outerWayEnclosingRect.h.y = newCoord.y;
            }
            else
            {
                coord_extend_bbox(&outerWayEnclosingRect, newCoord);
            }
            
            if ( first )
            {
                enclosingRect.l.x = enclosingRect.h.x = newCoord.x;
                enclosingRect.l.y = enclosingRect.h.y = newCoord.y;
                first = false;
            }
            else
            {
                coord_extend_bbox(&enclosingRect, newCoord);
            }
        }
        
        outerWays.emplace_back( OuterWay( outerWayEnclosingRect, outerWay ) );
    }
    
    for ( unsigned i = 0; i < nrChildBoundaries; i++ )
    {
        std::shared_ptr<Boundary> childBoundary ( new Boundary( densityIn ) );

        childBoundaries.emplace_back(childBoundary);
    }
}

int
bbox_contains_coord(const struct rect *r, const struct coord *c)
{
    if (r->h.x < c->x)
        return 0;
    if (r->l.x > c->x)
        return 0;
    if (r->h.y < c->y)
        return 0;
    if (r->l.y > c->y)
        return 0;
    return 1;
}


inline bool
coord_equal( const coord &a, const coord &b)
{
    return (a.x == b.x && a.y == b.y);
}


int
geom_poly_point_inside(const struct coord *coords, int count, const struct coord *c)
{
    if ( count < 3 )
    {
        return 0;
    }
    
    int ret=0;
    const struct coord *cp = coords;
    const struct coord *last=cp+count-1;
    while (cp < last)
    {
        if ((cp[0].y > c->y) != (cp[1].y > c->y) &&
            c->x < ( (long long) cp[1].x - cp[0].x ) * ( c->y -cp[0].y ) / ( cp[1].y - cp[0].y ) + cp[0].x )
        {
            ret=!ret;
        }
        cp++;
    }
    
    int lastIndex = count - 1;
    if ( coords[0].x != coords[lastIndex].x || coords[0].y != coords[lastIndex].y )
    {
        if ((coords[lastIndex].y > c->y) != (coords[0].y > c->y) &&
            c->x < ( (long long) coords[0].x - coords[lastIndex].x ) * ( c->y -coords[lastIndex].y ) / ( coords[0].y - coords[lastIndex].y ) + coords[lastIndex].x )
        {
            ret=!ret;
        }
    }
    
    return ret;
}


bool Boundary::ContainsCoord( const struct coord *c )
{
    if ( ! bbox_contains_coord(&enclosingRect, c) )
        return false;
    
    for ( const auto &outerWay : outerWays )
    {
        if ( ! bbox_contains_coord(&outerWay.first, c) )
        {
            continue;
        }

        for ( const auto &outerCoord : outerWay.second )
        {
            if ( coord_equal( outerCoord, *c ) )
                return true;
        }
        
        if ( geom_poly_point_inside(&outerWay.second[0], outerWay.second.size(), c) )
        {
            return true;
        }
    }
    
    return false;
}


bool Boundary::CoordinateIsInTown( const struct coord &c, const double townDensity )
{
    if ( ! ContainsCoord( &c ) )
    {
        return false;
    }
    
    if ( Density() >= townDensity )
    {
        return true;
    }
    
    for ( const auto &childBoundary : childBoundaries )
    {
        if ( childBoundary->CoordinateIsInTown( c, townDensity ) )
        {
            return true;
        }
    }
    
    return false;
}



double Boundary::Density()
{
    double density = ((double) roadStartsInBoundary) / ((double) totalArea);
    return density;
}

