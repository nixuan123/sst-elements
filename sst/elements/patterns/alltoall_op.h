//
// Copyright (c) 2011, IBM Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _ALLTOALL_OP_H
#define _ALLTOALL_OP_H

#include "patterns.h"
#include "state_machine.h"
#include "comm_pattern.h"



class Alltoall_op   {
    public:
	Alltoall_op(Comm_pattern * const& current_pattern, int msglen) :
	    cp(current_pattern),
	    alltoall_msglen(msglen)
	{
	    // Do some initializations
	    done= false;
	    state= START;
	    alltoall_nranks= cp->num_ranks;
	    receives= 0;
	}

        ~Alltoall_op() {}

	void resize(int new_size)
	{
	    alltoall_nranks= new_size;
	}

	uint32_t install_handler(void)
	{
	    return cp->SM->SM_create((void *)this, Alltoall_op::wrapper_handle_events);
	}

	// The Alltoall pattern generator can be in these states and deals
	// with these events.
	typedef enum {START, MAIN_LOOP, SEND, REMAINDER, WAIT} alltoall_state_t;

	// The start event should always be SM_START_EVENT
	typedef enum {E_START= SM_START_EVENT, E_NEXT_LOOP, E_INITIAL_DATA,
	    E_SEND_START, E_SEND_DONE, E_REMAINDER_START,
	    E_LAST_DATA, E_ALL_DATA} alltoall_events_t;



    private:
	// Wrapping a pointer to a non-static member function like this is from
	// http://www.newty.de/fpt/callback.html
	void handle_events(state_event sst_event);
	static void wrapper_handle_events(void *obj, state_event sst_event)
	{
	    Alltoall_op* mySelf = (Alltoall_op*) obj;
	    mySelf->handle_events(sst_event);
	}

	// We need to remember how to upcall into our parent object
	Comm_pattern *cp;

	// Simulated message size and communicator size
	int alltoall_msglen;
	int alltoall_nranks;

	alltoall_state_t state;
	int done;
	unsigned int i;
	int shift;
	int receives;
	long long bytes_sent;
	bool remainder_done;

	void state_INIT(state_event event);
	void state_MAIN_LOOP(state_event event);
	void state_SEND(state_event event);
	void state_REMAINDER(state_event event);
	void state_WAIT(state_event event);

        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
	    ar & BOOST_SERIALIZATION_NVP(cp);
	    ar & BOOST_SERIALIZATION_NVP(alltoall_msglen);
	    ar & BOOST_SERIALIZATION_NVP(alltoall_nranks);
	    ar & BOOST_SERIALIZATION_NVP(state);
	    ar & BOOST_SERIALIZATION_NVP(done);
	    ar & BOOST_SERIALIZATION_NVP(i);
	    ar & BOOST_SERIALIZATION_NVP(shift);
	    ar & BOOST_SERIALIZATION_NVP(receives);
	    ar & BOOST_SERIALIZATION_NVP(bytes_sent);
	    ar & BOOST_SERIALIZATION_NVP(remainder_done);
        }

};

#endif // _ALLTOALL_OP_H
