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

class TestJob(Job):
    def __init__(self,job_id,size):
        Job.__init__(self,job_id,size)
        #声明函数声明了作业和ID的大小、对等体数量、消息数量、消息大小、是否发送为定时的广播
        self._declareParams("main",["num_peers","num_messages","message_size","send_untimed_bcast"])
        self.num_peers = size
        self._lockVariable("num_peers")

    #getName方法返回作业的名称，这里是叫做TestJob
    def getName(self):
        return "TestJob"
    #build方法用于构建网络接口组件NIC，并为其设置统计信息和参数，他还设置了NIC的
    #逻辑节点ID，并实例化了一个网络接口
    def build(self, nID, extraKeys, link = None):
        nic = sst.Component("testNic_%d"%nID, "merlin.test_nic")
        self._applyStatisticsSettings(nic)
        nic.addParams(self._getGroupParams("main"))
        nic.addParams(extraKeys)
        # Get the logical node id
        id = self._nid_map[nID]
        nic.addParam("id", id)

        #  Add the linkcontrol
        return NetworkInterface._instanceNetworkInterfaceBackCompat(
            self.network_interface,nic,"networkIF",0,self.job_id,self.size,id,True,link)

#初始化时，设置作业的ID和带线啊哦，并声明了一组不同的参数，如offered_load(提供的负载)、link_bw
#(链路的带宽)、warmup_time(预热时间)等。
class OfferedLoadJob(Job)://构造函数，接受job_id（作业的唯一标识符）和size（作业的大小）
    def __init__(self,job_id,size):
        #初始化父类
        Job.__init__(self,job_id,size)
        #声明参数：用于配置和运行模拟的关键参数，如提供的负载、对等体数量、
        #消息大小、链路带宽、预热时间收集时间和排空时间
        self._declareParams("main",["offered_load","num_peers","message_size","link_bw","warmup_time","collect_time","drain_time"])
        #声明类变量，它是在类级别上共享的变量，所有类的实例都可以访问它
        self._declareClassVariables(["pattern"])
        #将size参数的值赋给num_peers属性，这表明作业的主机数量由size决定
        self.num_peers = size
        #锁定变量num_peers，确保num_peers的值在作业执行期间不会被意外修改
        self._lockVariable("num_peers")
    #设置作业的名称，可被用于在模拟的控制台输出、日志文件或者用户界面显示作业的名称
    def getName(self):
        return "Offered Load Job"
    
    #build方法，他接受节点ID和额外的参数键值对
    def build(self, nID, extraKeys):
        #创建了一个sst.Component实例，它是模拟的一个网络接口卡
        nic = sst.Component("offered_load_%d"%nID, "merlin.offered_load")
        #将统计设置应用到新创建的网络接口卡上，让网卡也能拥有进行统计消息的能力
        self._applyStatisticsSettings(nic)
        #通过调用 _getGroupParams("main") 获取了一些主要参数，
        #并使用 addParams 方法将它们添加到 nic 对象中。
        nic.addParams(self._getGroupParams("main"))
        nic.addParams(extraKeys)
        #从_nid_map字典中获取了一个与nID相关联的id
        id = self._nid_map[nID]
        #使用addParam方法将其添加到nic对象的参数中
        nic.addParam("id", id)

        # Add pattern generator
        #，生成某种模式或数据流
        self.pattern.addAsAnonymous(nic, "pattern", "pattern.")

        #  Add the linkcontrol
        #调用self.network_interface.build方法来构建网络接口，并传递了nic对象
        #和其他的一些参数。这个方法返回了一个元组，其中包含了网络接口对象和端口的名称
        networkif, port_name = self.network_interface.build(nic,"networkIF",0,self.job_id,self.size,id,True)

        return (networkif, port_name)

#IncastJob类用于创建和管理在一个仿真环境中的"incast"类型的作业
#总的来说，这段代码是在一个仿真环境中构建一个网络接口卡组件，并为
#其配置了一些参数和模式生成器，用于模拟一个节点向多个节点发送数据
#包的“incast”类型的网络流量模式。这个组件可能是用于测试和评估网络
#在面对大量入站（inbound）流量时的性能和行为。
class IncastJob(Job):
    #传入job_id和size参数来初始化IncastJob实例
    def __init__(self,job_id,size):
        #声明了一系列主要的参数，如主机数量，目标节点的标识符，要发送的数据包数量
        #数据包的大小和开始发送数据包前的延迟
        Job.__init__(self,job_id,size)
        self._declareParams("main",["num_peers","target_nids","packets_to_send","packet_size","delay_start"])
        self.num_peers = size
        self._lockVariable("num_peers")

    def getName(self):
        return "Incast Job"

    def build(self, nID, extraKeys):
        //创建了一个网卡对象
        nic = sst.Component("incast_%d"%nID, "merlin.simple_patterns.incast")
        self._applyStatisticsSettings(nic)
        nic.addParams(self._getGroupParams("main"))
        nic.addParams(extraKeys)
        id = self._nid_map[nID]

        #  Add the linkcontrol
        networkif, port_name = self.network_interface.build(nic,"networkIF",0,self.job_id,self.size,id,True)
        return (networkif, port_name)
