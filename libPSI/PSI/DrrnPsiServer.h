#pragma once
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/CuckooIndex.h>
#include <cryptoTools/Network/Channel.h>
#include <libPSI/PSI/KkrtPsiSender.h>
#include <libOTe/NChooseOne/Kkrt/KkrtNcoOtSender.h>

namespace osuCrypto
{

    class DrrnPsiServer
    {
    public:

        void init(u8 serverId, Channel chan, Channel srvChl, u64 databaseSize, u64 clientSetSize, block seed, double binScaler = 1);

		//void setCuckooParam(osuCrypto::u64 &serverSetSize, int ssp);

		void setInputs(span<block> inputs, u64 numThreads = 1);

        void send(Channel clientChl, Channel srvChl, u64 numThreads = 1);

		span<block> mInputs;

        //CuckooParam mCuckooParams;
        CuckooIndex<NotThreadSafe> mIndex;
        KkrtNcoOtSender otSend;
        PRNG mPrng;
        KkrtPsiSender mPsi;

        u64 mClientSetSize, mServerSetSize, mNumSimpleBins, mBinSize;
        u8 mServerId;
        block mHashingSeed;
    };

}
