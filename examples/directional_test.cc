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


// This file is used to test the directional network option.
// For directional networks, the user must specify a list of neighbors that
// can be seen. If a neighbor can not be seen, all packets are dropped for
// that neighbr. If a neighbor can be seen, the configured error is then 
// applied to the packet transmit.
//
// The scenario has the following:
//  - 13 nodes
//  - node 0 is at the center of a circle
//  - nodes 1-12 are placed on a circle of radius 50 in a clock face layout
//       (i.e., node 1 at 1 o'clock position, node 2 at 2 o'clock position, etc.)
//  - no mobility
//  - simple wireless model has:
//         + constant error rate of 0
//         + tx range of 100 so that all nodes are in range of node 0
//         + drop head queue
//         + 10Mbps data rate
//         + simple wireless is configured to NOT drop any packets due to queueing
//  - OLSR used for routing
//  - On/Off application used for node 0 to send 1Mb/s to all 12 neighbor nodes
//  - capability to enable pcap capture

// The following nodes are added to the list of neighbors for directional networking:
//    1, 3, 4, 7, 10, 11
//
//                 X
//             11     1
//           10         X
//           X     O     3
//            X         4
//              7     X
//                 X

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("directional_test");

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

#define NUM_NODES 13
#define NUM_DIR_NBR_NODES 6  // nodes 1, 3, 4, 7, 10, 11 are directional neighbors
uint32_t pkts_rcvd_by_node[NUM_NODES];

#define APP_PKT_SIZE 1000
std::string PktSize = "1000";


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
	
	// ***********************************************************************
	// parse command line
	// ***********************************************************************
	CommandLine cmd;
	cmd.AddValue ("pcap", "Set to 1 to collect pcap traces", collectPcap);
	cmd.Parse (argc,argv);
	
	std::cout << "Running scenario for " << simtime << " seconds "<< std::endl;

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
	
	// Uncomment these two lines if you would like to also use contention
	//phy->EnableFixedContention();
	//phy->SetFixedContentionRange(100.0);
	
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
		std::cout << "node id " << node->GetId() << " has macAddress of " << simpleWireless->GetAddress() << std::endl;
		
		// Set queue type to use
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
			stringStream << "QUEUE_node_" << node->GetId() << ".pcap";
			fileStr = stringStream.str();
			simpleWireless->EnablePcapAll(fileStr);
		}
	}
	
	
	// ------------------------------------------------------------------------
	// Set up directional network. Do this after adding all the devices because
	// we need to get MAC addresses for the neighbors we want to add
	// Only node 0 has directional neighbors.
	
	// Get node 0 device on the container. 
	// we added them in numerical order so we know that 0 is node0
	Ptr<NetDevice> dev0 = devices.Get(0);
	Ptr<SimpleWirelessNetDevice> swDev0 = DynamicCast<SimpleWirelessNetDevice>(dev0);
	swDev0->SetAttributeFailSafe ("FixedNeighborListEnabled", BooleanValue (true));
	std::map<uint32_t, Mac48Address> nbrSet;
	// Now in this example, we use an iterator to get the devices.
	for ( NetDeviceContainer::Iterator dIt = devices.Begin(); dIt != devices.End(); ++dIt)
	{
		Ptr<Node> node = (*dIt)->GetNode();
		uint32_t id = node->GetId();
		
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
	// ------------------------------------------------------------------------


	// set up call back for traces
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/QueueLatency", MakeCallback (&QueueLatencyStats));
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/PhyTxBegin", MakeCallback (&TransmitStatsSW));

	// ***********************************************************************
	// Define positions. 
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector ( 0.0,   0.0, 0.0));
	positionAlloc->Add (Vector (25.0,  43.0, 0.0));
	positionAlloc->Add (Vector (43.0,  25.0, 0.0));
	positionAlloc->Add (Vector (50.0,   0.0, 0.0));
	positionAlloc->Add (Vector (43.0, -25.0, 0.0));
	positionAlloc->Add (Vector (25.0, -43.0, 0.0));
	positionAlloc->Add (Vector ( 0.0, -50.0, 0.0));
	positionAlloc->Add (Vector (-25.0,-43.0, 0.0));
	positionAlloc->Add (Vector (-43.0,-25.0, 0.0));
	positionAlloc->Add (Vector (-50.0,  0.0, 0.0));
	positionAlloc->Add (Vector (-43.0, 25.0, 0.0));
	positionAlloc->Add (Vector (-25.0, 43.0, 0.0));
	positionAlloc->Add (Vector ( 0.0,  50.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (myNodes);

	
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
			// **** Choose if you want bcast or unicast traffic by uncommented appropriate line below.
			
			// start the OnOff app on source to destinations  (using broadcast)
			OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("255.255.255.255"), 8080)); 
			std::cout << "Node 0 installed app to send to 255.255.255.255" << std::endl;
			
			// start the OnOff app on source to destinations  (using unicast)
			//OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), 8080)); 
			//std::cout << "Node 0 installed app to send to " << interfaces.GetAddress (1) << std::endl;
			
			onoff.SetAttribute ("PacketSize", StringValue (PktSize));
			onoff.SetAttribute ("DataRate", StringValue ("1000000"));
			onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			
			ApplicationContainer apps = onoff.Install (myNodes.Get (0));
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
			std::cout << "Node " << id << " installed sink to receive on " << interfaces.GetAddress (id) << std::endl;
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
	// For OLSR we need to get some stats
	// ***********************************************************************
	double rcvPercentData = 0.0;
	uint32_t dataDropped = app_count_sent*NUM_DIR_NBR_NODES - app_count_recv;
	if (app_count_sent)
		rcvPercentData = ((double)app_count_recv/((double)app_count_sent*NUM_DIR_NBR_NODES))*100.0;
		
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
		std::cout << "Packets received by Node " << i << ": " << pkts_rcvd_by_node[i] << std::endl;
	}


	
	NS_LOG_INFO ("Run Completed Successfully");
	
	return 0;
}
