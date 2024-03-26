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

#总的来说这段代码定义了一个C接口，允许C或C++代码通过genMerlinPyModule函数与
#Python模块进行交互，这种技术通常用于扩展Python的功能，或者在C/C++项目中嵌入
#Python脚本
#ifndef COMPONENTS_MERLIN_PYMODULE_H
#define COMPONENTS_MERLIN_PYMODULE_H

#ifdef __cplusplus
#检查是否在C++环境中编译

#用于告诉编译器在这个{}块内的函数使用C语言
#的链接方式。这样做的目的是让C++编译器生成
#与C语言兼容的函数名，以便C代码可以链接这些函数。
extern "C" {
#endif

#这个函数的目的是加载Merlin Python模块。函数的具体实现在pymodule.cc文件中
void* genMerlinPyModule(void);

#ifdef __cplusplus
}
#endif
#endif // COMPONENTS_MERLIN_PYMODULE_H
