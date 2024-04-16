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
#include "fattree.h"

#include <algorithm>
#include <stdlib.h>


using namespace SST::Merlin;
using namespace std;

//解析字符串函数，三个参数，一个是胖树的层级结构字符串，一个是不同层级的下行端口组成的数组，另外一个是上行端口组成的数组
void
topo_fattree::parseShape(const std::string &shape, int *downs, int *ups) const
{
    size_t start = 0;//记录当前字符串的起始位置
    size_t end = 0;//记录当前字符串的结束位置
    //计算字符串shape中冒号":"的数量，比如[4,4:4,4:8]有两个冒号，层级就是3
    //levels=[4,4:4,4:8],说明胖树为k=8的结构，每个路由器的上下行端口数都为4
    int levels = std::count(shape.begin(), shape.end(), ':') + 1;
    //开始一个循环，迭代每一层级
    //从层级为0(接入层)开始遍历
    for ( int i = 0; i < levels; i++ ) {
        寻找下一个冒号的位置，更新变量
        end = shape.find(':',start);
        size_t length = end - start;
        //获取到当前层级的信息并储存在sub变量中,比如[4,4]
        std::string sub = shape.substr(start,length);
        
        // Get the up and down
        //在当前层级信息中找寻","位置
        size_t comma = sub.find(',');
        string down;//存储下行端口的数量
        string up;//存储上行端口的数量
        //若没有找到","，则说明该层级只有下行端口的信息，没有上行端口的信息
        //说明已经遍历到了最高层根层/核心层
        if ( comma == string::npos ) {
            down = sub;//将sub字符串赋值给down，表示下行端口的数量
            up = "0";//上行端口数为0
        }
        //如果存在","的话，说明当前层级同时拥有上行端口和下行端口的数量信息
        else {
            down = sub.substr(0,comma);//
            up = sub.substr(comma+1);
        }
        //使用strtol函数将down和up的字符串值转换为整数，并存储在downs和ups
        //数组的相应位置，注意downs和ups数组是从胖树的第0层交换机开始计数的
        //若现在是在遍历接入层downs[0]=4,up[0]=4,同理汇聚层downs[1]=4,ups[1]=4
        //downs[2]=8,ups[2]=0
        downs[i] = strtol(down.c_str(), NULL, 0);
        ups[i] = strtol(up.c_str(), NULL, 0);

        // Get things ready to look at next level
        //更新start变量，使其指向下一个层级信息的起始位置
        start = end + 1;
    }
}

//topo_fattree的构造函数，ComponentId_t用于标识这个topo_fattree实例
topo_fattree::topo_fattree(ComponentId_t cid, Params& params, int num_ports, int rtr_id, int num_vns) :
    Topology(cid),
    id(rtr_id),
    num_ports(num_ports),
    num_vns(num_vns),
    num_vcs(-1)//将虚拟通道初始化值为1，说明虚拟通道的数量尚未设置
{
    string shape = params.find<std::string>("shape");

    vns = new vn_info[num_vns];//动态分配虚拟网络信息数组，用于存储每个虚拟网络的信息

    //定义一个字符串向量，用于存储每个虚拟网络的路由算法
    std::vector<std::string> vn_route_algos;
    //检查参数对象params中是否存在一个名为"routing_alg"的数组
    if ( params.is_value_array("routing_alg") ) {
        //如果存在，则将其内容存储在vn_route_algos向量中
        params.find_array<std::string>("routing_alg", vn_route_algos);
        //如果vn_route_algos数组的大小和虚拟网络的数量不匹配，则报错
        if ( vn_route_algos.size() != num_vns ) {
            fatal(CALL_INFO, -1, "ERROR: When specifying routing algorithms per VN, algorithm list length must match number of VNs (%d VNs, %lu algorithms).\n",num_vns,vn_route_algos.size());        
        }
    }
    //由于参数对象需要用户进行输入，下面是用户没有进行输入的情况：
    //如果参数对象中不存在名为"routing_alg"的数组，使用params.find函数获取默认的路由算法
    //这里默认的路由算法是确定性路由算法，除非在参数中明确指出了其他值
    else {
        std::string route_algo = params.find<std::string>("routing_alg", "deterministic");
        //用for循环将确定性路由算法添加到num_vns个虚拟网络中
        for ( int i = 0; i < num_vns; ++i ) vn_route_algos.push_back(route_algo);
    }
    //用于跟踪当前分配的虚拟通道(VC)号
    int curr_vc = 0;
    //这个循环用于为每个虚拟网络配置虚拟通道信息
    for ( int i = 0; i < num_vns; ++i ) {
        //设置当前虚拟网络的起始虚拟通道(VC)号为curr_vc
        vns[i].start_vc = curr_vc;
        if ( vn_route_algos[i] == "adaptive" ) {
            //表示当前虚拟网络允许使用自适应路由
            vns[i].allow_adaptive = true;
            //这个虚拟网络将使用一个虚拟通道
            vns[i].num_vcs = 1;
        }
        else if ( vn_route_algos[i] == "deterministic" ) {
            //表示当前网络不允许使用自适应路由
            vns[i].allow_adaptive = false;
            //这个网络将使用一个虚拟通道
            vns[i].num_vcs = 1;
        }
        //如果这个虚拟网络的路由算法既不是自适应路由算法也不是确定性路由算法
        //则输出一个致命错误
        else {
            output.fatal(CALL_INFO,-1,"Unknown routing mode specified: %s\n",vn_route_algos[i].c_str());
        }
        //配置完当前虚拟网络的虚拟通道信息后，更新cuur_vc的值为当前虚拟网络使用
        //的虚拟通道数量之和，在下一次迭代时，curr_vc将指向下一个可用的虚拟通道编号
        curr_vc += vns[i].num_vcs;
    }
    //从params参数中找寻名为"adaptive_threshold"的参数值，如果该参数存在，他将被
    //赋予相应的值;如果不存在，他将被赋予默认值0.5
    //这个变量是用于决定何时激活自适应路由算法的一个阈值。当网络拥塞或其他条件达到
    //或者超过这个阈值时，路由器会从确定性路由切换到自适应路由
    adaptive_threshold = params.find<double>("adaptive_threshold", 0.5);

    //计算冒号的数量从而得到胖树拓扑的层级数,一般都为3层
    int levels = std::count(shape.begin(), shape.end(), ':') + 1;
    //定义存储不同层级的路由器的上、下行端口数量数组
    //比如ups=[4,4,0] downs=[4,4,8]
    int* ups = new int[levels];
    int* downs= new int[levels];

    //调用自定义的解析字符串函数，填充定义的成员变量downs和ups
    parseShape(shape, downs, ups);

    //初始化一个total_host的整数型变量，初始值为1，他将存储fattree拓扑中总共的主机数量
    int total_hosts = 1;
    //迭代层级
    for ( int i = 0; i < levels; i++ ) {
        //每个层级的路由器下行端口数量相乘得到总的主机数(最后叶子节点的数量)
        total_hosts *= downs[i];
    }
    //计算拓扑中每一层的路由器数量，计算基于已经确定的总主机数和每层路由器中的上、下行端口数
    int* routers_per_level = new int[levels];//用于存储每一个层级的路由器数量
    routers_per_level[0] = total_hosts / downs[0];//算出第一层(数组下标为0)的路由器数量

    //这个for循坏用于迭代第二层开始的每一层路由器数量
    for ( int i = 1; i < levels; i++ ) {
        routers_per_level[i] = routers_per_level[i-1] * ups[i-1] / downs[i];
    }

    //假设（k=4） 则最后routers_per_level=[8,8,4]
    
    //确定fattree拓扑中特定路由器的位置，包括它所在的层级、层内ID以及层组ID
    int count = 0;//这个参数用于确定 已经遍历过的路由器数量
    rtr_level = -1;//这个用于存储 当前路由器 所在的层级。初始值设置为-1，表示还没有确定层级
    int routers_per_level_group = 1;//用于存储每一层 路由器组 的数量
    
    for ( int i = 0; i < levels; i++ ) {
        //计算当前层级中路由器的局部ID(lid),id表示当前路由器的id，与mesh不同，mesh中还是叫rtr_id.
        int lid = id - count;
        //更新count变量，增加当前层级的路由器数量，
        count += routers_per_level[i];
        //如果当前路由器ID小于更新后的count，意味着当前路由器位于i层
        if ( id < count ) {
            rtr_level = i;//设置rtr_level为当前层级 i
            level_id = lid;//将局部id赋值给level_i变量
            //level_group(组id)
            level_group = lid / routers_per_level_group;
            //跳出循环，因为找到了路由器所在的层级
            break;
        }
        routers_per_level_group *= ups[i];
    }
    //在Fattree拓扑中计算当前路由器的端口数量和可到达的主机ID范围
    down_ports = downs[rtr_level];//将当前路由器在rtr_level层级的下行端口数赋值给down_ports变量
    up_ports = ups[rtr_level];//将..上行...up_ports变量
    
    // Compute reachable IDs
    //初始化一个名为rid的整形变量，用于计算当前路由器可以到达的路由器或主机的总数
    int rid = 1;
    //通过一个for循环来计算从第一层(下标为0)到当前层级(rtr_level)
    //累乘的结果表示的是当前路由器可以到达的下游主机数量(叶子节点)
    //比如对于k=4的胖树中的路由器id=8的路由器来说，rid的结果为2*2=4
    for ( int i = 0; i <= rtr_level; i++ ) {
        rid *= downs[i];
    }
    //计算当前路由器的下行链路数，当k=4时，对于rid=8的路由器来说
    //down_router_factor= 4 / 2 = 2
    down_route_factor = rid / downs[rtr_level];

    //计算当前路由器组中最低的主机或路由器ID，level_group是当前路由器所在的
    //层组ID，rid是当前路由器通过下行链路可以到达的最大主机数量
    low_host = level_group * rid;
    //rid-1表示当前层组中的节点数量，将这个值加到low_host上得到当前层组中最高
    //的可到达ID
    high_host = low_host + rid - 1;
    //low、high_host定义了当前路由器可以到达的主机ID范围
    
}


topo_fattree::~topo_fattree()
{
    delete[] vns;
}

//确定性路由算法，根据确定性路由算法来决定数据包的下一跳端口。确定性路由算法
//是一种简单的路由算法，它根据数据包的目的地址来选择下一条端口，而不考虑当前网络
//的拥塞状况
void topo_fattree::route_deterministic(int port, int vc, internal_router_event* ev)  {
    //获取主机ID
    int dest = ev->getDest();
    // Down routes
    //判断检查数据包的目的地址是否在当前路由器可以到达的范围内
    //如果在，说明数据包应该沿着下行路径被路由
    if ( dest >= low_host && dest <= high_host ) {
        ev->setNextPort((dest - low_host) / down_route_factor);
    }
    // Up routes
    //如果目的地址不在当前的路由器的范围内，说明数据包应该沿着上行路径被路由
    else {
        ev->setNextPort(down_ports + ((dest/down_route_factor) % up_ports));
    }
}

//这段代码负责根据当前的网络状况和路由策略(确定性或者自适应)来路由数据包
//这段函数首先尝试使用确定性路由，然后根据需要应用自适应路由
void topo_fattree::route_packet(int port, int vc, internal_router_event* ev)
{
    route_deterministic(port,vc,ev);
    
    int dest = ev->getDest();
    // Down routes are always deterministic and are already done in route
    //如果目的地址在当前路由器的直接连接范围内(即low_host与high_host之间)
    //说明不需要做进一步的路由决策，直接返回
    if ( dest >= low_host && dest <= high_host ) {
        return;
    }
    // Up routes can be adaptive, so things can change from the normal path
    //如果目的主机ID不在当前路由器的直接连接范围内，即数据包需要向上路由，则进入这个分支
    else {
        //获取数据包所属的虚拟网络编号
        int vn = ev->getVN();
        // If we're not adaptive, then we're already routed
        //检查当前虚拟网络是否允许使用自适应路由，如果不允许，则直接返回，使用
        //之前的确定性路由结果
        if ( !vns[vn].allow_adaptive ) return;

        // If the port we're supposed to be going to has a buffer with
        // fewer credits than the threshold, adaptively route
        //获取当前数据包根据确定性路由算法计算出的下一跳端口
        int next_port = ev->getNextPort();
        //获取数据包使用的虚拟通道号
        int vc = ev->getVC();
        //计算出数据包在输出端口的虚拟通道索引
        int index  = next_port*num_vcs + vc;
        //检查当前端口的输出信用值是否大于或等于阈值，如果是，表示大哥前端口没有拥塞
        //数据包可以正常发送，直接返回
        if ( outputCredits[index] >= thresholds[index] ) return;
        
        // Send this on the least loaded port.  For now, just look at
        // current VC, later we may look at overall port loading.  Set
        // the max to be the "natural" port and only adaptively route
        // if it's not the best one (ties go to natural port)

        //初始化最大信用值为当前端口的输出信用值
        int max = outputCredits[index];
        
        // If all ports have zero credits left, then we just set
        // it to the port that it would normally go to without
        // adaptive routing.

        //假设数据包将通过确定性路由的端口进行发送
        int port = next_port;
        //循环遍历所有可能的端口，找到具有最多可用信用值的端口
        for ( int i = (down_ports * num_vcs) + vc; i < num_ports * num_vcs; i += num_vcs ) {
            if ( outputCredits[i] > max ) {
                max = outputCredits[i];
                port = i / num_vcs;
            }
        }
        //使用最佳端口更新ev对象的下一跳端口，如果这个端口与确定性路由的端口
        //相同，那么实际上没有进行自适应路由；如果不同，数据包将通过自适应选择
        //的端口发送
        ev->setNextPort(port);
    }
}


//处理传入的路由器事件
internal_router_event* topo_fattree::process_input(RtrEvent* ev)
{
    //强转
    internal_router_event* ire = new internal_router_event(ev);
    ire->setVC(ire->getVN());//获取新创建的ire的虚拟网络编号(VN)。
    //然后将虚拟网络编号设置为该事件的虚拟通道号，在这种情况下，虚拟通道和虚拟网络编号相同
    return ire;
}

//处理未定时数据的路由，例如控制信息或管理信息
void topo_fattree::routeUntimedData(int inPort, internal_router_event* ev, std::vector<int> &outPorts)
{
    //是否为未定时广播地址，若是说明该数据包需要被广播到所有下游端口
    if ( ev->getDest() == UNTIMED_BROADCAST_ADDR ) {
        // Send to all my downports except the one it came from
        //遍历所有下行端口，若它不是数据包进入的端口，则将该端口添加到
        //输出端口列表的outPorts中
        for ( int i = 0; i < down_ports; i++ ) {
            //将当前遍历的下行端口i添加到输出端口列表中，除非i等于进入
            //端口inPort,这是为了避免将数据包送回到它来的那个端口
            if ( i != inPort ) outPorts.push_back(i);
        }
        

        // If I'm not at the top level (no up_ports) an I didn't
        // receive this from an up_port, send to one up port
        //判断当前路由器是否位于顶层(即没有上行端口up_ports)
        if ( up_ports != 0 && inPort < down_ports ) {
            //如果不是顶层，则数据包时通过一个下行端口进入的
            //将一个上行端口添加到输出端口的列表中，通过计算inPort除以
            //up_ports的余数来选择一个上行端口，这样做可以确保在多个上行
            //端口之间均匀地分发广播数据包
            outPorts.push_back(down_ports+(inPort % up_ports));
        }
    }
    //如果数据包的目的主机ID不是广播地址
    else {
        //用确定性路由算法计算数据包的下一跳端口，由于未定时数据不使用虚拟通道
        //所以在这里将虚拟通道号(VC)设置为0
        route_deterministic(inPort, 0, ev);
        //将计算出的下一跳端口添加到输出端口列表outPorts中
        outPorts.push_back(ev->getNextPort());
    }
}

//
internal_router_event* topo_fattree::process_UntimedData_input(RtrEvent* ev)
{
    return new internal_router_event(ev);
}

//获取当前路由器不同下行端口号所对应的主机ID
//通过当前路由器所能到达的最小主机ID+端口号来获取
int
topo_fattree::getEndpointID(int port)
{
    return low_host + port;
}

//返回当前路由器端口的连接状态
Topology::PortState topo_fattree::getPortState(int port) const
{
    //如果当前路由器在接入层
    if ( rtr_level == 0 ) {
        //说明是接入层的下行端口
        if ( port < down_ports ) return R2N;
        else if ( port >= down_ports ) return R2R;//说明是接入层的上行端口
        else return UNCONNECTED;//单个节点(k=1的胖树),无任何连接
    } 
    //非接入层都是R2R
    else {
        return R2R;
    }
}

void topo_fattree::setOutputBufferCreditArray(int const* array, int vcs) {
        num_vcs = vcs;
        outputCredits = array;
        thresholds = new int[num_vcs * num_ports];
        for ( int i = 0; i < num_vcs * num_ports; i++ ) {
            thresholds[i] = outputCredits[i] * adaptive_threshold;
        }
        
}
