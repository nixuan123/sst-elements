#!/usr/bin/env python
#
# Copyright 2009-2023 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2023, NTESS
# All rights reserved.
#
# Portions are copyright of other developers:
# See the file CONTRIBUTORS.TXT in the top level directory
# of the distribution for more information.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sst
from sst.merlin.base import *

#Python中定义了一个名为topoFatTree的类，它继承自Topology类
class topoFatTree(Topology):
    #构造函数
    def __init__(self):
        Topology.__init__(self)#调用父类的构造函数来初始化继承的属性
        #这些变量将被所有的topoFatTree类的实例共享，用于注册变量，以便他们可以
        #被正确的访问，网络链路的延迟时间、主机链路的延迟时间、端点的集合(例如哪些主机或设备连接到网络)
        #每个节点上行端口的数量、每个节点的下行端口数量、每个层级的路由器数量、每个层级的组、每个层级的起始id
        #网络中主机的总数
        self._declareClassVariables(["link_latency","host_link_latency","bundleEndpoints","_ups","_downs","_routers_per_level","_groups_per_level","_start_ids",
                                     "_total_hosts"])
        #main表示这些参数属于的主要配置类别、或者是全局配置的一部分、[fattree拓扑的结构、在拓扑中使用的路由算法、设置自适应路由的阈值]
        self._declareParams("main",["shape","routing_alg","adaptive_threshold"])        
        #设置了一个回调函数，当类的某个参数被写入或者修改时，这个回调函数将被触发
        self._setCallbackOnWrite("shape",self._shape_callback)
        #订阅了平台的参数集topology，它是一个包含了网络拓扑相关的所有参数的参数集
        #这用于确保类的配置和平台的全局拓扑配置保持一致
        self._subscribeToPlatformParamSet("topology")

    #回调函数，用于在shape参数被修改时指向特定的操作
    #他接受三个参数：方法所属的实例、被修改的变量名、变量的新值
    def _shape_callback(self,variable_name,value):
        #锁定或者保护变量，防止在参数变更处理的过程中被其他操作修改
        self._lockVariable(variable_name)
        #将传入的心智value赋给类的成员变量shape，意味着变量被更改时，同时shape也会更新为新的值
        shape = value
        
        # Process the shape
        self._ups = []
        self._downs = []
        self._routers_per_level = []
        self._groups_per_level = []#每层组数量
        self._start_ids = []#每层的起始ID

        #若shape是"4:2:1",则解析之后levels将为[4,2,1]
        levels = shape.split(":")

        #处理逐个层级的链路信息，即上行链路和下行链路的总数
        for l in levels:
            links = l.split(",")
            #将links数组中第一个元素(下行链路数)转换为整数，并将其追加到self._downs列表中
            self._downs.append(int(links[0]))
            #判断links的长度是否大于1，即是否还有上行链路的信息
            if len(links) > 1:
                # #将links数组中第二个元素(上行链路数)转换为整数，并将其追加到self._ups列表中
                self._ups.append(int(links[1]))
        
        #计算胖树拓扑的主机数量，不同层级(从接入层/第0层开始)单个路由器的下行端口数量累乘得到
        self._total_hosts = 1
        for i in self._downs:
            self._total_hosts *= i

        #用于存储每一个层级的路由器数量，使用列表推导式创建了一个新列表，其长度和
        #每层单个路由器的下行端口数量
        #因此，_routers_per_level列表将为拓扑中的每个层级预留一个位置，
        #用于存储该层级的路由器数量。
        self._routers_per_level = [0] * len(self._downs)
        #接入层(第0层)的路由器的数量 = 拓扑的主机数量 // 接入层的单个路由器下行端口数量
        #例如 一个[4,4:4,4:8] 的fattree拓扑 8 // 2 = 4
        self._routers_per_level[0] = self._total_hosts // self._downs[0]
        #遍历非接入层，从1遍历到层级数-1
        for i in range(1,len(self._downs)):
            self._routers_per_level[i] = self._routers_per_level[i-1] * self._ups[i-1] // self._downs[i]
        
        #初始化一个_start_ids字典，字典长度为层级数
        self._start_ids = [0] * len(self._downs)
        #起始路由器id从0开始，位于接入层的最左边，然后逐1递增
        for i in range(1,len(self._downs)):
            self._start_ids[i] = self._start_ids[i-1] + self._routers_per_level[i-1]

        #计算fattree拓扑中每一层的组数量。这里的组指的是一组路由器，它们
        #共同服务于一定范围内的主机或下一层的设备
        #初始化为1，意味着如果self._downs列表中的每个元素代表一个层级
        #那么初始化时，每个层级被假设为只有一个组
        self._groups_per_level = [1] * len(self._downs);
        #判断上行端口数组是否为空，如果不为空，则说明拓扑中存在多个层级，而不仅仅是单一层级
        if self._ups: # if ups is empty, then this is a single level and the following line will fail
            #计算接入层的组数量，即用总主机数整除下行端口数量，结果被赋值给
            #_group_per_level列表的第一个元素，即接入层的组数量
            self._groups_per_level[0] = self._total_hosts // self._downs[0]

        for i in range(1,len(self._downs)-1):
            self._groups_per_level[i] = self._groups_per_level[i-1] // self._downs[i]

        
    #获取拓扑的名称
    def getName(self):
        return "Fat Tree"


    #获取总主机数
    def getNumNodes(self):
        return self._total_hosts

    
    def getRouterNameForId(self,rtr_id):
        #获取start_ids列表的长度，并将其赋值给num_levels
        num_levels = len(self._start_ids)

        # Check to make sure the index is in range
        #检查给定的路由器ID(rtr_id)在Fattree拓扑中是否有效
        #level被赋值为根层的路由器的层级(num_level-1)
        level = num_levels - 1
        #如果路由器的id大于等于(根路由器id+根层的路由器数量)或者小于0
        #就会打印报错信息，因为根层最右的id是拓扑中最大的
        if rtr_id >= (self._start_ids[level] + self._routers_per_level[level]) or rtr_id < 0:
            print("ERROR: topoFattree.getRouterNameForId: rtr_id not found: %d"%rtr_id)
            sst.exit()

        # Find the level
        #根据给定的路由器id找寻它所在的层级，从根层开始找
        for x in range(num_levels-1,0,-1):
            #如果路由器id大于等于该层的起始id，则找到了
            if rtr_id >= self._start_ids[x]:
                break
            #否则层级递减
            level = level - 1

        # Find the group
        #之前已经算出该路由器所在层级，现在根据给定的路由器ID确定该路由器所在的组和路由器在组中的位置
        #代表当前路由器在该层级的偏移量，从左至右
        remainder = rtr_id - self._start_ids[level]
        
        #计算每个组中的路由器数量，当前层级的路由器数量整除组数量
        routers_per_group = self._routers_per_level[level] // self._groups_per_level[level]
        
        #计算当前路由器所在组的组号
        group = remainder // routers_per_group

        #计算路由器在组内的位置
        router = remainder % routers_per_group
        
        return self.getRouterNameForLocation((level,group,router))
            
    #在拓扑当中处理路由器的名称和位置信息
    def getRouterNameForLocation(self,location):
        #比如location=[1,0,0]的路由器返回字符串为 "rtr_l1_g0_r0"
        return "rtr_l%s_g%d_r%d"%(location[0],location[1],location[2])

    #在sst模块中查询是否有一个这个路由器名称的组件，如果没有就创建，如果有就返回
    def findRouterByLocation(self,location):
        return sst.findComponentByName(self.getRouterNameForLocation(location));
    
    
    
    def _build_impl(self, endpoint):
        #检查host_link_latency属性是否存在，如果不存在，着意味着没有为
        #主机链路延迟单独设置值
        if not self.host_link_latency:
            #将其设置为link_latency的值
            self.host_link_latency = self.link_latency
        
        #Recursive function to build levels
        #递归函数构建拓扑的各个层级
        def fattree_rb(self, level, group, links):
            #计算当前路由器的组内第一个路由器的id，通过该层的起始id和路由器所在组号和每个组路由器数量确定
            #假设当前路由器ID=4，location=[1,0,0],下面的id变量=4
            id = self._start_ids[level] + group * (self._routers_per_level[level]//self._groups_per_level[level])

            #初始化一个空字典，用于存储主机链路
            host_links = []
            
            #检查当前路由器是否在接入层
            if level == 0:
                # create all the nodes
                #在接入层的情况下，循环创建主机，循环从0开始到(下行端口数量-1)结束
                #假设当前的路由器ID=1 ，location=[0,1,0]
                for i in range(self._downs[0]):
                    #主机id = 1 * 2 + 0 = 2
                    node_id = id * self._downs[0] + i
                    #print("group: %d, id: %d, node_id: %d"%(group, id, node_id))
                    #调用build方法来构建一个节点，并返回一个包含节点对象的端口名称的元组
                    (ep, port_name) = endpoint.build(node_id, {})
                    #检查节点对象ep是否存在
                    if ep:
                        #创建一个新的连接主机的链接对象hlink,并使用主机ID作为标识
                        hlink = sst.Link("hostlink_%d"%node_id)
                        
                        #检查是否需要将连接标记为不可切断
                        if self.bundleEndpoints:
                           hlink.setNoCut()
                        
                        #将链接添加到节点，并指定端口名称和主机链路延迟
                        ep.addLink(hlink, port_name, self.host_link_latency)
                        
                        #将创建的链接添加到主机链路字典 host_links 中
                        host_links.append(hlink)

                # Create the edge router
                #记录当前路由器的ID
                rtr_id = id
                #创建一个接入层的路由器实例
                rtr = self._instanceRouter(self._ups[0] + self._downs[0], rtr_id)

                #为这个创建好的实例添加一个fattree拓扑
                topology = rtr.setSubComponent(self.router.getTopologySlotName(),"merlin.fattree")

                #应用统计设置到拓扑
                self._applyStatisticsSettings(topology)

                #为拓扑添加主要参数
                topology.addParams(self._getGroupParams("main"))
                
                # Add links
                #为接入层路由器添加主机链路到,host_links字典的长度为该路由器的下行端口数
                #也即该路由器所连接的主机数量
                for l in range(len(host_links)):
                    rtr.addLink(host_links[l],"port%d"%l, self.link_latency)
                
                #为接入层路由器添加路由器链接，links数组存储了当前层级(接入层)的上、下行链路数
                for l in range(len(links)):
                    rtr.addLink(links[l],"port%d"%(l+self._downs[0]), self.link_latency)
                return

            #处理非接入层的情况
            
            #计算每个组中的路由器数量
            rtrs_in_group = self._routers_per_level[level] // self._groups_per_level[level]
            
            # Create the down links for the routers
            #创建一个列表，包含了rtrs_in_group个空列表，用于存储该组所有路由器的组间的连接
            rtr_links = [ [] for index in range(rtrs_in_group) ]
            
            #循环创建组间连接，并存储在group_links列表中
            for i in range(rtrs_in_group):
                #循环遍历非接入层的下行端口，组间连接
                for j in range(self._downs[level]):
                    #循环遍历组中的路由器并为其添加下行链路，也叫组间连接
                    rtr_links[i].append(sst.Link("link_l%d_g%d_r%d_p%d"%(level,group,i,j)));

            # Now create group links to pass to lower level groups from router down links
            # 创建一个列表，包含了当前路由器下行端口数量个空列表，用于存储当前路由器的特定下行端口组间的连接
            group_links = [ [] for index in range(self._downs[level]) ]
            for i in range(self._downs[level]):
                for j in range(rtrs_in_group):
                    group_links[i].append(rtr_links[j][i])

            for i in range(self._downs[level]):
                fattree_rb(self,level-1,group*self._downs[level]+i,group_links[i])

            # Create the routers in this level.
            # Start by adding up links to rtr_links
            for i in range(len(links)):
                rtr_links[i % rtrs_in_group].append(links[i])

            for i in range(rtrs_in_group):
                rtr_id = id + i
                rtr = self._instanceRouter(self._ups[level] + self._downs[level], rtr_id)

                topology = rtr.setSubComponent(self.router.getTopologySlotName(),"merlin.fattree")
                self._applyStatisticsSettings(topology)
                topology.addParams(self._getGroupParams("main"))
                # Add links
                for l in range(len(rtr_links[i])):
                    rtr.addLink(rtr_links[i][l],"port%d"%l, self.link_latency)
        #  End recursive function

        level = len(self._ups)
        if self._ups: # True for all cases except for single level
            #  Create the router links
            rtrs_in_group = self._routers_per_level[level] // self._groups_per_level[level]

            # Create the down links for the routers
            rtr_links = [ [] for index in range(rtrs_in_group) ]
            for i in range(rtrs_in_group):
                for j in range(self._downs[level]):
                    rtr_links[i].append(sst.Link("link_l%d_g0_r%d_p%d"%(level,i,j)));

            # Now create group links to pass to lower level groups from router down links
            group_links = [ [] for index in range(self._downs[level]) ]
            for i in range(self._downs[level]):
                for j in range(rtrs_in_group):
                    group_links[i].append(rtr_links[j][i])


            for i in range(self._downs[len(self._ups)]):
                fattree_rb(self,level-1,i,group_links[i])

            # Create the routers in this level
            radix = self._downs[level]
            for i in range(self._routers_per_level[level]):
                rtr_id = self._start_ids[len(self._ups)] + i
                rtr = self._instanceRouter(radix,rtr_id);

                topology = rtr.setSubComponent(self.router.getTopologySlotName(),"merlin.fattree",0)
                self._applyStatisticsSettings(topology)
                topology.addParams(self._getGroupParams("main"))

                for l in range(len(rtr_links[i])):
                    rtr.addLink(rtr_links[i][l], "port%d"%l, self.link_latency)

        else: # Single level case
            # create all the nodes
            for i in range(self._downs[0]):
                node_id = i
#                print("Instancing node " + str(node_id))
        rtr_id = 0
#        print("Instancing router " + str(rtr_id))


