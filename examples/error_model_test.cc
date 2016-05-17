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


// This file is used to test the error models added to the send side and
// support CONSTANT, PER_CURVE and STOCHASTIC.
//
// The scenario has the following:
//  - 101 nodes
//  - node 0 is at the center of a circle
//  - nodes 1-100 are randomly placed on a circle of radius 100 
//  - NO mobility
//  - simple wireless model has:
//         + User specified error type
//         + tx range of 100 
//         + NO queue
//  - OLSR used for routing
//  - On/Off application used for node 0 to send 1Mb/s to all 100 neighbor nodes

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("error_model_test");

uint32_t count_sent = 0;
// These are only used by OLSR
uint32_t count_recv = 0;
uint32_t pkts_sent_data = 0;
uint32_t bytes_sent_data = 0;
uint32_t pkts_sent_cntl = 0;
uint32_t bytes_sent_cntl = 0;

#define APP_PKT_SIZE 1000
std::string PktSize = "1000";

#define NUM_NODES 101  // node 0 is source + N neighbors
uint32_t pkts_rcvd_by_node[NUM_NODES];

// ******************************************************************
// This function supports OLSR when running on Simple Wireless
// ******************************************************************
static void TransmitStatsSW (Ptr<const Packet> p, Mac48Address from, Mac48Address to , uint16_t protocol)
{
  // Figure out if this is OLSR or data
  if (to.IsBroadcast () &&  p->GetSize() != (APP_PKT_SIZE + 28))
  {
		pkts_sent_cntl++;
		bytes_sent_cntl += p->GetSize();
		//std::cout << Simulator::Now ().GetSeconds () << " Node sending CONTROL packet of " << p->GetSize() << " bytes to address " << to << std::endl;
  }
  else
  {
	  pkts_sent_data++;
	  bytes_sent_data += p->GetSize();
	  //std::cout << Simulator::Now ().GetSeconds () << " Node sending DATA packet of " << p->GetSize() << " bytes to address " << to << std::endl;
  }
}

static void MacRxSuccess (std::string context, Ptr<const Packet> p)
{
  int id = atoi(context.c_str());
  if (p->GetSize() == (APP_PKT_SIZE + 28))
  {
    pkts_rcvd_by_node[id]++;
    // Uncomment this line for STOCHASTIC so that you can graph packets received vs time
    std::cout << Simulator::Now ().GetSeconds () << " Node " << id << " receiving packet of " << p->GetSize() << " bytes."<< std::endl;
  }
   
   
}


// ******************************************************************
// These functions are related to the Applications
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
	double dataRate = 1000000.0; 
	
	bool collectPcap = false;
	std::string errorModel = "CONSTANT";
	double errorRate = 0.0;
	double errorUpAvg = 15000000.0; // 15 seconds up
	double errorDownAvg = 5000000.0; // 5 second down
	
	// ***********************************************************************
	// parse command line
	// ***********************************************************************
	CommandLine cmd;
	cmd.AddValue ("pcap", "Set to 1 to collect pcap traces", collectPcap);
	cmd.AddValue ("errorModel", "Error model to use. Must be one of: CONSTANT, CURVE, STOCHASTIC", errorModel);
	cmd.AddValue ("errorRate", "Error rate if CONSTANT error model is used", errorRate);
	cmd.AddValue ("errorUpAvg", "Average link up duration (microseconds) if STOCHASTIC error model is used", errorUpAvg);
	cmd.AddValue ("errorDownAvg", "Average link down duration (microseconds) if STOCHASTIC error model is used", errorDownAvg);
	cmd.Parse (argc,argv);
	
	if ((errorModel != "CONSTANT") && (errorModel != "CURVE") && (errorModel != "STOCHASTIC") )
	{
		NS_ABORT_MSG ("Invalid errorModel type: Use --errorModel=CONSTANT or --errorModel=CURVE or --errorModel=STOCHASTIC");
	}
	
	std::cout << "Running scenario for " << simtime << " seconds using error type of "<< errorModel << std::endl;

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
	if ( errorModel.compare ("CURVE") == 0 )
	{
		phy->setErrorModelType(PER_CURVE);
		phy->addToPERmodel(0.0, 0.0);
		phy->addToPERmodel(10.0, 0.0);
		phy->addToPERmodel(20.0, 0.05);
		phy->addToPERmodel(30.0, 0.07);
		phy->addToPERmodel(40.0, 0.12);
		phy->addToPERmodel(50.0, 0.15);
		phy->addToPERmodel(60.0, 0.5);
		phy->addToPERmodel(70.0, 0.6);
		phy->addToPERmodel(80.0, 0.70);
		phy->addToPERmodel(90.0, 0.80);
		phy->addToPERmodel(100.0, 1.0);
	}
	else if ( errorModel.compare ("CONSTANT") == 0 )
	{
		phy->setErrorModelType (CONSTANT);
		phy->setErrorRate (errorRate);
	}
	else if ( errorModel.compare ("STOCHASTIC") == 0 )
	{
		phy->setErrorModelType(STOCHASTIC);
		phy->SetAttribute("AvgLinkUpDuration", TimeValue (MicroSeconds (errorUpAvg)));
		phy->SetAttribute("AvgLinkDownDuration", TimeValue (MicroSeconds (errorDownAvg)));
	}
	
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
			stringStream << "ErrorModelTest_node_" << node->GetId() << ".pcap";
			fileStr = stringStream.str();
			simpleWireless->EnablePcapAll(fileStr);
		}
	}
	
	// Must be done AFTER adding all the devices. Does nothing if not running STOCHASTIC error model
	phy->InitStochasticModel();

		
	// set up call back for traces
	Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::SimpleWirelessNetDevice/PhyTxBegin", MakeCallback (&TransmitStatsSW));

	// ***********************************************************************
	// Define positions. 
	ObjectFactory pos;
	pos.SetTypeId ("ns3::UniformDiscPositionAllocator");
	pos.Set ("X", DoubleValue (0.0));
	pos.Set ("Y", DoubleValue (0.0));
	pos.Set ("rho", DoubleValue (100));
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
	// For OLSR we need to get some stats
	// ***********************************************************************
	std::cout << "App Sent Count: " << count_sent << "\nApp Receive Count: " << count_recv << std::endl;
	std::cout << "Control Sent Count: " << pkts_sent_cntl << "\nData Sent Count: " << pkts_sent_data << std::endl;
	for (int i = 1; i < NUM_NODES; i++)
	{
		std::cout << "Packets received by Node " << i << ": " << pkts_rcvd_by_node[i] << std::endl;
	}

	
	NS_LOG_INFO ("Run Completed Successfully");
	
	return 0;
}
