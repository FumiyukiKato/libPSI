#include "bloomFilterMain.h"

#include "MPSI/AknBfMPsiReceiver.h"
#include "MPSI/AknBfMPsiSender.h"

#include <fstream>
using namespace libPSI;
#include "util.h"

#include "Common/Defines.h"
#include "Network/BtEndpoint.h" 
#include "OT/KosOtExtReceiver.h"
#include "OT/KosOtExtSender.h"

#include "OT/LzKosOtExtReceiver.h"
#include "OT/LzKosOtExtSender.h"
#include "Common/Log.h"
#include "Common/Timer.h"
#include "Crypto/PRNG.h"

#define LAZY_OT

void bfSend()
{


	Log::setThreadName("CP_Test_Thread");
	u64 numThreads(64);

	std::fstream online, offline;
	online.open("./online.txt", online.trunc | online.out);
	offline.open("./offline.txt", offline.trunc | offline.out);
	u64 numTrial(4);


	Log::out << "role  = sender (" << numThreads << ") akn" << Log::endl;

	std::string name("psi");

	BtIOService ios(0);
	BtEndpoint sendEP(ios, "localhost", 1212, true, name);

	std::vector<Channel*> sendChls_(numThreads);

	for (u64 i = 0; i < numThreads; ++i)
	{
		sendChls_[i] = &sendEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
	}
	u8 dummy[1];

	senderGetLatency(*sendChls_[0]);


	//for (auto pow : {/* 8,12,*/ 16/*, 20 */ })
	for (auto pow : { 8,12, 16,20 })
	{

		for (auto cc : { 1,/*2,*/4,/*8,16,*/32,64 })
		{
			std::vector<Channel*> sendChls;

			if (pow == 8)
				cc = std::min(8, cc);

			//Log::out << "numTHreads = " << cc << Log::endl;

			sendChls.insert(sendChls.begin(), sendChls_.begin(), sendChls_.begin() + cc);

			u64 offlineTimeTot(0);
			u64 onlineTimeTot(0);
			//for (u64 numThreads = 1; numThreads < 129; numThreads *= 2)
			for (u64 jj = 0; jj < numTrial; jj++)
			{

				//u64 repeatCount = 1;
				u64 setSize = (1 << pow), psiSecParam = 40;
				PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


				std::vector<block> sendSet;
				sendSet.resize(setSize);

				for (u64 i = 0; i < setSize; ++i)
				{
					sendSet[i] = prng.get_block();
				}


#ifdef LAZY_OT
				LzKosOtExtReceiver otRecv;
				LzKosOtExtSender otSend;
#else
				KosOtExtReceiver otRecv;
				KosOtExtSender otSend;
#endif // LAZY_OT



				AknBfMPsiSender sendPSIs;

				gTimer.reset();

				u64 otIdx = 0;
				//Log::out << "sender init" << Log::endl;
				sendPSIs.init(setSize, psiSecParam, otSend, sendChls, prng.get_block());

				//return;
				sendChls[0]->asyncSend(dummy, 1);
				//Log::out << "sender init done" << Log::endl;

				sendPSIs.sendInput(sendSet, sendChls);


			}

		}


	}
	for (u64 i = 0; i < numThreads; ++i)
	{
		sendChls_[i]->close();// = &sendEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
	}
	//sendChl.close();
	//recvChl.close();

	sendEP.stop();

	ios.stop();
}

void bfRecv()
{

	Log::setThreadName("CP_Test_Thread");
	u64 numThreads(64);

	std::fstream online, offline;
	online.open("./online.txt", online.trunc | online.out);
	offline.open("./offline.txt", offline.trunc | offline.out);
	u64 numTrial(4);

	std::string name("psi");

	BtIOService ios(0);
	BtEndpoint recvEP(ios, "localhost", 1212, false, name);

	std::vector<Channel*> recvChls_(numThreads);
	for (u64 i = 0; i < numThreads; ++i)
	{
		recvChls_[i] = &recvEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
	}

	Log::out << "role  = recv(" << numThreads << ") akn" << Log::endl;
	u8 dummy[1];
	recverGetLatency(*recvChls_[0]);

	//for (auto pow : {/* 8,12,*/16/*,20*/ })
	for (auto pow : { 8,12,16,20 })
	{
		for (auto cc : { 1,4,32,64 })
		{
			std::vector<Channel*> recvChls;

			if (pow == 8)
				cc = std::min(8, cc);

			u64 setSize = (1 << pow), psiSecParam = 40;

			Log::out << "numTHreads = " << cc << "  n=" << setSize << Log::endl;

			recvChls.insert(recvChls.begin(), recvChls_.begin(), recvChls_.begin() + cc);

			u64 offlineTimeTot(0);
			u64 onlineTimeTot(0);
			//for (u64 numThreads = 1; numThreads < 129; numThreads *= 2)
			for (u64 jj = 0; jj < numTrial; jj++)
			{

				//u64 repeatCount = 1;
				PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


				std::vector<block> sendSet(setSize), recvSet(setSize);




				for (u64 i = 0; i < setSize; ++i)
				{
					sendSet[i] = recvSet[i] = prng.get_block();
				}





#define LAZY_OT
#ifdef LAZY_OT
				LzKosOtExtReceiver otRecv;
				LzKosOtExtSender otSend;
#else
				KosOtExtReceiver otRecv;
				KosOtExtSender otSend;
#endif // LAZY_OT


				AknBfMPsiReceiver recvPSIs;

				gTimer.reset();

				u64 otIdx = 0;


				Timer timer;
				auto start = timer.setTimePoint("start");
				recvPSIs.init(setSize, psiSecParam, otRecv, recvChls, ZeroBlock);
				//return;

				recvChls[0]->recv(dummy, 1);
				auto mid = timer.setTimePoint("init");


				AknBfMPsiReceiver& recv = recvPSIs;

				recv.sendInput(recvSet, recvChls);
				auto end = timer.setTimePoint("done");

				auto offlineTime = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start).count();
				auto onlineTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid).count();


				offlineTimeTot += offlineTime;
				onlineTimeTot += onlineTime;
				Log::out << setSize << "  " << offlineTime << "  " << onlineTime << Log::endl;


				//Log::out << "threads =  " << numThreads << Log::endl << timer << Log::endl << Log::endl << Log::endl;


				//Log::out << numThreads << Log::endl;
				//Log::out << timer << Log::endl;

				//Log::out << gTimer << Log::endl;

				//if (recv.mIntersection.size() != setSize)
				//	throw std::runtime_error("");







			}



			online << onlineTimeTot / numTrial << "-";
			offline << offlineTimeTot / numTrial << "-";

		}
	}

	for (u64 i = 0; i < numThreads; ++i)
	{
		recvChls_[i]->close();// = &recvEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
	}
	//sendChl.close();
	//recvChl.close();

	recvEP.stop();

	ios.stop();
}








void bf(int role)
{

	Log::setThreadName("CP_Test_Thread");
	u64 numThreads(64);

	std::fstream online, offline;
	online.open("./online.txt", online.trunc | online.out);
	offline.open("./offline.txt", offline.trunc | offline.out);
	u64 numTrial(8);


	Log::out << "role  = " << role << Log::endl;

	for (auto pow : { 8,12,16,20 })
	{

		u64 offlineTimeTot(0);
		u64 onlineTimeTot(0);
		//for (u64 numThreads = 1; numThreads < 129; numThreads *= 2)
		for (u64 jj = 0; jj < numTrial; jj++)
		{

			//u64 repeatCount = 1;
			u64 setSize = (1 << pow), psiSecParam = 40;
			PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));


			std::vector<block> sendSet(setSize), recvSet(setSize);




			for (u64 i = 0; i < setSize; ++i)
			{
				sendSet[i] = recvSet[i] = prng.get_block();
			}


			std::shuffle(sendSet.begin(), sendSet.end(), prng);


			std::string name("psi");

			BtIOService ios(0);
			BtEndpoint recvEP(ios, "localhost", 1212, false, name);
			BtEndpoint sendEP(ios, "localhost", 1212, true, name);

			std::vector<Channel*> sendChls, recvChls;
			sendChls.resize(numThreads);
			recvChls.resize(numThreads);
			for (u64 i = 0; i < numThreads; ++i)
			{
				recvChls[i] = &recvEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
				sendChls[i] = &sendEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
			}

#define LAZY_OT
#ifdef LAZY_OT
			LzKosOtExtReceiver otRecv;
			LzKosOtExtSender otSend;
#else
			KosOtExtReceiver otRecv;
			KosOtExtSender otSend;
#endif // LAZY_OT


			std::thread thrd;

			AknBfMPsiSender sendPSIs;
			AknBfMPsiReceiver recvPSIs;
			thrd = std::thread([&]() {


				u64 otIdx = 0;

				sendPSIs.init(setSize, psiSecParam, otSend, sendChls, prng.get_block());
				sendPSIs.sendInput(sendSet, sendChls);

			});

			gTimer.reset();

			u64 otIdx = 0;


			Timer timer;
			auto start = timer.setTimePoint("start");
			recvPSIs.init(setSize, psiSecParam, otRecv, recvChls, ZeroBlock);
			auto mid = timer.setTimePoint("init");


			AknBfMPsiReceiver& recv = recvPSIs;

			recv.sendInput(recvSet, recvChls);
			auto end = timer.setTimePoint("done");

			auto offlineTime = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start).count();
			auto onlineTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid).count();


			offlineTimeTot += offlineTime;
			onlineTimeTot += onlineTime;
			Log::out << setSize << "  " << offlineTime << "  " << onlineTime << Log::endl;


			//Log::out << "threads =  " << numThreads << Log::endl << timer << Log::endl << Log::endl << Log::endl;


			//Log::out << numThreads << Log::endl;
			//Log::out << timer << Log::endl;

			//Log::out << gTimer << Log::endl;

			//if (recv.mIntersection.size() != setSize)
			//	throw std::runtime_error("");



			thrd.join();

			for (u64 i = 0; i < numThreads; ++i)
			{
				sendChls[i]->close();// = &sendEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
				recvChls[i]->close();// = &recvEP.addChannel("chl" + std::to_string(i), "chl" + std::to_string(i));
			}
			//sendChl.close();
			//recvChl.close();

			recvEP.stop();
			sendEP.stop();

			ios.stop();


		}



		online << onlineTimeTot / numTrial << "-";
		offline << offlineTimeTot / numTrial << "-";
	}
}
