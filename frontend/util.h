#pragma once
#include "cryptoTools/Network/BtChannel.h"

using namespace osuCrypto;

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
    size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}



struct LaunchParams
{
    LaunchParams()
        :mVerbose(0),
        mTrials(1),
        mStatSecParam(40)
    {
    }

    std::vector<Channel*> getChannels(u64 n)
    {
        return  std::vector<Channel*>( mChls.begin(), mChls.begin() + n);
    }

    std::string mHostName;
    std::vector<Channel*> mChls;
    std::vector<u64> mNumItems;
    std::vector<u64> mNumThreads;

    u64 mBitSize;
    u64 mVerbose;
    u64 mTrials;
    u64 mStatSecParam;
};


#include "cryptoTools/Network/Channel.h"
void senderGetLatency(osuCrypto::Channel& chl);

void recverGetLatency(osuCrypto::Channel& chl);




void printTimings(
    std::string tag,
    std::vector<osuCrypto::Channel *> &chls,
    long long offlineTime, long long onlineTime,
    LaunchParams & params,
    const osuCrypto::u64 &setSize,
    const osuCrypto::u64 &numThreads);

void printHeader();
