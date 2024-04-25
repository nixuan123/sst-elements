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
//专门用于处理hm内部路由的函数
class topo_mesh_event : public internal_router_event {
public:
    
    int dimensions;//现在这个dimensions规定了为固定值4
    int routing_dim;//路由的维度，板内有两个维度，板间只有一个维度
    //这个dest_loc是一个int型数组，用于表示目标路由器的位置，现在可能为[1,1,1,0]
    int* dest_loc;

    topo_mesh_event() {}
    //这个dim现在传值只能为4
    topo_mesh_event(int dim) {	dimensions = dim; routing_dim = 0; dest_loc = new int[dim]; hm_dim = 0; }
    virtual ~topo_mesh_event() { delete[] dest_loc; }
    //下面这个clone函数是为了复制一个topo_mesh_event事件对象，可能需要目标位置的信息，但又不希望对其进行改变所以才会有clone函数
    virtual internal_router_event* clone(void) override
    {
        topo_mesh_event* tte = new topo_mesh_event(*this);
        tte->dest_loc = new int[dimensions];
        //用memcpy函数给dest_loc分配四个int大小的空间
        memcpy(tte->dest_loc, dest_loc, dimensions*sizeof(int));
        return tte;
    }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        internal_router_event::serialize_order(ser);
        //操作符 & 在这里是重载的，它根据 ser 的当前模式（PACK 或 UNPACK）来决定是序列化还是反序列化。  
        ser & dimensions;
        ser & routing_dim;
        //dest_loc数组的大小和dimensions有关，是一个动态分配的数组，需要在反序列化的情况下
        //为其分配内存。在反序列化时，我们需要为新对象创建数组并填充数据
        if ( ser.mode() == SST::Core::Serialization::serializer::UNPACK ) {
            dest_loc = new int[dimensions];
        }
        在序列化时，数组已经存在，我们只需要将其内容保存到流中。
        for ( int i = 0 ; i < dimensions ; i++ ) {
            ser & dest_loc[i];
        }
    }

protected:

private:
    ImplementSerializable(SST::Merlin::topo_mesh_event)

};

//这个类继承自topo_mesh_event类，表示在多维网格拓扑中初始化阶段的事件
//这个类主要用于网格模拟中，特别是在初始化网格连接和配置时
class topo_mesh_init_event : public topo_mesh_event {
public:
    //这个phase表示初始化事件的当前阶段
    int phase;

    topo_mesh_init_event() {}
    //带参数的构造函数，赋值为父类的dim，phase初始化为0
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
                       "example, 2x2 denotes 2 links in the x and y dimensions."},
        {"mesh.local_ports", "Number of endpoints attached to each router."},
        


        {"shape", "Shape of the mesh specified as the number of routers in each dimension, where each dimension is "
                  "separated by a colon.  For example, 4x4x2x2.  Any number of dimensions is supported."},
        {"width", "Number of links between routers in each dimension, specified in same manner as for shape.  "
                  "For example, 2x2 denotes 2 links in the x and y dimensions and one in the z dimension."},
        {"local_ports",  "Number of endpoints attached to each router."}
    )


private:
    
    //router_id保持不变，通过router_id可以解析出hm板子内部路由器的位置
    int router_id;
    //目的路由器id的位置
    int* id_loc;
    
    //【新加】建立一个变量用于存储交换机的起始id
    int switch_fid;
    
    //为每个路由器添加一个存储当前路由器的行和列交换机的信息，第一个存储行交换机，第二个存储列交换机
    int (* switches)[2];

    int dimensions=4;//由于Hammingmesh由四个参数构成拓扑形装，所以我们默认dimensions为4
    //若字符串为"3x3x2x2"，则解析出来的dim_size数组为[3,3,2,2]
    //前两个是板内的维度大小，后面两个是hm/板间维度的大小
    int* dim_size;
    int* dim_width;
    
    int (* port_start)[2]; // port_start[dim][direction: 0=pos, 1=neg]

    int num_local_ports;//每个路由器需要连接的主机端口数量
    int *num_switch_ports;//【新增】行、列交换机分别需要的端口数量
    int hm_id;//【新增】路由器的hm板id
    int local_port_start;

    int num_vns;
    
public:
    //初始化需要多接受hm_id参数，便于唯一确定路由器的位置
    topo_mesh(ComponentId_t cid, Params& params, int num_ports, int rtr_id, int num_vns );
    ~topo_mesh();
    //这个ev对象是topo_mesh_event(在本文件中定义)的父类
    virtual void route_packet(int port, int vc, internal_router_event* ev);
    //这个函数返回一个internal_router_event对象，它的主要职责是将输入的事件封装
    //到一个新的topo_mesh_event对象中，并为后续的路由决策准备必要的信息
    virtual internal_router_event* process_input(RtrEvent* ev);

    virtual void routeUntimedData(int port, internal_router_event* ev, std::vector<int> &outPorts);
    virtual internal_router_event* process_UntimedData_input(RtrEvent* ev);
    //获取端口的状态函数
    virtual PortState getPortState(int port) const;
    //根据端口号获取主机ID
    virtual int getEndpointID(int port);

    //获取每个虚拟网络的虚拟通道
    virtual void getVCsPerVN(std::vector<int>& vcs_per_vn) {
        for ( int i = 0; i < num_vns; ++i ) {
            vcs_per_vn[i] = 1;
        }
    }
    
protected:
    //这个函数是用于hm拓扑中的路径选择
    virtual int choose_multipath(int start_port, int num_ports, int dest_dist);
private:
    //确定路由器/或者交换机在拓扑中的位置信息
    void idToLocation(int id, int *location) const;
    //解析字符串中的维度信息(现在固定是4)
    void parseDimString(const std::string &shape, int *output) const;
    //用于获取目的路由器id的函数
    int get_dest_router(int dest_id) const;
    //用于获取目的路由器的id
    int get_dest_local_port(int dest_id) const;
    //用于获取目的路由器的行列交换机信息
    int get_dest_switches(int dest_id) const;//【新加】


};

}
}

#endif // COMPONENTS_MERLIN_TOPOLOGY_MESH_H
