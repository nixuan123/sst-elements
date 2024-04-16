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
        self._routers_per_level = []#每个层级的路由器数量
        self._groups_per_level = []#每层组数量
        self._start_ids = []#每层的起始ID

        #若shape是"2,2:2,2:4",则解析之后levels将为[[2,2],[2,2],4]
        levels = shape.split(":")

        #处理逐个层级的链路信息，即上行链路和下行链路的总数
        for l in levels:
            #比如现在是第一个层级，links=[2,2]
            links = l.split(",")
            #将links数组中第一个元素(下行链路数)转换为整数，并将其追加到self._downs列表中
            self._downs.append(int(links[0]))
            #判断links的长度是否大于1，即是否还有上行链路的信息
            if len(links) > 1:
                # #将links数组中第二个元素(上行链路数)转换为整数，并将其追加到self._ups列表中
                self._ups.append(int(links[1]))
        此时_downs=[2,2,2],ups=[2,2,0]
        
        #计算胖树拓扑的主机数量，不同层级(从接入层/第0层开始)单个路由器的下行端口数量累乘得到
        self._total_hosts = 1
        for i in self._downs:
            self._total_hosts *= i

        #因此，_routers_per_level列表将为拓扑中的每个层级预留一个位置，
        #用于存储不同层级的路由器数量。
        self._routers_per_level = [0] * len(self._downs)
        
        #接入层(第0层)的路由器的数量 
        #例如 一个[4,4:4,4:8] 的fattree拓扑 8 // 2 = 4
        self._routers_per_level[0] = self._total_hosts // self._downs[0]
        
        #接着往上遍历非接入层，从1遍历到_downs数组的长度-1
        for i in range(1,len(self._downs)):
            self._routers_per_level[i] = self._routers_per_level[i-1] * self._ups[i-1] // self._downs[i]
        
        #初始化一个_start_ids字典,用于记录每个层级的起始路由器id,字典长度为_downs数组的长度
        self._start_ids = [0] * len(self._downs)
        
        #起始路由器id从0开始，位于接入层的最左边，然后逐1递增
        for i in range(1,len(self._downs)):
            self._start_ids[i] = self._start_ids[i-1] + self._routers_per_level[i-1]

        #对于k=4的fattree(或者说[2,2:2,2:4])，上面_start_ids=[0,8,16]
        
        #将每个层级的组数初始化为1
        self._groups_per_level = [1] * len(self._downs);
        
        #如果上行端口数组为空，说明是单个路由器构成的胖树，本身就是接入层的路由器
        if self._ups: # if ups is empty, then this is a single level and the following line will fail
            #将第0层的路由器数量设置为主机数量除以下行端口数量,为1
            self._groups_per_level[0] = self._total_hosts // self._downs[0]
        
        #如果上行端口数组不为空，计算其他层级的路由器数量信息
        for i in range(1,len(self._downs)-1):
            self._groups_per_level[i] = self._groups_per_level[i-1] // self._downs[i]

        
    #获取拓扑的名称
    def getName(self):
        return "Fat Tree"


    #获取总主机数
    def getNumNodes(self):
        return self._total_hosts

    #给拓扑中的路由器通过路由器id进行命名
    def getRouterNameForId(self,rtr_id):
        #获取start_ids列表的长度，并将其赋值给num_levels，用于记录当前拓扑的层级数
        num_levels = len(self._start_ids)

        # Check to make sure the index is in range
        #level层级赋值为顶层
        level = num_levels - 1

        #如果当前路由器id大于胖树拓扑中顶层最右边的路由器id或者说小于0，则报错
        if rtr_id >= (self._start_ids[level] + self._routers_per_level[level]) or rtr_id < 0:
            print("ERROR: topoFattree.getRouterNameForId: rtr_id not found: %d"%rtr_id)
            sst.exit()

        # Find the level
        #循环从num_levels-1开始到0(不包括0)结束，递减频率为1
        for x in range(num_levels-1,0,-1):
            #如果路由器id大于等于该层的起始id，则路由器位于当前层级
            if rtr_id >= self._start_ids[x]:
                break#跳出for循环，level变量的值即为路由器位于的层级
            #否则层级递减
            level = level - 1

        # Find the group
        #之前已经算出该路由器所在层级 level，现在算路由器在组内的偏移量
        remainder = rtr_id - self._start_ids[level]
        
        #计算每个组中的路由器数量
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
        #主机链路延迟单独设置值
        if not self.host_link_latency:
            #如果没有设置就将其设置为link_latency的值
            self.host_link_latency = self.link_latency
        
        #Recursive function to build levels
        #采用一个递归函数fattree_rb构建整个拓扑
        def fattree_rb(self, level, group, links):
            #假设构建一个level层级的胖树
            id = self._start_ids[level] + group * (self._routers_per_level[level]//self._groups_per_level[level])

            #初始化一个空字典，用于存储主机链路
            host_links = []
            
            #检查当前路由器是否在接入层
            if level == 0:
                # create all the nodes
                #在接入层的情况下创建该路由器的所有的主机节点
                for i in range(self._downs[0]):
                    #为该路由器连接的第一个主机编号
                    node_id = id * self._downs[0] + i
                    #print("group: %d, id: %d, node_id: %d"%(group, id, node_id))
                    
                    #调用build方法来构建一个节点，主机编号为node_id
                    (ep, port_name) = endpoint.build(node_id, {})
                    
                    #如果节点创建成功，开始为节点分配链接
                    if ep:
                        #创建一个新的连接主机的链接对象hlink,并使用主机ID作为标识
                        hlink = sst.Link("hostlink_%d"%node_id)
                        
                        #检查是否需要将连接标记为不可切断
                        if self.bundleEndpoints:
                           hlink.setNoCut()
                        
                        #将链接添加到主机节点
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
                #将之前创建好的链路以及链路信息添加到当前的接入层rtr路由器中，处理R2N链路
                for l in range(len(host_links)):
                    rtr.addLink(host_links[l],"port%d"%l, self.link_latency)
                
                #为接入层路由器添加路由器链接，links数组存储了当前层级(接入层)的上、下行链路数，处理R2R链路
                for l in range(len(links)):
                    rtr.addLink(links[l],"port%d"%(l+self._downs[0]), self.link_latency)
                return

            #处理非接入层的情况，非接入层的链路都是R2R
            
            #计算每个组中的路由器数量
            rtrs_in_group = self._routers_per_level[level] // self._groups_per_level[level]
            
            # Create the down links for the routers
            #为当前路由器创建下行链路
            rtr_links = [ [] for index in range(rtrs_in_group) ]
            
            #循环创建组间连接，并存储在group_links列表中
            for i in range(rtrs_in_group):
                #循环遍历非接入层的下行端口，组间连接
                for j in range(self._downs[level]):
                    #循环遍历组中的路由器并为其添加下行链路，l-层级 g-组号 r-偏移量 p-端口号
                    rtr_links[i].append(sst.Link("link_l%d_g%d_r%d_p%d"%(level,group,i,j)));

            #上面for循环之后，对于k=4，id=8的路由器来说，rtr_links=[[link_l1_g0_r0_p0,link_l1_g0_r0_p1],[link_l1_g0_r1_p0,link_l1_g0_r1_p1]]

            # Now create group links to pass to lower level groups from router down links
            #这个数组存储的是组连接，它是R2R连接，相当于同组中相同端口号的连接的集合
            group_links = [ [] for index in range(self._downs[level]) ]
            for i in range(self._downs[level]):
                for j in range(rtrs_in_group):
                    group_links[i].append(rtr_links[j][i])
            
            #上面for循环之后，对于k=4，id=8的路由器来说，group_links=[[link_l1_g0_r0_p0,link_l1_g0_r1_p0],[link_l1_g0_r0_p1,link_l1_g0_r1_p1]]

            #构建当前路由器下行端口数量的路由器的拓扑
            for i in range(self._downs[level]):
                fattree_rb(self,level-1,group*self._downs[level]+i,group_links[i])

            # Create the routers in this level.
            # Start by adding up links to rtr_links,接着向rtr_links列表中添加上行链路信息
            
            for i in range(len(links)):
                rtr_links[i % rtrs_in_group].append(links[i])

            for i in range(rtrs_in_group):
                rtr_id = id + i
                rtr = self._instanceRouter(self._ups[level] + self._downs[level], rtr_id)
                
                #定义路由器的拓扑插槽名称，它是属于fattree类型的
                topology = rtr.setSubComponent(self.router.getTopologySlotName(),"merlin.fattree")
                
                #应用统计设置到拓扑组件上，为了收集和分析网络性能数据
                self._applyStatisticsSettings(topology)

                #添加一组参数到拓扑组件中，包括网络特定的配置，比如带宽、延迟等
                topology.addParams(self._getGroupParams("main"))
                
                # Add links
                #将每条链路添加到路由器上
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
        
        #单个路由器的情况
        else: # Single level case
            # create all the nodes
            for i in range(self._downs[0]):
                node_id = i
#                print("Instancing node " + str(node_id))
        rtr_id = 0
#        print("Instancing router " + str(rtr_id))


