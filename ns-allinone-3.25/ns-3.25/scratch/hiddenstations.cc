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

  // Set time resolution
  Time::SetResolution (Time::NS);
  // Enable log components and set log levels
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_ALL);

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

  // Create Channel 
  PointToPointHelper  pointToPoint;

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
  positionAlloc->Add (Vector (5.0, 5.0, 0.0));
  positionAlloc->Add (Vector (5.0, 10.0, 0.0));
  positionAlloc->Add (Vector (0.0, 5.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 5.0, 0.0));
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

  // Setting applications
  UdpEchoServerHelper myServer (9);

  // Install four UDP echo server applications on the AP node
  ApplicationContainer serverApp1 = myServer.Install (wifiApNode.Get (0));
  serverApp1.Start (Seconds (0.0));
  serverApp1.Stop (Seconds (simulationTime + 1));

  UdpClientHelper myClient1 (ApInterface.GetAddress (0), 9);
  myClient1.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  myClient1.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  myClient1.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer serverApp2 = myServer.Install (wifiApNode.Get (1));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (Seconds (simulationTime + 1));

  UdpClientHelper myClient2 (ApInterface.GetAddress (1), 9);
  myClient2.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  myClient2.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  myClient2.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer serverApp3 = myServer.Install (wifiApNode.Get (2));
  serverApp3.Start (Seconds (0.0));
  serverApp3.Stop (Seconds (simulationTime + 1));

  UdpClientHelper myClient3 (ApInterface.GetAddress (2), 9);
  myClient3.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  myClient3.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  myClient3.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer serverApp4 = myServer.Install (wifiApNode.Get (3));
  serverApp4.Start (Seconds (0.0));
  serverApp4.Stop (Seconds (simulationTime + 1));

  UdpClientHelper myClient4 (ApInterface.GetAddress (3), 9);
  myClient4.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  myClient4.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  myClient4.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer serverApp5 = myServer.Install (wifiApNode.Get (4));
  serverApp5.Start (Seconds (0.0));
  serverApp5.Stop (Seconds (simulationTime + 1));

  UdpClientHelper myClient5 (ApInterface.GetAddress (4), 9);
  myClient5.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  myClient5.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  myClient5.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  

  // Install four UDP echo client applications on each MS node connecting to 
  // corresponding server application

  // n1:
  UdpEchoClientHelper echoClient_MS1 (StaInterface.GetAddress (1), 9);
  echoClient_MS1.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  echoClient_MS1.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  echoClient_MS1.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer MS1_client0 = echoClient_MS1.Install(wifiStaNodes.Get (0));
  MS1_client0.Start (Seconds (1.0));
  MS1_client0.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS1_client2 = echoClient_MS1.Install(wifiStaNodes.Get (2));
  MS1_client2.Start (Seconds (1.0));
  MS1_client2.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS1_client3 = echoClient_MS1.Install(wifiStaNodes.Get (3));
  MS1_client3.Start (Seconds (1.0));
  MS1_client3.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS1_client4 = echoClient_MS1.Install(wifiStaNodes.Get (4));
  MS1_client4.Start (Seconds (1.0));
  MS1_client4.Stop (Seconds (simulationTime + 1));

  // n2:
  UdpEchoClientHelper echoClient_MS2 (StaInterface.GetAddress (2), 9);
  echoClient_MS2.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  echoClient_MS2.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  echoClient_MS2.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer MS2_client0 = echoClient_MS2.Install(wifiStaNodes.Get (0));
  MS2_client0.Start (Seconds (1.0));
  MS2_client0.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS2_client1 = echoClient_MS2.Install(wifiStaNodes.Get (1));
  MS2_client1.Start (Seconds (1.0));
  MS2_client1.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS2_client3 = echoClient_MS2.Install(wifiStaNodes.Get (3));
  MS2_client3.Start (Seconds (1.0));
  MS2_client3.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS2_client4 = echoClient_MS2.Install(wifiStaNodes.Get (4));
  MS2_client4.Start (Seconds (1.0));
  MS2_client4.Stop (Seconds (simulationTime + 1));

  // n3:
  UdpEchoClientHelper echoClient_MS3 (StaInterface.GetAddress (3), 9);
  echoClient_MS3.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  echoClient_MS3.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  echoClient_MS3.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer MS3_client0 = echoClient_MS3.Install(wifiStaNodes.Get (0));
  MS3_client0.Start (Seconds (1.0));
  MS3_client0.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS3_client1 = echoClient_MS3.Install(wifiStaNodes.Get (1));
  MS3_client1.Start (Seconds (1.0));
  MS3_client1.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS3_client2 = echoClient_MS3.Install(wifiStaNodes.Get (2));
  MS3_client2.Start (Seconds (1.0));
  MS3_client2.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS3_client4 = echoClient_MS3.Install(wifiStaNodes.Get (4));
  MS3_client4.Start (Seconds (1.0));
  MS3_client4.Stop (Seconds (simulationTime + 1));

  // ns4:
  UdpEchoClientHelper echoClient_MS4 (StaInterface.GetAddress (4), 9);
  echoClient_MS4.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  echoClient_MS4.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  echoClient_MS4.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer MS4_client0 = echoClient_MS4.Install(wifiStaNodes.Get (0));
  MS4_client0.Start (Seconds (1.0));
  MS4_client0.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS4_client1 = echoClient_MS4.Install(wifiStaNodes.Get (1));
  MS4_client1.Start (Seconds (1.0));
  MS4_client1.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS4_client2 = echoClient_MS4.Install(wifiStaNodes.Get (2));
  MS4_client2.Start (Seconds (1.0));
  MS4_client2.Stop (Seconds (simulationTime + 1));
  ApplicationContainer MS4_client3 = echoClient_MS4.Install(wifiStaNodes.Get (3));
  MS4_client3.Start (Seconds (1.0));
  MS4_client3.Stop (Seconds (simulationTime + 1));


  // Saturated UDP traffic from stations to AP
  // ApplicationContainer clientApp1 = myClient1.Install (wifiStaNodes);
  // clientApp1.Start (Seconds (1.0));
  // clientApp1.Stop (Seconds (simulationTime + 1));

  phy.EnablePcap ("SimpleHtHiddenStations_Ap", apDevice.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta1", staDevices.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta2", staDevices.Get (1));

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();
  Simulator::Destroy ();

  uint32_t totalPacketsThrough = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
  double throughput = totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);
  std::cout << "Throughput: " << throughput << " Mbit/s" << '\n';

  return 0;
}
