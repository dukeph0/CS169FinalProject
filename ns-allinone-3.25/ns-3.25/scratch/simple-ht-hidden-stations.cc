/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Sébastien Deronne
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
 * Author: Sébastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

// This example considers two hidden stations in an 802.11n network which supports MPDU aggregation.
// The user can specify whether RTS/CTS is used and can set the number of aggregated MPDUs.
//
// Example: ./waf --run "simple-ht-hidden-stations --enableRts=1 --nMpdus=8"
//
// Network topology:
//
//   Wifi 192.168.1.0
//
//        AP
//   *    *    *
//   |    |    |
//   n1   n2   n3
//
// Packets in this simulation aren't marked with a QosTag so they are considered
// belonging to BestEffort Access Class (AC_BE).

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimplesHtHiddenStations");

int main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472; //bytes
  uint64_t simulationTime = 10; //seconds
  uint32_t nMpdus = 1;
  uint32_t maxAmpduSize = 0;
  bool enableRts = 0;

  CommandLine cmd;
  cmd.AddValue ("nMpdus", "Number of aggregated MPDUs", nMpdus);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("enableRts", "Enable RTS/CTS", enableRts); // 1: RTS/CTS enabled; 0: RTS/CTS disabled
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (!enableRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));
    }
  else
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("990000"));

  //Set the maximum size for A-MPDU with regards to the payload size
  maxAmpduSize = nMpdus * (payloadSize + 200);

  // Set the maximum wireless range to 5 meters in order to reproduce a hidden nodes scenario, i.e. the distance between hidden stations is larger than 5 meters
  Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (5));

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (4);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  channel.AddPropagationLoss ("ns3::RangePropagationLossModel"); //wireless range limited to 5 meters!

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs0"));
  WifiMacHelper mac;

  Ssid ssid = Ssid ("simple-mpdu-aggregation");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false),
               "BE_MaxAmpduSize", UintegerValue (maxAmpduSize));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (MicroSeconds (102400)),
               "BeaconGeneration", BooleanValue (true),
               "BE_MaxAmpduSize", UintegerValue (maxAmpduSize));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Setting mobility model
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  // AP is between the two stations, each station being located at 5 meters from the AP.
  // The distance between the two stations is thus equal to 10 meters.
  // Since the wireless range is limited to 5 meters, the two stations are hidden from each other.
  positionAlloc->Add (Vector (5.0, 5.0, 0.0));  //AP:n0
  positionAlloc->Add (Vector (5.0, 10.0, 0.0)); //MS:n1
  positionAlloc->Add (Vector (0.0, 5.0, 0.0));  //MS:n2
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));  //MS:n3
  positionAlloc->Add (Vector (10.0, 5.0, 0.0)); //MS:n4
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer StaInterface;
  StaInterface = address.Assign (staDevices);
  Ipv4InterfaceContainer ApInterface;
  ApInterface = address.Assign (apDevice);
  // Ipv4InterfaceContainer MsInterface;
  // MsInterface = address.Assign ()

  // Setting applications
  UdpServerHelper myServer (9);
  ApplicationContainer serverApp = myServer.Install (wifiApNode);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  UdpClientHelper myClient (ApInterface.GetAddress (1), 9);
  myClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  myClient.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  myClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  // Saturated UDP traffic from stations to AP
  ApplicationContainer clientApp1 = myClient.Install (wifiStaNodes);
  clientApp1.Start (Seconds (1.0));
  clientApp1.Stop (Seconds (simulationTime + 1));

  // Install four UDP echo server applications on the AP Node
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps1 = echoServer.Install(wifiApNode.Get(1));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (10.0));
  ApplicationContainer serverApps2 = echoServer.Install(wifiApNode.Get(2));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));
  ApplicationContainer serverApps3 = echoServer.Install(wifiApNode.Get(3));
  serverApps3.Start (Seconds (1.0));
  serverApps3.Stop (Seconds (10.0));
  ApplicationContainer serverApps4 = echoServer.Install(wifiApNode.Get(4));
  serverApps4.Start (Seconds (1.0));
  serverApps4.Stop (Seconds (10.0));

  // Create echo client and set its attributes
  
  UdpEchoClientHelper echoClient1 (StaInterface.GetAddress(1), 9);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue(1));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient2 (StaInterface.GetAddress(2), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue(1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient3 (StaInterface.GetAddress(3), 9);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue(1));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient4 (StaInterface.GetAddress(4), 9);
  echoClient4.SetAttribute ("MaxPackets", UintegerValue(1));
  echoClient4.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient4.SetAttribute ("PacketSize", UintegerValue (1024));
  
  // Install four UDP client applications on each MS node connecting to each
  // corresponding server application.

  // Install echo server to node 1-4, and set start and stop time
  ApplicationContainer clientApps1 = echoClient1.Install(wifiStaNodes.Get (1));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (10.0));

  ApplicationContainer clientApps2 = echoClient2.Install(wifiStaNodes.Get (2));
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (10.0));

  ApplicationContainer clientApps3 = echoClient3.Install(wifiStaNodes.Get (3));
  clientApps3.Start (Seconds (1.0));
  clientApps3.Stop (Seconds (10.0));

  ApplicationContainer clientApps4 = echoClient4.Install(wifiStaNodes.Get (4));
  clientApps4.Start (Seconds (1.0));
  clientApps4.Stop (Seconds (10.0));
  

  phy.EnablePcap ("SimpleHtHiddenStations_Ap", apDevice.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta1", staDevices.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta2", staDevices.Get (1));

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();
  Simulator::Destroy ();

  uint32_t totalPacketsThrough1 = DynamicCast<UdpServer> (serverApps1.Get (0))->GetReceived ();
  double throughput1 = totalPacketsThrough1 * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "ServerApp1 Throughput: " << throughput1 << " Mbit/s" << '\n';

  uint32_t totalPacketsThrough2 = DynamicCast<UdpServer> (serverApps2.Get (0))->GetReceived ();
  double throughput2 = totalPacketsThrough2 * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "ServerApp2 Throughput: " << throughput2 << " Mbit/s" << '\n';

  uint32_t totalPacketsThrough3 = DynamicCast<UdpServer> (serverApps3.Get (0))->GetReceived ();
  double throughput3 = totalPacketsThrough3 * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "ServerApp3 Throughput: " << throughput3 << " Mbit/s" << '\n';

  uint32_t totalPacketsThrough4 = DynamicCast<UdpServer> (serverApps4.Get (0))->GetReceived ();
  double throughput4 = totalPacketsThrough4 * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "ServerApp4 Throughput: " << throughput4 << " Mbit/s" << '\n';

  return 0;
}
