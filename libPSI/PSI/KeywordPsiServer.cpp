#include "KeywordPsiServer.h"

namespace osuCrypto
{
	void KeywordPsiServer::init(u8 serverId, Channel clientChl, u64 databaseSize, u64 clientSetSize, block seed)
	{
		mPrng.SetSeed(seed);
		mClientSetSize = clientSetSize;
		mServerSetSize = databaseSize;
		mServerId = serverId;
	}

	void KeywordPsiServer::send(Channel clientChl, span<block> inputs)
	{
		if (inputs.size() != mServerSetSize) {
			throw std::runtime_error(LOCATION);
		}

		//u64 numLeafBlocksPerBin = ((1 << ((sizeof(block)*8) - 1)) + 127) / 128;
		//    u64 gDepth = 2;
		//    u64 kDepth = std::max<u64>(gDepth, log2floor(numLeafBlocksPerBin)) - gDepth;
		//    u64 groupSize = (numLeafBlocksPerBin + (1 << kDepth) - 1) / (1 << kDepth);
		//    if (groupSize > 8) throw     std::runtime_error(LOCATION);
		//    std::vector<block> pirData((1 << kDepth) * groupSize * 128);

		//	std::cout << kDepth << " " << groupSize << std::endl;
		
		u64 kDepth = 119;
		u64 groupSize = 5;

		BgiPirServer pir;
		pir.init(kDepth, groupSize);

		std::vector<block> k(kDepth + 1 + groupSize);

		u64 numQueries = mClientSetSize;

		for (u64 i = 0; i < numQueries; ++i)
		{
			clientChl.recv(k.data(), k.size() * sizeof(block));
			span<block> kk(k.data(), kDepth + 1);
			span<block> g(k.data() + kDepth + 1, groupSize);
			
			u8 sum = 0;

			for (u64 j = 0; j < inputs.size(); ++j)
			{
				span<u8> ss((u8*)&inputs[i], sizeof(block));
				sum = sum ^ BgiPirServer::evalOne(ss, kk, g);
			}

			clientChl.send(&sum, sizeof(u8));
		}
	}
}
