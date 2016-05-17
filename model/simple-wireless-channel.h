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
#ifndef SIMPLE_WIRELESS_CHANNEL_H
#define SIMPLE_WIRELESS_CHANNEL_H

#include <vector>
#include "ns3/channel.h"
#include "ns3/mac48-address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/enum.h"
#include "ns3/string.h"



namespace ns3 {

class SimpleWirelessNetDevice;
class Packet;

enum ErrorModelType {
    /**
     * In Constant mode, the packet error rate is constant within the
     * specified distance range.
     */
    CONSTANT,
    /**
     * In PER Curve mode, the packet error curves are used to
     * determine the error rate. The distance is used to look up the
     * error rate. Note that the range value specified is ignored.
     * User must build the PER curve by calling addToPERmodel.
     */
    PER_CURVE,
    /**
     * In Stochastic mode, there are no per packet errors. Instead,
     * the link to each neighbor is either on or off for randomly
     * selected durations.
     */
    STOCHASTIC
};

//***************************************************************
// Define key for stochastic error map key
//***************************************************************
class StochasticKey {
   public:
   int srcNodeId;
   int destNodeId;
 
   StochasticKey(int k1, int k2)
      : srcNodeId(k1), destNodeId(k2){}  
      
   // Ordering is:
   //   1. lowest source node id
   //   2. lowest destination node id

   bool operator<(const StochasticKey &right) const   
   {
      if (srcNodeId == right.srcNodeId) 
      {
         return(destNodeId < right.destNodeId);
      }
      else 
      {
         return (srcNodeId < right.srcNodeId);
      }
   }    
};

struct StochasticLink
{
   bool  linkState;  // 1 = ON, 0 = OFF
   Time  stateExpireTime;
};

typedef std::map<StochasticKey, StochasticLink> ::iterator  StochasIt;

/**
 * \ingroup channel
 * \brief A simple channel, for simple things and testing
 */
class SimpleWirelessChannel : public Channel
{
public:
  static TypeId GetTypeId (void);
  SimpleWirelessChannel ();

  void Send (Ptr<Packet> p, uint16_t protocol, Mac48Address to, Mac48Address from,
               Ptr<SimpleWirelessNetDevice> sender, Time txTime, uint32_t destId);

  void Add (Ptr<SimpleWirelessNetDevice> device);

  // inherited from ns3::Channel
  virtual uint32_t GetNDevices (void) const;
  virtual Ptr<NetDevice> GetDevice (uint32_t i) const;
  
  void setErrorModelType(ErrorModelType type);
  void setErrorRate(double error);
  void addToPERmodel(double distance, double error);
  bool packetInError(double distance);
  void EnableFixedContention(void);
  void SetFixedContentionRange(double error);
  void InitStochasticModel();
  bool CheckStochasticError(uint32_t srcId, uint32_t dstId);
  
private:
  std::vector<Ptr<SimpleWirelessNetDevice> > m_devices;
  double m_range;
  double m_errorRate;
  ErrorModelType m_ErrorModel;
  Ptr<UniformRandomVariable> m_random;
  std::map<double, double>  mPERmap;
  
  bool   m_fixedContentionEnabled;
  double m_fixedContentionRange;
  Ptr<ExponentialRandomVariable> m_randomUp;
  Ptr<ExponentialRandomVariable> m_randomDown;
  
  Time m_upDuration;
  Time m_downDuration;
  std::map<StochasticKey, StochasticLink>   m_StochasticLinks;
  
};

} // namespace ns3

#endif /* SIMPLE_WIRELESS_CHANNEL_H */
