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

void RxTrace (std::string context, Ptr<const Packet> pkt, const Address& a, const Address& b){
  std::cout << context << std::endl;
  std::cout << "\tRxTrace: size = " << pkt->GetSize()
    << " From Address: " << InetSocketAddress::ConvertFrom(a).GetIpv4() 
    << " LocalAddress: " << InetSocketAddress::ConvertFrom(b).GetIpv4() << std::endl;
}

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
  uint32_t nPackets = 0; // set to infinity

  CommandLine cmd;
  cmd.AddValue ("nMpdus", "Number of aggregated MPDUs", nMpdus);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("enableRts", "Enable RTS/CTS", enableRts); // 1: RTS/CTS enabled; 0: RTS/CTS disabled
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("nPackets", "Number of packets to echo", nPackets);
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
  wifiStaNodes.Create (4); // increase ms nodes to four
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

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

  // Create MS and AP locations to project specifications
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

  ApplicationContainer serverApp = myServer.Install (wifiApNode.Get(0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  ApplicationContainer serverApp2 = myServer.Install (wifiApNode.Get(0));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (Seconds (simulationTime + 1));

  ApplicationContainer serverApp3 = myServer.Install (wifiApNode).Get(0);
  serverApp3.Start (Seconds (1.0));
  serverApp3.Stop (Seconds (simulationTime + 1));

  ApplicationContainer serverApp4 = myServer.Install (wifiApNode.Get(0));
  serverApp4.Start (Seconds (1.0));
  serverApp4.Stop (Seconds (simulationTime + 1));

  UdpEchoClientHelper myClient (ApInterface.GetAddress (0), 9);
  myClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  myClient.SetAttribute ("Interval", TimeValue (Time ("0.1"))); //packets/s
  myClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  UdpEchoClientHelper myClient2 (ApInterface.GetAddress (0), 10);
  myClient2.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  myClient2.SetAttribute ("Interval", TimeValue (Time ("0.1"))); //packets/s
  myClient2.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  UdpEchoClientHelper myClient3 (ApInterface.GetAddress (0), 11);
  myClient3.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  myClient3.SetAttribute ("Interval", TimeValue (Time ("0.1"))); //packets/s
  myClient3.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  UdpEchoClientHelper myClient4 (ApInterface.GetAddress (0), 12);
  myClient4.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  myClient4.SetAttribute ("Interval", TimeValue (Time ("0.1"))); //packets/s
  myClient4.SetAttribute ("PacketSize", UintegerValue (payloadSize));


  // Saturated UDP traffic from stations to AP
  ApplicationContainer clientApp1 = myClient.Install (wifiStaNodes.Get(0));
  clientApp1.Start (Seconds (1.0));
  clientApp1.Stop (Seconds (simulationTime + 1));

  ApplicationContainer clientApp2 = myClient2.Install (wifiStaNodes.Get(1));
  clientApp2.Start (Seconds (2.0));
  clientApp2.Stop (Seconds (simulationTime + 1));

  ApplicationContainer clientApp3 = myClient3.Install (wifiStaNodes.Get(2));
  clientApp3.Start (Seconds (3.0));
  clientApp3.Stop (Seconds (simulationTime + 1));

  ApplicationContainer clientApp4 = myClient4.Install (wifiStaNodes.Get(3));
  clientApp4.Start (Seconds (4.0));
  clientApp4.Stop (Seconds (simulationTime + 1));

  Config::Set("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/MaxPackets", UintegerValue(3));

  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/RxWithAddresses", MakeCallback(&RxTrace)); // 
  // Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/TxWithAddresses", MakeCallback(&TxTrace));

  phy.EnablePcap ("SimpleHtHiddenStations_Ap", apDevice.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta1", staDevices.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta2", staDevices.Get (1));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta3", staDevices.Get (2));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta4", staDevices.Get (3));

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();
  Simulator::Destroy ();



  // uint32_t totalPacketsThrough = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
  // uint32_t totalPacketsThrough = 
  // double throughput = totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);
  // std::cout << "ServerApp1 Throughput: " << throughput << " Mbit/s" << '\n';

  // uint32_t totalPacketsThrough2 = DynamicCast<UdpServer> (serverApp2.Get (0))->GetReceived ();
  // double throughput2 = totalPacketsThrough2 * payloadSize * 8 / (simulationTime * 1000000.0);
  // std::cout << "ServerApp2 Throughput: " << throughput2 << " Mbit/s" << '\n';

  // uint32_t totalPacketsThrough3 = DynamicCast<UdpServer> (serverApp3.Get (0))->GetReceived ();
  // double throughput3 = totalPacketsThrough3 * payloadSize * 8 / (simulationTime * 1000000.0);
  // std::cout << "ServerApp3 Throughput: " << throughput3 << " Mbit/s" << '\n';

  // uint32_t totalPacketsThrough4 = DynamicCast<UdpServer> (serverApp4.Get (0))->GetReceived ();
  // double throughput4 = totalPacketsThrough4 * payloadSize * 8 / (simulationTime * 1000000.0);
  // std::cout << "ServerApp4 Throughput: " << throughput4 << " Mbit/s" << '\n';
  

  return 0;
}
