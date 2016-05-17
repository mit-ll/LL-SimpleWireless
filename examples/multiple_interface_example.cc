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

// Network topology
//
//   0 ------------ 1      --- simple wireless network
//     ++++++++++++        +++ directional simple wireless network
//
// This network has 2 nodes and 2 interfaces per node.
// Both interfaces are simple wireless
// One interface on each node is directional
//
// Node 0 sends traffic over both interfaces. part way into the simulation,
// node 0 loses its directional neighbor and the traffic over that link stops
// being received.




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

uint32_t count_sent_app1 = 0;
uint32_t count_recv_app1 = 0;
uint32_t count_sent_app2 = 0;
uint32_t count_recv_app2 = 0;


// ******************************************************************
// These functions support the Applications
// ******************************************************************
static void SinkReceivedBytes (Ptr<const Packet> p, const Address & from)
{
	if (p->GetSize() > 500)
		count_recv_app1++;
	else
		count_recv_app2++;


	std::cout << Simulator::Now ().GetSeconds () << " Node receiving packet of " << p->GetSize() << " bytes."<< std::endl;
}

static void AppSendBytes (Ptr<const Packet> p)
{
   if (p->GetSize() > 500)
		count_sent_app1++;
	else
		count_sent_app2++;

	std::cout << Simulator::Now ().GetSeconds () << " Node sending packet of " << p->GetSize() << " bytes. "<< std::endl;
}


static void removeDirectionalNbr (Ptr<SimpleWirelessNetDevice> sw)
{
	// remove directional neigbhor 1
	sw->DeleteDirectionalNeighbor(1);
}

int 
main (int argc, char *argv[])
{
	NodeContainer::Iterator it;
	double simtime = 126;
	double dataRate = 10000000.0;  //10Mb/s

	// create node container for the omni network
	NodeContainer n1;
	n1.Create (2);
	// create node container for the directional network
	NodeContainer n2_dir = NodeContainer (n1.Get (0), n1.Get (1));


	InternetStackHelper internet;
	internet.Install (n1);
  
  
	// Create container to hold devices
	NetDeviceContainer d1_omni;
	NetDeviceContainer d2_dir;

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
	Config::SetDefault ("ns3::DropHeadQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
	Config::SetDefault ("ns3::DropHeadQueue::MaxPackets", UintegerValue (100));
	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (100.0));
	
	// channel
	Ptr<SimpleWirelessChannel> phy1 = CreateObject<SimpleWirelessChannel> ();
	phy1->setErrorRate(0.0);
	phy1->setErrorModelType(CONSTANT);
	
	// create simple wireless device on each node
	for (it = n1.Begin (); it != n1.End (); ++it) 
	{ 
		Ptr<Node> node = *it;
		
		// create device
		Ptr<SimpleWirelessNetDevice> simpleWireless1 = CreateObject<SimpleWirelessNetDevice> ();
		simpleWireless1->SetChannel(phy1);
		simpleWireless1->SetNode(node);
		simpleWireless1->SetAddress(Mac48Address::Allocate ());
		simpleWireless1->SetDataRate((DataRate (dataRate)));
		
		// create queue type
		Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
		simpleWireless1->SetQueue(queue);
		
		node->AddDevice (simpleWireless1);
		d1_omni.Add (simpleWireless1);
	}

	// ***********************************************************************
	// create second network
	Ptr<SimpleWirelessChannel> phy2 = CreateObject<SimpleWirelessChannel> ();
	phy2->setErrorRate(0.0);
	phy2->setErrorModelType(CONSTANT);
	
	// create simple wireless device on each node
	for (it = n2_dir.Begin (); it != n2_dir.End (); ++it) 
	{ 
		Ptr<Node> node = *it;
		uint32_t id = node->GetId();
		
		// create device
		Ptr<SimpleWirelessNetDevice> simpleWireless2 = CreateObject<SimpleWirelessNetDevice> ();
		simpleWireless2->SetChannel(phy2);
		simpleWireless2->SetNode(node);
		simpleWireless2->SetAddress(Mac48Address::Allocate ());
		simpleWireless2->SetDataRate((DataRate (dataRate)));
		std::cout << "node id " << id << " has macAddress of " << simpleWireless2->GetAddress() << std::endl;
		
		// create queue type
		Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
		simpleWireless2->SetQueue(queue);
		
		if (id == 0)
		{
			// schedule function which removed node 0's directional neighbor
			Simulator::Schedule (Seconds (60.0), &removeDirectionalNbr, simpleWireless2);
		}

		node->AddDevice (simpleWireless2);
		d2_dir.Add (simpleWireless2);
	}
	
	
	// ***********************************************************************
	// Set up directional network. Do this after adding all the devices because
	// we need to get MAC addresses for the neighbors we want to add
	
	// Get node 0, 1 device on the container. MUST use d2_dir container
	// since that is the container with the SW interfaces that use directional
	// network. 
	// In this example we get the ptr to the NetDevice using the index in the
	// containter. We added them in numerical order so we know that 0 is node0,
	// 1 is node 1, etc.
	Ptr<NetDevice> dev0 = d2_dir.Get(0);
	Ptr<NetDevice> dev1 = d2_dir.Get(1);
	Ptr<SimpleWirelessNetDevice> swDev0 = DynamicCast<SimpleWirelessNetDevice>(dev0);
	Ptr<SimpleWirelessNetDevice> swDev1 = DynamicCast<SimpleWirelessNetDevice>(dev1);
	
	// node 0 has directional neighbors to node 1
	swDev0->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
	// Here we use the function that takes a single node to add 
	Address addr = swDev1->GetAddress();
	Mac48Address macAddr = Mac48Address::ConvertFrom (addr);
	swDev0->AddDirectionalNeighbor(1,macAddr);
	
	// node 1 has directional neighbors to node 0
	swDev1->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
	addr = swDev0->GetAddress();
	macAddr = Mac48Address::ConvertFrom (addr);
	swDev1->AddDirectionalNeighbor(0,macAddr);
	

	// ***********************************************************************
	// add IP addresses.
	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer interfaces = ipv4.Assign (d1_omni);
	
	ipv4.SetBase ("10.1.2.0", "255.255.255.0");
	ipv4.Assign (d2_dir);
	
	// Create router nodes, initialize routing database and set up the routing
	// tables in the nodes.
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	
	// ***********************************************************************
	// Define positions. 
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector ( 0.0,   0.0, 0.0));
	positionAlloc->Add (Vector ( 0.0,  50.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (n1);

	
	// ***********************************************************************
	// Set up application
	// ***********************************************************************
	//  start the packet sink on node 1
	PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 8080));
	ApplicationContainer apps_sink = sink.Install (n1.Get (1));
	apps_sink.Start (Seconds (0.0));
	std::cout << "Node 1 installed sink " << std::endl;
	
	// start the OnOff app on source to destinations in the omni network
	OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.1.1.255"), 8080)); 
	onoff.SetAttribute ("PacketSize", StringValue ("1000"));
	onoff.SetAttribute ("DataRate", StringValue ("100000"));
	onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
	onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
	ApplicationContainer apps1 = onoff.Install (n1.Get (0));
	apps1.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
	apps1.Start (Seconds (5.0));
	apps1.Stop (Seconds (simtime - 1.0));
	
	// **** Choose if you want bcast or unicast traffic by uncommented appropriate line below.
	// start the OnOff app on source to destinations in the directional network
	onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.1.2.255"), 8080)); 
	//onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.1.2.2"), 8080)); 
	onoff.SetAttribute ("PacketSize", StringValue ("500"));
	onoff.SetAttribute ("DataRate", StringValue ("100000"));
	onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
	onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
	ApplicationContainer apps2 = onoff.Install (n2_dir.Get (0));
	apps2.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
	apps2.Start (Seconds (5.0));
	apps2.Stop (Seconds (simtime - 1.0));

	// set up the sink receive callback on all packet sinks
	Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkReceivedBytes));
	
	// ***********************************************************************
	// and finally ... off we go!
	// ***********************************************************************
	
	Simulator::Stop (Seconds(simtime));
	Simulator::Run ();
	Simulator::Destroy ();
	
	std::cout << "App1 Sent: " << count_sent_app1 << "  Received: " << count_recv_app1 << std::endl;
	std::cout << "App2 Sent: " << count_sent_app2 << "  Received: " << count_recv_app2 << std::endl;

	
	NS_LOG_INFO ("Run Completed Successfully");
}
