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

#ifndef PRIORITY_H
#define PRIORITY_H

#include "ns3/packet.h"
#include "ns3/queue.h"

#include <pcap.h>
#undef DLT_IEEE802_11_RADIO // Avoid namespace collision with ns3::YansWifiPhyHelper::DLT_IEEE802_11_RADIO

namespace ns3 {

class TraceContainer;

/**
 * \ingroup queue
 *
 * \brief A strict priority queue with two subqueues, one for control packets and one for data
 */
class PriorityQueue : public Queue {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief PriorityQueue Constructor
   *
   * Creates a priority queue with a maximum size of 100 packets by default
   */
  PriorityQueue ();

  virtual ~PriorityQueue();

  /**
   * \brief PriorityQueue Constructor
   *
   * Initializes priority queue
   */
  void Initialize ();

  /**
   * Attach a queue to hold data packets to the PriorityQueue.
   *
   * The PriorityQueue "owns" a sub queue that implements a queueing 
   * method such as DropTailQueue, DropHeadQueue or RedQueue
   *
   * \param queue Ptr to the new queue.
   */
  void SetDataQueue (Ptr<Queue> q);

  /**
   * Attach a queue to hold control packets to the PriorityQueue.
   *
   * The PriorityQueue "owns" a sub queue that implements a queueing 
   * method such as DropTailQueue, DropHeadQueue or RedQueue
   *
   * \param queue Ptr to the new queue.
   */
  void SetControlQueue (Ptr<Queue> q);

  /**
   * Get a copy of the attached Queue that holds control packets.
   *
   * \returns Ptr to the queue.
   */
  Ptr<Queue> GetControlQueue (void) const;

  /**
   * Get a copy of the attached Queue that holds data packets.
   *
   * \returns Ptr to the queue.
   */
  Ptr<Queue> GetDataQueue (void) const;

  /**
   * \brief Enumeration of the modes supported in the class.
   *
   */
  enum PacketClass
  {
    PACKET_CLASS_CONTROL,     /**< Packet classifier matched packet to control type */
    PACKET_CLASS_DATA,        /**< Packet classifier matched packet to control type */
  };

private:
  virtual bool DoEnqueue (Ptr<Packet> p);
  virtual Ptr<Packet> DoDequeue (void);
  virtual Ptr<const Packet> DoPeek (void) const;
  PacketClass Classify (Ptr<const Packet> p);

  Ptr<Queue> m_controlQueue;         //!< queue for control traffic
  Ptr<Queue> m_dataQueue;            //!< queue for data traffic

  std::string m_classifier;          //!< classfier for control packets
  pcap_t * m_pcapHandle;             //!< handle for libpcap
  struct bpf_program m_bpf;          //!< compiled classifier for control packets
};

} // namespace ns3

#endif /* PRIORITY_H */
