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
    
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/simple-wireless-module.h"


// This file is used to test the fixed contention option.
// When this feature is enabled, all nodes within a certain
// range (user specified, defaults to the tx range) act
// like contention and are used to reduce the datarate to
// data rate/ # neighbors
//
// The scenario has the following:
//  - 20 nodes
//  - node 0 is at the center of a circle
//  - nodes 1-19 are randomly placed on a circle of radius 100 
//  - random mobility within the circle
//  - simple wireless model has:
//         + constant error rate of 0
//         + tx range of 50 
//         + No queue
//         + fixed contention enabled
//         + fixed contention range is defaulted to 50 but is user configurable
//  - OLSR used for routing
//  - On/Off application used for node 0 to send 1Mb/s to all 19 neighbor nodes

// By changing the contention range, the effective data rate will decrease.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("fixed_contention_test");

uint32_t app_count_sent = 0;
uint32_t app_count_recv = 0;
uint32_t pkts_sent_data = 0;
uint32_t pkts_rcvd_data = 0;
uint32_t pkts_sent_cntl = 0;

// variables for queue latency: overall, data and control
double_t avg_queue_latency = 0.0;
uint32_t queue_pkt_count = 0;
double_t avg_queue_latency_data = 0.0;
uint32_t queue_pkt_count_data = 0;
double_t avg_queue_latency_cntl = 0.0;
uint32_t queue_pkt_count_cntl = 0;

#define APP_PKT_SIZE 1000
std::string PktSize = "1000";

#define NUM_NODES 101  // node 0 is source + N neighbors
uint32_t pkts_rcvd_by_node[NUM_NODES];

uint32_t nodePlacementRadius = 100;

// ******************************************************************
// This function supports OLSR when running on Simple Wireless
// ******************************************************************
static void TransmitStatsSW (Ptr<const Packet> p, Mac48Address from, Mac48Address to , uint16_t protocol)
{
  // Figure out if this is OLSR or data
  if (p->GetSize() == (APP_PKT_SIZE + 28))
  {
     pkts_sent_data++;
  }
  else
  {
    pkts_sent_cntl++;
  }
}


static void MacRxSuccess (std::string context, Ptr<const Packet> p)
{
  int id = atoi(context.c_str());
  if (p->GetSize() == (APP_PKT_SIZE + 28))
  {
     pkts_rcvd_by_node[id]++;
     pkts_rcvd_data++;
  }
}

// ******************************************************************
// This function supports Simple Wireless
// ******************************************************************
static void QueueLatencyStats (Ptr<const Packet> p, Time latency)
{
  double_t pkt_latency = double_t(latency.GetMicroSeconds())/1000000.0;
  
  queue_pkt_count++;
  avg_queue_latency = avg_queue_latency * (queue_pkt_count-1)/queue_pkt_count + pkt_latency/queue_pkt_count;
  
  // add 28 bytes to ap size for UDP/IP header and also add 14 for ethernet header. 
  // Packet passed in this trace still has the Ethernet header 
  if (p->GetSize() == (APP_PKT_SIZE + 28 + 14))
  {
     queue_pkt_count_data++;
     avg_queue_latency_data = avg_queue_latency_data * (queue_pkt_count_data-1)/queue_pkt_count_data + pkt_latency/queue_pkt_count_data;
     //std::cout << Simulator::Now ().GetSeconds () << " DATA Packet latency: " << std::setprecision(9) << pkt_latency << " seconds"<< std::endl;
  }
  else
  {
    queue_pkt_count_cntl++;
    avg_queue_latency_cntl = avg_queue_latency_cntl * (queue_pkt_count_cntl-1)/queue_pkt_count_cntl + pkt_latency/queue_pkt_count_cntl;
    //std::cout << Simulator::Now ().GetSeconds () << " CONTROL Packet ("<< p->GetSize() << ") latency: " << std::setprecision(9) << pkt_latency << " seconds"<< std::endl;
  }

}


// ******************************************************************
// These functions support OLSR and are related to the Applications
// ******************************************************************

static void SinkReceivedBytes (Ptr<const Packet> p, const Address & from)
{
  app_count_recv++;
  //std::cout << Simulator::Now ().GetSeconds () << " Node receiving packet of " << p->GetSize() << " bytes. count_recv is "<< count_recv << std::endl;
}

static void AppSendBytes (Ptr<const Packet> p)
{
   app_count_sent++;
   //std::cout << Simulator::Now ().GetSeconds () << " Node sending packet of " << p->GetSize() << " bytes. count_sent is "<< count_sent << std::endl;
}


// ******************************************************************
// MAIN
// ******************************************************************

int 
main (int argc, char *argv[])
{
	NodeContainer::Iterator it;
	NodeContainer::Iterator it2;
	std::list<Ipv4Address> destAddresses; // used by OLSR

	Ipv4Address sourceNodeAddr;
	
	// ***********************************************************************
	// Initialize all value that are to be used in the scenario
	// ***********************************************************************
	double simtime = 65;
	bool collectPcap = false;
	double dataRate = 10000000.0; 
	double contentionRange = 50.0;
	
	// ***********************************************************************
	// parse command line
	// ***********************************************************************
	CommandLine cmd;
	cmd.AddValue ("pcap", "Set to 1 to collect pcap traces", collectPcap);
	cmd.AddValue ("contentionRange", "Distance to use for simple wireless contention range", contentionRange);
	cmd.Parse (argc,argv);
	
	std::cout << "Running scenario for " << simtime << " seconds with contention range "<< contentionRange << std::endl;

	// ***********************************************************************
	// Create all the nodes
	// ***********************************************************************
	NodeContainer myNodes;
	myNodes.Create (NUM_NODES);
	NodeContainer const & n = NodeContainer::GetGlobal ();
	
	// Create container to hold devices
	NetDeviceContainer devices;

	// ***********************************************************************
	// Set up the physical/radio layer
	// ***********************************************************************
	// Set transmission range
	Config::SetDefault ("ns3::SimpleWirelessChannel::MaxRange", DoubleValue (100.0));
	
	// Create error model and set as default for the device Receive side
	// ALWAYS set the error rate to 0 here. The error is handled on the send side
	// by the channel model in the simple wireless
	Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
	em->SetAttribute ("ErrorRate", DoubleValue (0.0));
	em->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
	Config::SetDefault ("ns3::SimpleWirelessNetDevice::ReceiveErrorModel", PointerValue(em));

	// create channel
	Ptr<SimpleWirelessChannel> phy = CreateObject<SimpleWirelessChannel> ();
	phy->setErrorRate(0.0);
	phy->setErrorModelType(CONSTANT);
	phy->EnableFixedContention();
	phy->SetFixedContentionRange(contentionRange);
	
	// create simple wireless device on each node
	std::string fileStr;
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		Ptr<Node> node = *it;
		
		// create device
		Ptr<SimpleWirelessNetDevice> simpleWireless = CreateObject<SimpleWirelessNetDevice> ();
		simpleWireless->SetChannel(phy);
		simpleWireless->SetNode(node);
		simpleWireless->SetAddress(Mac48Address::Allocate ());
		simpleWireless->SetDataRate((DataRate (dataRate)));
		
		Config::SetDefault ("ns3::DropHeadQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
		Config::SetDefault ("ns3::DropHeadQueue::MaxPackets", UintegerValue (100));
		Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
		simpleWireless->SetQueue(queue);
		
		// Set up trace to pass node id on the RX end
		std::ostringstream oss;
		oss << node->GetId();
		simpleWireless->TraceConnect ("MacRx", oss.str(), MakeCallback (&MacRxSuccess));
		
		node->AddDevice (simpleWireless);
		devices.Add (simpleWireless);
		
		// set up pcap capture
		if (collectPcap)
		{
			std::ostringstream stringStream;
			stringStream << "CONTENTION_node_" << node->GetId() << ".pcap";
			fileStr = stringStream.str();
			simpleWireless->EnablePcapAll(fileStr);
		}
	}
		
	// set up call back for traces
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/QueueLatency", MakeCallback (&QueueLatencyStats));
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/PhyTxBegin", MakeCallback (&TransmitStatsSW));
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkReceivedBytes));

	// ***********************************************************************
	// Define positions. 
	ObjectFactory pos;
	pos.SetTypeId ("ns3::UniformDiscPositionAllocator");
	pos.Set ("X", DoubleValue (0.0));
	pos.Set ("Y", DoubleValue (0.0));
	pos.Set ("rho", DoubleValue (nodePlacementRadius));
	Ptr<PositionAllocator> positionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
	
	// ***********************************************************************
	// Define and install random mobility. Here we are using the circle set up above
	// as the position allocator for mobility (i.e., the area they can move in)
	// ***********************************************************************
	MobilityHelper mobility;
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (myNodes);
	
	// print starting positions
	for (it = n.Begin (); it != n.End (); ++it) 
	{ 
		// get node ptr, position and node id
		Ptr<Node> node = *it; 
		Ptr<MobilityModel> mob = node->GetObject<MobilityModel> (); 
		int id =  node->GetId();
		
		// set up source node
		if (id == 0) 
		{
			// placed at the center and is the source node!
			mob->SetPosition (Vector (0.0,0.0,0.0));
		}
		Vector pos = mob->GetPosition (); 
		double distance = sqrt(pos.x*pos.x + pos.y*pos.y);
		std::cout << "Node " << id << ". Position (" << pos.x << ", " << pos.y << ", " << pos.z << ")  Distance to Node 0: "<< distance << std::endl; 
	}
	

	
	// ***********************************************************************
	// Set up routing OLSR
	// ***********************************************************************
	// don't have to use the Ipv4ListRoutingHelper but it prints
	// the routing table in a better format than directly installing olsr.
	InternetStackHelper stack;
	OlsrHelper olsr;
	Ipv4ListRoutingHelper list;
	
	// Add the routing to the route helper
	list.Add (olsr, 10);

	// now set the routing and install on all nodes
	stack.SetRoutingHelper (list); 
	stack.Install (myNodes);
	
	// set up IP addresses
	Ipv4AddressHelper address;
	address.SetBase ("10.0.0.0", "255.255.0.0");
	Ipv4InterfaceContainer interfaces = address.Assign (devices);
	
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
			onoff.SetAttribute ("PacketSize", StringValue (PktSize));
			onoff.SetAttribute ("DataRate", StringValue ("1000000"));
			onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
			
			ApplicationContainer apps = onoff.Install (myNodes.Get (0));
			std::cout << "Node 0 installed app to send to 255.255.255.255" << std::endl;
			apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
			
			apps.Start (Seconds (5.0));
			apps.Stop (Seconds (simtime - 5.0));
		}
		else
		{
			// on all other nodes start a packet sink
			PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (id), 8080));
			ApplicationContainer apps_sink = sink.Install (myNodes.Get (id));
			apps_sink.Start (Seconds (0.0));
			//std::cout << "Node " << id << " installed sink to receive on " << interfaces.GetAddress (id) << std::endl;
			pkts_rcvd_by_node[id] = 0;
		}
	}
	
	// set up the sink receive callback on all packet sinks
	Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkReceivedBytes));
	
	// ***********************************************************************
	// and finally ... off we go!
	// ***********************************************************************
	
	Simulator::Stop (Seconds(simtime));
	Simulator::Run ();
	Simulator::Destroy ();
	
	// ***********************************************************************
	// stats
	// ***********************************************************************
	double rcvPercentData = 0.0;
	uint32_t dataDropped = app_count_sent*(NUM_NODES -1) - app_count_recv;
	if (app_count_sent)
		rcvPercentData = ((double)app_count_recv/((double)app_count_sent*(NUM_NODES -1)))*100.0;
		
	std::cout << "App Packets Sent: " << app_count_sent << "\nApp Packets Received: " << app_count_recv
				<< "\nControl Packets Sent: " << pkts_sent_cntl  
				<< "\nData Packets Sent: "    << pkts_sent_data 
				<< "\nData Packets Received: "      <<  pkts_rcvd_data 
				<< "\nData Packets Dropped: " << dataDropped 
				<< "\n% Data Received: " << std::fixed << std::setprecision(1) << rcvPercentData << std::noshowpoint << std::setprecision(0)<<std::endl;
	std::cout << "Average Queue Latency Data: " << std::fixed << std::setprecision(6) << avg_queue_latency_data 
					<< "\nAverage Queue Latency Control: " << std::fixed << std::setprecision(6) << avg_queue_latency_cntl << std::noshowpoint << std::setprecision(0) <<std::endl;
	std::cout << "Overall Average Queue Latency: " << std::fixed << std::setprecision(6) << avg_queue_latency << std::noshowpoint << std::setprecision(0) <<std::endl;
	
	// get queue packets dropped
	Ptr<NetDevice> dev = devices.Get(0);
	PointerValue val;
	dev->GetAttribute("TxQueue", val);
	Ptr<Queue> queue = val.Get<Queue>();
	Ptr<DropHeadQueue> dropHead = DynamicCast<DropHeadQueue>(queue);
	std::cout << "Packets Dropped at Queue on Node 0: " << dropHead->GetTotalDroppedPackets() << std::endl;
	
	for (int i = 1; i < NUM_NODES; i++)
	{
		//std::cout << "Packets received by Node " << i << ": " << pkts_rcvd_by_node[i] << std::endl;
	}

	
	NS_LOG_INFO ("Run Completed Successfully");
	
	return 0;
}
