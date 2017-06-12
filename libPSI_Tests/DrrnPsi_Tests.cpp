#include "BinOtPsi_Tests.h"

#include "Common.h"
#include "cryptoTools/Network/Endpoint.h"
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/Log.h"

#include "libPSI/PSI/DrrnPsiClient.h"
#include "libPSI/PSI/DrrnPsiServer.h"

#include <array>

using namespace osuCrypto;

void Psi_drrn_EmptySet_Test_Impl()
{
	setThreadName("client");
	u64 psiSecParam = 40;
	u64 clientSetSize = 128;
	u64 srvSetSize = 1 << 12;

	PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));
	PRNG prng1(_mm_set_epi32(4253465, 34354565, 1, 23987045));

	std::vector<block> clientSet(clientSetSize), srvSet(srvSetSize);
	for (u64 i = 0; i < clientSetSize; ++i)
	{
		clientSet[i] = prng.get<block>();
	}

	for (u64 i = 0; i < srvSet.size(); ++i)
	{
		srvSet[i] = prng1.get<block>();
	}

	// whp no match

	IOService ios(0);
	Endpoint epcs0(ios, "localhost", EpMode::Server, "cs0");
	Endpoint epcs1(ios, "localhost", EpMode::Server, "cs1");
	Endpoint eps0s(ios, "localhost", EpMode::Server, "ss");
	Endpoint eps0c(ios, "localhost", EpMode::Client, "cs0");
	Endpoint eps1c(ios, "localhost", EpMode::Client, "cs1");
	Endpoint eps1s(ios, "localhost", EpMode::Client, "ss");

	Channel cs0Chl = epcs0.addChannel("c");
	Channel cs1Chl = epcs1.addChannel("c");
	Channel s0cChl = eps0c.addChannel("c");
	Channel s1cChl = eps1c.addChannel("c");
	Channel s1sChl = eps1s.addChannel("c");
	Channel s0sChl = eps0s.addChannel("c");

	DrrnPsiClient client;
	DrrnPsiServer s0, s1;

	auto s0thrd = std::thread([&]() { s0.init(0, s0cChl, s0sChl, srvSetSize, clientSetSize, prng.get<block>()); });
	auto s1thrd = std::thread([&]() { s1.init(1, s1cChl, s1sChl, srvSetSize, clientSetSize, prng.get<block>()); });
	client.init(cs0Chl, cs1Chl, srvSetSize, clientSetSize, prng.get<block>());

	s0thrd.join();
	s1thrd.join();

	s0thrd = std::thread([&]() { s0.send(s0cChl, s0sChl, srvSet); });
	s1thrd = std::thread([&]() { s1.send(s1cChl, s1sChl, srvSet); });
	client.recv(cs0Chl, cs1Chl, clientSet);

	s0thrd.join();
	s1thrd.join();

	if (client.mIntersection.size() > 0) {
		throw UnitTestFail();
	}
}

void Psi_drrn_SingletonSet_Test_Impl()
{
	setThreadName("client");
	u64 psiSecParam = 40;
	u64 clientSetSize = 128;
	u64 srvSetSize = 1 << 12;

	PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));
	PRNG prng1(_mm_set_epi32(4253465, 34354565, 1, 23987045));

	std::vector<block> clientSet(clientSetSize), srvSet(srvSetSize);
	for (u64 i = 0; i < clientSetSize; ++i)
	{
		clientSet[i] = prng.get<block>();
	}

	for (u64 i = 0; i < srvSet.size(); ++i)
	{
		srvSet[i] = prng1.get<block>();
	}

	clientSet[5] = srvSet[23];

	IOService ios(0);
	Endpoint epcs0(ios, "localhost", EpMode::Server, "cs0");
	Endpoint epcs1(ios, "localhost", EpMode::Server, "cs1");
	Endpoint eps0s(ios, "localhost", EpMode::Server, "ss");
	Endpoint eps0c(ios, "localhost", EpMode::Client, "cs0");
	Endpoint eps1c(ios, "localhost", EpMode::Client, "cs1");
	Endpoint eps1s(ios, "localhost", EpMode::Client, "ss");

	Channel cs0Chl = epcs0.addChannel("c");
	Channel cs1Chl = epcs1.addChannel("c");
	Channel s0cChl = eps0c.addChannel("c");
	Channel s1cChl = eps1c.addChannel("c");
	Channel s1sChl = eps1s.addChannel("c");
	Channel s0sChl = eps0s.addChannel("c");

	DrrnPsiClient client;
	DrrnPsiServer s0, s1;

	auto s0thrd = std::thread([&]() { s0.init(0, s0cChl, s0sChl, srvSetSize, clientSetSize, prng.get<block>()); });
	auto s1thrd = std::thread([&]() { s1.init(1, s1cChl, s1sChl, srvSetSize, clientSetSize, prng.get<block>()); });
	client.init(cs0Chl, cs1Chl, srvSetSize, clientSetSize, prng.get<block>());

	s0thrd.join();
	s1thrd.join();

	s0thrd = std::thread([&]() { s0.send(s0cChl, s0sChl, srvSet); });
	s1thrd = std::thread([&]() { s1.send(s1cChl, s1sChl, srvSet); });
	client.recv(cs0Chl, cs1Chl, clientSet);

	s0thrd.join();
	s1thrd.join();
	
	if (client.mIntersection.size() != 1 || *client.mIntersection.begin() != 5) {
		throw UnitTestFail();
	}
}


void Psi_drrn_FullSet_Test_Impl()
{
	setThreadName("client");
	u64 psiSecParam = 40;
	u64 clientSetSize = 128;
	u64 srvSetSize = 1 << 12;

	PRNG prng(_mm_set_epi32(4253465, 34354565, 234435, 23987045));
	PRNG prng1(_mm_set_epi32(4253465, 34354565, 0, 23987045));

	std::vector<block> clientSet(clientSetSize), srvSet(srvSetSize);

	for (u64 i = 0; i < srvSet.size(); ++i)
	{
		srvSet[i] = prng1.get<block>();
	}
	for (u64 i = 0; i < clientSetSize; ++i)
	{
		clientSet[i] = srvSet[i];
	}

	IOService ios(0);
	Endpoint epcs0(ios, "localhost", EpMode::Server, "cs0");
	Endpoint epcs1(ios, "localhost", EpMode::Server, "cs1");
	Endpoint eps0s(ios, "localhost", EpMode::Server, "ss");
	Endpoint eps0c(ios, "localhost", EpMode::Client, "cs0");
	Endpoint eps1c(ios, "localhost", EpMode::Client, "cs1");
	Endpoint eps1s(ios, "localhost", EpMode::Client, "ss");

	Channel cs0Chl = epcs0.addChannel("c");
	Channel cs1Chl = epcs1.addChannel("c");
	Channel s0cChl = eps0c.addChannel("c");
	Channel s1cChl = eps1c.addChannel("c");
	Channel s1sChl = eps1s.addChannel("c");
	Channel s0sChl = eps0s.addChannel("c");

	DrrnPsiClient client;
	DrrnPsiServer s0, s1;

	auto s0thrd = std::thread([&]() { s0.init(0, s0cChl, s0sChl, srvSetSize, clientSetSize, prng.get<block>()); });
	auto s1thrd = std::thread([&]() { s1.init(1, s1cChl, s1sChl, srvSetSize, clientSetSize, prng.get<block>()); });
	client.init(cs0Chl, cs1Chl, srvSetSize, clientSetSize, prng.get<block>());

	s0thrd.join();
	s1thrd.join();

	s0thrd = std::thread([&]() { s0.send(s0cChl, s0sChl, srvSet); });
	s1thrd = std::thread([&]() { s1.send(s1cChl, s1sChl, srvSet); });
	client.recv(cs0Chl, cs1Chl, clientSet);

	s0thrd.join();
	s1thrd.join();

    bool failed = false;

	if (client.mIntersection.size() != clientSetSize) {
		std::cout << "wrong size " << client.mIntersection.size() << std::endl;
		//throw UnitTestFail();
        failed = true;
	}

	for (u64 i = 0; i < clientSetSize; ++i)
	{

		bool b = client.mIntersection.find(i) == client.mIntersection.end();
		if (b)
		{
			std::cout << "missing " << i << std::endl;
			//throw UnitTestFail();
            failed = true;
		}
	}
	
    if (failed) throw UnitTestFail();
}
