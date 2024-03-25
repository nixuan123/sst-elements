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
from sst.merlin import *

if __name__ == "__main__":
    #创建一个叫topos的字典，其中包含了不同的拓扑实例，字典的键是整数，代表拓扑的ID，而
    #值是通过调用相应拓扑类的构造函数创建的拓扑对象。
    topos = dict( [(1,topoTorus()), (2,topoFatTree()), (3,topoDragonFly()), (4,topoSimple()), (5,topoMesh()), (6,topoDragonFly2()) ])
    #创建了一个名为endpoint的字典，其中包含了不同类型端点的实例。比如（1，TestEndpoint）创建了一个测试端点
    #的实例，并将其与键1关联。
    endpoints = dict([(1,TestEndPoint()), (2, TrafficGenEndPoint()), (3, BisectionEndPoint())])
    #在模拟环境中设置不同的拓扑结构、端点和统计输出方式，以便进行网络模拟实验
    statoutputs = dict([(1,"sst.statOutputConsole"), (2,"sst.statOutputCSV"), (3,"sst.statOutputTXT")])


    #Merlin系统涉及语言生成器
    print "Merlin SDL Generator\n"

    #用户选择拓扑
    print "Please select network topology:"
    for (x,y) in topos.iteritems():
        print "[ %d ]  %s" % (x, y.getName() )
    topo = int(raw_input())
    if topo not in topos:
        print "Bad answer.  try again."
        sys.exit(1)

    topo = topos[topo]


    #用户选择端点
    print "Please select endpoint:"
    for (x,y) in endpoints.iteritems():
        print "[ %d ]  %s" % (x, y.getName() )
    ep = int(raw_input())
    if ep not in endpoints:
        print "Bad answer. try again."
        sys.exit(1)


    endPoint = endpoints[ep];
    #模拟框架的参数配置中指定了交叉开关的仲裁策略为最近最少使用策略。
    sst.merlin._params["xbar_arb"] = "merlin.xbar_arb_lru"


    print "Set statistics load level (0 = off):"
    stats = int(raw_input())
    if ( stats != 0 ):
        print "Statistic dump period (0 = end of sim only):"
        rate = raw_input();
        if ( rate == "" ):
            rate = "0"
        sst.setStatisticLoadLevel(stats)

        #用户选择统计输出的类型
        print "Please select statistics output type:"
        for (x,y) in statoutputs.iteritems():
            print "[ %d ]  %s" % (x, y)
        output = int(raw_input())
        if output not in statoutputs:
            print "Bad answer.  try again."
            sys.exit(1)

        #这段代码让用户选择统计输出类型，并为需要文件输出
        #的统计类型设置文件名和字段分隔符。这样，模拟运行时，
        #统计数据将根据用户的配置被输出到指定的文件中。
        sst.setStatisticOutput(statoutputs[output]);
        if (output != 1):
            print "Filename for stats output:"
            filename = raw_input()
            sst.setStatisticOutputOptions({
                    "filepath" : filename,
                    "separator" : ", "
                    })
        #当 enableAllStatistics 方法被调用时，
        #endPoint 对象将开始收集各种统计数据，并根据
        #rate 参数指定的频率进行更新或输出。这使得模拟的
        #用户或开发者可以在模拟过程中监控网络性能和行为，或
        #者在模拟结束后进行分析。
        endPoint.enableAllStatistics(rate)


    topo.prepParams()
    endPoint.prepParams()
    topo.setEndPoint(endPoint)
    topo.build()

    if ( stats != 0 ):
        sst.enableAllStatisticsForComponentType("merlin.hr_router", {"type":"sst.AccumulatorStatistic",
                                                                     "rate":rate});
        #stats.append("port%d_send_bit_count"%l)
        #stats.append("port%d_send_packet_count"%l)
        #stats.append("port%d_xbar_stalls"%l)

