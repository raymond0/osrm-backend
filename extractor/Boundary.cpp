//
//  Boundary.cpp
//  OSRM
//
//  Created by Ray Hunter on 12/08/2015.
//
//

#include "Boundary.h"

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
    
    bool first = true;
    
    for ( int i = 0; i < nrOuterWays; i++ )
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
        
        for ( int j = 0; j < coordCount; j++ )
        {
            struct coord newCoord;
            densityIn.read((char *) &newCoord.x, sizeof(newCoord.x));
            densityIn.read((char *) &newCoord.y, sizeof(newCoord.y));
            
            outerWay.emplace_back(newCoord);
            
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
        
        outerWays.emplace_back( outerWay );
    }
    
    for ( int i = 0; i < nrChildBoundaries; i++ )
    {
        std::shared_ptr<Boundary> childBoundary ( new Boundary( densityIn ) );

        childBoundaries.emplace_back(childBoundary);
    }
}

int
bbox_contains_coord(struct rect *r, const struct coord *c)
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


bool
coord_equal( const coord &a, const coord &b)
{
    return (a.x == b.x && a.y == b.y);
}


int
geom_poly_point_inside(struct coord *cp, int count, const struct coord *c)
{
    int ret=0;
    struct coord *last=cp+count-1;
    while (cp < last)
    {
        if ((cp[0].y > c->y) != (cp[1].y > c->y) &&
            c->x < ( (long long) cp[1].x - cp[0].x ) * ( c->y -cp[0].y ) / ( cp[1].y - cp[0].y ) + cp[0].x )
        {
            ret=!ret;
        }
        cp++;
    }
    return ret;
}


bool Boundary::ContainsCoord( const struct coord *c )
{
    if ( ! bbox_contains_coord(&enclosingRect, c) )
        return false;
    
    for ( auto outerWay : outerWays )
    {
        for ( auto outerCoord : outerWay )
        {
            if ( coord_equal( outerCoord, *c ) )
                return true;
        }
        
        if ( geom_poly_point_inside(&outerWay[0], outerWay.size(), c) )
        {
            return true;
        }
    }
    
    return false;
}


std::shared_ptr< Boundary > Boundary::SmallestBoundaryForCoordinate( const struct coord &c )
{
    if ( ! ContainsCoord( &c ) )
    {
        return nullptr;
    }
    
    std::shared_ptr< Boundary > smallestBoundary = nullptr;
    long long smallestArea = LLONG_MAX;
    
    for ( auto childBoundary : childBoundaries )
    {
        auto childSmallest = ( childBoundary->SmallestBoundaryForCoordinate( c ) );
        
        if( childSmallest == nullptr )
            continue;
        
        auto childArea = childSmallest->totalArea;
        if ( childArea < smallestArea )
        {
            smallestBoundary = childSmallest;
            smallestArea = childArea;
        }
    }
    
    if ( smallestBoundary != nullptr )
        return smallestBoundary;
    
    return shared_from_this();
}


double Boundary::Density()
{
    double density = ((double) roadStartsInBoundary) / ((double) totalArea);
    return density;
}


bool Boundary::IsProbablyOutOfTown()
{
    double density = Density();
    // < 4.somehing-e06
    return density <= 0.000004;
}
