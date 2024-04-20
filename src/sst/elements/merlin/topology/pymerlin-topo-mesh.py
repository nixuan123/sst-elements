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

# Class to do most of the work for both mesh and torus
#定义了一个_topoMeshBase类，调用的时父类Topology的构造参数
class _topoMeshBase(Topology):
    def __init__(self):
        #父类Topology的构造参数，初始化继承Topology的属性和方法
        Topology.__init__(self)
        #_declareClassVariables是一个类方法，用于声明类的实例变量
        self._declareClassVariables(["link_latency","host_link_latency","bundleEndpoints","_num_dims","_dim_size","_dim_width"])
        #_declareParams用于声明类的参数，这些参数可以在类的实例化时设置
        self._declareParams("main",["shape", "width", "local_ports"])
        #self._defineOptionalParams([])
        #当shape、width或local_ports被修改时，会调用self._shape_callback方法
        self._setCallbackOnWrite("shape",self._shape_callback)
        self._setCallbackOnWrite("width",self._shape_callback)
        self._setCallbackOnWrite("local_ports",self._shape_callback)
        #订阅平台参数集，使得类的实例可以接收和响应来自模拟平台的参数更新。在这个例子中，它订阅了名为“topology”的参数集
        self._subscribeToPlatformParamSet("topology")

    #当类的实例中的shape或width参数被修改时，它会被自动调用，目的时确保在修改
    #拓扑的形状或宽度参数时，类的内部状态保持一致和同步。
    def _shape_callback(self,variable_name,value):
        #锁定指定的变量，放置回调函数执行期间，其他代码修改该变量的值，确保数据的一致性
        self._lockVariable(variable_name)
        #如果改变的变量是local_ports，则可以直接返回，因为他的改动不会影响后面的计算
        if variable_name == "local_ports":
            return
        #判断shape和width变量是否被锁定，如果没有被锁定，则直接返回，因为要确保在执行
        #shape和width变量计算之前，两个变量是已经被锁定了的
        if not self._areVariablesLocked(["shape","width"]):
            return
        #如果被改动的变量是shape，width的值也要随之改变
        if variable_name == "shape":
            shape = value
            width = self.width
        #如果被改动的变量是width，shape的值也要随之改变
        else:
            shape = self.shape
            width = value
            
        # Get the size in dimension通过shape字符串获取mesh的维度数组[2,2,2,2]
        self._dim_size = [int(x) for x in shape.split('x')]

        # Get the number of links in each dimension and check to
        # make sure the number of dimensions matches the shape[1,1]
        self._dim_width = [int(x) for x in width.split('x')]

        #获取mesh的维度【4】固定值为4
        self._num_dims = len(self._dim_size)

        
        #两个数组的长度在逻辑上是相等的，都为2
        #if len(self._dim_size) != len(self._dim_width):
            #print("topo%s:  Incompatible number of dimensions set for shape (%s) and width (%s)."%(self.getName(),shape,width))
            #exit(1)
    
    #下面两个方法没有具体实现，需要子类继承并实现
    def _getTopologyName():
        pass
        
    def _includeWrapLinks():
        pass

    #计算并返回拓扑结构中的主机总数
    def getNumNodes(self):
        if not self._dim_size or not self.local_ports:
            print("topo%s: calling getNumNodes before shape, width and local_ports was set."%self.getName())
            exit(1)
        #num_routers变量用于累乘，来计算hm拓扑中总的路由器数量
        num_routers = 1;
        for x in self._dim_size:
            num_routers = num_routers * x
        #返回总的主机数量 
        return num_routers * int(self.local_ports)
    
    #【新增】计算并返回拓扑结构中的路由器总数
    def getNumRouters(self):
        if not self._dim_size:
            print("topo%s: calling getNumRouters before shape, width was set."%self.getName())
            exit(1)
        #num_routers变量用于累乘，来计算hm拓扑中总的路由器数量
        num_routers = 1;
        for x in self._dim_size:
            num_routers = num_routers * x
        #返回总的路由器数量 
        return num_routers
    
    #【新增】计算并返回路由器的hm_id
    def getHmid(self, rtr_id):
        return rtr_id // (self._dim_size[0] * self._dim_size[1])

    #【新增】获取hm拓扑一列和一行所需要的端口数
    def _switch_x_and_y_ports(self):
        switchxy = list((0 for _ in range(2)))
        switchxy[0] = 2 * self._dim_size[0] * self._dim_size[2]
        switchxy[1] = 2 * self._dim_size[1] * self._dim_size[3]
    return switchxy

    def setShape(self,shape,width,local_ports):
        this.shape = shape
        this.width = width
        this.local_ports = local_ports
    
    #将arr[1,1,1,1]通过下面函数转化为'1x1x1x1'
    def _formatShape(self, arr):
        return 'x'.join([str(x) for x in arr])

    #将路由器标识符rtr_id转换为在拓扑上的位置location【改动】
    def _idToLoc(self, rtr_id):
        hm_id = self.getHmid(rtr_id)
        location = list((0 for _ in range(self._num_dims)))
        r_id = rtr_id
    
        # 计算 switch_fid
        switch_fid = self._dim_size[0] * self._dim_size[1] * self._dim_size[2] * self._dim_size[3]
    
        # 根据 rtr_id 的值设置 location 数组
        if 0 < rtr_id < switch_fid:
            # 设置前两位
            inner_id=rtr_id%(_dim_size[0]*_dim_size[1])
            location[0] = inner_id % _dim_size[1]
            location[1] = inner_id // _dim_size[1]
        
            # 设置后两位
            hm_id = rtr_id // (_dim_size[0] * _dim_size[1])
            location[2] = hm_id % _dim_size[3]
            location[3] = hm_id // _dim_size[3]
        elif switch_fid <= rtr_id < switch_fid + self._dim_size[2]:
            # 设置前两位为 rtr_id
            location[0] = r_id
            location[1] = r_id
    
            # 设置后两位
            num = rtr_id - switch_fid
            location[2] = r_id
            location[3] = num
        elif switch_fid + self._dim_size[2] <= rtr_id < switch_fid + self._dim_size[2] + self._dim_size[3]:
            # 设置前两位为 rtr_id
            location[0] = r_id
            location[1] = r_id
    
            # 设置后两位
            num = rtr_id - (switch_fid + self._dim_size[2])
            location[2] = num
            location[3] = r_id
        else:
            # 如果 rtr_id 不在预期的范围内，设置每个位置为 rtr_id
            for i in range(self._num_dims):  # 这里应该是 self._num_dims 而不是 dimensions
                location[i] = r_id
    
        return location
       
    def getRouterNameForId(self,rtr_id):
        return self.getRouterNameForLocation(self._idToLoc(rtr_id))
    
    #根据路由器的位置返回路由器的名称，例如：rtr_1x1x1x1
    def getRouterNameForLocation(self,location):
        return "rtr_%s"%(self._formatShape(location))
    
    #在模拟系统sst中查找具有"rtr_1x1x1x1"名称的组件
    def findRouterByLocation(self,location):
        return sst.findComponentByName(self.getRouterNameForLocation(location))

    #这个构建方法是在构建拓扑结构的实现阶段设置主机链接的延迟
    def _build_impl(self, endpoint):
        if self.host_link_latency is None:
            self.host_link_latency = self.link_latency
        
        # get some local variables from the parameters
        local_ports = int(self.local_ports)
        num_dims = len(self._dim_size)

        # Calculate number of routers and endpoints
        #计算路由器的数量（hm板子上的路由器数量）
        num_routers = 1
        for x in self._dim_size:
            num_routers = num_routers * x

        #计算hm拓扑中行/列交换机(路由器)的数量
        num_switchx = self._dim_size[2]
        num_switchy = self._dim_size[3]
        num_switches = num_switchx + num_switchy

        #计算hm板子上所有路由器总共的本地端口(终端)数[不包括行列交换机]
        num_peers = num_routers * local_ports

        #计算hm板子上的单个路由器所需的总的端口数
        radix = local_ports
        for x in range(num_dims):
            radix = radix + (self._dim_width[x] * 2)

        #计算hm拓扑中行/列交换机(路由器)所需要的端口数
        radix_switchxy = list()
        radix_switchxy = self._switch_x_and_y_ports
        
        #建立一个字典，用于存储交换机实例
        switches = dict()  
        
        #创建一个变量，记录交换机的起始id
        switch_fid = self._dim_size[0] * self._dim_size[1] * self._dim_size[2] * self._dim_size[3]
        #创建一个字典，记录交换机们的id,例如switch_ids=[16,17,18,19]
        switch_ids= dict()
        for i in range(num_switches):
            switch_ids[i]=switch_fid+i
        
        #创建一个字典，用于记录相应交换机遍历到的端口号，将他们初始化为0
        #例如switch_port=[[16,0],[17,0],[18,0],[19,0]]
        switch_port = dict()
        for i in range(num_switches):
            switch_port[switch_ids[i]]=-1

        #创建一个字典，用于存储交换机映射后的位置
        #例如switch_loc=[[16,16,16,0],[17,17,17,1],[18,18,0,18],[19,19,1,19]]
        switch_loc= dict()
        for i in range(num_switches):
            switch_loc[i]=self._idToLoc(switch_ids[i])
            
        #在网络拓扑中创建和管理连接，通过getLink函数实现
        #创建一个空字典,可以以键值对的方式存储
        links = dict()
        #定义了一个getLink函数
        def getLink(leftName, rightName, num):
            #定义连接的名称：例如link_0x1x0x0_1x1x0x0_0
            name = "link_%s_%s_%d"%(leftName, rightName, num)
            if name not in links:
                links[name] = sst.Link(name)
            return links[name]

        #只有处在边缘位置的路由器才需要调用这个函数 
        def edgeRouterGetSwitchIds(rtr_id):
            #定义一个列表用于保存当前路由器的行列交换机信息
            my_switches=list()
            #先获取路由器的位置，例如dim_size=[2,2,2,2]的情况下,loc=[1,0,1,0]
            loc=self._idToLoc(rtr_id)
            #判断路由器是否在对角的位置,如果是对角位置，需要返回行、列两个交换机的id
            if ((loc[0]==0&&loc[1]=0)||(loc[0]==self._dim_size[1]-1&&loc[1]==0)||(loc[0]==0&&loc[1]==self._dim_size[0]-1)
               ||(loc[0]==self._dim_size[1]-1&&loc[1]==self._dim_size[0]-1)):
                   #找寻行交换机
                   for value in switch_loc.value():
                       if value[3]==loc[3]:
                           #对应的全局变量端口+1
                           switch_port[value[0]]+=1
                           break
                   #找寻列交换机
                   for value in switch_loc.value():
                       if value[2]==loc[2]:
                           #对应的全局变量端口+1
        
                           
               
        
        
        #创建交换机【新增】
        for i in range(num_switches)
            count = switch_fid+i
            # set up 'mydims'
            #例如：mydims=[16,16,16,0]
            mydims = self._idToLoc(count)
            #例如mylocst='16x16x16x0'
            mylocstr = self._formatShape(mydims)
            num_hx_routers=self._dim_size[0]*self._dim_size[1]*self._dim_size[2]
            num_hy_routers=self._dim_size[0]*self._dim_size[1]*self._dim_size[3]
            if i < num_switchx:
                #创建x维度的交换机
                rtr=self._instanceRouter(radix_switchxy[0],count)
                switches[mylocstr]=rtr
                topology = rtr.setSubComponent(self.router.getTopologySlotName(),self._getTopologyName())
                self._applyStatisticsSettings(topology)
                topology.addParams(self._getGroupParams("main"))
                #配置连接
                
                port=0
                for i in (radix_switchxy[0]):
                    
            else:    
                #创建y维度的交换机
                rtr=self._instanceRouter(radix_switchxy[1],count+self.dim_size[2]+j)
                switches[mylocstr]=rtr
        
        #添加hm板上的链路
        for i in range(num_routers):
            #配置板子上的路由器对象和链路
               # set up 'mydims'
               #例如：mydims=[1,1,1,1]
               mydims = self._idToLoc(i)
               #例如mylocst='1x1x1x1'
               mylocstr = self._formatShape(mydims)

               #实例化一个路由器对象，他的端口数为radix,rtr是一个Component
               rtr = self._instanceRouter(radix,i)

               #设置一个路由器组件的拓扑结构
               #getTopologySlotName是为了获取拓扑结构在路由器组件中的位置或者标识符
               #topology接收一个与拓扑相关的SubComponent组件
               topology = rtr.setSubComponent(self.router.getTopologySlotName(),self._getTopologyName())
               
               #应用统计设置topology对象，包括设置如何收集和报告网络中的性能数据，包括传输延迟、吞吐量等
               self._applyStatisticsSettings(topology)
               
               #用于给topology对象添加一组参数，与"main"相关的
               topology.addParams(self._getGroupParams("main"))

               #port变量用于跟踪当前的端口号
               port = 0
               #下面的for循环用于表示与当前路由器相连的其他路由器在拓扑中的位置
               #现在规定只有两个维度
               for dim in range (2):
                   #复制当前路由器的位置信息，例如0x0x0x0
                   theirdims = mydims[:]

                   # Positive direction
                   #如果路由器的前两个维度的数值加1要小于前两个维度的大小
                   #说明路由器的相邻路由器还是在hm板子内
                   if mydims[dim]+1 < self._dim_size[dim]:
                       #如果条件为真，计算相邻路由器的位置，例如1x0x0x0
                       theirdims[dim] = (mydims[dim] +1 )
                       #将相邻路由器的位置转换为"1x0x0x0"字符串的形式
                       theirlocstr = self._formatShape(theirdims)
                       #遍历当前维度的所有端口
                       for num in range(self._dim_width[dim]):
                           #将当前路由器的名称和相邻路由器的名称作为参数建立当前维度的link对象，连接的端口号由port指定，并且设置延迟为self.link_latency
                           rtr.addLink(getLink(mylocstr, theirlocstr, num), "port%d"%port, self.link_latency)
                           #每次添加连接后，port变量递增，准备为下一个链接设置端口
                           port = port+1
                   #如果在hm板子的最右边，需要添加两条链路分别连接行和列交换机，然后跳到负向进行负向端口的初始化与配置
                   else:
                       #计算需要连接的交换机位置
                       #写一个函数
                       rtr.addLink(getLink())
                       port += self._dim_width[dim]

                   # Negative direction
                   #配负向，负向配完跳维度
                   if mydims[dim] > 0:
                       theirdims[dim] = ((mydims[dim] -1) + self._dim_size[dim]) % self._dim_size[dim]
                       theirlocstr = self._formatShape(theirdims)
                       for num in range(self._dim_width[dim]):
                           rtr.addLink(getLink(theirlocstr, mylocstr, num), "port%d"%port, self.link_latency)
                           port = port+1
                   else:
                       port += self._dim_width[dim]

               for n in range(local_ports):
                   nodeID = local_ports * i + n
                   (ep, port_name) = endpoint.build(nodeID, {})
                   if ep:
                       nicLink = sst.Link("nic.%d:%d"%(i, n))
                       if self.bundleEndpoints:
                          nicLink.setNoCut()
                       nicLink.connect( (ep, port_name, self.host_link_latency), (rtr, "port%d"%port, self.host_link_latency) )
                   port = port+1
         


class topoMesh(_topoMeshBase):

    def __init__(self):
        _topoMeshBase.__init__(self)

    def getName(self):
        return "Mesh"

    def _getTopologyName(self):
        return "merlin.mesh"

    def _includeWrapLinks(self):
        return False


class topoTorus(_topoMeshBase):

    def __init__(self):
        _topoMeshBase.__init__(self)

    def getName(self):
        return "Torus"

    def _getTopologyName(self):
        return "merlin.torus"

    def _includeWrapLinks(self):
        return True


class topoSingle(Topology):

    def __init__(self):
        Topology.__init__(self)
        self._declareClassVariables(["link_latency","bundleEndpoints"])
        self._declareParams("main",["num_ports"])
        self._subscribeToPlatformParamSet("topology")

    def getName(self):
        return "Single Router"

    def getNumNodes(self):
        return self.num_ports

    def getRouterNameForId(self,rtr_id):
        return "router"
        
    def _build_impl(self, endpoint):
        rtr = self._instanceRouter(self.num_ports,0)

        topo = rtr.setSubComponent(self.router.getTopologySlotName(),"merlin.singlerouter",0)
        self._applyStatisticsSettings(topo)
        topo.addParams(self._getGroupParams("main"))
        
        for l in range(self.num_ports):
            (ep, portname) = endpoint.build(l, {})
            if ep:
                link = sst.Link("link%d"%l)
                if self.bundleEndpoints:
                    link.setNoCut()
                link.connect( (ep, portname, self.link_latency), (rtr, "port%d"%l, self.link_latency) )

