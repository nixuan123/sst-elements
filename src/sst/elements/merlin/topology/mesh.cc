// Copyright 2009-2023 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2023, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>
#include "mesh.h"

#include <algorithm>
#include <stdlib.h>



using namespace SST::Merlin;


topo_mesh::topo_mesh(ComponentId_t cid, Params& params, int num_ports, int rtr_id, int num_vns,int* hm_id) :
    Topology(cid),
    hm_id(hm_id),//新添一个hm_id，这样就可以唯一确定这个路由器在hm拓扑中的位置
    router_id(rtr_id),
    num_vns(num_vns)
{

    // Get the various parameters
    std::string shape;
    //获取mesh.shape的字符串，例如返回"4x4x2x2"
    shape = params.find<std::string>("shape");
    if ( !shape.compare("") ) {
    }

    // Need to parse the shape string to get the number of dimensions
    // and the size of each dimension
    dimensions = 2;

    dim_size = new int[dimensions];
    dim_width = new int[dimensions];
    port_start = new int[dimensions][2];
    //dimensions默认为4，若传入2x2x2x2，dim_size=[2,2,2,2]
    parseDimString(shape, dim_size);

    std::string width = params.find<std::string>("width", "");
    if ( width.compare("") == 0 ) {
        for ( int i = 0 ; i < dimensions ; i++ )
            dim_width[i] = 1;
    } else {
        parseDimString(width, dim_width);
    }

    int next_port = 0;
    for ( int d = 0 ; d < dimensions ; d++ ) {
        for ( int i = 0 ; i < 2 ; i++ ) {
            port_start[d][i] = next_port;
            next_port += dim_width[d];
        }
    }

    num_local_ports = params.find<int>("local_ports", 1);

    int needed_ports = 0;
    for ( int i = 0 ; i < dimensions ; i++ ) {
        needed_ports += 2 * dim_width[i];
    }


    if ( num_ports < (needed_ports+num_local_ports) ) {
        output.fatal(CALL_INFO, -1, "Number of ports should be at least %d for this configuration\n", needed_ports+num_local_ports);
    }

    local_port_start = needed_ports;// Local delivery is on the last ports
    //id_loc现在是一个有四个参数的数组，例如通过[1,1,1,0]确定他在hm拓扑中的唯一位置
    id_loc = new int[dimensions]; 
    //此函数新添一个hm_id参数,若hm_id=2,router_id=3,则id_loc=[1,1,0,1]
    idToLocation(hm_id, router_id, id_loc);
}

topo_mesh::~topo_mesh()
{
    delete [] id_loc;
    delete [] dim_size;
    delete [] dim_width;
    delete [] port_start;
}

//路由具体该怎么走
void
topo_mesh::route_packet(int port, int vc, internal_router_event* ev)
{
    int dest_router = get_dest_router(ev->getDest());
    if ( dest_router == router_id ) {
        ev->setNextPort(get_dest_local_port(ev->getDest()));
    } else {
        topo_mesh_event *tt_ev = static_cast<topo_mesh_event*>(ev);

        for ( int dim = tt_ev->routing_dim ; dim < dimensions ; dim++ ) {
            if ( tt_ev->dest_loc[dim] != id_loc[dim] ) {

                int go_pos = (id_loc[dim] < tt_ev->dest_loc[dim]);

                int p = choose_multipath(
                        port_start[dim][(go_pos) ? 0 : 1],
                        dim_width[dim],
                        abs(id_loc[dim] - tt_ev->dest_loc[dim]));

                tt_ev->setNextPort(p);

                if ( id_loc[dim] == 0 && port < local_port_start ) { // Crossing dateline
                    int new_vc = vc ^ 1;
                    tt_ev->setVC(new_vc); // Toggle VC
                    output.verbose(CALL_INFO, 1, 1, "Crossing dateline.  Changing from VC %d to %d\n", vc, new_vc);
                }

                break;

            } else {
                // Time to change direction
                tt_ev->routing_dim++;
                tt_ev->setVC(vc & (~1)); // Reset the VC
            }
        }
    }
}



internal_router_event*
topo_mesh::process_input(RtrEvent* ev)
{
    topo_mesh_event* tt_ev = new topo_mesh_event(dimensions);
    tt_ev->setEncapsulatedEvent(ev);
    tt_ev->setVC(tt_ev->getVN() * 2);
    
    // Need to figure out what the mesh address is for easier
    // routing.
    int run_id = get_dest_router(tt_ev->getDest());
    idToLocation(run_id, tt_ev->dest_loc);

	return tt_ev;
}


void topo_mesh::routeUntimedData(int port, internal_router_event* ev, std::vector<int> &outPorts)
{
    topo_mesh_init_event *tt_ev = static_cast<topo_mesh_init_event*>(ev);
    if ( tt_ev->phase == 0 ) {
        if ( (0 == router_id) && (ev->getDest() == UNTIMED_BROADCAST_ADDR) ) {
            /* Broadcast has arrived at 0.  Switch Phases */
            tt_ev->phase = 1;
        } else {
            route_packet(port, 0, ev);
            outPorts.push_back(ev->getNextPort());
            return;
        }
    }

    /*
     * Find dimension came in on
     * Send in positive direction in all dimensions that level and higher (unless at end)
     */
    int inc_dim = 0;
    if ( tt_ev->phase == 2 ) {
        for ( ; inc_dim < dimensions ; inc_dim++ ) {
            if ( port == port_start[inc_dim][1] ) {
                break;
            }
        }
    }

    tt_ev->phase = 2;

    for ( int dim = inc_dim ; dim < dimensions ; dim++ ) {
        if ( (id_loc[dim] + 1) < dim_size[dim] ) {
            outPorts.push_back(port_start[dim][0]);
        }
    }

    // Also, send to hosts
    for ( int p = 0 ; p < num_local_ports ; p++ ) {
        if ( (local_port_start + p) != port ) {
            outPorts.push_back(local_port_start +p);
        }
    }
}


internal_router_event* topo_mesh::process_UntimedData_input(RtrEvent* ev)
{
    topo_mesh_init_event* tt_ev = new topo_mesh_init_event(dimensions);
    tt_ev->setEncapsulatedEvent(ev);
    if ( tt_ev->getDest() == UNTIMED_BROADCAST_ADDR ) {
        /* For broadcast, first send to rtr 0 */
        idToLocation(0, tt_ev->dest_loc);
    } else {
        int rtr_id = get_dest_router(tt_ev->getDest());
        idToLocation(rtr_id, tt_ev->dest_loc);
    }
    return tt_ev;
}


Topology::PortState
topo_mesh::getPortState(int port) const
{
    if (port >= local_port_start) {
        if ( port < (local_port_start + num_local_ports) )
            return R2N;
        return UNCONNECTED;
    }

    //printf("id: %d.   Port Check %d\n", router_id, port);
    for ( int d = 0 ; d < dimensions ; d++ ) {
        if ( (port >= port_start[d][0] && (port < (port_start[d][0]+dim_width[d]))) ) {
            //printf("\tPort matches pos Dim: %d.  [%d, %d)\n", d, port_start[d][0], (port_start[d][0]+dim_width[d]));
            if ( id_loc[d] == (dim_size[d]-1) ) {
                //printf("\tReturning Unconnected\n");
                return UNCONNECTED;
            }
            return R2R;
        } else if ( (port >= port_start[d][1] && (port < (port_start[d][1]+dim_width[d]))) ) {
            //printf("\tPort matches neg Dim: %d.  [%d, %d)\n", d, port_start[d][0], (port_start[d][0]+dim_width[d]));
            if ( id_loc[d] == 0 ) {
                //printf("\tReturning Unconnected\n");
                return UNCONNECTED;
            }
            return R2R;
        }
    }
    return R2R;
}

//根据din_size数组的范围确定location数组
void idToLocation(int hm_id, int run_id, int *location) {
    // 根据 hm_id 计算 location 数组的后两位
    location[dimensions - 2] = (hm_id % dim_size[dimensions - 2]);
    location[dimensions - 1] = (hm_id / dim_size[dimensions - 2]) % dim_size[dimensions - 1];

    // 根据 run_id 计算 location 数组的前两位
    location[0] = (run_id % dim_size[0]);
    location[1] = (run_id / dim_size[0]) % dim_size[1];

    // 确保每个位置的值都在 0 到 dim_size 对应维度的范围内
    for (int i = 0; i < dimensions; i++) {
        if (location[i] >= dim_size[i]) {
            location[i] = 0; // 如果计算出的值超出范围，则重置为 0
        }
    }
}
//解析mesh.shape字符串的函数，例如传入4x4x2x2，解析出来的数组output应为[4,4,2,2]
void
topo_mesh::parseDimString(const std::string &shape, int *output) const
{
    size_t start = 0;
    size_t end = 0;
    for ( int i = 0; i < dimensions; i++ ) {
        end = shape.find('x',start);
        size_t length = end - start;
        std::string sub = shape.substr(start,length);
        output[i] = strtol(sub.c_str(), NULL, 0);
        start = end + 1;
    }
}


//新添了一个hm_id参数
int
topo_mesh::get_dest_router(int hm_id, int dest_id) const
{
    return dest_id / num_local_ports;
}

int
topo_mesh::get_dest_local_port(int dest_id) const
{
    return local_port_start + (dest_id % num_local_ports);
}


int
topo_mesh::choose_multipath(int start_port, int num_ports, int dest_dist)
{
    if ( num_ports == 1 ) {
        return start_port;
    } else {
        return start_port + (dest_dist % num_ports);
    }
}

int
topo_mesh::getEndpointID(int port)
{
    if ( !isHostPort(port) ) return -1;
    return (router_id * num_local_ports) + (port - local_port_start);
}

