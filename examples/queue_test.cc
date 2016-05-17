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


// This file is used to test the queue options available:
//     - no queues
//     - drop queue tail (provided with ns3)
//     - drop queue head (new; provided with the MIT LL simple wireless model)
//     - priority queue (new; provided with the MIT LL simple wireless model)
//
// All queues are FIFO
//
// Drop Queue Tail - when queues are full, drop at the tail of the queue. That is, the
//                   newly received packet is dropped
// Drop Queue Head - when queues are full, drop at the head of the queue. That is, the
//                   oldest packet is dropped and newly received packet is inserted 
//                   into the queue
// Priority Queue - defines two queues: control and data. Each queue is configured
//                  independently to be either drop head or drop tail. Differentiation
//                  of control and data is based on a user specified pcap filter string
//
// The scenario has the following:
//  - 2 nodes placed 50 units apart with no mobility
//  - simple wireless model with user configurable data rate
//  - OLSR used for routing
//  - On/Off application used to send 1Mb/s
//  - capability to enable pcap capture
//
// Queue capabilities can be tested by changing the data rate of the simple wireless model

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("queue_test");

uint32_t app_count_sent = 0;
uint32_t app_count_recv = 0;
uint32_t pkts_sent_data = 0;
uint32_t bytes_sent_data = 0;
uint32_t pkts_sent_cntl = 0;
uint32_t bytes_sent_cntl = 0;
uint32_t pkts_rcvd_data = 0;
uint32_t pkts_rcvd_cntl = 0;

// variables for queue latency: overall, data and control
double_t avg_queue_latency = 0.0;
uint32_t queue_pkt_count = 0;
double_t avg_queue_latency_data = 0.0;
uint32_t queue_pkt_count_data = 0;
double_t avg_queue_latency_cntl = 0.0;
uint32_t queue_pkt_count_cntl = 0;

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
     bytes_sent_data += p->GetSize();
  }
  else
  {
    pkts_sent_cntl++;
    bytes_sent_cntl += p->GetSize();
  }
}


static void MacRxSuccess (std::string context, Ptr<const Packet> p)
{
  if (p->GetSize() == (APP_PKT_SIZE + 28))
  {
     pkts_rcvd_data++;
  }
  else
  {
    pkts_rcvd_cntl++;
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
	double simtime = 120;
	bool collectPcap = false;
	double dataRate = 10000000.0; 
	std::string queueType = "DropTail";
	
	// ***********************************************************************
	// parse command line
	// ***********************************************************************
	CommandLine cmd;
	cmd.AddValue ("pcap", "Set to 1 to collect pcap traces", collectPcap);
	cmd.AddValue ("datarate", "Data Rate of wireless link in bits per second", dataRate);
	cmd.AddValue ("queueType", "Set Queue type to NoQueue, DropHead, DropTail or PriorityHead or PriorityTail", queueType);
	cmd.Parse (argc,argv);
	
	std::cout << "Running scenario for " << simtime << " seconds with queue type: "<< queueType <<" and data rate: " << std::fixed << std::setprecision(1) << dataRate <<"bps"<< std::endl;
	
	if ((queueType != "NoQueue") && (queueType != "DropHead") && (queueType != "DropTail") && (queueType != "PriorityHead") && (queueType != "PriorityTail"))
	{
		NS_ABORT_MSG ("Invalid queue type: Use --queueType=DropHead or --queueType=DropTail or --queueType=PriorityHead or --queueType=PriorityTail");
	}

	// ***********************************************************************
	// Create all the nodes
	// ***********************************************************************
	NodeContainer NpNodes;
	NpNodes.Create (2);
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
		
		// Set queue type to use. Set nothing if NoQueue
		if (queueType == "DropHead") 
		{
			Config::SetDefault ("ns3::DropHeadQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
			Config::SetDefault ("ns3::DropHeadQueue::MaxPackets", UintegerValue (100));
			Ptr<DropHeadQueue> queue = CreateObject<DropHeadQueue> ();
			simpleWireless->SetQueue(queue);
		}
		else if (queueType == "DropTail")
		{
			Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
			Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (100));
			Ptr<DropTailQueue> queue = CreateObject<DropTailQueue> ();
			simpleWireless->SetQueue(queue);
		}
		else if (queueType == "PriorityHead")
		{
			Config::SetDefault ("ns3::PriorityQueue::ControlPacketClassifier", StringValue ("port 698"));
			Config::SetDefault ("ns3::DropHeadQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
			Config::SetDefault ("ns3::DropHeadQueue::MaxPackets", UintegerValue (100));
			Ptr<DropHeadQueue> controlQueue = CreateObject<DropHeadQueue> ();
			Ptr<DropHeadQueue> dataQueue = CreateObject<DropHeadQueue> ();
			Ptr<PriorityQueue> queue = CreateObject<PriorityQueue> ();
			queue->Initialize();
			queue->SetControlQueue(controlQueue);
			queue->SetDataQueue(dataQueue);
			simpleWireless->SetQueue(queue);
		}
		else if (queueType == "PriorityTail")
		{
			Config::SetDefault ("ns3::PriorityQueue::ControlPacketClassifier", StringValue ("port 698"));
			Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
			Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (100));
			Ptr<DropTailQueue> controlQueue = CreateObject<DropTailQueue> ();
			Ptr<DropTailQueue> dataQueue = CreateObject<DropTailQueue> ();
			Ptr<PriorityQueue> queue = CreateObject<PriorityQueue> ();
			queue->Initialize();
			queue->SetControlQueue(controlQueue);
			queue->SetDataQueue(dataQueue);
			simpleWireless->SetQueue(queue);
		}
		
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
		
	// set up call back for traces
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/QueueLatency", MakeCallback (&QueueLatencyStats));
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/PhyTxBegin", MakeCallback (&TransmitStatsSW));

	// ***********************************************************************
	// Define positions. Nodes are 50 apart
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (0.0, 0.0, 0.0));
	positionAlloc->Add (Vector (50.0, 0.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (NpNodes);

	
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
		
		// print the olsr routing table to a file every 10 seconds
		//Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("400node_OLSR.routes", std::ios::out);
		//olsr.PrintRoutingTableAllEvery (Seconds (10), routingStream);

	// now set the routing and install on all nodes
	stack.SetRoutingHelper (list); 
	stack.Install (NpNodes);
	
	// set up IP addresses
	Ipv4AddressHelper address;
	address.SetBase ("10.0.0.0", "255.255.0.0");
	Ipv4InterfaceContainer interfaces = address.Assign (devices);
	
	// ***********************************************************************
	// Set up application
	// ***********************************************************************
	//  start the packet sink on destination node
	PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), 8080));
	ApplicationContainer apps_sink = sink.Install (NpNodes.Get (1));
	apps_sink.Start (Seconds (0.0));
	std::cout << "Node 1 installed sink to receive on " << interfaces.GetAddress (1) << std::endl;

	// set up the sink receive callback on all packet sinks
	Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&SinkReceivedBytes));
	
	// Now start the OnOff app on source to destinations 
	OnOffHelper onoff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), 8080)); 
	onoff.SetAttribute ("PacketSize", StringValue (PktSize));
	onoff.SetAttribute ("DataRate", StringValue ("1000000"));
	onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
	onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
	
	ApplicationContainer apps = onoff.Install (NpNodes.Get (0));
	std::cout << "Node 0 installed app to send to " << interfaces.GetAddress (1) << std::endl;
	apps.Get (0)->TraceConnectWithoutContext ("Tx", MakeCallback (&AppSendBytes));
	
	apps.Start (Seconds (5.0));
	apps.Stop (Seconds (simtime - 5.0));

	
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
	uint32_t dataDropped = app_count_sent - app_count_recv;
	if (app_count_sent)
		rcvPercentData = ((double)app_count_recv/(double)app_count_sent)*100.0;
		
	double rcvPercentCntrl = 0.0;
	uint32_t cntlDropped = pkts_sent_cntl - pkts_rcvd_cntl;
	if (pkts_sent_cntl)
		rcvPercentCntrl = ((double)pkts_rcvd_cntl/(double)pkts_sent_cntl)*100.0;
		
	std::cout << "App Packets Sent: " << app_count_sent << "\nApp Packets Received: " << app_count_recv
				<< "\nControl Packets Sent: " << pkts_sent_cntl << "\nControl Bytes Sent: "   << bytes_sent_cntl 
				<< "\nData Packets Sent: "    << pkts_sent_data << "\nData Bytes Sent: "      <<  bytes_sent_data
				<< "\nControl Packets Received: "    << pkts_rcvd_cntl << "\nData Packets Received: "      <<  pkts_rcvd_data 
				<< "\nData Packets Dropped: " << dataDropped << "\nControl Packets Dropped: " << cntlDropped
				<< "\n% Data Received: " << std::fixed << std::setprecision(1) << rcvPercentData << std::noshowpoint << std::setprecision(0)
				<< "\n% Control Received: " << std::fixed << std::setprecision(1) << rcvPercentCntrl << std::noshowpoint << std::setprecision(0) <<std::endl;
	if ( (queueType == "PriorityHead") || (queueType == "PriorityTail") )
	{
		std::cout << "Average Queue Latency Data: " << std::fixed << std::setprecision(6) << avg_queue_latency_data 
					<< "\nAverage Queue Latency Control: " << std::fixed << std::setprecision(6) << avg_queue_latency_cntl << std::noshowpoint << std::setprecision(0) <<std::endl;
	}
	else
	{
		std::cout << "Average Queue Latency: " << std::fixed << std::setprecision(6) << avg_queue_latency << std::noshowpoint << std::setprecision(0) <<std::endl;
	}
	
	NS_LOG_INFO ("Run Completed Successfully");
	
	return 0;
}
