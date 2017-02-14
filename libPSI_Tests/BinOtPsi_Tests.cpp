#include "BinOtPsi_Tests.h"

#include "Common.h"
#include "cryptoTools/Network/BtEndpoint.h"
#include "cryptoTools/Common/Defines.h"
#include "MPSI/Beta/OtBinMPsiReceiver.h"
#include "MPSI/Beta/OtBinMPsiSender.h"
#include "cryptoTools/Common/Log.h"

#include "libOTe/NChooseOne/KkrtNcoOtReceiver.h"
#include "libOTe/NChooseOne/KkrtNcoOtSender.h"


#include "libOTe/NChooseOne/Oos/OosNcoOtReceiver.h"
#include "libOTe/NChooseOne/Oos/OosNcoOtSender.h"


#include "libOTe/NChooseOne/RR17/Rr17NcoOtReceiver.h"
#include "libOTe/NChooseOne/RR17/Rr17NcoOtSender.h"


#include "MPSI/Beta/CuckooHasher.h"

#include <array>

using namespace osuCrypto;




void OtBinPsi_CuckooHasher_Test_Impl()
{
    u64 setSize = 10000;

    u64 h = 2;
    std::vector<u64> _hashes(setSize * h + 1);
    MatrixView<u64> hashes(_hashes.begin(), _hashes.end(), h);
    PRNG prng(ZeroBlock);

    for (u64 i = 0; i < hashes.size()[0]; ++i)
    {
        for (u64 j = 0; j < h; ++j)
        {
            hashes[i][j] = prng.get<u64>();
        }
    }

    CuckooHasher hashMap0;
    CuckooHasher hashMap1;
    CuckooHasher::Workspace w(1);

    hashMap0.init(setSize, 40, true);
    hashMap1.init(setSize, 40, true);


    for (u64 i = 0; i < setSize; ++i)
    {
        //if (i == 6) hashMap0.print();

        hashMap0.insert(i, hashes[i]);

        std::vector<u64> tt{ i };
        MatrixView<u64> mm(hashes[i].data(), 1, 2, false);
        hashMap1.insertBatch(tt, mm, w);


        //if (i == 6) hashMap0.print();
        //if (i == 6) hashMap1.print();

        //if (hashMap0 != hashMap1)
        //{
        //    std::cout << i << std::endl;

        //    throw UnitTestFail();
        //}

    }

    if (hashMap0 != hashMap1)
    {
        throw UnitTestFail();
    }
}

void OtBinPsi_CuckooHasher_parallel_Test_Impl()
{
#ifdef THREAD_SAFE_CUCKOO

    u64 numThreads = 4;
    u64 step = 16;
    u64 setSize = 1 << 16;
    u64 h = 2;
    CuckooHasher hashMap;

    hashMap.init(setSize, 40, true);

    MatrixView<u64> hashes(setSize, h);
    PRNG prng(ZeroBlock);
    prng.get(hashes.data(), setSize * h);
    std::vector<std::thread> thrds(numThreads);

    for (u64 t = 0; t < numThreads; ++t)
    {

        thrds[t] = std::thread([&, t]() 
        {

            CuckooHasher::Workspace ws(step);

            u64 start = t * setSize / numThreads;
            u64 end = (t + 1) * setSize / numThreads;

            for (u64 i = start; i < end; i += step)
            {
                u64 ss = std::min(step, setSize - i);
                std::vector<u64> idx(ss);

                MatrixView<u64> range(hashes[i].data(), ss, h, false);

                for (u64 j = 0; j < ss; ++j)
                    idx[j] = j + i;

                hashMap.insertBatch(idx, range, ws);
            }
        });
    }

    for (u64 t = 0; t < numThreads; ++t)
        thrds[t].join();

    for (u64 i = 0; i < setSize; ++i)
    {
        if (hashMap.find(hashes[i]) != i)
            throw UnitTestFail();
    }

#endif

}




//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                 KKRT16 encode protocol                     //////////////////////
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







void OtBinPsi_Kkrt_EmptrySet_Test_Impl()
{
    u64 setSize = 8, psiSecParam = 40, bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> recvChl{ &ep1.addChannel(name, name) };
    std::vector<Channel*> sendChl{ &ep0.addChannel(name, name) };

    KkrtNcoOtReceiver otRecv0, otRecv1;
    KkrtNcoOtSender otSend0, otSend1;


    u64 baseCount = 128 * 7;
    std::vector<std::array<block, 2>> sendBlks(baseCount);
    std::vector<block> recvBlks(baseCount);
    BitVector choices(baseCount);
    choices.randomize(prng);

    for (u64 i = 0; i < baseCount; ++i)
    {
        sendBlks[i][0] = prng.get<block>();
        sendBlks[i][1] = prng.get<block>();
        recvBlks[i] = sendBlks[i][choices[i]];
    }

    otRecv0.setBaseOts(sendBlks);
    otSend0.setBaseOts(recvBlks, choices);

    for (u64 i = 0; i < baseCount; ++i)
    {
        sendBlks[i][0] = prng.get<block>();
        sendBlks[i][1] = prng.get<block>();
        recvBlks[i] = sendBlks[i][choices[i]];
    }

    otRecv1.setBaseOts(sendBlks);
    otSend1.setBaseOts(recvBlks, choices);

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {


        send.init(setSize, psiSecParam, sendChl, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChl);
    });

    recv.init(setSize, psiSecParam, recvChl, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChl);

    thrd.join();

    sendChl[0]->close();
    recvChl[0]->close();

    ep0.stop();
    ep1.stop();
    ios.stop();


    if (recv.mIntersection.size())
        throw UnitTestFail();
}


void OtBinPsi_Kkrt_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 8, psiSecParam = 40, numThreads(1), bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    std::shuffle(sendSet.begin(), sendSet.end(), prng);


    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = &ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = &ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }


    KkrtNcoOtReceiver otRecv0, otRecv1;
    KkrtNcoOtSender otSend0, otSend1;

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {


        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);

    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i]->close();
        recvChls[i]->close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();


    if (recv.mIntersection.size() != setSize)
        throw UnitTestFail();

}

void OtBinPsi_Kkrt_SingltonSet_Test_Impl()
{
    setThreadName("Sender");
    u64 setSize = 128, psiSecParam = 40, bitSize = 128;

    PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    sendSet[0] = recvSet[0];

    std::string name("psi");
    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    Channel& recvChl = ep1.addChannel(name, name);
    Channel& sendChl = ep0.addChannel(name, name);

    KkrtNcoOtReceiver otRecv0, otRecv1;
    KkrtNcoOtSender otSend0, otSend1;

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {


        send.init(setSize, psiSecParam, sendChl, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChl);
    });

    recv.init(setSize, psiSecParam, recvChl, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChl);

    thrd.join();

    sendChl.close();
    recvChl.close();

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size() != 1 ||
        recv.mIntersection[0] != 0)
        throw UnitTestFail();
}









//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
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









void OtBinPsi_Oos_EmptrySet_Test_Impl()
{
    u64 setSize = 8, psiSecParam = 40, bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> recvChl{ &ep1.addChannel(name, name) };
    std::vector<Channel*> sendChl{ &ep0.addChannel(name, name) };
    std::string solution(SOLUTION_DIR);
    LinearCode code;
    code.loadBinFile(solution + "/../libOTe/libOTe/Tools/bch511.bin");

    OosNcoOtReceiver otRecv0(code, 40), otRecv1(code, 40);
    OosNcoOtSender otSend0(code, 40), otSend1(code, 40);


    //u64 baseCount = 128 * 7;
    //std::vector<std::array<block, 2>> sendBlks(baseCount);
    //std::vector<block> recvBlks(baseCount);
    //BitVector choices(baseCount);
    //choices.randomize(prng);

    //for (u64 i = 0; i < baseCount; ++i)
    //{
    //    sendBlks[i][0] = prng.get<block>();
    //    sendBlks[i][1] = prng.get<block>();
    //    recvBlks[i] = sendBlks[i][choices[i]];
    //}

    //otRecv0.setBaseOts(sendBlks);
    //otSend0.setBaseOts(recvBlks, choices);

    //for (u64 i = 0; i < baseCount; ++i)
    //{
    //    sendBlks[i][0] = prng.get<block>();
    //    sendBlks[i][1] = prng.get<block>();
    //    recvBlks[i] = sendBlks[i][choices[i]];
    //}

    //otRecv1.setBaseOts(sendBlks);
    //otSend1.setBaseOts(recvBlks, choices);

    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {

        OtBinMPsiSender send;

        send.init(setSize, psiSecParam, sendChl, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChl);
    });

    recv.init(setSize, psiSecParam, recvChl, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChl);

    thrd.join();

    sendChl[0]->close();
    recvChl[0]->close();

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size())
        throw UnitTestFail();
}


void OtBinPsi_Oos_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 8, psiSecParam = 40, numThreads(1), bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    //std::shuffle(sendSet.begin(), sendSet.end(), prng);


    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = &ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = &ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }

    LinearCode code;
    code.loadBinFile(std::string(SOLUTION_DIR) + "/../libOTe/libOTe/Tools/bch511.bin");

    OosNcoOtReceiver otRecv0(code, 40), otRecv1(code, 40);
    OosNcoOtSender otSend0(code, 40), otSend1(code, 40);

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);


    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i]->close();
        recvChls[i]->close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();


    if (recv.mIntersection.size() != setSize)
    {
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

void OtBinPsi_Oos_parallel_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 1 << 4, psiSecParam = 40, numThreads(2), bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    std::shuffle(sendSet.begin(), sendSet.end(), prng);


    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = &ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = &ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }

    LinearCode code;
    code.loadBinFile(std::string(SOLUTION_DIR) + "/../libOTe/libOTe/Tools/bch511.bin");

    OosNcoOtReceiver otRecv0(code, 40), otRecv1(code, 40);
    OosNcoOtSender otSend0(code, 40), otSend1(code, 40);

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);


    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i]->close();
        recvChls[i]->close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();


    if (recv.mIntersection.size() != setSize)
        throw UnitTestFail();
}
void OtBinPsi_Oos_SingltonSet_Test_Impl()
{
    setThreadName("Sender");
    u64 setSize = 128, psiSecParam = 40, bitSize = 128;

    PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    sendSet[setSize / 2] = recvSet[0];

    std::string name("psi");
    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    Channel& recvChl = ep1.addChannel(name, name);
    Channel& sendChl = ep0.addChannel(name, name);

    LinearCode code;
    code.loadBinFile(std::string(SOLUTION_DIR) + "/../libOTe/libOTe/Tools/bch511.bin");

    OosNcoOtReceiver otRecv0(code, 40), otRecv1(code, 40);
    OosNcoOtSender otSend0(code, 40), otSend1(code, 40);

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
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






//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////                                                            //////////////////////
////////////////////          RR17 standard model encode protocol               //////////////////////
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




void OtBinPsi_Rr17_EmptrySet_Test_Impl()
{
    u64 setSize = 8, psiSecParam = 40, bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> recvChl{ &ep1.addChannel(name, name) };
    std::vector<Channel*> sendChl{ &ep0.addChannel(name, name) };
    std::string solution(SOLUTION_DIR);

    Rr17NcoOtReceiver otRecv0, otRecv1;
    Rr17NcoOtSender otSend0, otSend1;


    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {

        OtBinMPsiSender send;

        send.init(setSize, psiSecParam, sendChl, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChl);
    });

    recv.init(setSize, psiSecParam, recvChl, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChl);

    thrd.join();

    sendChl[0]->close();
    recvChl[0]->close();

    ep0.stop();
    ep1.stop();
    ios.stop();

    if (recv.mIntersection.size())
        throw UnitTestFail();
}


void OtBinPsi_Rr17_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 8, psiSecParam = 40, numThreads(1), bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    std::shuffle(sendSet.begin(), sendSet.end(), prng);


    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = &ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = &ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }


    Rr17NcoOtReceiver otRecv0, otRecv1;
    Rr17NcoOtSender otSend0, otSend1;

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);


    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i]->close();
        recvChls[i]->close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();


    if (recv.mIntersection.size() != setSize)
    {
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

void OtBinPsi_Rr17_parallel_FullSet_Test_Impl()
{
    setThreadName("CP_Test_Thread");
    u64 setSize = 1 << 4, psiSecParam = 40, numThreads(2), bitSize = 128;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = recvSet[i] = prng.get<block>();
    }

    std::shuffle(sendSet.begin(), sendSet.end(), prng);
    for (u64 i = 0; i < setSize; ++i)
    {
        std::cout << i << " " << sendSet[i] << "  " << recvSet[i] << std::endl;
    }


    std::string name("psi");

    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    std::vector<Channel*> sendChls(numThreads), recvChls(numThreads);
    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i] = &ep1.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
        recvChls[i] = &ep0.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
    }


    Rr17NcoOtReceiver otRecv0, otRecv1;
    Rr17NcoOtSender otSend0, otSend1;

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
    std::thread thrd([&]() {

        send.init(setSize, psiSecParam, sendChls, otSend0, otRecv0, prng.get<block>());
        send.sendInput(sendSet, sendChls);
    });

    recv.init(setSize, psiSecParam, recvChls, otRecv1, otSend1, ZeroBlock);
    recv.sendInput(recvSet, recvChls);


    thrd.join();

    for (u64 i = 0; i < numThreads; ++i)
    {
        sendChls[i]->close();
        recvChls[i]->close();
    }

    ep0.stop();
    ep1.stop();
    ios.stop();


    if (recv.mIntersection.size() != setSize)
        throw UnitTestFail();
}
void OtBinPsi_Rr17_SingltonSet_Test_Impl()
{
    setThreadName("Sender");
    u64 setSize = 128, psiSecParam = 40, bitSize = 128;

    PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));

    std::vector<block> sendSet(setSize), recvSet(setSize);
    for (u64 i = 0; i < setSize; ++i)
    {
        sendSet[i] = prng.get<block>();
        recvSet[i] = prng.get<block>();
    }

    sendSet[0] = recvSet[0];

    std::string name("psi");
    BtIOService ios(0);
    BtEndpoint ep0(ios, "localhost", 1212, true, name);
    BtEndpoint ep1(ios, "localhost", 1212, false, name);


    Channel& recvChl = ep1.addChannel(name, name);
    Channel& sendChl = ep0.addChannel(name, name);


    Rr17NcoOtReceiver otRecv0, otRecv1;
    Rr17NcoOtSender otSend0, otSend1;

    OtBinMPsiSender send;
    OtBinMPsiReceiver recv;
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