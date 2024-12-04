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

#include "ns3/command-line.h"
#include "ns3/eht-phy.h"
#include "ns3/frame-exchange-manager.h"
#include "ns3/ofdm-phy.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/wifi-utils.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // Assume packets are sent by packet socket client
    int macAndUpperLayerHdrSize = 42;
    std::vector<int> mcss{6};
    std::vector<double> bws{20};
    std::vector<int> sizes{1500};
    bool printLog = false;

    auto sifsTime = MicroSeconds(16);
    auto slotTime = MicroSeconds(9);

    // Define AIFSN for each AC
    std::map<std::string, int> acAifsn = {
        {"AC_VO", 2},
        {"AC_VI", 2},
        {"AC_BE", 3},
        {"AC_BK", 7}
    };

    std::cout << "ac,mcs,bw,payload,data_bps,basic_bps,tau_t_slots,tau_f_slots\n";

    for (const auto& [acName, aifsn] : acAifsn)
    {
        auto aifsTime = sifsTime + MicroSeconds(aifsn * slotTime.GetMicroSeconds());
        for (int i = 0; i < mcss.size(); ++i)
        {
            for (int j = 0; j < bws.size(); ++j)
            {
                for (int k = 0; k < sizes.size(); ++k)
                {
                    auto mcsIndex = mcss[i];
                    auto bandWidth = bws[j];
                    auto payloadSize = sizes[k];
                    std::string dataModeStr = "EhtMcs" + std::to_string(mcsIndex);
                    auto dataMode = WifiMode(dataModeStr);
                    auto basicRate = EhtPhy::GetNonHtReferenceRate(dataMode.GetMcsValue());
                    auto ackMode = OfdmPhy::GetOfdmRate(basicRate);

                    auto dataVector =
                        WifiTxVector(dataMode,
                                     0,
                                     GetPreambleForTransmission(WIFI_MOD_CLASS_EHT, false),
                                     NanoSeconds(800),
                                     1,
                                     1,
                                     0,
                                     bandWidth,
                                     false
                            );
                    auto dataTotalTime =
                        WifiPhy::CalculateTxDuration(payloadSize + macAndUpperLayerHdrSize,
                                                     dataVector,
                                                     WIFI_PHY_BAND_5GHZ);

                    auto ackVector =
                        WifiTxVector(ackMode,
                                     0,
                                     WIFI_PREAMBLE_LONG,
                                     NanoSeconds(800),
                                     1,
                                     1,
                                     0,
                                     20,
                                     false);
                    auto ackTotalTime = WifiPhy::CalculateTxDuration(
                        GetAckSize(),
                        ackVector,
                        WIFI_PHY_BAND_5GHZ);
                    auto taoT = dataTotalTime + sifsTime + ackTotalTime + aifsTime;
                    auto taoF = dataTotalTime + aifsTime;
                    double taoTSlots = taoT.ToDouble(ns3::Time::US) / slotTime.ToDouble(ns3::Time::US);
                    double taoFSlots = taoF.ToDouble(ns3::Time::US) / slotTime.ToDouble(ns3::Time::US);

                    if (printLog)
                    {
                        std::clog << "MODEL PARAMETERS FOR " << bandWidth << " MHz, " << "MCS=" <<
                            mcsIndex << ", AC=" << acName << ":\n";
                        std::clog << "Data mode: " << dataMode.GetUniqueName() << "\n";
                        std::clog << "PHY rate for data frame (bps): "
                            << EhtPhy::GetDataRate(
                                dataMode.GetMcsValue(),
                                bandWidth,
                                NanoSeconds(800),
                                1)
                            << std::endl;
                        std::clog << "Ack mode: " << ackMode.GetUniqueName() << "\n";
                        std::clog << "PHY rate for ACK frame (bps): "
                            << basicRate
                            << std::endl;
                        std::clog << "Data frame:\n";
                        std::clog << "\tPSDU size: " << payloadSize + 42 << std::endl;
                        std::clog << "\ttx duration: " << dataTotalTime << std::endl;
                        std::clog << "ACK:\n";
                        std::clog << "\tPSDU size: " << GetAckSize() << std::endl;
                        std::clog << "\ttx duration: " << ackTotalTime << std::endl;
                        std::clog << "SIFS: " << sifsTime << std::endl;
                        std::clog << "AIFS (" << acName << "): " << aifsTime << std::endl;
                        std::clog << "Successful tx holding time: " << taoT << std::endl;
                        std::clog << "Successful tx holding time (in slots): " << taoTSlots <<
                            std::endl;
                        std::clog << "Failed (collided) tx holding time: " << taoF << std::endl;
                        std::clog << "Failed (collided) tx holding time (in slots): " << taoFSlots <<
                            "\n\n";
                    }

                    std::cout << acName << "," << mcsIndex << "," << bandWidth << "," << payloadSize << "," <<
                        EhtPhy::GetDataRate(
                            dataMode.GetMcsValue(),
                            bandWidth,
                            NanoSeconds(800),
                            1)
                        << "," << basicRate << "," << taoTSlots << "," << taoFSlots << "\n";
                }
            }
        }
    }
}