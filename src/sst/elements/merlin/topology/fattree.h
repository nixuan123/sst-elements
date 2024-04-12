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


#ifndef COMPONENTS_MERLIN_TOPOLOGY_FATTREE_H
#define COMPONENTS_MERLIN_TOPOLOGY_FATTREE_H

#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

#include "sst/elements/merlin/router.h"

namespace SST {
namespace Merlin {

class topo_fattree: public Topology {

public:
    //使用SST的ELI（Element Library Interface）系统进行注册
    //这意味着它可以被SST框架识别和使用
    SST_ELI_REGISTER_SUBCOMPONENT(
        topo_fattree,
        "merlin",
        "fattree",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Fattree topology object",
        SST::Merlin::Topology
    )
    //使用SST的ELI（Element Library Interface）系统进行文档化
    SST_ELI_DOCUMENT_PARAMS(
        // Parameters needed for use with old merlin python module
        {"fattree.shape",               "Shape of the fattree"},
        {"fattree.routing_alg",         "Routing algorithm to use. [deterministic | adaptive]","deterministic"},
        {"fattree.adaptive_threshold",  "Threshold used to determine if a packet will adaptively route."},

        {"shape",               "Shape of the fattree"},
        {"routing_alg",         "Routing algorithm to use. [deterministic | adaptive]","deterministic"},
        {"adaptive_threshold",  "Threshold used to determine if a packet will adaptively route."}
    )
//fattree.shape指的是Fattree拓扑的形装，这个参数定义了网格的层次结构，通常是一个三元组(b、k、h)
//其中b是每个父节点下的子节点数，k是网络的根层级数，h是叶节点层级数

//fattree.routing_alg使用的路由算法，这个参数有两个可选值：一个是确定性还有一个是自适应。确定性意味
//着每个数据包都遵循固定的路径，而自适应路由允许数据包根据网络状况选择不同的路径

//adaptive_threshold:自适应路由中使用的阈值，当网络拥塞超过这个阈值时，数据包会选择不同于
//默认路径的路由

//同样的参数也被定义在另一组中，但是没有前缀"fattree."。这可能是为了向后兼容旧版本的Merlin Python模块，
//允许用户使用不带前缀的参数名称来配置拓扑。
private:
    int rtr_level;//表示当前路由器在fattree拓扑中的层级，在fattree结构中，根节点位于顶层，叶子节点位于底层，中间的层级包含交换机或者路由器
    int level_id;//用于标识当前路由器在其所在层级中的唯一ID
    int level_group;//用于标识当前路由器所属的层级组，这在多根或者分层的Fattree变体中特别有用

    ////当前路由器连接的主机范围
    int high_host;//最高编主机号
    int low_host;//最低编主机号

    int down_route_factor;//计算向下路由(从当前层级到下一层级)的因子，这可能于端口分配或路由策略有关

//    int levels;
    int id;//当前路由器的唯一标识符
    int up_ports;//当前路由器向上的接口数量
    int down_ports;//当前路由器向下的接口数量
    int num_ports;//当前路由器总的端口数量
    int num_vns;//虚拟网络的数量
    int num_vcs;//虚拟通道的数量

    int const* outputCredits;//指向一个整数数组的指针，用于存储当前路由器的输出信用值。在模拟器中，通常用于控制流量和避免拥塞
    int* thresholds;//用于存储当前路由器的阈值，用于决定何时激活自适应路由或其他网络策略
    double adaptive_threshold;//自适应路由的阈值，当网络条件达到或超过这个阈值时，路由器可能会改变其路由行为

    //一个结构体，用于存储有关虚拟网络的信息，包括起始虚拟通道号（start_vc）
    //虚拟通道数量(num_vcs)和是否允许自适应路由(allow_adaptive)
    struct vn_info {
        int start_vc;
        int num_vcs;
        bool allow_adaptive;
    };
    //该结构体用于存储所有虚拟网络的信息
    vn_info* vns;

    void parseShape(const std::string &shape, int *downs, int *ups) const;


public:
    topo_fattree(ComponentId_t cid, Params& params, int num_ports, int rtr_id, int num_vns);
    ~topo_fattree();

    virtual void route_packet(int port, int vc, internal_router_event* ev);
    virtual internal_router_event* process_input(RtrEvent* ev);

    virtual void routeUntimedData(int port, internal_router_event* ev, std::vector<int> &outPorts);
    virtual internal_router_event* process_UntimedData_input(RtrEvent* ev);

    virtual int getEndpointID(int port);

    virtual PortState getPortState(int port) const;

    virtual void setOutputBufferCreditArray(int const* array, int vcs);

    virtual void getVCsPerVN(std::vector<int>& vcs_per_vn) {
        for ( int i = 0; i < num_vns; ++i ) {
            vcs_per_vn[i] = vns[i].num_vcs;
        }
    }

private:
    void route_deterministic(int port, int vc, internal_router_event* ev);
};



}
}

#endif // COMPONENTS_MERLIN_TOPOLOGY_FATTREE_H
