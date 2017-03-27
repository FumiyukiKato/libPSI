#pragma once
#include "cryptoTools/Common/ArrayView.h"
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Crypto/PRNG.h"

namespace osuCrypto
{

    class DktMPsiReceiver
    {
    public:
        DktMPsiReceiver();
        ~DktMPsiReceiver();


        u64 mN, mSecParam;
        PRNG mPrng;

        std::vector<u64> mIntersection;

        void init(u64 n, u64 secParam, block seed);


        void sendInput(ArrayView<block> inputs, ArrayView<Channel> chl0);

    };

}