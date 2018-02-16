#include "Grr18MPSI_Tests.h"

#include "Common.h"
#include "cryptoTools/Network/Endpoint.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/Log.h"

#include "libPSI/MPSI/Grr18/Grr18MPsiReceiver.h"
#include "libPSI/MPSI/Grr18/Grr18MPsiSender.h"

#include "libOTe/NChooseOne/Oos/OosNcoOtReceiver.h"
#include "libOTe/NChooseOne/Oos/OosNcoOtSender.h"

using namespace oc;

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                        RR17a PSI                           //////////////////////
////////////////////                  OOS16 encode protocol                     //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

void Grr18_Oos_EmptrySet_Test_Impl()
{
    u64 setSize = 8, psiSecParam = 40;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    std::string name("psi");

    IOService ios(0);
    Endpoint ep0(ios, "localhost", 1212, EpMode::Client, name);
    Endpoint ep1(ios, "localhost", 1212, EpMode::Server, name);

    std::vector<Channel> recvChl{ ep1.addChannel(name, name) };
    std::vector<Channel> sendChl{ ep0.addChannel(name, name) };

    OosNcoOtReceiver otRecv0, otRecv1;
    OosNcoOtSender otSend0, otSend1;

    
    Grr18MPsiReceiver recv;
    std::thread thrd([&]() {

        Grr18MPsiSender send;

        send.init(setSize, psiSecParam, sendChl, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChl);
    });

    recv.init(setSize, psiSecParam, recvChl, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChl);

    thrd.join();

    sendChl[0].close();
    recvChl[0].close();

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size())
        throw UnitTestFail();
}

void Grr18_Oos_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 8, psiSecParam = 40, numThreads(1);
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    //std::shuffle(sendSet.begin(), sendSet.end(), prng);

    std::string name("psi");

    IOService ios(0);
    Endpoint ep0(ios, "localhost", 1212, EpMode::Client, name);
    Endpoint ep1(ios, "localhost", 1212, EpMode::Server, name);

    std::vector<Channel> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }

    OosNcoOtReceiver otRecv0, otRecv1;
    OosNcoOtSender otSend0, otSend1;

    Grr18MPsiSender send;
    Grr18MPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);

    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i].close();
        recvChls[i].close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size() != setSize)
    {
        std::cout << "failed " << recv.mIntersection.size() << " != " << setSize << std::endl;

        for (u64 i = 0; i < setSize; ++i)
        {
            if (std::find(recv.mIntersection.begin(), recv.mIntersection.end(), i) == recv.mIntersection.end())
            {
                std::cout << i << "  ";
            }
        }
        std::cout << std::endl;
        throw UnitTestFail();
    }
}

void Grr18_Oos_parallel_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 1 << 4, psiSecParam = 40, numThreads(2);
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    std::shuffle(sendSet.begin(), sendSet.end(), prng);

    std::string name("psi");

    IOService ios(0);
    Endpoint ep0(ios, "localhost", 1212, EpMode::Client, name);
    Endpoint ep1(ios, "localhost", 1212, EpMode::Server, name);

    std::vector<Channel> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }

    OosNcoOtReceiver otRecv0, otRecv1;
    OosNcoOtSender otSend0, otSend1;

    Grr18MPsiSender send;
    Grr18MPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);

    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i].close();
        recvChls[i].close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size() != setSize)
        throw UnitTestFail();
}
void Grr18_Oos_SingltonSet_Test_Impl()
{
    setThreadName("Sender");
    u64 setSize = 1, psiSecParam = 40;

    PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    sendSet[setSize / 2] = recvSet[0];

    std::string name("psi");
    IOService ios(0);

    Endpoint ep0(ios, "localhost", 1212, EpMode::Client, name);
    Endpoint ep1(ios, "localhost", 1212, EpMode::Server, name);

    Channel recvChl = ep1.addChannel(name, name);
    Channel sendChl = ep0.addChannel(name, name);

    OosNcoOtReceiver otRecv0, otRecv1;
    OosNcoOtSender otSend0, otSend1;

    Grr18MPsiSender send;
    Grr18MPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChl, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChl);
    });

    recv.init(setSize, psiSecParam, recvChl, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChl);

    thrd.join();

    //std::cout << gTimer << std::endl;

    sendChl.close();
    recvChl.close();

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size() != 1 ||
        recv.mIntersection[0] != 0)
        throw UnitTestFail();
}