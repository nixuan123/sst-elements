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
            
        # Get the size in dimension通过shape字符串获取mesh的维度数组【4，4】
        self._dim_size = [int(x) for x in shape.split('x')]

        # Get the number of links in each dimension and check to
        # make sure the number of dimensions matches the shape【3，3】
        self._dim_width = [int(x) for x in width.split('x')]

        #获取mesh的维度【2】
        self._num_dims = len(self._dim_size)

        #两个数组的长度在逻辑上是相等的，都为2
        if len(self._dim_size) != len(self._dim_width):
            print("topo%s:  Incompatible number of dimensions set for shape (%s) and width (%s)."%(self.getName(),shape,width))
            exit(1)
    #下面两个方法没有具体实现，需要子类继承并实现
    def _getTopologyName():
        pass
        
    def _includeWrapLinks():
        pass

    #计算并返回拓扑结构中的节点路由器总数
    def getNumNodes(self):
        if not self._dim_size or not self.local_ports:
            print("topo%s: calling getNumNodes before shape, width and local_ports was set."%self.getName())
            exit(1)
        #num_routers变量用于累乘，来计算总的路由器数量
        num_routers = 1;
        for x in self._dim_size:
            num_routers = num_routers * x
        #返回路由器数量 x 每个路由器本地端口数量 = 总的节点（终端）数量 
        return num_routers * int(self.local_ports)


    def setShape(self,shape,width,local_ports):
        this.shape = shape
        this.width = width
        this.local_ports = local_ports
    #将arr[2,4]通过下面函数转化为"2x4"
    def _formatShape(self, arr):
        return 'x'.join([str(x) for x in arr])

    #将路由器标识符rtr_id转换为在拓扑上的位置foo
    def _idToLoc(self,rtr_id):
        foo = list()
        for i in range(self._num_dims-1, 0, -1):
            div = 1
            for j in range(0, i):
                div = div * self._dim_size[j]
            value = (rtr_id // div)
            foo.append(value)
            rtr_id = rtr_id - (value * div)
        foo.append(rtr_id)
        foo.reverse()
        return foo

    def getRouterNameForId(self,rtr_id):
        return self.getRouterNameForLocation(self._idToLoc(rtr_id))
    
    #根据路由器的位置返回路由器的名称，例如：rtr_3x3
    def getRouterNameForLocation(self,location):
        return "rtr_%s"%(self._formatShape(location))
    
    #在模拟系统sst中查找具有"rtr_3x3"名称的组件
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
        #计算路由器的数量
        num_routers = 1
        for x in self._dim_size:
            num_routers = num_routers * x

        #计算所有路由器总共的本地端口（终端）数
        num_peers = num_routers * local_ports

        #计算每个路由器所需的总的端口数
        radix = local_ports
        for x in range(num_dims):
            radix = radix + (self._dim_width[x] * 2)
            
        #在网络拓扑中创建和管理连接，通过getLink函数实现
        #创建一个空字典
        links = dict()
        #定义了一个getLink函数
        def getLink(leftName, rightName, num):
            #定义连接的名称：例如link_A_B_1、link_A_B_2
            name = "link_%s_%s_%d"%(leftName, rightName, num)
            if name not in links:
                links[name] = sst.Link(name)
            return links[name]

        #此循环从0遍历到num_routers(不包括num_routers)
        for i in range(num_routers):
            # set up 'mydims'
            #例如：mydims=[1,3]
            mydims = self._idToLoc(i)
            #例如mylocst=1x3
            mylocstr = self._formatShape(mydims)

            #实例化一个路由器对象
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
            for dim in range(num_dims):
                #复制当前路由器的位置信息
                theirdims = mydims[:]

                # Positive direction
                #检查当前路由器是否还没到最右端或者在最右端但是有环连接（torus）
                if mydims[dim]+1 < self._dim_size[dim] or self._includeWrapLinks():
                    #如果条件为真，计算相邻路由器的位置
                    theirdims[dim] = (mydims[dim] +1 ) % self._dim_size[dim]
                    #将相邻路由器的位置转换为"3x3"字符串的形式
                    theirlocstr = self._formatShape(theirdims)
                    #遍历当前维度的所有端口
                    for num in range(self._dim_width[dim]):
                        #将当前路由器的名称和相邻路由器的名称作为参数建立当前维度的link对象，连接的端口号由port指定，并且设置延迟为self.link_latency
                        rtr.addLink(getLink(mylocstr, theirlocstr, num), "port%d"%port, self.link_latency)
                        #每次添加连接后，port变量递增，准备为下一个链接设置端口
                        port = port+1
                #如果在该维度的最右边，则port加上连接数跳到负向进行负向端口的初始化与配置
                else:
                    port += self._dim_width[dim]

                # Negative direction
                #配负向，负向配完跳维度
                if mydims[dim] > 0 or self._includeWrapLinks():
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

