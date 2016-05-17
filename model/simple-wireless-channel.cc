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
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/ptr.h"
#include "ns3/mobility-model.h"
#include "simple-wireless-channel.h"
#include "simple-wireless-net-device.h"
#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("SimpleWirelessChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SimpleWirelessChannel);

TypeId 
SimpleWirelessChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SimpleWirelessChannel")
    .SetParent<Channel> ()
    .AddConstructor<SimpleWirelessChannel> ()
    .AddAttribute ("MaxRange",
                   "Maximum Transmission Range (meters)",
                   DoubleValue (250),
                   MakeDoubleAccessor (&SimpleWirelessChannel::m_range),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RangeErrorModel", 
                   "Type or range based error model",
                   EnumValue (CONSTANT),
                   MakeEnumAccessor (&SimpleWirelessChannel::m_ErrorModel),
                   MakeEnumChecker (CONSTANT, "Constant",
                                    PER_CURVE, "PER_CURVE",
                                    STOCHASTIC, "STOCHASTIC"))
    .AddAttribute ("RangeErrorRate",
                   "Error rate when using constant Range Error Model",
                   StringValue("0.0"),
                   MakeDoubleAccessor (&SimpleWirelessChannel::m_errorRate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableFixedContention", 
                   "Enabled or Disabled",
                   BooleanValue (false),
                   MakeBooleanAccessor (&SimpleWirelessChannel::m_fixedContentionEnabled),
                   MakeBooleanChecker ())              
    .AddAttribute ("FixedContentionRange",
                   "Maximum Range (meters) for Fixed Contention",
                   DoubleValue (0),  // default to 0 so we can use the tx range as default if the user does not set this
                   MakeDoubleAccessor (&SimpleWirelessChannel::m_fixedContentionRange),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("AvgLinkUpDuration",
                   "Average time that the link to a neighbor is up for Stochastic Error Model",
                   TimeValue (MicroSeconds (10000.0)),
                   MakeTimeAccessor (&SimpleWirelessChannel::m_upDuration),
                   MakeTimeChecker ())
    .AddAttribute ("AvgLinkDownDuration",
                   "Average time that the link to a neighbor is down for Stochastic Error Model",
                   TimeValue (MicroSeconds (100.0)),
                   MakeTimeAccessor (&SimpleWirelessChannel::m_downDuration),
                   MakeTimeChecker ())
    ;
  return tid;
}

SimpleWirelessChannel::SimpleWirelessChannel ()
{
	// Default to a constant error model with 0 errors
	m_random = CreateObject<UniformRandomVariable> ();
	m_ErrorModel = CONSTANT;
	m_errorRate = 0.0;
	m_fixedContentionEnabled = false;
	m_fixedContentionRange = 0;
}

void
SimpleWirelessChannel::Send (Ptr<Packet> p, uint16_t protocol, 
                                Mac48Address to, Mac48Address from,
                                Ptr<SimpleWirelessNetDevice> sender, Time txTime, uint32_t destId)
{
  NS_LOG_FUNCTION (p << protocol << to << from << sender);
  
  uint32_t senderNodeId = sender->GetNode()->GetId();
  
  if (m_fixedContentionEnabled)
  {
     sender->ClearNbrCount();
     
     // if the range has not been set, use the tx range.
     // We do this here because we want the value for range that the user selected value.
     // If we set the contention range to m_range in other places (like the call to
     // EnableFixedContention), at that point the range could still be the default 
     // value and not the value the user has set. 
     if (m_fixedContentionRange == 0)
         m_fixedContentionRange = m_range;
  }

  for (std::vector<Ptr<SimpleWirelessNetDevice> >::const_iterator i = m_devices.begin (); i != m_devices.end (); ++i)
    {
      Ptr<SimpleWirelessNetDevice> tmp = *i;
      uint32_t destNodeId = tmp->GetNode()->GetId();
      
      // don't send to ourselves
      if (tmp == sender)
        {
          NS_LOG_INFO ("Node " << senderNodeId << " NOT sending to node " << destNodeId << ". Node is self");
          continue;
        }
        
      // See if we have directional networking enabled and if so if this is destination node
      if ( (destId != NO_DIRECTIONAL_NBR) && (destNodeId != destId) )
      {
         NS_LOG_INFO ("Node " << senderNodeId << " NOT sending to node " << destNodeId << ". Directional networking enabled and node is not destination " << destId);
         continue;
      }
      
      // See if we are using stochastic. If so see if the sender's link
      // to the destination is up or down
      if (CheckStochasticError (senderNodeId, destNodeId))
      {
         NS_LOG_INFO ("Node " << senderNodeId << " NOT sending to node " << destNodeId << ". Stochastic error enabled and link to node is in OFF state");
         continue;
      }

      Ptr<MobilityModel> a = sender->GetNode ()->GetObject<MobilityModel> ();
      Ptr<MobilityModel> b = tmp->GetNode ()->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (a && b, "Error:  nodes must have mobility models");
      
      // Get distance and determine error rate based on that
      // and the error model
      double distance = a->GetDistanceFrom (b);
      
      
      // if fixed contention is enabled then we need to peg the neighbor count
      if ( (m_fixedContentionEnabled) && (distance < m_fixedContentionRange) )
      {
         sender->IncrementNbrCount();
         NS_LOG_INFO ("Node " << senderNodeId << " pegging nbr count for contention. distane is " << distance << ". count is now " << sender->GetNbrCount());
      }
      
      // Is this packet beyond the transmission range?
      if (distance > m_range)
      {
         NS_LOG_INFO ("Node " << senderNodeId << " NOT sending to node " << destNodeId << ". distance of " << distance << "  is out of range");
         continue;
      }
      
      // Is this packet in error or can we send it based on the distance?
      if (packetInError(distance))
      {
         continue;
      }

      // propagation delay. speed of light is 3.3 ns/meter
      double propDelay = 3.3 * distance;
      NS_LOG_INFO ("Node " << senderNodeId << " sending to node " << destNodeId 
        << " at distance " << distance << " meters; time (ns): "<< Simulator::Now().GetNanoSeconds ()
        << " txDelay: " << txTime << "  propDelay: " << propDelay);
        
      Simulator::ScheduleWithContext (destNodeId, NanoSeconds (txTime + propDelay),
                                      &SimpleWirelessNetDevice::Receive, tmp, p->Copy (), protocol, to, from);

    }
}

void 
SimpleWirelessChannel::Add (Ptr<SimpleWirelessNetDevice> device)
{
  m_devices.push_back (device);
}

uint32_t 
SimpleWirelessChannel::GetNDevices (void) const
{
  return m_devices.size ();
}

Ptr<NetDevice> 
SimpleWirelessChannel::GetDevice (uint32_t i) const
{
  return m_devices[i];
}


//********************************************************************
// contention functions
void SimpleWirelessChannel::EnableFixedContention(void)
{
  m_fixedContentionEnabled = true;
  
  // Set up all the devices to support contention.
  // IMPORTANT NOTE: There may not be any devices at this point on the channel.
  // If there are not any, then the first packet sent by the device will use the
  // full data rate and will not use contention. That is, the device will be notified
  // by the channel when the channel gets the first packet from the device and that is
  // after the data rate has been used to set the tx time. All subsequent packets sent
  // by the device will use contention just not the first one.
  for (std::vector<Ptr<SimpleWirelessNetDevice> >::const_iterator i = m_devices.begin (); i != m_devices.end (); ++i)
  {
    Ptr<SimpleWirelessNetDevice> tmp = *i;
    tmp->ClearNbrCount();
  }
}
  
void SimpleWirelessChannel::SetFixedContentionRange(double range)
{
  m_fixedContentionRange = range;
}

//********************************************************************
// Error Model functions
void SimpleWirelessChannel::setErrorModelType(ErrorModelType type) 
{
  m_ErrorModel = type;
  
  // reset range to 0 if error model is PER CURVE so that
  // we can compare to distances added to the curve to
  // get the max range
  if (m_ErrorModel == PER_CURVE)
  {
     m_range = 0;
  }
}

void SimpleWirelessChannel::setErrorRate(double error) 
{
  m_errorRate = error;
}

void SimpleWirelessChannel::addToPERmodel(double distance, double error)
{
  // NOTE: distance is in meters
  mPERmap.insert(std::pair<double, double>(distance, error));
  
  if (distance > m_range)
  {
     m_range = distance;
  }
}


//********************************************************************
// Stochastic error functions

void SimpleWirelessChannel::InitStochasticModel()
{
  // Build the map of neighbor links, setting all links to
  // ON state and pick a duration for that state.
  if (m_ErrorModel == STOCHASTIC)
  {
     if (m_devices.size() == 0)
     {
        NS_LOG_ERROR ("InitStochasticModel called but there are no devices on the channel. Be sure to call InitStochasticModel AFTER devices have been added.");
     }
     
     m_randomUp = CreateObject<ExponentialRandomVariable> ();
     m_randomDown = CreateObject<ExponentialRandomVariable> ();

     m_randomUp->SetAttribute ("Mean", DoubleValue (m_upDuration.GetMicroSeconds()));
     m_randomDown->SetAttribute ("Mean", DoubleValue (m_downDuration.GetMicroSeconds()));
     
     Time currTime = Simulator::Now();
     
     for (std::vector<Ptr<SimpleWirelessNetDevice> >::const_iterator i = m_devices.begin (); i != m_devices.end (); ++i)
     {
        for (std::vector<Ptr<SimpleWirelessNetDevice> >::const_iterator j = m_devices.begin (); j != m_devices.end (); ++j)
        {
           int src = (*i)->GetNode()->GetId();
           int dst = (*j)->GetNode()->GetId();
           
           if (src == dst)
              continue;
           
           StochasticLink tempLink;
           tempLink.linkState = true;
           tempLink.stateExpireTime = currTime + MicroSeconds(m_randomUp->GetValue());
           //tempLink.stateExpireTime = Simulator::Now() + Seconds(1.0);
           
           m_StochasticLinks.insert(std::pair<StochasticKey, StochasticLink>(StochasticKey(src,dst), tempLink));
           NS_LOG_DEBUG("Add link to stochastic map. src: " << src << " dst: " << dst << " expireTime: " 
                      << std::setprecision (9) << tempLink.stateExpireTime.GetSeconds() << " state: " << tempLink.linkState);
        }
      }
   }
}


// This returns true if we should NOT send the packet.
// That is, return true == packet fails to send

bool SimpleWirelessChannel::CheckStochasticError(uint32_t srcId, uint32_t dstId)
{
  if (m_ErrorModel == STOCHASTIC)
  {
     // get entry from map for this src/dst pair
     StochasIt iter = m_StochasticLinks.find(StochasticKey(srcId, dstId));
     NS_ASSERT( iter != m_StochasticLinks.end());
     
     Time currTime = Simulator::Now();
     
     //std::cout << std::setprecision (9) << currTime.GetSeconds() << " Checking state for link src: " << srcId << " dst: " << dstId << " expireTime: " << iter->second.stateExpireTime << std::endl;
     
     if (currTime >= iter->second.stateExpireTime)
     {
        // the time at which the previous state was set to end has already passed.
        Time endTime = iter->second.stateExpireTime;
        bool tempState = iter->second.linkState;
        Time newDuration;
        // Pick the new states until we get to one that is at or greater
        // than the current time.
        while (endTime < currTime)
        {
            // Now pick duration for the new state
            if (!tempState)
            {
               newDuration = MicroSeconds(m_randomUp->GetValue());
            } 
            else
            {
               newDuration = MicroSeconds(m_randomDown->GetValue());
            }
            endTime += newDuration;
            tempState = !tempState;
            
            NS_LOG_DEBUG("---> " << std::setprecision (9) << currTime.GetSeconds() << " next state: " << tempState << " for link src: " << srcId << " dst: " << dstId 
                    << " duration of next state: " << std::setprecision (9) << newDuration.GetSeconds()
                    << " expireTime: " << std::setprecision (9) << endTime.GetSeconds());
         }
        
        // When we get here, the new state and time are selected
        iter->second.linkState = tempState;
        iter->second.stateExpireTime = endTime;
       
        NS_LOG_DEBUG(std::setprecision (9) << currTime.GetSeconds() << " New state " << iter->second.linkState << " for link src: " << srcId << " dst: " << dstId 
                    << " duration of next state: " << std::setprecision (9) << newDuration.GetSeconds()
                    << " expireTime: " << std::setprecision (9) << iter->second.stateExpireTime.GetSeconds());
        
     }
     else
     {
         NS_LOG_DEBUG(std::setprecision (9) << currTime.GetSeconds() << " State " << iter->second.linkState << " for link src: " << srcId << " dst: " << dstId 
                    << " expireTime: " << std::setprecision (9) << iter->second.stateExpireTime.GetSeconds());
     }
     
     // now return true or false depending on state
     // true = packet is in "error" and failes
     // false = packet not in error and sends
     if (iter->second.linkState)
     {
        return false;
     } 
     else
     {
        return true;
     } 
  }
  else
  {
     return false;
  }
}

//********************************************************************

bool SimpleWirelessChannel::packetInError(double distance)
{
  std::map<double, double>::iterator it;
  std::map<double, double>::iterator up_iter;
  std::map<double, double>::iterator low_iter;
  
  if (m_ErrorModel == CONSTANT)
  {
      if ( m_random->GetValue () < m_errorRate )
      {
        NS_LOG_INFO("Error Model: " << m_ErrorModel << " Checking for error at distance: " << distance << "  Too high error. Packet in error.");
        return true;
      }
  }
  else if (m_ErrorModel == PER_CURVE)
  {
     it = mPERmap.find(distance);
     if (it != mPERmap.end())
     {
       // we found an exact match in the map for this distance
       if (m_random->GetValue() < it->second)
       {
          NS_LOG_INFO("Error Model: " << m_ErrorModel << " Checking for error at distance: " << distance << "  Too high error. Packet in error.");
          return true;
       }
     }
     else
     {
       // we did not find an exact match in the map. That's OK
       // since it could actually be unlikely to find one.
       // Now we just extrapolate between the two entries in the map
       
       // first get iterator to upper bound
       up_iter = mPERmap.upper_bound(distance);
       if (up_iter == mPERmap.end())
       {
         // We shouldn't hit this situation because we already checked the distance
         // relative to the range but go ahead and leave this here
         // this distance is beyond the upper bound so error is 100%
         NS_LOG_INFO("Error Model: " << m_ErrorModel << " Checking for error at distance: " << distance << "  Too high error. Packet in error.");
         return true;
       }
       
       // Set low iter to upper then decrement it
       low_iter = up_iter;
       --low_iter;
       
       double errorRate = low_iter->second + ( ((distance - low_iter->first)/(up_iter->first - low_iter->first)) * (up_iter->second - low_iter->second));
       NS_LOG_INFO("Error Model: " << m_ErrorModel << "  distance: " << distance << "  calculated error rate: " << errorRate << "  low distance: " << low_iter->first << "  low error: " << low_iter->second 
                    << "  high distance: " << up_iter->first << "  high error: " << up_iter->second);
       
       if (m_random->GetValue() < errorRate)
       {
          return true;
       }
     }
  }


  // if we get here then there were no errors
  return false;
}


} // namespace ns3
