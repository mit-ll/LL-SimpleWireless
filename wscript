## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    obj = bld.create_ns3_module('simple-wireless', ['network'])
    obj.source = [
        'model/simple-wireless-net-device.cc',
        'model/simple-wireless-channel.cc',
        'model/drop-head-queue.cc',
        'model/priority-queue.cc',
        ]
    headers = bld(features='ns3header')
    headers.module = 'simple-wireless'
    headers.source = [
        'model/simple-wireless-net-device.h',
        'model/simple-wireless-channel.h',
        'model/drop-head-queue.h',
        'model/priority-queue.h',
        ]
    obj.env.append_value("LIB", ["pcap"])
    
    if (bld.env['ENABLE_EXAMPLES']):
        bld.recurse('examples')

