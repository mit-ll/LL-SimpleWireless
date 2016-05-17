/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (C) 2015 Massachusetts Institute of Technology
 * Copyright (c) 2010 University of Washington
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
 *
 */
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "simple-wireless-net-device.h"
#include "simple-wireless-channel.h"

#include <netinet/in.h>  // needed for noth for protocol # in sniffer

NS_LOG_COMPONENT_DEFINE ("SimpleWirelessNetDevice");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SimpleWirelessNetDevice);

//********************************************************
//  TimestampTag used to store a timestamp with a packet
//  when they are placed in the queue
//********************************************************
TypeId TimestampTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("TimestampTag")
    .SetParent<Tag> ()
    .AddConstructor<TimestampTag> ()
    .AddAttribute ("Timestamp",
                   "Some momentous point in time!",
                   EmptyAttributeValue (),
                   MakeTimeAccessor (&TimestampTag::GetTimestamp),
                   MakeTimeChecker ())
  ;
  return tid;
}

TypeId TimestampTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t TimestampTag::GetSerializedSize (void) const
{
  return 8;
}

void TimestampTag::Serialize (TagBuffer i) const
{
  int64_t t = m_timestamp.GetNanoSeconds ();
  i.Write ((const uint8_t *)&t, 8);
}

void TimestampTag::Deserialize (TagBuffer i)
{
  int64_t t;
  i.Read ((uint8_t *)&t, 8);
  m_timestamp = NanoSeconds (t);
}

void TimestampTag::SetTimestamp (Time time)
{
  m_timestamp = time;
}

Time TimestampTag::GetTimestamp (void) const
{
  return m_timestamp;
}

void TimestampTag::Print (std::ostream &os) const
{
  os << "t=" << m_timestamp;
}

//********************************************************

//********************************************************
//  DestinationIdTag used to store a destination node id 
//  with a packet when they are placed in the queue. 
//  This is used by directional networks.
//********************************************************
TypeId DestinationIdTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("DestinationIdTag")
    .SetParent<Tag> ()
    .AddConstructor<DestinationIdTag> ()
  ;
  return tid;
}

TypeId DestinationIdTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

DestinationIdTag::DestinationIdTag ()
{
}

DestinationIdTag::DestinationIdTag (uint32_t destId)
  : m_destnodeid (destId)
{
}

uint32_t DestinationIdTag::GetSerializedSize (void) const
{
  return 4;
}

void DestinationIdTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_destnodeid);
}

void DestinationIdTag::Deserialize (TagBuffer i)
{
  m_destnodeid = i.ReadU32 ();
}

void DestinationIdTag::SetDestinationId (uint32_t id)
{
  m_destnodeid = id;
}

uint32_t DestinationIdTag::GetDestinationId (void) const
{
  return m_destnodeid;
}

void DestinationIdTag::Print (std::ostream &os) const
{
  os << "t=" << m_destnodeid;
}

//********************************************************

TypeId 
SimpleWirelessNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SimpleWirelessNetDevice")
    .SetParent<NetDevice> ()
    .AddConstructor<SimpleWirelessNetDevice> ()
    .AddAttribute ("ReceiveErrorModel",
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&SimpleWirelessNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    .AddAttribute ("DataRate", 
                   "The default data rate for point to point links",
                   DataRateValue (DataRate ("1000000b/s")),
                   MakeDataRateAccessor (&SimpleWirelessNetDevice::m_bps),
                   MakeDataRateChecker ())
    // Transmit queueing discipline for the device which includes its own set
    // of trace hooks.
    .AddAttribute ("TxQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&SimpleWirelessNetDevice::m_queue),
                   MakePointerChecker<Queue> ())
    .AddAttribute ("FixedNeighborListEnabled", 
                   "Enabled or Disabled",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SimpleWirelessNetDevice::m_fixedNbrListEnabled),
                   MakeBooleanChecker ())
    .AddTraceSource ("PhyTxBegin",
                     "Trace source indicating a packet has begun transmitting",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_TxBeginTrace))
    .AddTraceSource ("PhyRxDrop",
                     "Trace source indicating a packet has been dropped by the device during reception",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_phyRxDropTrace))
    .AddTraceSource ("PhyRxBegin",
                     "Trace source indicating a packet "
                     "has begun being received from the channel medium "
                     "by the device",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_phyRxBeginTrace))
    .AddTraceSource ("PhyRxEnd",
                     "Trace source indicating a packet "
                     "has been completely received from the channel medium "
                     "by the device",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_phyRxEndTrace))
    // Trace sources designed to simulate a packet sniffer facility (tcpdump).
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_promiscSnifferTrace))
    .AddTraceSource ("QueueLatency",
                     "Trace source to report the latency of a packet in the queue. Datatype returned is Time.",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_QueueLatencyTrace))
    .AddTraceSource ("MacTx",
                     "A packet has been received from higher layers and is being processed in preparation for "
                     "queueing for transmission.",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_macTxTrace))
    .AddTraceSource ("MacRx",
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&SimpleWirelessNetDevice::m_macRxTrace))                     
    ;
  return tid;
}

SimpleWirelessNetDevice::SimpleWirelessNetDevice ()
  : m_channel (0),
    m_node (0),
    m_mtu (0xffff),
    m_ifIndex (0),
    m_txMachineState (READY),
    m_queue(NULL),
    m_pktRcvTotal(0),
    m_pktRcvDrop(0),
    m_pcapEnabled(false),
    m_fixedNbrListEnabled(false),
    m_nbrCount(0)
    
{}

void 
SimpleWirelessNetDevice::Receive (Ptr<Packet> packet, uint16_t protocol, 
                            Mac48Address to, Mac48Address from)
{
  NS_LOG_FUNCTION (packet << protocol << to << from);
  NetDevice::PacketType packetType;
  
  m_phyRxBeginTrace (packet, from, to, protocol);
  m_pktRcvTotal++;
  
  NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " receiving packet " << packet->GetUid () << "  from " << from << "  to " << to  );

  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) )
    {
      m_phyRxDropTrace (packet, from, to, protocol);
      m_pktRcvDrop++;
      return;
    }
    
  if (m_pcapEnabled)
  {
    // Add the ethernet header to the packet for sniffer
    // For some reason the Ethernet header is STRIPPED from the packet
    // by the time we get here so we need to reconstruct it for the
    // sniffer.
    // allocate buffer for packet and mac addresses
    uint8_t buffer[8192];
    uint8_t macBuffer[6];
    // get the dest and src addresses and copy into the buffer
    to.CopyTo(macBuffer);
    memcpy(&buffer[0], macBuffer, 6);
    from.CopyTo(macBuffer);
    memcpy(&buffer[6], macBuffer, 6);
    //copy the protocol and the actual packet data in the the buffer
    uint16_t tempProt = ntohs(protocol);
    memcpy(&buffer[12], &tempProt, 2);
    packet->CopyData(&buffer[14], packet->GetSize()); 
    // Now make a new temp packet to write to the sniffer
    Ptr<Packet> tempPacket = Create<Packet> (buffer, packet->GetSize() + 14);
    m_promiscSnifferTrace (tempPacket);
   }

  if (to == m_address)
    {
      packetType = NetDevice::PACKET_HOST;
    }
  else if (to.IsBroadcast ())
    {
      packetType = NetDevice::PACKET_BROADCAST;
    }
  else if (to.IsGroup ())
    {
      packetType = NetDevice::PACKET_MULTICAST;
    }
  else 
    {
      packetType = NetDevice::PACKET_OTHERHOST;
    }
    
  m_phyRxEndTrace (packet, from, to, protocol);
    
  if (packetType != NetDevice::PACKET_OTHERHOST)
    {
      m_macRxTrace (packet);
      m_rxCallback (this, packet, protocol, from);
    }
    

  if (!m_promiscCallback.IsNull ())
    {
      m_promiscCallback (this, packet, protocol, from, to, packetType);
    }
    NS_LOG_DEBUG ("Total Rcvd: " << m_pktRcvTotal << " Total Dropped: " << m_pktRcvDrop);
}

void 
SimpleWirelessNetDevice::SetChannel (Ptr<SimpleWirelessChannel> channel)
{
  m_channel = channel;
  m_channel->Add (this);
}

void
SimpleWirelessNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  m_receiveErrorModel = em;
}

void 
SimpleWirelessNetDevice::SetIfIndex(const uint32_t index)
{
  m_ifIndex = index;
}
uint32_t 
SimpleWirelessNetDevice::GetIfIndex(void) const
{
  return m_ifIndex;
}
Ptr<Channel> 
SimpleWirelessNetDevice::GetChannel (void) const
{
  return m_channel;
}
void
SimpleWirelessNetDevice::SetAddress (Address address)
{
  m_address = Mac48Address::ConvertFrom(address);
}
Address 
SimpleWirelessNetDevice::GetAddress (void) const
{
  //
  // Implicit conversion from Mac48Address to Address
  //
  return m_address;
}
bool 
SimpleWirelessNetDevice::SetMtu (const uint16_t mtu)
{
  m_mtu = mtu;
  return true;
}
uint16_t 
SimpleWirelessNetDevice::GetMtu (void) const
{
  return m_mtu;
}
bool 
SimpleWirelessNetDevice::IsLinkUp (void) const
{
  return true;
}
void 
SimpleWirelessNetDevice::AddLinkChangeCallback (Callback<void> callback)
{}
bool 
SimpleWirelessNetDevice::IsBroadcast (void) const
{
  return true;
}
Address
SimpleWirelessNetDevice::GetBroadcast (void) const
{
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}
bool 
SimpleWirelessNetDevice::IsMulticast (void) const
{
  return false;
}
Address 
SimpleWirelessNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  return Mac48Address::GetMulticast (multicastGroup);
}

Address SimpleWirelessNetDevice::GetMulticast (Ipv6Address addr) const
{
	return Mac48Address::GetMulticast (addr);
}

bool 
SimpleWirelessNetDevice::IsPointToPoint (void) const
{
  return false;
}

bool 
SimpleWirelessNetDevice::IsBridge (void) const
{
  return false;
}

//********************************************************************
// Directional Neighbor functions
bool SimpleWirelessNetDevice::AddDirectionalNeighbors(std::map<uint32_t, Mac48Address> nodesToAdd)
{
  // is directional neighbor feature enabled?
  // If not return false so caller knows there is a problem
  if (!m_fixedNbrListEnabled)
     return false;
  
  for ( std::map<uint32_t, Mac48Address> ::iterator it = nodesToAdd.begin(); it != nodesToAdd.end(); ++it)
  {
     mDirectionalNbrs.insert(std::pair<uint32_t, Mac48Address>(it->first, it->second));
     NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " added directional neighbor " << it->first << " mac Address " << it->second);
  }
  return true;
}

bool SimpleWirelessNetDevice::AddDirectionalNeighbor(uint32_t nodeid, Mac48Address macAddr)
{
  // is directional neighbor feature enabled?
  // If not return false so caller knows there is a problem
  if (!m_fixedNbrListEnabled)
     return false;
     
  mDirectionalNbrs.insert(std::pair<uint32_t, Mac48Address>(nodeid, macAddr));
  NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " added directional neighbor " << nodeid << " mac Address " << macAddr);
  return true;
}

void SimpleWirelessNetDevice::DeleteDirectionalNeighbors(std::set<uint32_t> nodeids)
{
  for ( std::set<uint32_t>::iterator it = nodeids.begin(); it != nodeids.end(); ++it)
  {
     std::map<uint32_t, Mac48Address>::iterator it2 = mDirectionalNbrs.find(*it);
     if (it2 != mDirectionalNbrs.end())
     {
        NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " deleted directional neighbor " << it2->first << " mac Address " << it2->second);
        mDirectionalNbrs.erase(it2);
     }
   }
}

void SimpleWirelessNetDevice::DeleteDirectionalNeighbor(uint32_t nodeid)
{
  std::map<uint32_t, Mac48Address>::iterator it = mDirectionalNbrs.find(nodeid);
  if (it != mDirectionalNbrs.end())
  {
	  NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " deleted directional neighbor " << nodeid << " mac Address " << it->second);
     mDirectionalNbrs.erase(it);
  }
}

//********************************************************************
// Fixed Contention functions
void SimpleWirelessNetDevice::ClearNbrCount(void)
{
  // When we clear the neighbor count, we actually set it to 1 and not 0
  // because we always have to count ourselves.
  m_nbrCount = 1;
}

void SimpleWirelessNetDevice::IncrementNbrCount(void)
{
  m_nbrCount++;
}

int SimpleWirelessNetDevice::GetNbrCount(void)
{
  return m_nbrCount;
}


//********************************************************************

void
SimpleWirelessNetDevice::TransmitStart (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  // This function is called to start the process of transmitting a packet.
  // We need to tell the channel that we've started wiggling the wire and
  // schedule an event that will be executed when the transmission is complete.
  NS_ASSERT_MSG (m_txMachineState == READY, "Must be READY to transmit");
  m_txMachineState = BUSY;
  m_currentPkt = p;
  
  if (m_pcapEnabled)
  {
     m_promiscSnifferTrace (p);
  }

  // Remove the timestamp tag. calculate queue latency and peg trace
  TimestampTag timeEnqueued;
  p->RemovePacketTag (timeEnqueued);
  Time latency = Simulator::Now() - timeEnqueued.GetTimestamp();
  m_QueueLatencyTrace(p, latency); 
  NS_LOG_DEBUG (Simulator::Now() << " Getting packet with timestamp: " << timeEnqueued.GetTimestamp() );
  
  // Remove ethernet header since it is not sent over the air
  // To this AFTER the queue latency trace in case the trace wants
  // to use anything in the Ethernet header
  EthernetHeader ethHeader;
  p->RemoveHeader(ethHeader);
  Mac48Address to = ethHeader.GetDestination ();
  Mac48Address from = ethHeader.GetSource ();
  uint16_t protocol = ethHeader.GetLengthType ();
  
  
  // Get dest Id tag. This could be the default NO_DIRECTIONAL_NBR
  DestinationIdTag destIdTag;
  p->RemovePacketTag (destIdTag);
  uint32_t destId = destIdTag.GetDestinationId();
  
  Time txTime = Seconds (m_bps.CalculateTxTime (p->GetSize ()));
  
  // If we have a non-zero neighbor count then that means we are using contention and
  // the data rate changes. Note that when using contention, we will always have at least
  // a value of 1 for neighbor count because we count ourselves.
  // If this device is omni then data rate changes to data rate/# neighbors. 
  // Thus multiply the tx time by the number of neighbors (which is the same as dividing the
  // data rate by # neighbors)
  // If this device is directional then the data rate is rate/2. Thus multiply the tx time by 2.
  
  // IMPORTANT NOTE: If we are using contention, the first packet we send may not "know"
  // that there is contention. This is a chicken and egg situation.
  // The device keeps the neighbor count (which is init'd to 0) but the channel is
  // the one that has contention. When contention is enabled on the channel, there may not be
  // any devices to initial with the contention (i.e., set nbr count to 1). The function in the
  // channel that sets contention enabled also notifies all the devices on the channel but if
  // there are not devices yet when the function is called then the first packet sent on the
  // channel by the device is what will cause the device to get init'd for contention. That is
  // after the device set the txTime so that first packet will be sent at full data rate.
  if (m_nbrCount)
  {
     if (m_fixedNbrListEnabled)
     {
        txTime = txTime * 2;
        NS_LOG_DEBUG ("Node " << m_node->GetId() << " txTime was increased to " << txTime << " because we have directonal neighbors. packet size is " << p->GetSize ());
     }
     else
     {
        txTime = txTime * m_nbrCount;
        NS_LOG_DEBUG ("Node " << m_node->GetId() << " txTime was increased to " << txTime << " because we have " << m_nbrCount << " neighbors. packet size is " << p->GetSize ());
     }
  }  
  // TO DO: do we need interframe gap??
  //Time txCompleteTime = txTime + m_tInterframeGap;
  Time txCompleteTime = txTime;

  NS_LOG_DEBUG ("Schedule TransmitCompleteEvent in " << txCompleteTime.GetMicroSeconds () << "usec");
  Simulator::Schedule (txCompleteTime, &SimpleWirelessNetDevice::TransmitComplete, this);

  m_TxBeginTrace (p, from, to, protocol);
  
  m_channel->Send (p, protocol, to, from, this, txTime, destId);
}

void
SimpleWirelessNetDevice::TransmitComplete (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  // This function is called to when we're all done transmitting a packet.
  // We try and pull another packet off of the transmit queue.  If the queue
  // is empty, we are done, otherwise we need to start transmitting the
  // next packet.
  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != 0, "SimpleWirelessNetDevice::TransmitComplete(): m_currentPkt zero");
  m_currentPkt = 0;
  
  NS_LOG_DEBUG (Simulator::Now() << " Tx complete. Packets in queue: " <<  m_queue->GetNPackets() << " Bytes in queue: " << m_queue->GetNBytes());
        

  Ptr<Packet> p = m_queue->Dequeue ();
  if (p == 0)
    {
      // No packet was on the queue, so we just exit.
      return;
    }

  // Got another packet off of the queue, so start the transmit process agin.
  TransmitStart (p);
}


bool 
SimpleWirelessNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (packet << dest << protocolNumber);
  Mac48Address to = Mac48Address::ConvertFrom (dest);
  
  NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " sending packet " << packet->GetUid () << "  to " << to );
  
  // For some reason the Ethernet header is STRIPPED from the packet
  // by the time we get here so we need to reconstruct it for for two reasons.
  // If queuing, add ethernet header to the packet in the queue so we can
  // retrieve the to, from and protocol. Also Ethernet header is
  // needed so we can apply a pcap filter in priority queues
  EthernetHeader ethHeader;
  ethHeader.SetSource (m_address);
  ethHeader.SetDestination (to);
  ethHeader.SetLengthType (protocolNumber);
  packet->AddHeader (ethHeader);
  
  m_macTxTrace (packet);
  
  // If directional networking is enabled, then we have to make a copy
  // of this packet and enqueue it for each destination.
  if (m_fixedNbrListEnabled)
  {
     std::map<uint32_t, Mac48Address>::iterator  it;
     
     // Look up the dest address in the eth header of the packet.
     // This is necessary because in directional networks, the dest
     // could have been changed by the trace
     packet->PeekHeader(ethHeader);
     to = ethHeader.GetDestination();
     
     if (to.IsBroadcast())
     {
        NS_LOG_INFO ("Address " << to << " is broadcast");
        // broadcast packet. Enqueue for all of our directional neighbors
        for ( it = mDirectionalNbrs.begin(); it != mDirectionalNbrs.end(); ++it)
        {
           EnqueuePacket(packet->Copy(),m_address,to,protocolNumber,it->first);
           NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " queueing packet to directional neighbor to node " << it->first);
        }
     }
     else
     {
        NS_LOG_INFO ("Address " << to << " is NOT broadcast");
        // unicast packet. Find the directional neighbor with matching MAC address. (There might not be one)
        for ( it = mDirectionalNbrs.begin(); it != mDirectionalNbrs.end(); ++it)
        {
           if (it->second == to)
           {
              EnqueuePacket(packet->Copy(),m_address,to,protocolNumber, it->first);
              NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " found node " << it->first << " with matching Mac Address " << to);
              break;
           } 
        }
     }
  }
  else
  {
     EnqueuePacket(packet,m_address,to,protocolNumber, NO_DIRECTIONAL_NBR);
     NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " queueing packet");
  }
  
  return true;

}


bool 
SimpleWirelessNetDevice::SendFrom(Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (packet << dest << protocolNumber);
  Mac48Address to = Mac48Address::ConvertFrom (dest);
  Mac48Address from = Mac48Address::ConvertFrom (source);
  
  // For some reason the Ethernet header is STRIPPED from the packet
  // by the time we get here so we need to reconstruct it for for two reasons.
  // If queuing, add ethernet header to the packet in the queue so we can
  // retrieve the to, from and protocol. Also Ethernet header is
  // needed so we can apply a pcap filter in priority queues
  EthernetHeader ethHeader;
  ethHeader.SetSource (from);
  ethHeader.SetDestination (to);
  ethHeader.SetLengthType (protocolNumber);
  packet->AddHeader (ethHeader);
  
  m_macTxTrace (packet);
  

  // If directional networking is enabled, then we have to make a copy
  // of this packet and enqueue it for each destination.
  if (m_fixedNbrListEnabled)
  {
     std::map<uint32_t, Mac48Address>::iterator  it;
     
     // Look up the dest address in the eth header of the packet.
     // This is necessary because in directional networks, the dest
     // could have been changed by the trace
     packet->PeekHeader(ethHeader);
     to = ethHeader.GetDestination();
     
     if (to.IsBroadcast())
     {
        NS_LOG_INFO ("Address " << to << " is broadcast");
        // broadcast packet. Enqueue for all of our directional neighbors
        for ( it = mDirectionalNbrs.begin(); it != mDirectionalNbrs.end(); ++it)
        {
           // Note that we do not alter the to (mac address) here but instead specify
           // the node id as the destination. This gets carried with the packet
           // as a destination tag and passed to the channel. At the channel it still
           // appears as a broadcast packet but the channel only uses the dest id so it
           // will know how to handle it from the perspective of directional networking.
           EnqueuePacket(packet->Copy(),from,to,protocolNumber,it->first);
           NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " queueing packet to directional neighbor to node " << it->first);
        }
     }
     else
     {
        NS_LOG_INFO ("Address " << to << " is NOT broadcast");
        // unicast packet. Find the directional neighbor with matching MAC address. (There might not be one)
        for ( it = mDirectionalNbrs.begin(); it != mDirectionalNbrs.end(); ++it)
        {
           if (it->second == to)
           {
              EnqueuePacket(packet->Copy(),from,to,protocolNumber, it->first);
              NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " found node " << it->first << " with matching Mac Address " << to);
              break;
           } 
        }
     }
  }
  else
  {
     EnqueuePacket(packet,from,to,protocolNumber, NO_DIRECTIONAL_NBR);
     NS_LOG_INFO ("Node " << this->GetNode()->GetId() << " queueing packet");
  }
  
  return true;
  
}

bool 
SimpleWirelessNetDevice::EnqueuePacket (Ptr<Packet> packet, Mac48Address from, Mac48Address to, uint16_t protocolNumber, uint32_t destId)
{
  
  if (m_queue)
  {
     // We are using queueing.

     // Add a timestamp tag for latency
     TimestampTag timestamp;
     timestamp.SetTimestamp (Simulator::Now ());
     packet->AddPacketTag (timestamp);
     
     // add destination tag
     DestinationIdTag idTag(destId);
     packet->AddPacketTag (idTag);
          
     NS_LOG_DEBUG ("Queueing packet for destination " << destId << ". Protocol "<<  protocolNumber << " Current state is: " << m_txMachineState);
    
    // We should enqueue and dequeue the packet to hit the tracing hooks.
    if (m_queue->Enqueue (packet))
    {
        // If the channel is ready for transition we send the packet right now
        if (m_txMachineState == READY)
        {
            packet = m_queue->Dequeue ();
            TransmitStart (packet);
        }
        return true;
    }
    
    // TO DO: do we return true or false here??
    return true;
  }
  else
  {
     // No queuing is being used. Just send the packet. 
     if (m_pcapEnabled)
     {
       m_promiscSnifferTrace (packet);
     }
     EthernetHeader ethHeader;
     packet->RemoveHeader(ethHeader);
     
     m_TxBeginTrace (packet, m_address, to, protocolNumber);
     Time txTime = Seconds (m_bps.CalculateTxTime (packet->GetSize ()));
     // If we have a non-zero neighbor count then that means we are using contention and
     // the data rate changes. 
     if (m_nbrCount)
     {
        if (m_fixedNbrListEnabled)
        {
           txTime = txTime * 2;
           NS_LOG_DEBUG ("Node " << m_node->GetId() << " txTime was increased to " << txTime << " because we have directonal neighbors. packet size is " << packet->GetSize ());
        }
        else
        {
           txTime = txTime * m_nbrCount;
           NS_LOG_DEBUG ("Node " << m_node->GetId() << " txTime was increased to " << txTime << " because we have " << m_nbrCount << " neighbors. packet size is " << packet->GetSize ());
        }
     }   
     m_channel->Send (packet, protocolNumber, to, from, this, txTime, destId);
     return true;
  }
}




Ptr<Node> 
SimpleWirelessNetDevice::GetNode (void) const
{
  return m_node;
}
void 
SimpleWirelessNetDevice::SetNode (Ptr<Node> node)
{
  m_node = node;
}
bool 
SimpleWirelessNetDevice::NeedsArp (void) const
{
  return true;
}
void 
SimpleWirelessNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  m_rxCallback = cb;
}

void
SimpleWirelessNetDevice::DoDispose (void)
{
  m_channel = 0;
  m_node = 0;
  m_receiveErrorModel = 0;
  NetDevice::DoDispose ();
}


void
SimpleWirelessNetDevice::SetPromiscReceiveCallback (PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

bool
SimpleWirelessNetDevice::SupportsSendFrom (void) const
{
  return true;
}

void
SimpleWirelessNetDevice::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_bps = bps;
}

void
SimpleWirelessNetDevice::SetQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue = q;
}

Ptr<Queue>
SimpleWirelessNetDevice::GetQueue (void) const
{ 
  NS_LOG_FUNCTION_NOARGS ();
  return m_queue;
}


void SimpleWirelessNetDevice::EnablePcapAll (std::string filename)
{
	PcapHelper pcapHelper;
	Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out, PcapHelper::DLT_EN10MB);
	pcapHelper.HookDefaultSink<SimpleWirelessNetDevice> (this, "PromiscSniffer", file);
	m_pcapEnabled = true;
}

} // namespace ns3
