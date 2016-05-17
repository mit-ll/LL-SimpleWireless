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
#ifndef SIMPLE_WIRELESS_NET_DEVICE_H
#define SIMPLE_WIRELESS_NET_DEVICE_H

#include <stdint.h>
#include <string>
#include "ns3/traced-callback.h"
#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include "ns3/trace-helper.h"
#include "ns3/data-rate.h"
#include "ns3/queue.h"
#include "ns3/ethernet-header.h"
#include "ns3/double.h"
#include "ns3/boolean.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace ns3 {

class SimpleWirelessChannel;
class Node;
class ErrorModel;

#define NO_DIRECTIONAL_NBR  0xFFFFFFFF


//********************************************************
//  TimestampTag used to store a timestamp with a packet
//  when they are placed in the queue
//********************************************************
class TimestampTag : public Tag {
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);

  // these are our accessors to our tag structure
  void SetTimestamp (Time time);
  Time GetTimestamp (void) const;

  void Print (std::ostream &os) const;

private:
  Time m_timestamp;

  // end class TimestampTag
};


//********************************************************
//  DestinationIdTag used to store a destination node id 
//  with a packet when they are placed in the queue. 
//  This is used by directional networks.
//********************************************************
class DestinationIdTag : public Tag {
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  DestinationIdTag();
  
  /**
   *  Constructs a DestinationIdTag with the given node id
   *
   *  \param destId Id to use for the tag
   */
  DestinationIdTag (uint32_t destId);

  // these are our accessors to our tag structure
  void SetDestinationId (uint32_t destId);
  uint32_t GetDestinationId (void) const;

  void Print (std::ostream &os) const;

private:
  uint32_t m_destnodeid;

  // end class DestinationIdTag
};



/**
 * \ingroup netdevice
 *
 * This device does not have a helper and assumes 48-bit mac addressing;
 * the default address assigned to each device is zero, so you must 
 * assign a real address to use it.  There is also the possibility to
 * add an ErrorModel if you want to force losses on the device.
 * 
 * \brief simple net device for simple things and testing
 */
class SimpleWirelessNetDevice : public NetDevice
{
public:
  static TypeId GetTypeId (void);
  SimpleWirelessNetDevice ();

  void Receive (Ptr<Packet> packet, uint16_t protocol, Mac48Address to, Mac48Address from);
  void SetChannel (Ptr<SimpleWirelessChannel> channel);

  /**
   * Attach a receive ErrorModel to the SimpleWirelessNetDevice.
   *
   * The SimpleWirelessNetDevice may optionally include an ErrorModel in
   * the packet receive chain.
   *
   * \see ErrorModel
   * \param em Ptr to the ErrorModel.
   */
  void SetReceiveErrorModel(Ptr<ErrorModel> em);
  
  /**
   * Set the Data Rate used for transmission of packets.  The data rate is
   * set in the Attach () method from the corresponding field in the channel
   * to which the device is attached.  It can be overridden using this method.
   *
   * @see Attach ()
   * @param bps the data rate at which this object operates
   */
  void SetDataRate (DataRate bps);
  
    /**
   * Attach a queue to the PointToPointNetDevice.
   *
   * The PointToPointNetDevice "owns" a queue that implements a queueing 
   * method such as DropTail or RED.
   *
   * @see Queue
   * @see DropTailQueue
   * @param queue Ptr to the new queue.
   */
  void SetQueue (Ptr<Queue> queue);

  /**
   * Get a copy of the attached Queue.
   *
   * @returns Ptr to the queue.
   */
  Ptr<Queue> GetQueue (void) const;
  
  //******************************************
  // Directional Neighbor functions
  bool AddDirectionalNeighbors(std::map<uint32_t, Mac48Address> nodesToAdd);
  bool AddDirectionalNeighbor(uint32_t nodeid, Mac48Address macAddr);
  void DeleteDirectionalNeighbors(std::set<uint32_t> nodeids);
  void DeleteDirectionalNeighbor(uint32_t nodeid);
  
  //******************************************
  // Fixed Contention functions
  void ClearNbrCount(void);
  void IncrementNbrCount(void);
  int GetNbrCount(void);

  
  void EnablePcapAll(std::string filename);

  // inherited from NetDevice base class.
  virtual void SetIfIndex(const uint32_t index);
  virtual uint32_t GetIfIndex(void) const;
  virtual Ptr<Channel> GetChannel (void) const;
  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;
  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;
  virtual bool IsMulticast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;
  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;
  virtual bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
  virtual bool SendFrom(Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
  virtual Ptr<Node> GetNode (void) const;
  virtual void SetNode (Ptr<Node> node);
  virtual bool NeedsArp (void) const;
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  virtual bool EnqueuePacket(Ptr<Packet> packet, Mac48Address from, Mac48Address to, uint16_t protocolNumber, uint32_t destId);

  virtual Address GetMulticast (Ipv6Address addr) const;

  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom (void) const;

protected:
  virtual void DoDispose (void);
private:
  Ptr<SimpleWirelessChannel> m_channel;
  NetDevice::ReceiveCallback m_rxCallback;
  NetDevice::PromiscReceiveCallback m_promiscCallback;
  Ptr<Node> m_node;
  uint16_t m_mtu;
  uint32_t m_ifIndex;
  Mac48Address m_address;
  Ptr<ErrorModel> m_receiveErrorModel;
  
  Ptr<Packet> m_currentPkt;
  
    /**
   * Start Sending a Packet Down the Wire.
   *
   * The TransmitStart method is the method that is used internally in the
   * NetDevice to begin the process of sending a packet out on
   * the channel.  The corresponding method is called on the channel to let
   * it know that the physical device this class represents has virtually
   * started sending signals.  An event is scheduled for the time at which
   * the bits have been completely transmitted.
   */
  void TransmitStart (Ptr<Packet>);

  /**
   * Stop Sending a Packet Down the Wire and Begin the Interframe Gap.
   *
   * The TransmitComplete method is used internally to finish the process
   * of sending a packet out on the channel.
   */
  void TransmitComplete (void);

  /**
   * Enumeration of the states of the transmit machine of the net device.
   */
  enum TxMachineState
  {
    READY,   /**< The transmitter is ready to begin transmission of a packet */
    BUSY     /**< The transmitter is busy transmitting a packet */
  };
  /**
   * The state of the Net Device transmit state machine.
   * @see TxMachineState
   */
  TxMachineState m_txMachineState;

  
    /**
   * The data rate that the Net Device uses to simulate packet transmission
   * timing.
   * @see class DataRate
   */
  DataRate       m_bps;
  
    /**
   * The Queue which this device uses as a packet source.
   * Management of this Queue has been delegated to the device
   * and it has the responsibility for deletion.
   * @see class Queue
   * @see class DropTailQueue
   */
  Ptr<Queue> m_queue;  

  /**
   * The trace source fired when a packet begins the reception process from
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet>, Mac48Address, Mac48Address, uint16_t > m_phyRxBeginTrace;

  /**
   * The trace source fired when a packet ends the reception process from
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet>, Mac48Address, Mac48Address, uint16_t > m_phyRxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet it has received
   * due to the error model being active.  Although SimpleWirelessNetDevice doesn't 
   * really have a Phy model, we choose this trace source name for alignment
   * with other trace sources.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet>, Mac48Address, Mac48Address, uint16_t > m_phyRxDropTrace;

  /**
   * A trace source that emulates a promiscuous mode protocol sniffer connected
   * to the device.  This trace source fire on packets destined for any host
   * just like your average everyday packet sniffer.
   *
   * The trace is captured on send and receive.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_promiscSnifferTrace;
  
  /**
   * The trace source fired when a packet begins the transmission process on
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet>, Mac48Address, Mac48Address, uint16_t > m_TxBeginTrace;
  
  /**
   * The trace source fired when a packet is dequeued to be sent.
   * It does NOT track queue latency for packets that get dropped
   * in the queue only that that are actually sent from the queue
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet>, Time> m_QueueLatencyTrace;
  
  /**
   * The trace source fired when packets come into the "top" of the device
   * at the L3/L2 transition, before being queued for transmission.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macTxTrace;
  
  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3
   * transition).  This is a non- promiscuous trace.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macRxTrace;

  
  uint32_t  m_pktRcvTotal;
  uint32_t  m_pktRcvDrop;
  bool      m_pcapEnabled;
  
  bool   m_fixedNbrListEnabled;
  std::map<uint32_t, Mac48Address> mDirectionalNbrs;
  
  int  m_nbrCount;
};

} // namespace ns3

#endif /* SIMPLE_WIRELESS_NET_DEVICE_H */
