/*
 * Copyright (C) 2015 Massachusetts Institute of Technology
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// This uses global routing
//
// Network topology
//
//                 1
//                 ||
//                 || 
//  7---           ||       ---5
//      |---4======0======2-|
//  8---           ||       ---6
//                 ||
//                 || 
//                 3

// Node 0 is placed at the origin (0,0)
// Nodes 1, 2, 3, 4 are 50m away from Node 0 
// Nodes 5, 6 are placed 10m away from Node 2
// Nodes 7, 8 are placed 10m away from Node 4


// two wireless networks:
//   - one network has all nodes and has a range of 40m
//   - second network has node 0, 1, 2, 3, 4 and has a range of 100m
//         + node 0 uses a directional network with node 1 and 4 as neighbors
//         + node 2 uses a directional network but has no neighbors
// Note that nodes 0, 1, 2, 3, 4 each have two interfaces

// Traffic:
//    - Node 0 sends broadcast traffic. Received at nodes 1 & 4
//    - Node 2 sends broadcast traffic. Received at nodes 5 & 6


#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/simple-wireless-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedDirectionalNetworkExample");

uint32_t count_sent = 0;
uint32_t count_recv = 0;


// ******************************************************************
// These functions support the Applications
// ******************************************************************
static void SinkReceivedBytes (Ptr<const Packet> p, const Address & from)
{
  count_recv++;
  //std::cout << Simulator::Now ().GetSeconds () << " Node receiving packet of " << p->GetSize() << " bytes. count_recv is "<< count_recv << std::endl;
}

static void AppSendBytes (Ptr<const Packet> p)
{
   count_sent++;
   //std::cout << Simulator::Now ().GetSeconds () << " Node sending packet of " << p->GetSize() << " bytes. count_sent is "<< count_sent << std::endl;
}

int 
main (int argc, char *argv[])
{
	NodeContainer::Iterator it;
	double simtime = 60;
	double dataRate = 10000000.0;

  NodeContainer n;
  n.Create (9);
  // create node container for the second network
  NodeContainer n01234 = NodeContainer (n.Get (0), n.Get (1), n.Get (2), n.Get (3), n.Get (4));

  InternetStackHelper internet;
  internet.Install (n);
  
  
	// Create container to hold devices
	NetDeviceContainer dAll;
	NetDeviceContainer d2;

	// ***********************************************************************
	// Set up the physical/radio layer
	// ***********************************************************************
	// Create error model and set as default 
	Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
	em->SetAttribute ("ErrorRate", DoubleValue (0.0));
	em->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
	Config::SetDefault ("ns3::SimpleWirelessNetDevice::ReceiveErrorModel", PointerValue(em));

	// ***********************************************************************
	// create first network
	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (40.0));
	Ptr<SimpleWirelessChannel> phy1 = CreateObject<SimpleWirelessChannel> ();
	phy1->setErrorRate(0.0);
	phy1->setErrorModelType(CONSTANT);
	
	// create simple wireless device on each node
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		Ptr<Node> node = *it;
		
		// create device
		Ptr<SimpleWirelessNetDevice> simpleWireless1 = CreateObject<SimpleWirelessNetDevice> ();
		simpleWireless1->SetChannel(phy1);
		simpleWireless1->SetNode(node);
		simpleWireless1->SetAddress(Mac48Address::Allocate ());
		simpleWireless1->SetDataRate((DataRate (dataRate)));
		
		// Set queue type to use
		Config::SetDefault ("ns3::DropHeadQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
		Config::SetDefault ("ns3::DropHeadQueue::MaxPackets", UintegerValue (100));
		Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
		simpleWireless1->SetQueue(queue);
		
		node->AddDevice (simpleWireless1);
		dAll.Add (simpleWireless1);
	}
	
	// ***********************************************************************
	// create second network
	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (100.0));
	Ptr<SimpleWirelessChannel> phy2 = CreateObject<SimpleWirelessChannel> ();
	phy2->setErrorRate(0.0);
	phy2->setErrorModelType(CONSTANT);
	
	// create simple wireless device on each node
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		Ptr<Node> node = *it;
		uint32_t id = node->GetId();
		
		// network 2 is only on node 0, 1, 2, 3, 4
		if (id < 5)
		{
			// create device
			Ptr<SimpleWirelessNetDevice> simpleWireless2 = CreateObject<SimpleWirelessNetDevice> ();
			simpleWireless2->SetChannel(phy2);
			simpleWireless2->SetNode(node);
			simpleWireless2->SetAddress(Mac48Address::Allocate ());
			simpleWireless2->SetDataRate((DataRate (dataRate)));
			
			std::cout << "node id " << id << " has macAddress of " << simpleWireless2->GetAddress() << std::endl;
			
			// Set queue type to use
			Config::SetDefault ("ns3::DropHeadQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
			Config::SetDefault ("ns3::DropHeadQueue::MaxPackets", UintegerValue (100));
			Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
			simpleWireless2->SetQueue(queue);
			
			node->AddDevice (simpleWireless2);
			d2.Add (simpleWireless2);
		}
	}

	// ------------------------------------------------------------------------
	// Set up directional network. Do this after adding all the devices because
	// we need to get MAC addresses for the neighbors we want to add
	// network 2 is only on node 0, 1, 2, 3, 4
	// node 0 has directional neighbors to nodes 1, 4
	// node 2 has directional networking but no neighbors
	
	// Get node 0, 1, 2, 4 device on the container. MUST use d2 container
	// since that is the container with the SW interfaces that use directional
	// network. 
	// In this example we get the ptr to the NetDevice using the index in the
	// containter. We added them in numerical order so we know that 0 is node0,
	// 1 is node 1, etc.
	Ptr<NetDevice> dev0 = d2.Get(0);
	Ptr<NetDevice> dev1 = d2.Get(1);
	Ptr<NetDevice> dev2 = d2.Get(2);
	Ptr<NetDevice> dev4 = d2.Get(4);
	Ptr<SimpleWirelessNetDevice> swDev0 = DynamicCast<SimpleWirelessNetDevice>(dev0);
	Ptr<SimpleWirelessNetDevice> swDev1 = DynamicCast<SimpleWirelessNetDevice>(dev1);
	Ptr<SimpleWirelessNetDevice> swDev2 = DynamicCast<SimpleWirelessNetDevice>(dev2);
	Ptr<SimpleWirelessNetDevice> swDev4 = DynamicCast<SimpleWirelessNetDevice>(dev4);
	
	// node 0 has directional neighbors to nodes 1, 4
	swDev0->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
	// Here we use the function that takes map to add multiple neighbors one call
	std::map<uint32_t, Mac48Address> nbrSet;
	// Get mac addr of node 1 and add to map
	Address addr = swDev1->GetAddress();
	Mac48Address macAddr = Mac48Address::ConvertFrom (addr);
	nbrSet.insert(std::pair<uint32_t, Mac48Address>(1, macAddr));
	std::cout << "Adding node 1 with mac address " << macAddr<< std::endl;
	// Get mac addr of node 4 and add to map
	addr = swDev4->GetAddress();
	macAddr = Mac48Address::ConvertFrom (addr);
	nbrSet.insert(std::pair<uint32_t, Mac48Address>(4, macAddr));
	std::cout << "Adding node 4 with mac address " << macAddr<< std::endl;
	// Now add to dev on node 0
	swDev0->AddDirectionalNeighbors(nbrSet);
	
	// Now just set the fixed neigbor feature enabled on dev 2 but don't
	// add any neighbors because it has none
	swDev2->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
	// ------------------------------------------------------------------------


	// Later, we add IP addresses.
	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer interfaces = ipv4.Assign (dAll);
	
	ipv4.SetBase ("10.1.2.0", "255.255.255.0");
	ipv4.Assign (d2);
	
	// Create router nodes, initialize routing database and set up the routing
	// tables in the nodes.
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	
	// ***********************************************************************
	// Define positions. 
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector ( 0.0,   0.0, 0.0));
	positionAlloc->Add (Vector ( 0.0,  50.0, 0.0));
	positionAlloc->Add (Vector (50.0,   0.0, 0.0));
	positionAlloc->Add (Vector ( 0.0, -50.0, 0.0));
	positionAlloc->Add (Vector (-50.0,  0.0, 0.0));
	positionAlloc->Add (Vector (60.0,  -2.0, 0.0));
	positionAlloc->Add (Vector (60.0,   2.0, 0.0));
	positionAlloc->Add (Vector (-60.0,  2.0, 0.0));
	positionAlloc->Add (Vector (-60.0, -2.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (n);

	
	// ***********************************************************************
	// Set up application
	// ***********************************************************************
	//  start the packet sink on all nodes except node 0
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		Ptr<Node> node = *it; 
		uint32_t id = node->GetId();
		
		if (id == 0)
		{
			// start the OnOff app on source to destinations  (using broadcast)
			OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("255.255.255.255"), 8080)); 
			onoff.SetAttribute ("PacketSize", StringValue ("1000"));
			onoff.SetAttribute ("DataRate", StringValue ("100000"));
			onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
			
			ApplicationContainer apps = onoff.Install (n.Get (0));
			apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
			
			apps.Start (Seconds (5.0));
			apps.Stop (Seconds (simtime - 1.0));
		}
		else if (id == 2)
		{
			// start the OnOff app on source to destinations  (using broadcast)
			OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("255.255.255.255"), 8080)); 
			onoff.SetAttribute ("PacketSize", StringValue ("1000"));
			onoff.SetAttribute ("DataRate", StringValue ("100000"));
			onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
			
			ApplicationContainer apps = onoff.Install (n.Get (2));
			apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
			
			apps.Start (Seconds (5.0));
			apps.Stop (Seconds (simtime - 1.0));
		}
		
		// on nodes start a packet sink
		PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 8080));
		ApplicationContainer apps_sink = sink.Install (n.Get (id));
		apps_sink.Start (Seconds (0.0));
		std::cout << "Node " << id << " installed sink " << std::endl;
		
	}
	
	// set up the sink receive callback on all packet sinks
	Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkReceivedBytes));
	
	// ***********************************************************************
	// and finally ... off we go!
	// ***********************************************************************
	
	Simulator::Stop (Seconds(simtime));
	Simulator::Run ();
	Simulator::Destroy ();
	
	std::cout << "Sent: " << count_sent << "\nReceive Count: " << count_recv << std::endl;

	
	NS_LOG_INFO ("Run Completed Successfully");
}
