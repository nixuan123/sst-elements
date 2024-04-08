// -*- mode: c++ -*-

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


#ifndef COMPONENTS_MERLIN_TOPOLOGY_MESH_H
#define COMPONENTS_MERLIN_TOPOLOGY_MESH_H

#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

#include <string.h>

#include "sst/elements/merlin/router.h"

namespace SST {
namespace Merlin {
//专门用于处理mesh（现在被我改为hm）内部路由的函数
class topo_mesh_event : public internal_router_event {
public:
    //事件的初始化新添一个hm_id参数
    int hm_id;
    int dimensions;//现在这个dimensions规定了为固定值4
    int routing_dim;//路由的维度，可以理解为先遍历a,b维度,然后再遍历x,y维度
    //这个dest_loc是一个int型数组，用于表示目标路由器的位置
    int* dest_loc;

    topo_mesh_event() {}
    topo_mesh_event(int dim) {	dimensions = dim; routing_dim = 0; dest_loc = new int[dim]; }
    virtual ~topo_mesh_event() { delete[] dest_loc; }
    virtual internal_router_event* clone(void) override
    {
        topo_mesh_event* tte = new topo_mesh_event(*this);
        tte->dest_loc = new int[dimensions];
        memcpy(tte->dest_loc, dest_loc, dimensions*sizeof(int));
        return tte;
    }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        internal_router_event::serialize_order(ser);
        ser & dimensions;
        ser & routing_dim;

        if ( ser.mode() == SST::Core::Serialization::serializer::UNPACK ) {
            dest_loc = new int[dimensions];
        }

        for ( int i = 0 ; i < dimensions ; i++ ) {
            ser & dest_loc[i];
        }
    }

protected:

private:
    ImplementSerializable(SST::Merlin::topo_mesh_event)

};


class topo_mesh_init_event : public topo_mesh_event {
public:
    int phase;

    topo_mesh_init_event() {}
    topo_mesh_init_event(int dim) : topo_mesh_event(dim), phase(0) { }
    virtual ~topo_mesh_init_event() { }
    virtual internal_router_event* clone(void) override
    {
        topo_mesh_init_event* tte = new topo_mesh_init_event(*this);
        tte->dest_loc = new int[dimensions];
        memcpy(tte->dest_loc, dest_loc, dimensions*sizeof(int));
        return tte;
    }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        topo_mesh_event::serialize_order(ser);
        ser & phase;
    }

private:
    ImplementSerializable(SST::Merlin::topo_mesh_init_event)

};

//对topo_mesh进行修改
class topo_mesh: public Topology {

public:

    SST_ELI_REGISTER_SUBCOMPONENT(
        topo_mesh,
        "merlin",
        "mesh",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Multi-dimensional mesh topology object",
        SST::Merlin::Topology
    )

    SST_ELI_DOCUMENT_PARAMS(
        // Parameters needed for use with old merlin python module
        //mesh.shape接受四个参数，4x4x2x2代表它由2x2个4x4的二维mesh拓扑组成
        {"mesh.shape", "The parameter mesh.shape accepts four arguments, with 4x4x2x2 indicating that it is composed of 2x2  "
                       "two-dimensional meshes, each with a topology of 4x4."},
        {"mesh.width", "Number of links between routers in each dimension, specified in same manner as for shape.  For "
                       "example, 2x2x1 denotes 2 links in the x and y dimensions and one in the z dimension."},
        {"mesh.local_ports", "Number of endpoints attached to each router."},


        {"shape", "Shape of the mesh specified as the number of routers in each dimension, where each dimension is "
                  "separated by a colon.  For example, 4x4x2x2.  Any number of dimensions is supported."},
        {"width", "Number of links between routers in each dimension, specified in same manner as for shape.  "
                  "For example, 2x2x1 denotes 2 links in the x and y dimensions and one in the z dimension."},
        {"local_ports",  "Number of endpoints attached to each router."}
    )


private:
    //定义一个结构体DestInfo,一个路由器在拓扑中的位置由DestInfo中的两个变量唯一确定
    struct DestInfo{
       int hm_id;//hm板子id
       int router_id;//板子内部路由器的id
    }
    //新添一个hm_id数组参数,通过hm_id解析出hm板子在拓扑中的位置
    int hm_id;
    //router_id保持不变，通过router_id可以解析出hm板子内部路由器的位置
    int router_id;
    //即可能id_loc=[1,1,1,0]
    int* id_loc;

    int dimensions=4;//由于Hammingmesh由四个参数构成拓扑形装，所以我们默认dimensions为4
    //若字符串为"4x4x2x2"，则解析出来的dim_size数组为[4,4,2,2]
    int* dim_size;
    int* dim_width;

    int (* port_start)[2]; // port_start[dim][direction: 0=pos, 1=neg]

    int num_local_ports;
    int local_port_start;

    int num_vns;
    
public:
    //初始化需要多接受hm_id参数，这四个参数可以唯一确定路由器的位置
    topo_mesh(ComponentId_t cid, Params& params, int num_ports, int rtr_id, int num_vns,int hm_id);
    ~topo_mesh();

    virtual void route_packet(int port, int vc, internal_router_event* ev);
    virtual internal_router_event* process_input(RtrEvent* ev);

    virtual void routeUntimedData(int port, internal_router_event* ev, std::vector<int> &outPorts);
    virtual internal_router_event* process_UntimedData_input(RtrEvent* ev);

    virtual PortState getPortState(int port) const;
    virtual int getEndpointID(int port);

    virtual void getVCsPerVN(std::vector<int>& vcs_per_vn) {
        for ( int i = 0; i < num_vns; ++i ) {
            vcs_per_vn[i] = 1;
        }
    }
    
protected:
    //这个函数只是进行了定义，具体在mesh.cc文件里面实现
    virtual int choose_multipath(int start_port, int num_ports, int dest_dist);

private:
    //在定义中新添了一个hm_id参数
    void idToLocation(int hm_id, int id, int *location) const;
    void parseDimString(const std::string &shape, int *output) const;
    //目的路由位置的获取函数，添加了一个新的hm_id参数，所以返回值要设置为一个数组
    //将原来的int型函数改为int*指针型数组
    int* get_dest_router(int hm_id, int dest_id) const;
    int* get_dest_local_port(int dest_id) const;


};

}
}

#endif // COMPONENTS_MERLIN_TOPOLOGY_MESH_H
