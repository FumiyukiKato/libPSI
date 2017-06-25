#include "DrrnPsiServer.h"
#include <libOTe/TwoChooseOne/IknpOtExtReceiver.h>
#include <libPSI/PIR/BgiPirServer.h>
#include <libPSI/Tools/SimpleIndex.h>
#include <algorithm>
namespace osuCrypto
{
    void DrrnPsiServer::init(u8 serverId, Channel clientChl, Channel srvChl, u64 databaseSize, u64 clientSetSize, block seed, double binScaler)
    {
        if (mServerId == 0) gTimer.setTimePoint("DrrnSrv init start");
        auto ssp(40);
        mPrng.SetSeed(seed);
        mClientSetSize = clientSetSize;
        mServerSetSize = databaseSize;
        mServerId = serverId;

        mCuckooParams = CuckooIndex<>::selectParams(mServerSetSize, ssp, true);

        u64 numBalls = clientSetSize * mCuckooParams.mNumHashes;
        mNumSimpleBins = static_cast<u64>((numBalls / log2floor(numBalls)) * binScaler);

        if (mServerId == 0) gTimer.setTimePoint("DrrnSrv balls_bins start");
        mBinSize = SimpleIndex::get_bin_size(mNumSimpleBins, numBalls, ssp);


        if (serverId == 0)
        {
            // i think these are the right set sizes for the final PSI
            auto serverPsiInputSize = mBinSize * mNumSimpleBins;
            auto clientPsiInputSize = clientSetSize * mCuckooParams.mNumHashes;
            mPsi.init(serverPsiInputSize, clientPsiInputSize, 40, clientChl, otSend, mPrng.get<block>());
        }

        if (mServerId == 0) gTimer.setTimePoint("DrrnSrv init end");

    }

    void DrrnPsiServer::setInputs(span<block> inputs, u64 numThreads)
    {

        mHashingSeed = ZeroBlock; // todo, make random;
        mIndex.init(inputs.size(), 40, true);
        mIndex.insert(inputs, mHashingSeed);

        mInputs = inputs;

#ifndef NDEBUG
        mIndex.validate(inputs, mHashingSeed);
#endif
    }


    void DrrnPsiServer::send(Channel clientChl, Channel srvChl, u64 numThreads)
    {
        //numThreads = 1;
        if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
        if (mInputs.size() != mServerSetSize ||
            mInputs.size() != mCuckooParams.mN ||
            mInputs.size() != mIndex.mParams.mN)
        {
            std::cout << " failed " << std::endl;
            throw std::runtime_error(LOCATION);
        }


        u64 cuckooSlotsPerBin = (mIndex.mBins.size() + mNumSimpleBins) / mNumSimpleBins;

        if (mServerId == 0) gTimer.setTimePoint("DrrnSrv send start");

        // power of 2
        u64 numLeafBlocksPerBin = (cuckooSlotsPerBin + 127) / 128;
        u64 gDepth = 2;
        u64 kDepth = std::max<u64>(gDepth, log2floor(numLeafBlocksPerBin)) - gDepth;
        u64 groupSize = (numLeafBlocksPerBin + (u64(1) << kDepth) - 1) / (u64(1) << kDepth);
        if (groupSize > 8) throw     std::runtime_error(LOCATION);
        std::vector<block> shares(mNumSimpleBins * mBinSize);

        u64 keySize = kDepth + 1 + groupSize;
        std::vector<std::future<u64>> futrs(mNumSimpleBins);
        Matrix<block> k(mNumSimpleBins * mBinSize, keySize);
        //std::vector<u64> idxs(mNumSimpleBins * mBinSize);
        for (u64 bIdx = 0; bIdx < futrs.size(); ++bIdx)
        {
            futrs[bIdx] = clientChl.asyncRecv(k.data() + keySize * bIdx * mBinSize, k.stride() * sizeof(block) * mBinSize);
            //futrs[bIdx] = clientChl.asyncRecv(idxs.data() + bIdx * mBinSize, sizeof(u64) * mBinSize);
        }
        //auto fIter = futrs.begin();


        //u64 cuckooIdx = 0;
        auto routine = [this, numThreads, kDepth, keySize, groupSize, &futrs, &k, &shares](u64 tIdx)
        {
            std::vector<block> pirData((u64(1) << kDepth) * groupSize * 128);

            for (u64 binIdx = tIdx; binIdx < mNumSimpleBins; binIdx += numThreads)
            {
                auto shareIter = shares.begin() + binIdx * mBinSize;

                //u64 idx = cuckooIdx * mNumSimpleBins / mIndex.mBins.size();

                auto cuckooIdx = (binIdx * mIndex.mBins.size()       + mNumSimpleBins - 1) / mNumSimpleBins;
                auto cuckooEnd = ((binIdx + 1) * mIndex.mBins.size() + mNumSimpleBins - 1) / mNumSimpleBins;
                //auto curSize = std::min<u64>(cuckooSlotsPerBin, mIndex.mBins.size() - cuckooIdx);

                for (u64 i = 0; cuckooIdx < cuckooEnd; ++i, ++cuckooIdx)
                {
                    auto& item = mIndex.mBins[cuckooIdx];
                    auto empty = item.isEmpty();
                    if (empty == false)
                    {
                        auto idx = item.idx();
                        pirData[i] = mInputs[idx];

                        //{
                        //    auto hash = mIndex.mHashes[idx];
                        //    std::cout << IoStream::lock << "sinput[" << idx << "] = " << mInputs[idx] << " -> pir["<< i << "], hash= "  << hash << " ("
                        //        << CuckooIndex<>::getHash(hash, 0, mIndex.mBins.size()) << ", "
                        //        << CuckooIndex<>::getHash(hash, 1, mIndex.mBins.size()) << ", "
                        //        << CuckooIndex<>::getHash(hash, 2, mIndex.mBins.size()) << ") " << mIndex.mBins[cuckooIdx].hashIdx()
                        //        << std::endl << IoStream::unlock;
                        //}
                    }
                }


                futrs[binIdx].get();
                auto kIter = k.data() + keySize * binIdx * mBinSize;
                //auto idxIter = idxs.data() + binIdx * mBinSize;
                for (u64 i = 0; i < mBinSize; ++i)
                {

                    span<block> kk(kIter, kDepth + 1);
                    span<block> g(kIter + kDepth + 1, groupSize);
                    *shareIter = BgiPirServer::fullDomain(pirData, kk, g);
                    //if (neq(kIter[keySize - 1], ZeroBlock))
                    //{
                    //    std::cout << IoStream::lock << "bIdx " << binIdx << " " << i << " key " << kIter[keySize - 1] << "   shareIdx=" << (shareIter - shares.begin()) << " pir["<< *idxIter <<"] " << pirData[*idxIter]<< std::endl << IoStream::unlock;
                    //}
                    ++shareIter;
                    kIter += keySize;
                    //++idxIter;
                }
            }
        };
        if (mServerId == 0) gTimer.setTimePoint("DrrnSrv PSI start");

        std::vector<std::thread> thrds(numThreads - 1);
        for (u64 i = 1; i < numThreads; ++i)
            thrds[i - 1] = std::thread([i, &routine]() { routine(i); });

        routine(0);

        for (u64 i = 1; i < numThreads; ++i)
            thrds[i - 1].join();

        if (mServerId)
        {
            block rSeed;
            clientChl.recv((u8*)&rSeed, sizeof(block));

            PRNG prng(rSeed ^ OneBlock);
            if (shares.size() > (1ull << 32) - 1) throw std::runtime_error(LOCATION);

            std::vector<u32> pi1(shares.size()), sigma(mIndex.mParams.mNumHashes);
            for (u32 i = 0; i < sigma.size(); ++i) sigma[i] = i;
            for (u32 i = 0; i < pi1.size(); ++i) pi1[i] = i;
            std::random_shuffle(pi1.begin(), pi1.end(), prng);

            std::vector<block>
                r(shares.size()),
                s(shares.size()),
                piS1(shares.size()),
                pi1SigmaRS(shares.size());

            AES rGen(rSeed);
            rGen.ecbEncCounterMode(shares.size() * 0, shares.size(), r.data());
            rGen.ecbEncCounterMode(shares.size() * 1, shares.size(), piS1.data());
            rGen.ecbEncCounterMode(shares.size() * 2, shares.size(), s.data());

            auto rIter = r.begin();
            for (u64 i = 0; i < mClientSetSize; ++i)
            {
                std::random_shuffle(sigma.begin(), sigma.end(), prng);
                for (u64 j = 1; j < sigma.size(); ++j)
                {
                    std::swap(rIter[j], rIter[sigma[j]]);
                }
                rIter += sigma.size();
            }

            for (u64 i = 0; i < pi1SigmaRS.size(); ++i)
            {
                auto pi1i = pi1[i];
                pi1SigmaRS[i] = r[pi1i] ^ s[pi1i];
                //std::cout << "pi1(r + s)[" << i << "] " << pi1SigmaRS[i] << " = " << r[pi1i]<<" ^ "<<s[pi1i] << std::endl;
            }
            srvChl.asyncSend(std::move(pi1SigmaRS));

            auto piSIter = piS1.begin();
            for (u64 j =0; piSIter != piS1.end(); ++piSIter, ++j)
            {
                shares[j] = shares[j] ^ *piSIter;
            }

            srvChl.asyncSend(std::move(shares));
            //srvChl.asyncSend(std::move(piS1));
        }
        else
        {
            std::vector<u32> pi0(shares.size());
            std::vector<block> piS0(shares.size()), pi1SigmaRS(shares.size()), piSigmaR0(shares.size());
            clientChl.recv((u8*)pi0.data(), pi0.size() * sizeof(u32));
            clientChl.recv(piS0);

            srvChl.recv((u8*)pi1SigmaRS.data(), pi1SigmaRS.size() * sizeof(block));

            for (u64 i = 0; i < shares.size(); ++i)
            {
                //std::cout << "pi(r + s)[" << i << "]=" << pi1SigmaRS[pi0[i]] << std::endl;
                piSigmaR0[i] = pi1SigmaRS[pi0[i]] ^ piS0[i];
            }

            // reuse the memory
            auto& piSigmaRV1 = pi1SigmaRS;
            srvChl.recv(piSigmaRV1);
            //srvChl.recv(piS1);

            for (u64 i = 0; i < shares.size(); ++i)
            {
                //std::cout << "pi(r)[" << i << "] " << (piS1[i] ^ piSigmaR0[i]) << std::endl;
                shares[i] = shares[i] ^ piSigmaR0[i] ^ piSigmaRV1[i];

                //std::cout << "s i=" << i << " " << (shares[i] ^ (piS0[i] ^ piS1[i])) << " " << (piS0[i] ^ piS1[i]) << " = "
                //    << piS0[i] << " + " << piS1[i] << std::endl;
            }
            //u64  j = 0, end = shares.size() - 7;
            //for (; j < end; j += 8)
            //{
            //    shares[j + 0] = shares[j + 0] ^ otherShare[j + 0];
            //    shares[j + 1] = shares[j + 1] ^ otherShare[j + 1];
            //    shares[j + 2] = shares[j + 2] ^ otherShare[j + 2];
            //    shares[j + 3] = shares[j + 3] ^ otherShare[j + 3];
            //    shares[j + 4] = shares[j + 4] ^ otherShare[j + 4];
            //    shares[j + 5] = shares[j + 5] ^ otherShare[j + 5];
            //    shares[j + 6] = shares[j + 6] ^ otherShare[j + 6];
            //    shares[j + 7] = shares[j + 7] ^ otherShare[j + 7];
            //}
            //for (; j < shares.size(); ++j)
            //{
            //    shares[j] = shares[j] ^ otherShare[j];
            //}

            //for (u64 i = 0; i < shares.size(); ++i)
            //{
            //    std::cout << IoStream::lock << "sshare[" << i << "] = " << shares[i] << std::endl << IoStream::unlock;
            //}
            mPsi.sendInput(shares, clientChl);
        }

        if (mServerId == 0) gTimer.setTimePoint("DrrnSrv send Done");

    }
}
