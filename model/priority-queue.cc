/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (C) 2015 Massachusetts Institute of Technology
 * Copyright (c) 2007 University of Washington
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

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"
#include "priority-queue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PriorityQueue");

NS_OBJECT_ENSURE_REGISTERED (PriorityQueue);

TypeId PriorityQueue::GetTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::PriorityQueue")
    .SetParent<Queue> ()
    .AddConstructor<PriorityQueue> ()
    
    .AddAttribute ("ControlQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&PriorityQueue::m_controlQueue),
                   MakePointerChecker<Queue> ())
    .AddAttribute ("DataQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&PriorityQueue::m_dataQueue),
                   MakePointerChecker<Queue> ())
    .AddAttribute ("ControlPacketClassifier", 
                   "Pcap style filter to classify control packets",
                   StringValue (),
                   MakeStringAccessor (&PriorityQueue::m_classifier),
                   MakeStringChecker ())
  ;

  return tid;
}

PriorityQueue::PriorityQueue () :
  Queue ()
{
  NS_LOG_FUNCTION (this);

  m_pcapHandle = pcap_open_dead (DLT_EN10MB, 1500);
  NS_ASSERT_MSG (m_pcapHandle, "failed to open pcap handle");
}

PriorityQueue::~PriorityQueue ()
{
  NS_LOG_FUNCTION (this);

  m_controlQueue = 0;
  m_dataQueue = 0;

  pcap_close (m_pcapHandle);
}

void
PriorityQueue::Initialize ()
{
  NS_LOG_FUNCTION (this);

  int ret = pcap_compile (m_pcapHandle, &m_bpf,
                          m_classifier.c_str (), 1, PCAP_NETMASK_UNKNOWN);
  NS_ASSERT_MSG (ret == 0, "failed to compile control packet classifer");
}

void
PriorityQueue::SetControlQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (this << q);
  m_controlQueue = q;
}

void
PriorityQueue::SetDataQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (this << q);
  m_dataQueue = q;
}

Ptr<Queue>
PriorityQueue::GetControlQueue (void) const 
{ 
  NS_LOG_FUNCTION_NOARGS ();
  return m_controlQueue;
}

Ptr<Queue>
PriorityQueue::GetDataQueue (void) const 
{ 
  NS_LOG_FUNCTION_NOARGS ();
  return m_dataQueue;
}

PriorityQueue::PacketClass 
PriorityQueue::Classify (Ptr<const Packet> p)
{
  pcap_pkthdr pcapPkthdr;
  pcapPkthdr.caplen = p->GetSize ();
  pcapPkthdr.len = p->GetSize ();
  uint8_t *data = new uint8_t[p->GetSize ()];
  p->CopyData (data, p->GetSize ());
  int ret = pcap_offline_filter (&m_bpf, &pcapPkthdr, data);
  delete [] data;

  if (ret == 0)
    {
      NS_LOG_DEBUG ("Packet is data packet");
      return PACKET_CLASS_DATA;
    }
  else
    {
      NS_LOG_DEBUG ("Packet is control packet");
      return PACKET_CLASS_CONTROL;
    }
}

bool 
PriorityQueue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  PacketClass packetClass = Classify (p);

  if (packetClass == PACKET_CLASS_CONTROL)
    {
      return m_controlQueue->Enqueue (p);
    }
  else
    {
      return m_dataQueue->Enqueue (p);
    }
}

Ptr<Packet>
PriorityQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> p = 0;
  if (!m_controlQueue->IsEmpty ())
    {
      p = m_controlQueue->Dequeue ();
    }
  else
    {
      p = m_dataQueue->Dequeue ();
    }

  return p;
}

Ptr<const Packet>
PriorityQueue::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  Ptr<const Packet> p = 0;
  if (!m_controlQueue->IsEmpty ())
    {
      p = m_controlQueue->Peek ();
    }
  else
    {
      p = m_dataQueue->Peek ();
    }

  return p;
}

} // namespace ns3

