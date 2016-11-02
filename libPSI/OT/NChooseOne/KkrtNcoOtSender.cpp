#include "KkrtNcoOtSender.h"
#include "Network/BtIOService.h"
#include "OT/Tools/Tools.h"
#include "Common/Log.h"
#include "KkrtDefines.h"

namespace osuCrypto
{
    //#define OTEXT_DEBUG
    //#define    PRINT_OTEXT_DEBUG
    using namespace std;

    void KkrtNcoOtSender::setBaseOts(
        ArrayView<block> baseRecvOts,
        const BitVector & choices)
    {
        if (choices.size() != baseRecvOts.size())
            throw std::runtime_error("size mismatch");

        if (choices.size() % (sizeof(block) * 8) != 0)
            throw std::runtime_error("only multiples of 128 are supported");


        mBaseChoiceBits = choices;
        mGens.resize(choices.size());

        for (int i = 0; i < baseRecvOts.size(); i++)
        {
            mGens[i].SetSeed(baseRecvOts[i]);
        }

        mChoiceBlks.resize(choices.size() / (sizeof(block) * 8));
        for (u64 i = 0; i < mChoiceBlks.size(); ++i)
        {
            mChoiceBlks[i] = toBlock(mBaseChoiceBits.data() + (i * sizeof(block)));
        }
    }

    std::unique_ptr<NcoOtExtSender> KkrtNcoOtSender::split()
    {
        auto* raw = new KkrtNcoOtSender();

        std::vector<block> base(mGens.size());

        for (u64 i = 0; i < base.size(); ++i)
        {
            base[i] = mGens[i].get<block>();
        }
        raw->setBaseOts(base, mBaseChoiceBits);

        return std::unique_ptr<NcoOtExtSender>(raw);
    }

    void KkrtNcoOtSender::init(
        u64 numOTExt)
    {
        const u8 superBlkSize(8);

        // round up
        numOTExt = ((numOTExt + 127) / 128) * 128;

        mT = std::move(MatrixView<block>(numOTExt, mGens.size() / 128));
        mCorrectionVals = std::move(MatrixView<block>(numOTExt, mGens.size() / 128));

        mCorrectionIdx = 0;

        // we are going to process SSOTs in blocks of 128 messages.
        u64 numBlocks = numOTExt / 128;
        u64 numSuperBlocks = (numBlocks + superBlkSize - 1) / superBlkSize;

        u64 doneIdx = 0;

        std::array<std::array<block, superBlkSize>, 128> t;

        u64 numCols = mGens.size();


        for (u64 superBlkIdx = 0; superBlkIdx < numSuperBlocks; ++superBlkIdx)
        {
            // compute at what row does the user want use to stop.
            // the code will still compute the transpose for these
            // extra rows, but it is thrown away.
            u64 stopIdx
                = doneIdx
                + std::min(u64(128) * superBlkSize, mT.size()[0] - doneIdx);

            for (u64 i = 0; i < numCols / 128; ++i)
            {

                for (u64 tIdx = 0, colIdx = i * 128; tIdx < 128; ++tIdx, ++colIdx)
                {
                    mGens[colIdx].mAes.ecbEncCounterMode(mGens[colIdx].mBlockIdx, superBlkSize, t[tIdx].data());
                    mGens[colIdx].mBlockIdx += superBlkSize;
                }

                sse_transpose128x1024(t);

                for (u64 rowIdx = doneIdx, j = 0; rowIdx < stopIdx; ++j)
                {
                    for (u64 k = 0; rowIdx < stopIdx && k < 128; ++rowIdx, ++k)
                    {
                        mT[rowIdx][i] = t[k][j];
                    }
                }

            }

            doneIdx = stopIdx;
        }
    }

    void KkrtNcoOtSender::encode(
        u64 otIdx,
        const ArrayView<block> inputword,
        block& val)
    {

#ifndef NDEBUG
        u64 expectedSize = mGens.size() / (sizeof(block) * 8);

        if (inputword.size() != expectedSize)
            throw std::invalid_argument("Bad input word" LOCATION);

        if (eq(mCorrectionVals[otIdx][0], ZeroBlock))
            throw std::invalid_argument("appears that we havent received the receive's choise yet. " LOCATION);


#endif // !NDEBUG

#ifdef AES_HASH
        std::array<block, 10> sums, hashOut;

        for (u64 i = 0; i < correlatedMgs.size(); ++i)
        {
            sums[i] = correlatedMgs[i] ^
                (otCorrectionMessage[i] ^ inputword[i]) & mChoiceBlks[i];
        }
        // compute the AES hash H(x) = AES(x_1) + x_1 + ... + AES(x_n) + x_n 
        mAesFixedKey.ecbEncBlocks(sums.data(), correlatedMgs.size(), hashOut.data());

        val = ZeroBlock;
        for (u64 i = 0; i < correlatedMgs.size(); ++i)
        {
            val = val ^ sums[i] ^ hashOut[i];
        }
#else
        SHA1  sha1;
        block sum;
        u8 hashBuff[SHA1::HashSize];

        for (u64 i = 0; i < mT.size()[1]; ++i)
        {
            sum = mT[otIdx][i] ^
                (mCorrectionVals[otIdx][i] ^ inputword[i]) & mChoiceBlks[i];

            sha1.Update((u8*)&sum, sizeof(block));
        }

        sha1.Final(hashBuff);
        val = toBlock(hashBuff);

#endif // AES_HASH

    }

    void KkrtNcoOtSender::getParams(
        bool maliciousSecure,
        u64 compSecParm,
        u64 statSecParam,
        u64 inputBitCount,
        u64 inputCount,
        u64 & inputBlkSize,
        u64 & baseOtCount)
    {
        baseOtCount = roundUpTo(compSecParm * 7, 128);
        inputBlkSize = baseOtCount / 128;
    }

    void KkrtNcoOtSender::recvCorrection(Channel & chl, u64 recvCount)
    {

#ifndef NDEBUG
        if (recvCount > mCorrectionVals.size()[0] - mCorrectionIdx)
            throw std::runtime_error("bad reciever, will overwrite the end of our buffer" LOCATION);

#endif // !NDEBUG        


        auto* dest = mCorrectionVals.begin() + (mCorrectionIdx * mCorrectionVals.size()[1]);
        chl.recv(dest,
            recvCount * sizeof(block) * mCorrectionVals.size()[1]);

        mCorrectionIdx += recvCount;
    }



}
