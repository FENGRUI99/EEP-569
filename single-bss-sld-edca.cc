/*
 * Copyright (c) 2024
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

#include "ns3/attribute-container.h"
#include "ns3/bernoulli_packet_socket_client.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/constant-rate-wifi-manager.h"
#include "ns3/eht-configuration.h"
#include "ns3/eht-phy.h"
#include "ns3/frame-exchange-manager.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/qos-utils.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-tx-stats-helper.h"
#include "ns3/wifi-utils.h"
#include "ns3/yans-wifi-helper.h"

#include <array>
#include <cmath>

#define PI 3.1415926535

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("single-bss-sld");

enum TrafficTypeEnum
{
    TRAFFIC_DETERMINISTIC,
    TRAFFIC_BERNOULLI,
    TRAFFIC_INVALID
};

// Per SLD traffic config
struct TrafficConfig
{
    WifiDirection m_dir;
    TrafficTypeEnum m_type;
    AcIndex m_linkAc; 
    double m_lambda;
    double m_determIntervalNs;
};

using TrafficConfigMap = std::map<uint32_t /* Node ID */, TrafficConfig>;

Time slotTime;

Ptr<PacketSocketClient>
GetDeterministicClient(const PacketSocketAddress& sockAddr,
                       const std::size_t pktSize,
                       const Time& interval,
                       const Time& start,
                       const AcIndex linkAc)
{
    NS_ASSERT(linkAc != AC_UNDEF);
    auto tid = wifiAcList.at(linkAc).GetLowTid(); // use the low TID

    auto client = CreateObject<PacketSocketClient>();
    client->SetAttribute("PacketSize", UintegerValue(pktSize));
    client->SetAttribute("MaxPackets", UintegerValue(0));
    client->SetAttribute("Interval", TimeValue(interval));
    client->SetAttribute("Priority", UintegerValue(tid));
    client->SetRemote(sockAddr);
    client->SetStartTime(start);
    return client;
}

Ptr<BernoulliPacketSocketClient>
GetBernoulliClient(const PacketSocketAddress& sockAddr,
                   const std::size_t pktSize,
                   const double prob,
                   const Time& start,
                   const AcIndex linkAc)
{
    NS_ASSERT(linkAc != AC_UNDEF);
    auto tid = wifiAcList.at(linkAc).GetLowTid();

    auto client = CreateObject<BernoulliPacketSocketClient>();
    client->SetAttribute("PacketSize", UintegerValue(pktSize));
    client->SetAttribute("MaxPackets", UintegerValue(0));
    client->SetAttribute("TimeSlot", TimeValue(slotTime));
    client->SetAttribute("BernoulliPr", DoubleValue(prob));
    client->SetAttribute("Priority", UintegerValue(tid));
    client->SetRemote(sockAddr);
    client->SetStartTime(start);
    return client;
}

int
main(int argc, char* argv[])
{
    std::ofstream g_fileSummary;
    g_fileSummary.open("wifi-edca.dat", std::ofstream::app);
    bool printTxStatsSingleLine{true};

    uint32_t rngRun{6};
    double simulationTime{20}; // seconds
    uint32_t payloadSize = 1500;
    double bssRadius{0.001};
    bool unlimitedAmpdu{false};
    uint8_t maxMpdusInAmpdu = 0;
    bool useRts{false}; //?
    int gi = 800;
    double apTxPower = 20;
    double staTxPower = 20;

    // link parameters
    double frequency{5};
    int mcs{6};
    int channelWidth = 20;

    // SLD STAs parameters
    std::size_t nSld{5};
    std::size_t nBE{2};
    std::size_t nBK{1};
    std::size_t nVI{1};
    std::size_t nVO{1};

    double perSldLambda{0.00001};
    uint8_t sldAcInt_BE{AC_BE}; // Access Category 需要增加类别？
    uint8_t sldAcInt_BK{AC_BK};
    uint8_t sldAcInt_VI{AC_VI};
    uint8_t sldAcInt_VO{AC_VO};

    // EDCA configuration for CWmins, CWmaxs
    /**
     * 不确定对不对
     */
    uint64_t acBECwmin{16};
    uint8_t acBECwStage{6};
    uint64_t acBKCwmin{16};
    uint8_t acBKCwStage{6};
    uint64_t acVICwmin{8};
    uint8_t acVICwStage{4};
    uint64_t acVOCwmin{4};
    uint8_t acVOCwStage{2};

    CommandLine cmd(__FILE__);
    cmd.AddValue("rngRun", "Seed for simulation", rngRun);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Application payload size in Bytes", payloadSize);
    cmd.AddValue("mcs", "MCS", mcs);
    cmd.AddValue("channelWidth", "Bandwidth", channelWidth);
    cmd.AddValue("nSld", "Number of SLD STAs on link 1", nSld);
    cmd.AddValue("perSldLambda",
                 "Per node Bernoulli arrival rate of SLD STAs",
                 perSldLambda);
    // cmd.AddValue("sldAcInt", "AC of SLD", sldAcInt);
    cmd.AddValue("nBE", "initial number of BE sta", nBE);
    cmd.AddValue("nBK", "initial number of BE sta", nBK);
    cmd.AddValue("nVI", "initial number of BE sta", nVI);
    cmd.AddValue("nVO", "initial number of BE sta", nVO);
    cmd.AddValue("acBECwmin", "Initial CW for AC_BE", acBECwmin);
    cmd.AddValue("acBECwStage", "Cutoff Stage for AC_BE", acBECwStage);
    cmd.AddValue("acBKCwmin", "Initial CW for AC_BK", acBKCwmin);
    cmd.AddValue("acBKCwStage", "Cutoff Stage for AC_BK", acBKCwStage);
    cmd.AddValue("acVICwmin", "Initial CW for AC_VI", acVICwmin);
    cmd.AddValue("acVICwStage", "Cutoff Stage for AC_VI", acVICwStage);
    cmd.AddValue("acVOCwmin", "Initial CW for AC_VO", acVOCwmin);
    cmd.AddValue("acVOCwStage", "Cutoff Stage for AC_VO", acVOCwStage);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(rngRun);
    RngSeedManager::SetRun(rngRun);
    uint32_t randomStream = rngRun;
    // auto sldAc = static_cast<AcIndex>(sldAcInt);
    auto BEAc = static_cast<AcIndex>(sldAcInt_BE);
    auto BKAc = static_cast<AcIndex>(sldAcInt_BK);
    auto VIAc = static_cast<AcIndex>(sldAcInt_VI);
    auto VOAc = static_cast<AcIndex>(sldAcInt_VO);
    

    uint64_t acBECwmax = acBECwmin * pow(2, acBECwStage);
    acBECwmax -= 1;
    acBECwmin -= 1;
    uint64_t acBKCwmax = acBKCwmin * pow(2, acBKCwStage);
    acBKCwmax -= 1;
    acBKCwmin -= 1;
    uint64_t acVICwmax = acVICwmin * pow(2, acVICwStage);
    acVICwmax -= 1;
    acVICwmin -= 1;
    uint64_t acVOCwmax = acVOCwmin * pow(2, acVOCwStage);
    acVOCwmax -= 1;
    acVOCwmin -= 1;

    if (nSld != (nBE + nBK + nVI + nVO))
    {
        std::cout << "wrong nSld parameter\n";
        return 0;
    }

    // node节点索引与AC类型映射
    std::vector<AcIndex> acList;

    for (uint32_t i = 0; i < nBK; ++i) acList.push_back(BKAc);
    for (uint32_t i = 0; i < nBE; ++i) acList.push_back(BEAc);
    for (uint32_t i = 0; i < nVI; ++i) acList.push_back(VIAc);
    for (uint32_t i = 0; i < nVO; ++i) acList.push_back(VOAc);

    if (useRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        Config::SetDefault("ns3::WifiDefaultProtectionManager::EnableMuRts", BooleanValue(true));
    }

    // Disable fragmentation
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold",
                       UintegerValue(payloadSize + 100));

    // Make retransmissions persistent
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSlrc",
                       UintegerValue(std::numeric_limits<uint32_t>::max()));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSsrc",
                       UintegerValue(std::numeric_limits<uint32_t>::max()));

    // Set infinitely long queue
    Config::SetDefault(
        "ns3::WifiMacQueue::MaxSize",
        QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, std::numeric_limits<uint32_t>::max())));

    // Don't drop MPDUs due to long stay in queue
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(Seconds(2 * simulationTime)));

    NodeContainer apNodeCon;
    NodeContainer staNodeCon;
    apNodeCon.Create(1);
    staNodeCon.Create(nSld);

    NetDeviceContainer apDevCon;
    NetDeviceContainer staDevCon;

    WifiHelper wifiHelp;
    wifiHelp.SetStandard(WIFI_STANDARD_80211be);

    SpectrumWifiPhyHelper phyHelp{};
    phyHelp.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    Ptr<MultiModelSpectrumChannel> phySpectrumChannel = CreateObject<
        MultiModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> lossModel =
        CreateObject<LogDistancePropagationLossModel>();
    phySpectrumChannel->AddPropagationLossModel(lossModel);

    std::string dataModeStr = "EhtMcs" + std::to_string(mcs);
    wifiHelp.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode",
                                     StringValue(dataModeStr));
    std::string channelStr = "{0, " + std::to_string(channelWidth) + ", ";
    if (frequency == 2.4)
    {
        channelStr += "BAND_2_4GHZ, 0}";
        phyHelp.AddChannel(phySpectrumChannel, WIFI_SPECTRUM_2_4_GHZ);
    }
    else if (frequency == 5)
    {
        channelStr += "BAND_5GHZ, 0}";
        phyHelp.AddChannel(phySpectrumChannel, WIFI_SPECTRUM_5_GHZ);
    }
    else if (frequency == 6)
    {
        channelStr += "BAND_6GHZ, 0}";
        phyHelp.AddChannel(phySpectrumChannel, WIFI_SPECTRUM_6_GHZ);
    }
    else
    {
        std::cout << "Unsupported frequency band!\n";
        return 0;
    }
    phyHelp.Set("ChannelSettings", StringValue(channelStr));

    WifiMacHelper macHelp;
    Ssid bssSsid = Ssid("BSS-SLD-ONLY");

    // Set up MLD STAs
    macHelp.SetType("ns3::StaWifiMac",
                    "MaxMissedBeacons",
                    UintegerValue(std::numeric_limits<uint32_t>::max()),
                    "Ssid",
                    SsidValue(bssSsid));
    phyHelp.Set("TxPowerStart", DoubleValue(staTxPower));
    phyHelp.Set("TxPowerEnd", DoubleValue(staTxPower));
    staDevCon = wifiHelp.Install(phyHelp, macHelp, staNodeCon);

    uint64_t beaconInterval = std::min<uint64_t>(
        (ceil((simulationTime * 1000000) / 1024) * 1024),
        (65535 * 1024)); // beacon interval needs to be a multiple of time units (1024 us)

    // Set up AP
    macHelp.SetType("ns3::ApWifiMac",
                    "BeaconInterval",
                    TimeValue(MicroSeconds(beaconInterval)),
                    "EnableBeaconJitter",
                    BooleanValue(false),
                    "Ssid",
                    SsidValue(bssSsid));
    phyHelp.Set("TxPowerStart", DoubleValue(apTxPower));
    phyHelp.Set("TxPowerEnd", DoubleValue(apTxPower));
    apDevCon = wifiHelp.Install(phyHelp, macHelp, apNodeCon);

    NetDeviceContainer allNetDevices;
    allNetDevices.Add(apDevCon);
    allNetDevices.Add(staDevCon);

    WifiHelper::AssignStreams(allNetDevices, randomStream);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval",
                TimeValue(NanoSeconds(gi)));

    if (!unlimitedAmpdu)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BK_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_MaxAmpduSize",
                    UintegerValue(maxMpdusInAmpdu * (payloadSize + 50)));
    }

    // Set cwmins and cwmaxs for all Access Categories on both AP and STAs
    // (including AP because STAs sync with AP via association, probe, and beacon)
    std::string prefixStr = "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/";
    std::list<uint64_t> acBeCwmins = {acBECwmin};
    std::list<uint64_t> acBeCwmaxs = {acBECwmax};
    std::list<uint64_t> acBkCwmins = {acBKCwmin};
    std::list<uint64_t> acBkCwmaxs = {acBKCwmax};
    std::list<uint64_t> acViCwmins = {acVICwmin};
    std::list<uint64_t> acViCwmaxs = {acVICwmax};
    std::list<uint64_t> acVoCwmins = {acVOCwmin};
    std::list<uint64_t> acVoCwmaxs = {acVOCwmax};
    Config::Set(prefixStr + "BE_Txop/MinCws", AttributeContainerValue<UintegerValue>(acBeCwmins));
    Config::Set(prefixStr + "BE_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acBeCwmaxs));
    Config::Set(prefixStr + "BK_Txop/MinCws", AttributeContainerValue<UintegerValue>(acBkCwmins));
    Config::Set(prefixStr + "BK_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acBkCwmaxs));
    Config::Set(prefixStr + "VI_Txop/MinCws", AttributeContainerValue<UintegerValue>(acViCwmins));
    Config::Set(prefixStr + "VI_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acViCwmaxs));
    Config::Set(prefixStr + "VO_Txop/MinCws", AttributeContainerValue<UintegerValue>(acVoCwmins));
    Config::Set(prefixStr + "VO_Txop/MaxCws", AttributeContainerValue<UintegerValue>(acVoCwmaxs));

    // Set all aifsn to 2 (so that all AIFS equal to legacy DIFS)
    std::list<uint64_t> aifsns_BK = {7};
    std::list<uint64_t> aifsns_BE = {3};
    std::list<uint64_t> aifsns = {2};
    Config::Set(prefixStr + "BE_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsns_BE));
    Config::Set(prefixStr + "BK_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsns_BK));
    Config::Set(prefixStr + "VI_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsns));
    Config::Set(prefixStr + "VO_Txop/Aifsns", AttributeContainerValue<UintegerValue>(aifsns));

    // Set all TXOP limit to 0
    std::list<Time> txopLimits_BK = {MicroSeconds(0)};
    std::list<Time> txopLimits_BE = {MicroSeconds(0)};
    std::list<Time> txopLimits_VI = {MicroSeconds(1536)};
    std::list<Time> txopLimits_VO = {MicroSeconds(320)};

    Config::Set(prefixStr + "BE_Txop/TxopLimits", AttributeContainerValue<TimeValue>(txopLimits_BE));
    Config::Set(prefixStr + "BK_Txop/TxopLimits", AttributeContainerValue<TimeValue>(txopLimits_BK));
    Config::Set(prefixStr + "VI_Txop/TxopLimits", AttributeContainerValue<TimeValue>(txopLimits_VI));
    Config::Set(prefixStr + "VO_Txop/TxopLimits", AttributeContainerValue<TimeValue>(txopLimits_VO));

    auto staWifiManager =
        DynamicCast<ConstantRateWifiManager>(DynamicCast<WifiNetDevice>(staDevCon.Get(0))
            ->GetRemoteStationManager());
    slotTime = staWifiManager->GetPhy()->GetSlot();
    auto sifsTime = staWifiManager->GetPhy()->GetSifs();
    auto difsTime = sifsTime + 2 * slotTime;

    // mobility.
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    double angle = (static_cast<double>(360) / nSld);
    positionAlloc->Add(Vector(1.0, 1.0, 0.0));
    for (uint32_t i = 0; i < nSld; ++i)
    {
        positionAlloc->Add(Vector(1.0 + (bssRadius * cos((i * angle * PI) / 180)),
                                  1.0 + (bssRadius * sin((i * angle * PI) / 180)),
                                  0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    NodeContainer allNodeCon(apNodeCon, staNodeCon);
    mobility.Install(allNodeCon);

    /* Setting applications */
    // random start time
    Ptr<UniformRandomVariable> startTime = CreateObject<UniformRandomVariable>();
    startTime->SetAttribute("Stream", IntegerValue(randomStream));
    startTime->SetAttribute("Min", DoubleValue(0.0));
    startTime->SetAttribute("Max", DoubleValue(1.0));

    // setup PacketSocketServer for every node
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodeCon);
    for (auto nodeIt = allNodeCon.Begin(); nodeIt != allNodeCon.End(); ++nodeIt)
    {
        PacketSocketAddress srvAddr;
        auto device = DynamicCast<WifiNetDevice>((*nodeIt)->GetDevice(0));
        srvAddr.SetSingleDevice(device->GetIfIndex());
        srvAddr.SetProtocol(1);
        auto psServer = CreateObject<PacketSocketServer>();
        psServer->SetLocal(srvAddr);
        (*nodeIt)->AddApplication(psServer);
        psServer->SetStartTime(Seconds(0)); // all servers start at 0 s
    }
    
    // set the configuration pairs for applications (UL, Bernoulli arrival)
    TrafficConfigMap trafficConfigMap; //流量配置表
    double sldDetermIntervalNs = slotTime.GetNanoSeconds() / perSldLambda; //计算确定性时间间隔，每次流量产生之间的固定时间间隔。lambda:平均到达率，表示单位时间内的流量生成次数

    for (uint32_t i = 0; i < nSld; ++i) //为每一个STA配置
    {
        AcIndex acType = acList[i];
        trafficConfigMap[i] = {WifiDirection::UPLINK, TRAFFIC_BERNOULLI, acType, perSldLambda, sldDetermIntervalNs};
    } 

    // for (uint32_t i = 0; i < nSld; ++i) //为每一个STA配置
    // {
    //     trafficConfigMap[i] = {WifiDirection::UPLINK, TRAFFIC_BERNOULLI, sldAc, //uplink上行流量，流量生成符合 Bernoulli 分布，sldAc访问类别
    //                            perSldLambda, sldDetermIntervalNs}; //每秒生产流量数，用于确定流量生成的时间间隔
    // } //对于不同的流量类型，需要设置不同的到达率perSldLambda?

    // next, setup clients according to the config 根据上一步生成的流量配置表 为每个sta设置client
    for (uint32_t i = 0; i < nSld; ++i)
    {
        auto mapIt = trafficConfigMap.find(i);
        Ptr<Node> clientNode = (mapIt->second.m_dir == WifiDirection::UPLINK) //客户端是STA (uplink)
                                   ? staNodeCon.Get(i)
                                   : apNodeCon.Get(0);
        Ptr<WifiNetDevice> clientDevice = DynamicCast<WifiNetDevice>(clientNode->GetDevice(0)); //获取客户端和服务器节点的 Wi-Fi 网络设备
        Ptr<Node> serverNode = (mapIt->second.m_dir == WifiDirection::UPLINK) //服务器是AP (uplink)
                                   ? apNodeCon.Get(0)
                                   : staNodeCon.Get(i);
        Ptr<WifiNetDevice> serverDevice = DynamicCast<WifiNetDevice>(serverNode->GetDevice(0));

        switch (mapIt->second.m_type) //设置流量类型
        {
        case TRAFFIC_DETERMINISTIC: { //Deterministic 流量
            PacketSocketAddress sockAddr;
            sockAddr.SetSingleDevice(clientDevice->GetIfIndex());
            sockAddr.SetPhysicalAddress(serverDevice->GetAddress());
            sockAddr.SetProtocol(1);
            clientNode->AddApplication(GetDeterministicClient(sockAddr, //GetDeterministicClient：创建一个确定性流量客户端应用
                                                              payloadSize,
                                                              NanoSeconds(
                                                                  mapIt->second.m_determIntervalNs),
                                                              Seconds(startTime->GetValue()),
                                                              mapIt->second.m_linkAc)); //m_linkAc 流量的接入类别，用于区分 EDCA 优先级
            break;
        }
        case TRAFFIC_BERNOULLI: { //Bernoulli 流量
            PacketSocketAddress sockAddr;
            sockAddr.SetSingleDevice(clientDevice->GetIfIndex());
            sockAddr.SetPhysicalAddress(serverDevice->GetAddress());
            sockAddr.SetProtocol(1);
            clientNode->AddApplication(GetBernoulliClient(sockAddr,
                                                          payloadSize,
                                                          mapIt->second.m_lambda,
                                                          Seconds(startTime->GetValue()),
                                                          mapIt->second.m_linkAc)); //
            break;
        }
        default: {
            std::cerr << "traffic type " << mapIt->second.m_type << " not supported\n";
            break;
        }
        }
    }

    // TX stats
    WifiTxStatsHelper wifiTxStats; //用了 WifiTxStatsHelper 来跟踪和收集关于无线网络设备传输的数据。
    wifiTxStats.Enable(allNetDevices); //启用了 wifiTxStats 对象来收集与 allNetDevices（所有网络设备）相关的传输统计数据
    wifiTxStats.Start(Seconds(5)); //设定了统计的开始时间，即从仿真开始后的第 5 秒开始收集传输统计数据
    wifiTxStats.Stop(Seconds(5 + simulationTime)); //设定了统计的结束时间，即仿真结束时

    // phyHelp.EnablePcap("single-bss-sld", allNetDevices);
    // AsciiTraceHelper asciiTrace;
    // phyHelp.EnableAsciiAll(asciiTrace.CreateFileStream("single-bss-sld.tr"));

    Simulator::Stop(Seconds(5 + simulationTime)); //设置仿真结束的时间。在仿真运行到 5 + simulationTime 秒时，仿真会停止
    Simulator::Run();

    auto finalResults = wifiTxStats.GetStatistics();
    auto successInfo = wifiTxStats.GetSuccessInfoMap();


    // // 打印 finalResults 的键值数量
    // size_t finalResultsLength = finalResults.m_numSuccessPerNodeLink.size(); // 根据实际字段判断
    // std::cout << "Length of finalResults: " << finalResultsLength << std::endl;

    // // 打印 successInfo 的外层和内层长度
    // size_t successInfoNodeCount = successInfo.size();
    // size_t successInfoTotalLinks = 0;
    // for (const auto& [nodeId, linkMap] : successInfo) {
    //     successInfoTotalLinks += linkMap.size();
    // }
    // std::cout << "Length of successInfo (Node IDs): " << successInfoNodeCount << std::endl;
    // std::cout << "Length of successInfo (Total Links): " << successInfoTotalLinks << std::endl;

    // total and mean delay calculation per node and link
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double>>>
        enqueueTimeMap;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double>>>
        dequeueTimeMap;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double>>>
        holTimeMap;
    for (const auto& nodeMap : successInfo)
    {
        for (const auto& linkMap : nodeMap.second)
        {
            for (const auto& record : linkMap.second)
            {
                enqueueTimeMap[nodeMap.first][linkMap.first].emplace_back(record.m_enqueueMs);
                dequeueTimeMap[nodeMap.first][linkMap.first].emplace_back(record.m_dequeueMs);
            }
            for (uint32_t i = 0; i < enqueueTimeMap[nodeMap.first][linkMap.first].size(); ++i)
            {
                if (i == 0)
                {
                    // This value is false (some data packet may be already in queue
                    // because our stats did not start at 0 second), and will be removed later
                    holTimeMap[nodeMap.first][linkMap.first].emplace_back(
                        enqueueTimeMap[nodeMap.first][linkMap.first][i]);
                }
                else
                {
                    holTimeMap[nodeMap.first][linkMap.first].emplace_back(
                        std::max(enqueueTimeMap[nodeMap.first][linkMap.first][i],
                                 dequeueTimeMap[nodeMap.first][linkMap.first][i - 1]));
                }
            }
            // remove the first element
            enqueueTimeMap[nodeMap.first][linkMap.first].erase(
                enqueueTimeMap[nodeMap.first][linkMap.first].begin());
            dequeueTimeMap[nodeMap.first][linkMap.first].erase(
                dequeueTimeMap[nodeMap.first][linkMap.first].begin());
            holTimeMap[nodeMap.first][linkMap.first].erase(
                holTimeMap[nodeMap.first][linkMap.first].begin());
        }
    }
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double>>
        totalQueuingDelayPerNodeLink;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double>>
        meanQueuingDelayPerNodeLink;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double>>
        totalAccessDelayPerNodeLink;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, double>>
        meanAccessDelayPerNodeLink;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double>>>
        accessDelaysPerNodeLink;
    std::map<uint32_t /* Node ID */, std::map<uint8_t /* Link ID */, std::vector<double>>>
        e2eDelaysPerNodeLink;
    for (const auto& nodeMap : successInfo)
    {
        for (const auto& linkMap : nodeMap.second)
        {
            for (uint32_t i = 0; i < enqueueTimeMap[nodeMap.first][linkMap.first].size(); ++i)
            {
                totalQueuingDelayPerNodeLink[nodeMap.first][linkMap.first] += holTimeMap[nodeMap.
                    first][linkMap.first][i] - enqueueTimeMap[nodeMap.first][
                    linkMap.first][i];
                totalAccessDelayPerNodeLink[nodeMap.first][linkMap.first] += dequeueTimeMap[nodeMap.
                    first][linkMap.first][i] - holTimeMap[nodeMap.first][linkMap.
                    first][i];
                accessDelaysPerNodeLink[nodeMap.first][linkMap.first].emplace_back(
                    dequeueTimeMap[nodeMap.first][linkMap.first][i]
                    - holTimeMap[nodeMap.first][linkMap.first][i]);
                e2eDelaysPerNodeLink[nodeMap.first][linkMap.first].emplace_back(
                    dequeueTimeMap[nodeMap.first][linkMap.first][i]
                    - enqueueTimeMap[nodeMap.first][linkMap.first][i]);
            }
            meanQueuingDelayPerNodeLink[nodeMap.first][linkMap.first] =
                totalQueuingDelayPerNodeLink[nodeMap.first][linkMap.first] / (finalResults.
                    m_numSuccessPerNodeLink[nodeMap.first][linkMap.first] - 1);
            meanAccessDelayPerNodeLink[nodeMap.first][linkMap.first] =
                totalAccessDelayPerNodeLink[nodeMap.first][linkMap.first] / (finalResults.
                    m_numSuccessPerNodeLink[nodeMap.first][linkMap.first] - 1);
        }
    }

    // successful tx prob of SLD STAs
    std::map<AcIndex, uint64_t> successMap;
    std::map<AcIndex, uint64_t> attemptMap;
    std::map<AcIndex, double> sldSuccPrMap; // 用于存储每种类型的成功概率

   // 遍历 SLD STAs 进行统计
    for (uint32_t i = 1; i < 1 + nSld; ++i) // 假设 SLD STAs 索引从 1 开始
    {
        AcIndex type = acList[i - 1]; // 从 acList 获取类型，注意索引从 0 开始

        // 获取与当前 STA 对应的成功传输信息
        const auto& linkMap = successInfo[i];
        for (const auto& records : linkMap)
        {
            for (const auto& pkt : records.second)
            {
                // 更新总的统计值
                successMap[type] += 1;                       // 成功的包数量
                attemptMap[type] += 1 + pkt.m_failures;      // 尝试的总次数 = 成功 + 失败次数
            }
        }
    }

    for (const auto& entry : successMap)
    {
        AcIndex type = entry.first;
        uint64_t successCount = entry.second;
        uint64_t attemptCount = attemptMap[type];

        // 计算成功概率
        double sldSuccPr = (attemptCount > 0) ? static_cast<double>(successCount) / attemptCount : 0.0;
        sldSuccPrMap[type] = sldSuccPr;
    }
    

    // 输出统计结果
    // for (const auto& entry : successMap)
    // {
    //     AcIndex type = entry.first;
    //     uint64_t successCount = entry.second;
    //     uint64_t attemptCount = attemptMap[type];

    //     std::cout << "Type: " << type
    //             << ", Success: " << successCount
    //             << ", Attempts: " << attemptCount
    //             << ", Success Rate: " 
    //             << (attemptCount > 0 ? (double)successCount / attemptCount : 0) 
    //             << std::endl;
    // }

    // 计算所有类型的吞吐量 (sldThpt)
    std::map<AcIndex, double> sldThptMap; // 用于存储每种类型的吞吐量

    for (const auto& entry : successMap)
    {
        AcIndex type = entry.first;
        uint64_t successCount = entry.second;

        // 吞吐量计算公式
        double sldThpt = static_cast<long double>(successCount) * payloadSize * 8 /
                        simulationTime / 1000000; // 转为 Mbps
        sldThptMap[type] = sldThpt;
    }

    std::map<AcIndex, long double> queDelayTotalMap;
    std::map<AcIndex, long double> accDelayTotalMap;
    std::map<AcIndex, double> meanQueDelayMap;
    std::map<AcIndex, double> meanAccDelayMap;
    std::map<AcIndex, double> meanE2eDelayMap;

    // 遍历 SLD STAs 按类型统计队列延迟和接入延迟
    for (uint32_t i = 1; i < 1 + nSld; ++i) // 假设 SLD STAs 索引从 1 开始
    {
        AcIndex type = acList[i - 1]; // 从 acList 获取类型，注意索引从 0 开始

        // 队列延迟统计
        const auto& queLinkMap = totalQueuingDelayPerNodeLink[i];
        for (const auto& item : queLinkMap)
        {
            queDelayTotalMap[type] += item.second;
        }

        // 接入延迟统计
        const auto& accLinkMap = totalAccessDelayPerNodeLink[i];
        for (const auto& item : accLinkMap)
        {
            accDelayTotalMap[type] += item.second;
        }
    }

    for (const auto& entry : successMap)
    {
        AcIndex type = entry.first;
        uint64_t successCount = entry.second;
        if (successCount > 0)
        {
            double meanQueDelay = queDelayTotalMap[type] / successCount;
            double meanAccDelay = accDelayTotalMap[type] / successCount;

            meanQueDelayMap[type] = meanQueDelay;
            meanAccDelayMap[type] = meanAccDelay;
            meanE2eDelayMap[type] = meanQueDelay + meanAccDelay;
        }
        else
        {
            meanQueDelayMap[type] = 0.0;
            meanAccDelayMap[type] = 0.0;
            meanE2eDelayMap[type] = 0.0;
        }
    }

    double sldSuccPr_BE = sldSuccPrMap[AC_BE];
    double sldSuccPr_BK = sldSuccPrMap[AC_BK];
    double sldSuccPr_VI = sldSuccPrMap[AC_VI];
    double sldSuccPr_VO = sldSuccPrMap[AC_VO];
    double sldSuccPr_total = (successMap[AC_BE] + successMap[AC_BK] + successMap[AC_VI] + successMap[AC_VO]) /
                            static_cast<double>(attemptMap[AC_BE] + attemptMap[AC_BK] + attemptMap[AC_VI] + attemptMap[AC_VO]);

    double sldThpt_BE = sldThptMap[AC_BE];
    double sldThpt_BK = sldThptMap[AC_BK];
    double sldThpt_VI = sldThptMap[AC_VI];
    double sldThpt_VO = sldThptMap[AC_VO];
    double sldThpt_total = sldThpt_BE + sldThpt_BK + sldThpt_VI + sldThpt_VO;

    double sldMeanQueDelay_BE = meanQueDelayMap[AC_BE];
    double sldMeanQueDelay_BK = meanQueDelayMap[AC_BK];
    double sldMeanQueDelay_VI = meanQueDelayMap[AC_VI];
    double sldMeanQueDelay_VO = meanQueDelayMap[AC_VO];
    double sldMeanQueDelay_total = (queDelayTotalMap[AC_BE] + queDelayTotalMap[AC_BK] +
                                    queDelayTotalMap[AC_VI] + queDelayTotalMap[AC_VO]) /
                                static_cast<double>(successMap[AC_BE] + successMap[AC_BK] +
                                                    successMap[AC_VI] + successMap[AC_VO]);

    double sldMeanAccDelay_BE = meanAccDelayMap[AC_BE];
    double sldMeanAccDelay_BK = meanAccDelayMap[AC_BK];
    double sldMeanAccDelay_VI = meanAccDelayMap[AC_VI];
    double sldMeanAccDelay_VO = meanAccDelayMap[AC_VO];
    double sldMeanAccDelay_total = (accDelayTotalMap[AC_BE] + accDelayTotalMap[AC_BK] +
                                    accDelayTotalMap[AC_VI] + accDelayTotalMap[AC_VO]) /
                                static_cast<double>(successMap[AC_BE] + successMap[AC_BK] +
                                                    successMap[AC_VI] + successMap[AC_VO]);

    double sldMeanE2eDelay_BE = meanE2eDelayMap[AC_BE];
    double sldMeanE2eDelay_BK = meanE2eDelayMap[AC_BK];
    double sldMeanE2eDelay_VI = meanE2eDelayMap[AC_VI];
    double sldMeanE2eDelay_VO = meanE2eDelayMap[AC_VO];
    double sldMeanE2eDelay_total = sldMeanQueDelay_total + sldMeanAccDelay_total;

    if (printTxStatsSingleLine)
    {
        g_fileSummary 
            << sldSuccPr_BE << ","
            << sldSuccPr_BK << ","
            << sldSuccPr_VI << ","
            << sldSuccPr_VO << ","
            << sldSuccPr_total << ","
            // << sldThpt << ","
            << sldThpt_BE << ","
            << sldThpt_BK << ","
            << sldThpt_VI << ","
            << sldThpt_VO << ","
            << sldThpt_total << ","
            // << sldMeanQueDelay << ","
            << sldMeanQueDelay_BE << ","
            << sldMeanQueDelay_BK << ","
            << sldMeanQueDelay_VI << ","
            << sldMeanQueDelay_VO << ","
            << sldMeanQueDelay_total << ","
            // << sldMeanAccDelay << ","
            << sldMeanAccDelay_BE << ","
            << sldMeanAccDelay_BK << ","
            << sldMeanAccDelay_VI << ","
            << sldMeanAccDelay_VO << ","
            << sldMeanAccDelay_total << ","
            // << sldMeanE2eDelay << ","
            << sldMeanE2eDelay_BE << ","
            << sldMeanE2eDelay_BK << ","
            << sldMeanE2eDelay_VI << ","
            << sldMeanE2eDelay_VO << ","
            << sldMeanE2eDelay_total << ","
            << rngRun << ","
            << simulationTime << ","
            << payloadSize << ","
            << mcs << ","
            << channelWidth << ","
            << nSld << ","
            << perSldLambda << ","
            // << +sldAcInt << ","
            << +sldAcInt_BE << ","
            << +sldAcInt_BK << ","
            << +sldAcInt_VI << ","
            << +sldAcInt_VO << ","
            << acBECwmin << ","
            << +acBECwStage << ","
            << acBKCwmin << ","
            << +acBKCwStage << ","
            << acVICwmin << ","
            << +acVICwStage << ","
            << acVOCwmin << ","
            << +acVOCwStage << "\n";
    }
    g_fileSummary.close();
    Simulator::Destroy();
    return 0;
}