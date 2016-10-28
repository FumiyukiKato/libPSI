#include "OtBinMPsiSender.h"

#include "Crypto/Commit.h"
#include "Common/Log.h"
#include "Common/Timer.h"
#include "OT/Base/naor-pinkas.h"
#include "OT/TwoChooseOne/KosOtExtReceiver.h"
#include "OT/TwoChooseOne/KosOtExtSender.h"
namespace osuCrypto
{

    OtBinMPsiSender::OtBinMPsiSender()
    {
    }


    OtBinMPsiSender::~OtBinMPsiSender()
    {
    }

    void OtBinMPsiSender::init(u64 n, u64 statSec, u64 inputBitSize,
        Channel & chl0,
        NcoOtExtSender&  ots,
        NcoOtExtReceiver& otRecv,
        block seed)
    {
        init(n, statSec, inputBitSize, { &chl0 }, ots, otRecv, seed);
    }

    void OtBinMPsiSender::init(u64 n, u64 statSec, u64 inputBitSize,
        const std::vector<Channel*>& chls,
        NcoOtExtSender& otSend,
        NcoOtExtReceiver& otRecv,
        block seed)
    {
        mStatSecParam = statSec;
        mN = n;
        gTimer.setTimePoint("init.send.start");

        // must be a multiple of 128...
        u64 baseOtCount;// = 128 * CodeWordSize;
        //u64 plaintextBlkSize;

        u64 compSecParam = 128;

        otSend.getParams(
            compSecParam, statSec, inputBitSize, mN, //  input
            mNcoInputBlkSize, baseOtCount); // output

        mOtMsgBlkSize = (baseOtCount + 127) / 128;

        mOtSend = &otSend;
        mOtRecv = &otRecv;

        //auto logn = std::log2(n);
        //mNumBins = (n + logn - 1) / logn;
        //mBinSize = logn * std::log2(logn);

        mPrng.SetSeed(seed);
        auto myHashSeed = mPrng.get<block>();
        auto& chl0 = *chls[0];


        Commit comm(myHashSeed), theirComm;
        chl0.asyncSend(comm.data(), comm.size());
        chl0.recv(theirComm.data(), theirComm.size());


        chl0.asyncSend(&myHashSeed, sizeof(block));
        block theirHashingSeed;
        chl0.recv(&theirHashingSeed, sizeof(block));

        mHashingSeed = myHashSeed ^ theirHashingSeed;

        gTimer.setTimePoint("init.send.hashSeed");


        mBins.init(n, inputBitSize, mHashingSeed, statSec);

        //mPsis.resize(mBins.mBinCount);

        u64 perBinOtCount = mBins.mMaxBinSize;// mPsis[0].PsiOTCount(mBins.mMaxBinSize, mBins.mRepSize);
        u64 otCount = perBinOtCount * mBins.mBinCount;

        gTimer.setTimePoint("init.send.baseStart");

        if (otSend.hasBaseOts() == false ||
            otRecv.hasBaseOts() == false)
        {
            // first do 128 public key OTs (expensive)
            std::array<std::array<block, 2>, gOtExtBaseOtCount> baseMsg;
            NaorPinkas base;
            base.send(baseMsg, mPrng, chl0, 2);


            // now extend these to enough recv OTs to seed the send Kco and the send Kos ot extension
            BitVector recvChoice(baseOtCount + gOtExtBaseOtCount); recvChoice.randomize(mPrng);
            std::vector<block> recvBaseMsg(baseOtCount + gOtExtBaseOtCount);
            KosOtExtReceiver kosRecv;
            kosRecv.setBaseOts(baseMsg);
            kosRecv.receive(recvChoice, recvBaseMsg, mPrng, chl0);


            // we now have a bunch of recv OTs, lets seed the NcoOtExtSender
            BitVector kcoSendBaseChoice; 
            kcoSendBaseChoice.copy(recvChoice, 0, baseOtCount);
            ArrayView<block> kcoSendBase(
                recvBaseMsg.begin(),
                recvBaseMsg.begin() + baseOtCount);
           
            otSend.setBaseOts(kcoSendBase, kcoSendBaseChoice);


            // now lets extend these recv OTs in the other direction
            BitVector kosSendBaseChoice;
            kosSendBaseChoice.copy(recvChoice, baseOtCount, gOtExtBaseOtCount);
            ArrayView<block> kosSendBase(
                recvBaseMsg.begin() + baseOtCount,
                recvBaseMsg.end());
            KosOtExtSender kos;
            kos.setBaseOts(kosSendBase, kosSendBaseChoice);

            // these send OTs will be stored here
            std::vector<std::array<block, 2>> sendBaseMsg(baseOtCount);
            kos.send(sendBaseMsg, mPrng, chl0);

            // now set these ~800 OTs as the base of our N choose 1 OTs NcoOtExtReceiver
            otRecv.setBaseOts(sendBaseMsg);
        }

        gTimer.setTimePoint("init.send.extStart");



        mRecvOtMessages.resize(otCount* mOtMsgBlkSize);// = std::move(MatrixView<std::array<block, 2>>(otCount, mNcoInputBlkSize));
        mSendOtMessages.resize(otCount* mOtMsgBlkSize);// = std::move(MatrixView<block>(otCount, mNcoInputBlkSize));

        auto sendRoutine = [&](u64 i, u64 total, NcoOtExtSender& ots, Channel& chl)
        {
            // round up to the next 128 to make sure we aren't wasting OTs in the extension...
            u64 start = std::min(roundUpTo(i *     otCount / total, 128), otCount);
            u64 end = std::min(roundUpTo((i + 1) * otCount / total, 128), otCount);

            // get the range of rows starting at start and ending at end
            MatrixView<block> range(
                mSendOtMessages.begin() + (start * mOtMsgBlkSize),
                mSendOtMessages.begin() + (end *mOtMsgBlkSize),
                mOtMsgBlkSize);

            ots.init(range);
        };

        auto recvOtRountine = [&]
        (u64 i, u64 total, NcoOtExtReceiver& ots, Channel& chl)
        {
            u64 start = std::min(roundUpTo(i *     otCount / total, 128), otCount);
            u64 end = std::min(roundUpTo((i + 1) * otCount / total, 128), otCount);

            // get the range of rows starting at start and ending at end
            MatrixView<std::array<block, 2>> range(
                mRecvOtMessages.begin() + (start * mOtMsgBlkSize),
                mRecvOtMessages.begin() + (end *mOtMsgBlkSize),
                mOtMsgBlkSize);

            ots.init(range);
        };

        u64 numThreads = chls.size() - 1;
        u64 numSendThreads = numThreads / 2;
        u64 numRecvThreads = numThreads - numSendThreads;


        std::vector<std::unique_ptr<NcoOtExtSender>> sendOts(numSendThreads);
        std::vector<std::unique_ptr<NcoOtExtReceiver>> recvOts(numRecvThreads);
        std::vector<std::thread> thrds(numThreads);
        auto thrdIter = thrds.begin();
        auto chlIter = chls.begin() + 1;


        for (u64 i = 0; i < numSendThreads; ++i)
        {
            sendOts[i] = std::move(otSend.split());
            auto extSeed = mPrng.get<block>();

            *thrdIter++ = std::thread([&, i, extSeed, chlIter]()
            {
                //Log::out << Log::lock << "s sendOt " << l << "  " << (**chlIter).getName() << Log::endl << Log::unlock;
                sendRoutine(i + 1, numSendThreads + 1, *sendOts[i], **chlIter);
            });
            ++chlIter;
        }

        for (u64 i = 0; i < numRecvThreads; ++i)
        {
            recvOts[i] = std::move(otRecv.split());
            auto extSeed = mPrng.get<block>();

            *thrdIter++ = std::thread([&, i, extSeed, chlIter]()
            {
                //Log::out << Log::lock << "s recvOt " << l << "  " << (**chlIter).getName() << Log::endl << Log::unlock;
                recvOtRountine(i, numRecvThreads, *recvOts[i], **chlIter);
            });
            ++chlIter;
        }

        sendRoutine(0, numSendThreads + 1, otSend, chl0);

        if (numRecvThreads == 0)
        {
            recvOtRountine(0, 1, otRecv, chl0);
        }


        for (auto& thrd : thrds)
            thrd.join();

        gTimer.setTimePoint("init.send.done");

    }


    void OtBinMPsiSender::sendInput(std::vector<block>& inputs, Channel & chl)
    {
        sendInput(inputs, { &chl });
    }

    void OtBinMPsiSender::sendInput(std::vector<block>& inputs, const std::vector<Channel*>& chls)
    {
        if (inputs.size() != mN)
            throw std::runtime_error(LOCATION);



        TODO("actually compute the required mask size!!!!!!!!!!!!!!!!!!!!!!");
        u64 maskSize = 16;

        if (maskSize > sizeof(block))
            throw std::runtime_error("masked are stored in blocks, so they can exceed that size");


        std::vector<std::thread>  thrds(chls.size());
        //std::vector<std::thread>  thrds(1);        

        std::atomic<u32> remaining((u32)thrds.size()), remainingMasks((u32)thrds.size());
        std::promise<void> doneProm, maskProm;
        std::shared_future<void>
            doneFuture(doneProm.get_future()),
            maskFuture(maskProm.get_future());

        std::mutex mtx;

        std::vector<std::vector<block>> ncoInputBuff(mNcoInputBlkSize);
        std::vector<block> recvMasks(mN);

        for (u64 hashIdx = 0; hashIdx < ncoInputBuff.size(); ++hashIdx)
            ncoInputBuff[hashIdx].resize(inputs.size());


        std::vector<u64> maskPerm(mN);

        auto permSeed = mPrng.get<block>();

        std::promise<void> permProm;
        std::shared_future<void> permDone(permProm.get_future());

        auto permThrd = std::thread([&]() {
            PRNG prng(permSeed);
            for (u64 i = 0; i < maskPerm.size(); ++i)
                maskPerm[i] = i;

            std::shuffle(maskPerm.begin(), maskPerm.end(), prng);
            //u64 l, u32Max = (u32(-1));
            //for (l = maskPerm.size(); l > u32Max; --l)
            //{
            //    u64 d = prng.get<u64>() % l;

            //    u64 pi = maskPerm[l];
            //    maskPerm[l] = maskPerm[d];
            //    maskPerm[d] = pi;
            //}
            //for (l = maskPerm.size(); l > 1; --l)
            //{

            //    u32 d = prng.get<u32>() % l;

            //    u64 pi = maskPerm[l];
            //    maskPerm[l] = maskPerm[d];
            //    maskPerm[d] = pi;
            //}
            permProm.set_value();
        });


        uPtr<Buff> sendMaskBuff(new Buff);
        sendMaskBuff->resize(maskPerm.size() * mBins.mMaxBinSize * maskSize);
        auto maskView = sendMaskBuff->getMatrixView<u8>(maskSize);
        
        gTimer.setTimePoint("online.send.spaw");

        for (u64 tIdx = 0; tIdx < thrds.size(); ++tIdx)
        {
            auto seed = mPrng.get<block>();
            thrds[tIdx] = std::thread([&, tIdx, seed]() {

                PRNG prng(seed);

                if (tIdx == 0) gTimer.setTimePoint("online.send.thrdStart");


                auto& chl = *chls[tIdx];
                auto startIdx = tIdx       * mN / thrds.size();
                auto endIdx = (tIdx + 1) * mN / thrds.size();

                // compute the region of inputs this thread should insert.
                //ArrayView<block> itemRange(
                //    inputs.begin() + startIdx,
                //    inputs.begin() + endIdx);

                std::vector<AES> ncoInputHasher(mNcoInputBlkSize);
                for (u64 i = 0; i < ncoInputHasher.size(); ++i)
                    ncoInputHasher[i].setKey(_mm_set1_epi64x(i) ^ mHashingSeed);


                for (u64 i = startIdx; i < endIdx; i += hasherStepSize)
                {
                    auto currentStepSize = std::min(hasherStepSize, inputs.size() - i);

                    for (u64 hashIdx = 0; hashIdx < ncoInputHasher.size(); ++hashIdx)
                    {
                        ncoInputHasher[hashIdx].ecbEncBlocks(
                            inputs.data() + i,
                            currentStepSize,
                            ncoInputBuff[hashIdx].data() + i);
                    }

                    // since we are using random codes, lets just use the first part of the code 
                    // as where each item should be hashed.
                    for (u64 j = 0; j < currentStepSize; ++j)
                    {
                        block& item = ncoInputBuff[0][i + j];
                        u64 addr = *(u64*)&item % mBins.mBinCount;

                        std::lock_guard<std::mutex> lock(mBins.mMtx[addr]);
                        mBins.mBins[addr].emplace_back(i + j);
                    }
                }
                //<< Log::lock << "Sender"<< Log::endl;
                //mBins.insertItemsWithPhasing(range, mStatSecParam, inputs.size());


                // block until all items have been inserted. the last to finish will set the promise...
                if (--remaining)
                    doneFuture.get();
                else
                    doneProm.set_value();

                if (tIdx == 0) gTimer.setTimePoint("online.send.insert");

                const u64 stepSize = 16;

                auto binStart = tIdx       * mBins.mBinCount / thrds.size();
                auto binEnd = (tIdx + 1) * mBins.mBinCount / thrds.size();

                auto otStart = binStart * mBins.mMaxBinSize;
                auto otEnd = binEnd * mBins.mMaxBinSize;



                std::vector<u16> permutation(mBins.mMaxBinSize);
                for (size_t i = 0; i < permutation.size(); i++)
                    permutation[i] = i;

                MatrixView<block> correlatedSendOts(
                    mSendOtMessages.begin() + (otStart * mOtMsgBlkSize),
                    mSendOtMessages.begin() + (otEnd * mOtMsgBlkSize),
                    mOtMsgBlkSize);

                MatrixView<std::array<block, 2>> correlatedRecvOts(
                    mRecvOtMessages.begin() + (otStart * mOtMsgBlkSize),
                    mRecvOtMessages.begin() + (otEnd * mOtMsgBlkSize),
                    mOtMsgBlkSize);


                u64 otIdx = 0;
                u64 maskIdx = otStart;
                std::vector<block> ncoInput(mNcoInputBlkSize);

                for (u64 bIdx = binStart; bIdx < binEnd;)
                {
                    u64 currentStepSize = std::min(stepSize, binEnd - bIdx);


                    std::unique_ptr<ByteStream> buff(new ByteStream());
                    buff->resize(sizeof(block) * mOtMsgBlkSize * currentStepSize * mBins.mMaxBinSize);

                    auto otCorrectionView = buff->getMatrixView<block>(mOtMsgBlkSize);
                    auto otCorrectionIdx = 0;

                    for (u64 stepIdx = 0; stepIdx < currentStepSize; ++bIdx, ++stepIdx)
                    {

                        auto& bin = mBins.mBins[bIdx];
                        std::random_shuffle(permutation.begin(), permutation.end(), prng);

                        for (u64 i = 0; i < permutation.size(); ++i)
                        {

                            if (permutation[i] < bin.size())
                            {
                                u64 inputIdx = bin[permutation[i]];
                                // ncoInput

                                for (u64 j = 0; j < ncoInput.size(); ++j)
                                {
                                    ncoInput[j] = ncoInputBuff[j][inputIdx];
                                }

                                auto otMsg = correlatedRecvOts[otIdx];
                                auto correction = otCorrectionView[otCorrectionIdx];

                                mOtRecv->encode(
                                    otMsg,                //  input
                                    ncoInput,             //  input
                                    correction,           // output
                                    recvMasks[inputIdx]); // output

                            }
                            else
                            {
                                // fill with random correction value.
                                prng.get((u8*)otCorrectionView[otCorrectionIdx].data(),
                                    mOtMsgBlkSize * sizeof(block));
                            }

                            otCorrectionIdx++;
                            otIdx++;
                        }
                    }

                    chl.asyncSend(std::move(buff));
                }


                Buff buff;
                otIdx = 0;

                if (tIdx == 0) gTimer.setTimePoint("online.send.recvMask");
                permDone.get();
                if (tIdx == 0) gTimer.setTimePoint("online.send.permPromDone");

                std::vector<u16> binPerm(mBins.mMaxBinSize);
                for (u64 i = 0; i < binPerm.size(); ++i)
                    binPerm[i] = i;

                for (u64 bIdx = binStart; bIdx < binEnd;)
                {

                    u64 currentStepSize = std::min(stepSize, binEnd - bIdx);

                    chl.recv(buff);
                    if (buff.size() != mOtMsgBlkSize * sizeof(block) * mBins.mMaxBinSize * currentStepSize)
                        throw std::runtime_error("not expected size");

                    auto otCorrectionBuff = buff.getMatrixView<block>(mOtMsgBlkSize);
                    u64 otCorrectionIdx = 0;



                    for (u64 stepIdx = 0; stepIdx < currentStepSize; ++bIdx, ++stepIdx)
                    {
                        auto& bin = mBins.mBins[bIdx];
                        

                        for (u64 i = 0; i < bin.size(); ++i)
                        {

                            u64 inputIdx = bin[i];
                            u64 baseMaskIdx = maskPerm[maskIdx] * mBins.mMaxBinSize;

                            u64 innerOtIdx = otIdx;
                            u64 innerOtCorrectionIdx = otCorrectionIdx;

                            for (u64 l = 0; l < mBins.mMaxBinSize; ++l)
                            {

                                for (u64 j = 0; j < mNcoInputBlkSize; ++j)
                                {
                                    ncoInput[j] = ncoInputBuff[j][inputIdx];
                                }

                                block sendMask;

                                auto otMsg = correlatedSendOts[innerOtIdx];
                                auto correction = otCorrectionBuff[innerOtCorrectionIdx];

                                mOtSend->encode(
                                    otMsg,
                                    ncoInput,
                                    correction,
                                    sendMask);

                                sendMask = sendMask ^ recvMasks[inputIdx];

                                TODO("add recv mask into this");

                                // truncate the block size mask down to "maskSize" bytes
                                // and store it in the maskView matrix at row maskIdx
                                memcpy(
                                    maskView[baseMaskIdx + l].data(),
                                    (u8*)&sendMask,
                                    maskSize);


                                ++innerOtIdx;
                                ++innerOtCorrectionIdx;
                            }

                            ++maskIdx;
                        }

                        otIdx += mBins.mMaxBinSize;
                        otCorrectionIdx += mBins.mMaxBinSize;
                    }

                }
                if (tIdx == 0) gTimer.setTimePoint("online.send.sendMask");

                // block until all masks are computed. the last to finish will set the promise...
                if (--remainingMasks)
                {
                    maskFuture.get();
                }
                else
                {
                    maskProm.set_value();
                }

                if (tIdx == 0)
                    chl.asyncSend(std::move(sendMaskBuff));

                if (tIdx == 0) gTimer.setTimePoint("online.send.finalMask");



            });
        }

        for (auto& thrd : thrds)
            thrd.join();

        permThrd.join();

    }

}


