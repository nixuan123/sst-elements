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
#sst_config.h文件是SST核心配置文件
#include <sst_config.h>
#Python.h文件时Python解释器的C接口头文件
#include <Python.h>

#include "pymodule.h"

#定义了一个静态字符数组pymerlin，其中包含了Python代码的字节序列
#这个数组可能包含Python模块的源代码(pymerlin.inc)，它将被编译成Python模块
static char pymerlin[] = {
#include "pymerlin.inc"
    0x00};

void* genMerlinPyModule(void)
{
    // Must return a PyObject
    #将pymerlin数组中的Python代码编译成一个PyObject对象，这个对象代表了编译后的Python代码
    #"pymerlin"是提供的文件名，Py_file_input表示代码类型是文件输入
    PyObject *code = Py_CompileString(pymerlin, "pymerlin", Py_file_input);
    #最后，使用PyImport_ExecCodeModule函数执行编译后的代码，并创建一个新的模块对象
    return PyImport_ExecCodeModule("sst.merlin", code);
}

