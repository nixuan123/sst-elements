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
    router_id(rtr_id),
    num_vns(num_vns),
    hm_id(hm_id),//新添一个hm_id
{

    // Get the various parameters
    std::string shape;
    //获取mesh.shape的字符串，例如返回"4x4x2x2"
    shape = params.find<std::string>("shape");
    if ( !shape.compare("") ) {
    }

    // Need to parse the shape string to get the number of dimensions
    // and the size of each dimension    
    dimensions = 4;//dimensions = std::count(shape.begin(),shape.end(),'x') + 1;
    //维度信息里面存的分别是a,b,x,y四个维度的值
    dim_size = new int[4];//dim_size = new int[dimensions];
    dim_width = new int[2];//dim_width = new int[dimensions];
    port_start = new int[2][2];//port_start = new int[dimensions][2];
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
    /*for ( int d = 0 ; d < dimensions ; d++ ) {
        for ( int i = 0 ; i < 2 ; i++ ) {
            port_start[d][i] = next_port;
            next_port += dim_width[d];
        }
    }*/
    //【改动】
    for ( int d = 0 ; d < 2 ; d++ ) {
        for ( int i = 0 ; i < 2 ; i++ ) {
            port_start[d][i] = next_port;
            next_port += dim_width[d];
        }
    }
    //每个路由器连接Node的端口数量
    num_local_ports = params.find<int>("local_ports", 1);

    //每个路由器需要的端口数量
    int needed_ports = 0;
    /*for ( int i = 0 ; i < dimensions ; i++ ) {
        needed_ports += 2 * dim_width[i];
    }*/

    //【改动】
    for ( int i = 0 ; i < 2 ; i++ ) {
        needed_ports += 2 * dim_width[i];
    }


    if ( num_ports < (needed_ports+num_local_ports) ) {
        output.fatal(CALL_INFO, -1, "Number of ports should be at least %d for this configuration\n", needed_ports+num_local_ports);
    }

    local_port_start = needed_ports;// Local delivery is on the last ports
    //id_loc现在是一个有四个参数的数组，例如通过[1,1,1,0]确定他在hm拓扑中的唯一位置
    id_loc = new int[4];//id_loc = new int[dimensions]; 
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

//路由器内部的路由具体该怎么走，即这个数据包下一步要送到该路由器的具体的端口id
//ev->getDest()返回的是一个网络标识符Node ID类型的数据
void
topo_mesh::route_packet(int port, int vc, internal_router_event* ev)
{   //通过网卡id定位到路由器id,但要注意还有一个hm_id没有确定
    int dest_router = get_dest_router(ev->getDest());
    //再次通过网卡id定位到hm_id
    int dest_hm =get_dest_hm(ev->getDest());
    //如果定位到的hm_id等于当前的hm_id
    if(dest_hm == hm_id){
	并且路由器id=当前的路由器id,即Node ID(网卡id/终端id)在当前路由器上
	if ( dest_router == router_id ) {
	//则将路由器内部路由的下一个端口设置为本地的连接网卡的端口号
        ev->setNextPort(get_dest_local_port(ev->getDest()));
    }
    //若定位到的路由器不在当前路由器上，则进行当前路由器端口选择，选择一个离目标近的
    //出口port。
    else {
        //强制转换，转换为专门用于处理mesh拓扑的事件对象
        topo_mesh_event *tt_ev = static_cast<topo_mesh_event*>(ev);
        //由于是板内路由，所以只需要遍历前面两个维度
        for ( int dim = tt_ev->routing_dim ; dim < 2 ; dim++ ) {
            if ( tt_ev->dest_loc[dim] != id_loc[dim] ) {

                int go_pos = (id_loc[dim] < tt_ev->dest_loc[dim]);
                //这个函数负责选择一个出口端口进行路由
                int p = choose_multipath(
                        port_start[dim][(go_pos) ? 0 : 1],
                        dim_width[dim],
                        abs(id_loc[dim] - tt_ev->dest_loc[dim]));
                //将事件的下一个端口设置为p
                tt_ev->setNextPort(p);
                //如果当前路由器在板内边缘位置并且是非R2N端口，需要设置虚拟通道
                if ( id_loc[dim] == 0 && port < local_port_start ) { // Crossing dateline
                    //切换为虚拟通道，通过异或操作来切换虚拟通道的编号来实现
		    int new_vc = vc ^ 1;
		    //使用tt_ev->setVC方法更新事件对象的虚拟通道编号
                    tt_ev->setVC(new_vc); // Toggle VC
                    output.verbose(CALL_INFO, 1, 1, "Crossing dateline.  Changing from VC %d to %d\n", vc, new_vc);
                }

                break;

            } else {//如果在当前维度相同，则调转一个维度继续遍历
                // Time to change direction
		//增加一个维度
                tt_ev->routing_dim++;
		//重置虚拟通道编号
                tt_ev->setVC(vc & (~1)); // Reset the VC
            }
        }
    }//end else
}//end if

//进行板间路由
else{
	//通过二级胖树，为了防止回路出现，还需要设置虚拟通道
}		
}



internal_router_event*
topo_mesh::process_input(RtrEvent* ev)
{   //topo_mesh_event于mesh.h文件中进行定义，它继承自internal_router
    //_event,并且他接受一个参数即维度的大小，现为固定值4
    topo_mesh_event* tt_ev = new topo_mesh_event(dimensions);
    //将传入的ev对象封装到新创建的tt_ev对象中，它将包含原始的路由器事件信息
    tt_ev->setEncapsulatedEvent(ev);
    //虚拟网络和虚拟通道映射的建立：通常是虚拟网络编号的2倍。
    //通过tt_ev对象的虚拟网络编号来设置tt_ev对象的虚拟通道(vc)编号
    tt_ev->setVC(tt_ev->getVN() * 2);
    
    // Need to figure out what the mesh address is for easier
    // routing. 重新设置tt_ev对象中的mesh地址
    int run_id = get_dest_router(tt_ev->getDest());
    idToLocation(hm_id, run_id, tt_ev->dest_loc);

    return tt_ev;
}

//这个函数是用于处理未定时路由，用于初始化数据的传输和确定数据包的输出端口
void topo_mesh::routeUntimedData(int port, internal_router_event* ev, std::vector<int> &outPorts)
{   //强制转换，将internal_router——event*类型的事件对象ev转换为topo_mesh_init_event类型
    //这是因为 topo_mesh_init_event 是 internal_router_event 的子类，包含了特定的初始化事件信息。
    topo_mesh_init_event *tt_ev = static_cast<topo_mesh_init_event*>(ev);
    //表示处于初始化阶段
    if ( tt_ev->phase == 0 ) {
	//如果当前的路由器ID为0(通常是根路由器或者主路由器)并且事件的目标地址是未定时广播地址，则表示
        //广播已经到达根路由器，需要将初始化的阶段从0->1
        if ( (0 == router_id) && (ev->getDest() == UNTIMED_BROADCAST_ADDR) ) {
            /* Broadcast has arrived at 0.  Switch Phases */
            tt_ev->phase = 1;
        } else {
	    //如果不满足上述条件，函数调用route_packet函数来处理普通的数据包路由
	    //并将计算出的下一个端口号添加到outPorts向量中，然乎返回。这表示在初
	    //始化阶段，非根路由器并且非广播地址的事件将按照常规数据包进行路由
            route_packet(port, 0, ev);
            outPorts.push_back(ev->getNextPort());
            return;
        }
    }

    /*
     * Find dimension came in on
     * Send in positive direction in all dimensions that level and higher (unless at end)
     确定数据包进入的维度
     */
    int inc_dim = 0;
    if ( tt_ev->phase == 2 ) {
        for ( ; inc_dim < dimensions ; inc_dim++ ) {
            if ( port == port_start[inc_dim][1] ) {
                break;
            }
        }
    }
    //在所有高于或等于该维度的维度上沿着正向发送数据包，除非已经到达网格的末端
    tt_ev->phase = 2;
    //沿着正向发送数据包
    for ( int dim = inc_dim ; dim < dimensions ; dim++ ) {
        if ( (id_loc[dim] + 1) < dim_size[dim] ) {
            outPorts.push_back(port_start[dim][0]);
        }
    }
    
    // Also, send to hosts，将数据包发送到本地主机
    for ( int p = 0 ; p < num_local_ports ; p++ ) {
        if ( (local_port_start + p) != port ) {
            outPorts.push_back(local_port_start +p);
        }
    }
}

//这个函数用于处理未定时数据的输入事件，这个函数在多维网格拓扑网络的上下文中工作
//主要用于初始化数据的传输和确定数据包的目标位置
internal_router_event* topo_mesh::process_UntimedData_input(RtrEvent* ev)
{   //创建topo_mesh_init_event对象
    topo_mesh_init_event* tt_ev = new topo_mesh_init_event(dimensions);
    //将ev对象封装到tt_ev中，使其拥有ev对象的初始化信息
    tt_ev->setEncapsulatedEvent(ev);
    //如果事件的目标网卡ID是未定时广播地址
    if ( tt_ev->getDest() == UNTIMED_BROADCAST_ADDR ) {
        /* For broadcast, first send to rtr 0 */
	//如果是广播地址，将目标地址的位置设置为根路由器的位置，现在添加了一个hm_id用于
	//确定该路由器在哪块hm板子上，dest_loc的值为[0,0,0,0]
        idToLocation(0, 0, tt_ev->dest_loc);
    } else {//若不是广播地址
        int rtr_id = get_dest_router(tt_ev->getDest());
	int hm_id = get_dest_hm(tt_ev->getDest());//【新加】
	//将目标位置设置为目标路由器的位置
        idToLocation(hm_id, rtr_id, tt_ev->dest_loc);
    }
    return tt_ev;
}

//这个函数用于确定并返回指定端口的状态，接受一个端口号，并返回该端口号的连接状态
//注意的是hm拓扑中不存在端口未连接的情况，所有端口都被充分利用
//getPortState 函数通过检查端口号与预定义的端口范围和路由器在网格拓扑中的位置，
//来确定端口的状态。这个状态信息对于路由器的路由决策和网络配置是非常重要的。
Topology::PortState
topo_mesh::getPortState(int port) const
{   //检查函数端口号是否大于或等于local_port_start，这是本地端口的起始编号
    if (port >= local_port_start) {
        if ( port < (local_port_start + num_local_ports) )
	    //表示该端口是连接到本地主机的端口
            return R2N;
	如果端口超出了本地端口的范围，表示端口未连接
        return UNCONNECTED;
    }

    //printf("id: %d.   Port Check %d\n", router_id, port);
    //对于维度d，函数检查端口号是否落在该维度的正向端口范围
    for ( int d = 0 ; d < dimensions ; d++ ) {
        if ( (port >= port_start[d][0] && (port < (port_start[d][0]+dim_width[d]))) ) {
            //printf("\tPort matches pos Dim: %d.  [%d, %d)\n", d, port_start[d][0], (port_start[d][0]+dim_width[d]));
            if ( id_loc[d] == (dim_size[d]-1) ) {
                //printf("\tReturning Unconnected\n");
		//这里需要更改，如果端口属于该维度正向范围并且是该维度上位置的最大值，则它连接的是二级胖树
                return UNCONNECTED;
            }
            return R2R;
	//函数检查端口号是否落在该维度的负向端口范围
        } else if ( (port >= port_start[d][1] && (port < (port_start[d][1]+dim_width[d]))) ) {
            //printf("\tPort matches neg Dim: %d.  [%d, %d)\n", d, port_start[d][0], (port_start[d][0]+dim_width[d]));
            if ( id_loc[d] == 0 ) {
                //printf("\tReturning Unconnected\n");
		//同理也需要更改
                return UNCONNECTED;
            }
            return R2R;
        }
    }
    return R2R;
}

/*
void
topo_mesh::idToLocation(int run_id, int *location) const
{
	for ( int i = dimensions - 1; i > 0; i-- ) {
		int div = 1;
		for ( int j = 0; j < i; j++ ) {
			div *= dim_size[j];
		}
		int value = (run_id / div);
		location[i] = value;
		run_id -= (value * div);
	}
	location[0] = run_id;
}*/

//根据dim_size数组的范围确定location数组
void
topo_mesh::idToLocation(int hm_id, int run_id, int *location) {
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


int
topo_mesh::get_dest_router(int dest_id) const
{
    return dest_id / num_local_ports;
}

int
topo_mesh::get_dest_local_port(int dest_id) const
{
    return local_port_start + (dest_id % num_local_ports);
}
//【新增】
int
topo_mesh::get_dest_hm(int dest_id) const
{
	return dest_id / num_local_ports / (dim_size[0]*dim_size[1])
}

//这个函数是已经确定在相同的hm_id板子上进行路由选择，后面我还需要定义一个
//在不同hm板上进行路由选择的函数
int
topo_mesh::choose_multipath(int start_port, int num_ports, int dest_dist)
{
    //如果路由器之间只有一条链路，则只有一个端口可供选择
    if ( num_ports == 1 ) {
        return start_port;
    } else {
	//若有多条，则将与目标维度距离映射到可用端口范围之内
        return start_port + (dest_dist % num_ports);
    }
}
//【新增函数：用于在hm板间进行路由选择】
int
topo_mesh::choose_hmpath(int start_port, int num_ports, int dest_dist){

}


//返回网卡id/终端id(唯一)
int
topo_mesh::getEndpointID(int port)
{
    //如果这个端口不是连接网卡的端口
    if ( !isHostPort(port) ) return -1;
    return (router_id * num_local_ports) + (port - local_port_start);
}

