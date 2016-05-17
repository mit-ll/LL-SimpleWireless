.. include:: replace.txt
.. highlight:: cpp

SimpleWireless
--------------

|ns3| nodes can contain a collection of NetDevice objects, much like an actual
computer contains separate interface cards for Ethernet, Wifi, Bluetooth, etc.
This chapter describes the |ns3| SimpleWirelessNetDevice and related models. 
By adding SimpleWirelessNetDevice objects to |ns3| nodes, one can create models
for wireless infrastructure and ad hoc networks.

Overview of the model
*********************

SimpleWireless models a wireless network interface controller that is
not based on any specific protocol. Instead, it provides a wireless interface that
is simple in protocol but still allows the user to configure it with useful features.
We will go into more detail below on these features but in brief,
|ns3| SimpleWireless provides models for these aspects of wireless communications:

* propagation delay
* transmission delay based on the channel data rate (MIT LL)
* support for specifying transmission range
* support for multiple types of error including constant, range based, and stochastic (MIT LL)
* queue support with user configurable queue method (MIT LL)
* queueing of packets to limit the data rate (MIT LL)
* support for differentiating control and data traffic (MIT LL)
* pcap capture of packets sent and received (MIT LL)
* support for simulated directional networking through fixed neighbor lists (MIT LL)
* support for fixed contention (MIT LL)


The items marked with (MITLL) are items that have been added by MIT Lincoln Laboratory to
the basic simple wireless model developed by Tom Henderson (available at
http://code.nsnam.org/tomh/ns-3-simple-wireless)

SimpleWireless does not include MAC protocols and does not model interference.

In |ns3|, nodes can have multiple SimpleWirelessNetDevices on separate channels, and the
SimpleWirelessNetDevice can coexist with other device types. Presently, however, there is
no model for cross-channel interference or coupling.

The source code for the SimpleWireless should be installed in the directory
``src/simplewireless``.

The implementation provides two levels of models:

* the PHY layer models (SimpleWirelessChannel)
* the device layer model (SimpleWirelessNetDevice)

Change Log
=======================
**Version 0.3.3**
* Added MacTx and MacRx traces to net-device so that simple wireless has this
trace that exists in MAC models
* Fix bug in net-device which switches the source and destination MAC addresses
during enqueing. Bug only affected scenarios in which queues are NOT used.
* Modify the queue latency trace in net-device to take a pointer to the packet
* Modify net-device to call queue latency trace before stripping the Ethernet
header from packet so that information from the header is available in the
trace for users to access

**Version 0.3.2**
* Modify implementation for directional networks so the packets are queued
at the device for each directional neighbor that is a destination for the
packet being sent. This includes support for sending unicast such that the
packet is only queued for that one directional neighbor.
* Modify the contention implementation so that the node itself is included
in the count of nodes sharing the bandwidth and also add support for 
directional networks. 
  
**Version 0.3.1**
* Fix bug in drop head queue which can cause a segmentation fault. 
Call to DropHeadQueue::DoDequeue was replaced with call to Dequeue method
of the base class so that the accounting of # of packets in the queue is
correct. 

**Version 0.3**
* Add stochastic error model
* Add PhyRxBegin and PhyRxEnd traces to SimpleWirelessNetDevice
* Add source Ethernet address to the SimpleWirelessNetDevice traces (destination Ethernet 
address and protocol were already provided to the traces)

**Version 0.2**
* Add support for simulated directional networking through fixed neighbor lists
* Add support for contention based transmisstion
* Fix bug in Priority Queue which caused segfault when running DCE in optimized mode
* Fix memory leak in Priority Queue in Classify function

**Version 0.1**
* Implement transmission delay based on the channel data rate
* Implement support for specifying transmission range
* Implement multiple types of error including constant and range based error curves
* Create two new queue types (DropHead and Priority Queue)
* Implement support for user selectable queue method limit the data rate
* Add support for differentiating control and data traffic
* Add pcap capture of packets sent and received


SimpleWirelessChannel
=======================

The SimpleWirelessChannel models the wireless channel used to transmit packets and this
includes the following features:
- Propogation Delay - calculated using packet size and spped of light
- Transmission Delay - calculated using packet size and the channel's data rate
- Transmission Range - user specified
- Packet Error - determined from the user configured range and error model

When the SimpleWirelessChannel receives a packet from the upper layer to transmit, it
iterates over all the devices attached to channel to determine if it should send
the packet to each device.

First, the channel looks to see if the device is attached to the node itself. 
The channel does not send the packet to the device that is attached to
the node itself (preventing the node from receiving its own packets).

Second, if directional networking is being used, the channel checks if the device 
is attached to the destination node of the packet. If not then the packet is not
sent to the device.

Next, the channel checks if stochastic error is being used and if so whether or not the
link between the sender and destination is currently on or off. If off, the packet is not
sent to the destiation node device.

Next, the channel checks if the destination node is within the maximum transmission range
of the sending node based on distance between the sender and the destination.
If the destination node is out of range then the packet is not sent to the destination
node device.

Finally, the channel checks if the packet would be in error if it were to be transmitted
to the device. This decision is based on the configured error model as follows:
+ If the error model is CONSTANT, then the packet is in error if a randomly
selected error value exceeds the configured error rate. Note that the error rate
is per packet and based on the size of the packet.
+ If the error model is PER_CURVE, then the packet error is determined using the 
packet error rate curve specified by the user. The distance between the sender
and the receive is used to pick an error value from this curve, linearly interpolating
between points on the curve. If a random selected value exceeds this error rate, then
the packet is considered in error and is not sent to the device. This too is packet based
error rate.
+ If the error model is STOCHASTIC, then there is no packet error. The decision on handling
of packets (sent or not sent) is made earlier when the state of the link is checked.
      
If a device passes all tests (the device is not on the sending node, the device is
in view of the sending, the link to the device is not in the OFF state if STOCHASTIC error
model is being use, the device is in range and the packet error rate is below the threshold)
then a reception event is scheduled for the destination device.

This process is repeated for all the devices on the channel.

The SimpleWirelessChannel is only involved on the send side and has no functions
associated with receiving packets.

It is important to note that packets that are in error are NOT sent. In other words, the
decision is made on the send side and not the receive side. This minimizes the overhead
of a simulation because there are no resources spent on transmitting and receiving a packet
that ultimately will fail reception due to errors. However, this does impede the modeling
of packet interference if it were to be implemented in the SimpleWireless model. Since that
is currently not implemented, it is perfectly fine to drop packets at the source and conserve
simulation resources.


SimpleWirelessNetDevice
=======================
The SimpleWirelessNetDevice models the wireless device on the interface of a node and
includes the following features:
- Queue support with user configurable queue method
- Transmission limited by data rate
- Support for differentiating control and data traffic
- Support for pcap capture of packets sent and received
- Support for simulated directional networking through fixed neighbor lists
- Support for fixed contention
 
The SimpleWirelessNetDevice supports the use of queue in which to hold packets for transmission.
When queues are used, their purpose is essentially to force a data rate for the device.
Queues are always FIFO but the drop method of the queue is configurable to be either
drop head or drop tail. In addition, there is support for priority queue which implements
separate control and data queues, each of which can be set independently as drop head or drop tail.
When a priority queue is used, a pcap string filter is used to differentiate control and
data packets. Note that it is possible to use no queuing which is the behavior of the original
simple wireless model.

When queues are used, the SimpleWirelessNetDevice maintains a transmit state flag to indicate
if the device is currently transmitting or is idle. When the SimpleWirelessNetDevice receives
a packet from the upper layer to transmit, it places the packet into the queue and if currently
idle, immediately transmits the packet and sets the flag to busy. After the tranmission
is complete, it sets the flag back to idle and checks the queue to see if there is another
packet waiting to be sent. The transmission time of a packet is based on the packet size
and the data rate (configurable).

Note that the busy state is set at the net device and not at the channel. As indicated above,
the channel does not actually send any packets that would be delivered in error at the
destination node. The device does not know about this and will be considered "busy" for the
amount of time required to transmit the packet even it the channel does not actually send it.

When queues are not used, the SimpleWirelessNetDevice immediately sends a packet when it 
receives it and does not maintain a busy flag.

The SimpleWirelessNetDevice implements a simple version of directional networks with
the use of a "neighbor list" which is used to identify the nodes that are in view of the
node as though there was a directional antenna. If this feature is disabled then all nodes
are within view of the node. If this feature is enabled, then only the nodes listed in
the node's neighbor list are considered in view of the node. When directional networking
is enabled for the device, the SimpleWirelessNetDevice enqueues outbound packets as follows:
- A broadcast packet is duplicated and enqueued once for each directional neighbor.
- A unicast packet is enqueued only if the destination is a directional neighbor.

Note that when the directional network feature is enabled, packets are enqueued for a specific
destination so that when the SimpleWirelessChannel gets the packet, there will be only one
destination device to which the packet will be sent. This differs from the case without a
directional network when only one packet is enqueued. As an example, suppose a node has 
3 neighbors. If directional networks are enabled, the device will enqueue 3 packets -- 1 for each
destination. When the channel receives the packet to send, it will only deliver it to one
device on that destination node. If directional networks are not enabled, the device enqueues
one packet. When the channel receives the packet to send, it will deliver it to the three
devices on each of its neighbor nodes.

The simple wireless model also implements a feature to account for contention. This is
called "fixed contention" feature. The model counts up the number of neigbor nodes that could 
cause contention on the channel and reduces the data rate available to a sending node
based on this count. Thus the data rate becomes data rate/# neighbors. 
The device uses this new data rate when it determines the transmit time used for setting
how long to keep the busy flag for the device set and for delay associated with transmission.
The manner in which the number of neighbors is counted is as follows: 
the count first includes the node itself. Then when the channel transmits
a packet, it loops over all devices on the channel. During this loop the number of destination
devices that are within the user specificed contention range are counted and this is the count
that is used for the next time that a packet arrives on the device to be queued (or sent). 
This the information is not the exact number of nodes within contention range at the instant
when a packet is queued but is based on the previous transmit.

The SimpleWirelessNetDevice supports pcap tracing of packets that are sent and 
received by the device. 

The SimpleWirelessNetDevice also supports a receive error model.

SimpleWireless Model Configuration Items
****************************************
The following items are configurable on the Simple Wireless Channel

MaxRange
+ description: Error model used on the send side
+ units: ---
+ default: CONSTANT
+ possible values: CONSTANT, PER_CURVE, or STOCHASTIC 

ErrorModel
+ description: Error model used on the send side
+ units: ---
+ default: CONSTANT
+ possible values: CONSTANT, PER_CURVE, or STOCHASTIC 
   
ErrorRate
+ description: Error rate associated with the CONSTANT error model. Not used if ErrorModel is set to PER_CURVE.
+ units: ---
+ default: 0.0
+ possible values: 0.0 to 1.0 inclusive

AvgLinkUpDuration
+ description: Average duration of link ON state for STOCHASTIC error model.
+ units: TimeValue
+ default: 10000 usec
+ possible values: any TimeValue

AvgLinkDownDuration
+ description: Avergage duration of link OFF state for STOCHASTIC error model.
+ units: TimeValue
+ default: 100 usec
+ possible values: any TimeValue

EnableFixedContention
+ description: Flag used to enabled or disable the Fixed Contention feature 
+ units: ---
+ default: false
+ possible values: true/false
   
FixedContentionRange
+ description: Range used to count the number of neighbors for fixed contention. 
                If fixed contention is enabled and this value is 0, then uses the value set for MaxRange.
+ units: meters
+ default: 0
+ possible values: any value > 0
   
PER Curve
+ description: Pairs of <distance,error rate> used to build the packet error rate (PER) curve.
+ units: meters,error
+ default: ---
+ possible values: distance: > 0 and error: 0.0-1.0
   

+ description: 
+ units: 
+ default: 
+ possible values: 


The following items are configurable on the Simple Wireless Device

ReceiveErrorModel
+ description: Error model used on the receive side.
+ units: ---
+ default: none
+ possible values: ---

DataRate
+ description: Datarate to use for the device.
+ units: data rate
+ default: 1000000b/s
+ possible values: ---

FixedNeighborListEnabled
+ description: Flag used to enabled or disable the simulated directional network with fixed neighbor list feature
+ units: ---
+ default: false
+ possible values: true/false

Fixed Neighbor List
+ description: Node ids to add to the fixed neighbor list when simulating directional networks with neighbor lists.
+ units: node id
+ default: none
+ possible values: any value >= 0

TxQueue
+ description: Type of queuing to use if any.
+ units: ---
+ default: NULL (no queue)
+ possible values: NULL, DropTailQueue, DropHeadQueue, PriorityQueue 
   
   
The following items are configurable on the Queues

Mode
+ description: Drop mode for the queue. 
                If PACKETS, then packets are dropped from the queue when it is at capacity without regard to size.
                If BYTES, then the size of the packets is considered when dropping packets.
+ units: 
+ default: QUEUE_MODE_PACKETS
+ possible values: QUEUE_MODE_PACKETS or QUEUE_MODE_BYTES
   
MaxPackets
+ description: Maximum number of packets to allow in the queue. Used when mode is QUEUE_MODE_PACKETS.
+ units: ---
+ default: 100
+ possible values: any value > 0
   
MaxBytes
+ description: Maximum number of bytes to allow in the queue. Used when mode is QUEUE_MODE_BYTES.
+ units: ---
+ default: 6,553,500
+ possible values: any value > 0
   
ControlPacketClassifier
+ description: String used to classify control packets using a pcap filter when Priority Queues are used.
                Examples:
                   To classify packets based on port: StringValue ("port 698")
                   To classify packets based on ether type: StringValue ("ether proto 0x88B5")
+ units: ---
+ default: none
+ possible values: any string


Using the SimpleWireless Model
******************************
To use the SimpleWireless model, users need to configure in the following steps:

1) Create the SimpleWirelessChannel
          Ptr<SimpleWirelessChannel> phy = CreateObject<SimpleWirelessChannel> ();
          
2) Configure the SimpleWirelessChannel
     a) Set the range for the channel 
               phy->SetDeviceAttribute ("MaxRange", DoubleValue (txRange));
               
        Default value can be set as follows but this must be done BEFORE creating the channel:
               Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (txRange));
               
     b) Set the error model type
               phy->setErrorModelType(CONSTANT);
               
     c) If the error model type is CONSTANT, set the error rate
               phy->setErrorRate(errorRate);
     
     d) If the error model type is PER_CURVE, build the error curve by calling the function
               phy->addToPERmodel(0.0, 0.0);
               phy->addToPERmodel(10.0, 0.0);
               phy->addToPERmodel(20.0, 0.0);
               phy->addToPERmodel(30.0, 0.007);
               phy->addToPERmodel(40.0, 0.1);
               phy->addToPERmodel(50.0, 0.4);
               phy->addToPERmodel(60.0, 0.7);
               phy->addToPERmodel(70.0, 0.9);
              
     e) If the error model type is STOCHASTIC, set the up and down durations
               phy->SetAttribute("AvgLinkUpDuration", TimeValue (MicroSeconds (errorUpAvg)));
               phy->SetAttribute("AvgLinkDownDuration", TimeValue (MicroSeconds (errorDownAvg)));
               
     f) If using the fixed contention feature, enable the feature
               phy->EnableFixedContention();
               
     g) If using the fixed contention feature with a non-default value for the contention range (default value is channel's transmission range) set the contention range value
               phy->SetFixedContentionRange(contentionRange);

3) On EACH node, create SimpleWirelessNetDevice
          Ptr<SimpleWirelessNetDevice> simpleWireless = CreateObject<SimpleWirelessNetDevice> ();
          
4) Configure the SimpleWirelessChannel
     a) Set the channel 
               simpleWireless->SetChannel(phy);
               
     b) Set the node
               simpleWireless->SetNode(node);
               
     c) Set the data rate
               simpleWireless->SetDataRate(DataRate ("1Mb/s"))
         
        Default value can be set as follows but this must be done BEFORE creating the devices:
               Config::SetDefault ("ns3::SimpleWirelessNetDevice::DataRate", DataRateValue (DataRate (dataRate)));
     
     d) Set the address
               simpleWireless->SetAddress(Mac48Address::Allocate ());
     
     e) Create a queue and set this on the device. For example:
               Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
               simpleWireless->SetQueue(queue);       

        The example model queue_test.cc show how to configure each type of queue

     f) Add the device to the node and device container
               node->AddDevice (simpleWireless);
               devices.Add (simpleWireless);

     g) Enable pcap capture if desired:
               std::ostringstream stringStream;
               stringStream << "scenario_name_" << node->GetId() << ".pcap";
               fileStr = stringStream.str();
               simpleWireless->EnablePcapAll(fileStr);

5) If being use, enable directional networking and pass a list of neighbors
   This must be done AFTER adding all the devices to each node
   NOTE that the code checks the return code from the call to AddDirectionalNeighbors
   
   Example: In this example, Node 0 has directional neighbors 1, 3, 4, 7, 10, and 11
         Ptr<NetDevice> dev0 = devices.Get(0);
         Ptr<SimpleWirelessNetDevice> swDev0 = DynamicCast<SimpleWirelessNetDevice>(dev0);
         simpleWireless->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
         // build a map of directional neighbor nodes and mac addresses
         std::map<uint32_t, Mac48Address> nbrSet;
         for ( NetDeviceContainer::Iterator dIt = devices.Begin(); dIt != devices.End(); ++dIt)
         {
            Ptr<Node> node = (*dIt)->GetNode();
            uint32_t id = node->GetId();
            // in this example, specific node ids are directional neighbors
            if ( id == 1 || id == 3 || id == 4 || id == 7 || id == 10 || id == 11 )
            {
               // Get mac addr of node and add to map
               Address addr = (*dIt)->GetAddress();
               Mac48Address macAddr = Mac48Address::ConvertFrom (addr);
               nbrSet.insert(std::pair<uint32_t, Mac48Address>(id, macAddr));
               std::cout << "Adding node " << id << " with mac address " << macAddr<< std::endl;
            }
         }
         // Now add to dev on node 0
         if (!swDev0->AddDirectionalNeighbors(nbrSet))
         {
            NS_FATAL_ERROR ("Call to AddDirectionalNeighbors failed. Please enabled directional neighbors.");
            return 0;
         }
               
    There are also functions to allow dynamic simulated directional networking through fixed neighbor lists
    by adding and removing neighbor to/from the list over time. This can be done using calls to the methods:
         AddDirectionalNeighbor
         DeleteDirectionalNeighbor


6) Initial the Stochastic error model. This must be done AFTER adding all the devices and does nothing if not running STOCHASTIC error model
           phy->InitStochasticModel();

SimpleWirelessNetDevice Model Traces
************************************
The following traces are available for the device:

* PhyTxBegin - called when the device sends the packet to the channel for Tx

* PhyRxBegin - called when a packet has begun being received by the device

* PhyRxEnd   - called when a packet has been completely received by the device

* PhyRxDrop - called if a packet is dropped by the device during receive
   
* PromiscSniffer - called for pcap capture of packets; captures on send and receive
   
* QueueLatency - called when a packet is dequeued for transmission
      
* MacTx        - called when a packet has been received from higher layers and is being queued for transmission

* MacRx       - called when a packet has been received over the air and is being forwarded up the local protocol stack

SimpleWirelessChannel Model Traces
************************************
NONE

SimpleWireless Examples
************************************
The examples directory has the following examples which show how to configure the
Simple Wireless model and use the features that it has.

directional_test.cc            Provides an example of how to use the simulated directional network feature

error_model_test.cc            Provides an example of how to use the error models added to the send side (CONSTANT, PER_CURVE and STOCHASTIC)

fixed_contention_test.cc       Provides an example of how to use the fixed contention feature

multiple_interface_example.cc  Provides a simple 2 node example of how to configure multiple interfaces on a node

mixed_directional_network.cc   Provides a more complex example of multiple interfaces per node in combination with the simulated directional networks.

queue_test.cc                  Provides examples of how to configure each type of queuing.

