
#ifdef HAVE_CONFIG_H
#include "thrucommon_config.h"
#endif

#include "ThruLogging.h"

#include "CircuitBreaker.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

CircuitBreaker::CircuitBreaker (uint16_t threshold,
                                uint16_t timeout_in_seconds)
{

    T_DEBUG("CircuitBreaker: threshold=%d, timeout_in_seconds=%d",
             threshold, timeout_in_seconds);


    this->threshold = threshold;
    this->timeout_in_seconds = timeout_in_seconds;

    this->reset ();
}

bool CircuitBreaker::allow ()
{
    T_DEBUG ("allow: ");
    // if timeout has elapsed give half open a try
    if (this->state == OPEN && this->next_check < time (0))
    {
        T_DEBUG ("allow:    going half-open");
        this->state = HALF_OPEN;
    }
    return this->state == CLOSED || this->state == HALF_OPEN;
}

void CircuitBreaker::success ()
{
    T_DEBUG ("success: ");
    // if we're half-open and got a success close things up, note that
    // we should never be called if we're in the fully open state, that'd be a
    // bug in the client code
    if (this->state == HALF_OPEN)
    {
        T_DEBUG ("success:    in half-open, reset");
        this->reset ();
    }
}

void CircuitBreaker::failure ()
{
    T_DEBUG( "failure: ");
    if (this->state == HALF_OPEN)
    {
        T_DEBUG("failure:    in half-open, trip");
        this->trip ();
    }
    else
    {
        ++failure_count;
        // if we've breached our threshold trip
        if (failure_count > this->threshold)
        {
            T_DEBUG( "failure:    threashold breached,  trip");
            this->trip ();
        }
    }
}

void CircuitBreaker::reset ()
{
    T_DEBUG ( "reset:");
    this->state = CLOSED;
    this->failure_count = 0;
}

void CircuitBreaker::trip ()
{
    T_DEBUG ( "trip:");
    if (this->state != OPEN)
    {
        T_DEBUG("trip: tripped");
        this->state = OPEN;
        this->next_check = time (0) + (time_t)timeout_in_seconds;
    }
}
