#define enableChatRestoration
#define enableGamestateRestoration

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <stack>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "utils.h"

int rewindTime = 1000;
int rewindOneButton = VK_RCONTROL;
int rewindAllButton = VK_RMENU;
int ejectDllButton = VK_RSHIFT;

typedef int(_cdecl* FS_Read)(void* buffer, int len, int f);
FS_Read FS_ReadOrg;

std::ifstream file;
std::stack<customSnapshot> frameData;
std::array<int, 20> serverTimes;
uint32_t fileStreamMode = notInitialized;
uint32_t rewindMode = notRewinding;
uint32_t snapshotCount = 0;
uint32_t countBuffer = 0;

static_assert(sizeof(size_t) == sizeof(uint32_t), "This code is meant for 32 bit!");

void CL_FirstSnapshotWrapper()
{
	int clientNum = *reinterpret_cast<int*>(0x98FCE0);
	DWORD CL_FirstSnapshot = 0x486AA0;

	Utils::WriteBytes(0x486AF2, "90 90 90 90 90", true); // to avoid some division by 0 bug

	_asm 
	{
		pushad
		mov eax, clientNum
		call CL_FirstSnapshot
		popad
	}

	Utils::Re_StoreBytesWrapper(0x486AF2, 0, restoreOneAddress);
}

void ResetOldClientData(int serverTime)
{
	*reinterpret_cast<int*>(0xF44898) = serverTime;	// cl.snap.serverTime
	*reinterpret_cast<int*>(0xF48274) = 0;			// cl.serverTime
	*reinterpret_cast<int*>(0xF48278) = 0;			// cl.oldServerTime
	*reinterpret_cast<int*>(0xF4827C) = 0;			// cl.oldFrameServerTime
	*reinterpret_cast<int*>(0xF48280) = 0;			// cl.serverTimeDelta

	*reinterpret_cast<int*>(0xBB154C) = 0;			// clc.timeDemoFrames
	*reinterpret_cast<int*>(0xBB1550) = 0;			// clc.timeDemoStart
	*reinterpret_cast<int*>(0xBB1554) = 0;			// clc.timeDemoBaseTime
	*reinterpret_cast<int*>(0xBD3628) = 0;			// cls.realtime
	*reinterpret_cast<int*>(0xBD362C) = 0;			// cls.realFrametime

	*reinterpret_cast<int*>(0x98B718) = 0;			// cg_t -> processedSnapshotNum
	*reinterpret_cast<int*>(0x98FCF8) = 0;			// cg_t -> latestSnapshotNum
	*reinterpret_cast<int*>(0x98FCFC) = 0;			// cg_t -> latestSnapshotTime
	*reinterpret_cast<int*>(0x98FD00) = 0;			// cg_t -> snap
	*reinterpret_cast<int*>(0x98FD04) = 0;			// cg_t -> nextSnap
}

void RestoreOldGamestate()
{
	if (frameData.size() > 1) {
		frameData.pop();

		if (rewindMode == rewindAll) {
			while (frameData.size() > 1)
				frameData.pop();
		}

		rewindMode = notRewinding;
		serverTimes = {};
		countBuffer = frameData.top().fileOffset;
		file.seekg(countBuffer);

		*reinterpret_cast<int*>(0xB6AC80) = 0;										// clean mini console / kill feed
		memset(reinterpret_cast<char*>(0x8FE808), 0, 64 * 60);						// uav data; 64 players, 60 bytes per player

		ResetOldClientData(frameData.top().serverTime);
		CL_FirstSnapshotWrapper();

		*reinterpret_cast<int*>(0x9E6760) = frameData.top().landTime;				// needed to prevent the player's viewmodel from disappearing off screen 

		*reinterpret_cast<int*>(0xF6B294) = frameData.top().parseEntitiesNum;
		*reinterpret_cast<int*>(0xF6B298) = frameData.top().parseClientsNum;
		memcpy(reinterpret_cast<char*>(0xFAFABC), &frameData.top().snapshots, sizeof(customSnapshot::snapshots));
		memcpy(reinterpret_cast<char*>(0x106A6BC), &frameData.top().entities, sizeof(customSnapshot::entities));
		memcpy(reinterpret_cast<char*>(0x10F86BC), &frameData.top().clients, sizeof(customSnapshot::clients));

#ifdef enableChatRestoration
		*reinterpret_cast<int*>(0x9EE01C) = frameData.top().axisScore;
		*reinterpret_cast<int*>(0x9EE020) = frameData.top().alliesScore;
		*reinterpret_cast<int*>(0xB914D4) = frameData.top().serverCommandSequence1;
		*reinterpret_cast<int*>(0x98B714) = frameData.top().serverCommandSequence2;
		memcpy(reinterpret_cast<char*>(0x98C7A4), &frameData.top().chat, sizeof(customSnapshot::chat));
#endif
#ifdef enableGamestateRestoration
		memcpy(reinterpret_cast<char*>(0xF48290), &frameData.top().gameState, sizeof(customSnapshot::gameState));
#endif
	}
}

void StoreCurrentGamestate(int fps)
{
	// this function is executed in a hook that precedes snapshot parsing; so the data must be stored in the most recent entry on the std::stack (frameData)
	if ((fps && snapshotCount > 4 && (snapshotCount - 1) % (rewindTime / fps) == 0) || snapshotCount == 4) {
		if (frameData.size() && frameData.top().fileOffset && !frameData.top().serverTime) {
			frameData.top().serverTime = *reinterpret_cast<int*>(0xF44898);
			frameData.top().landTime = *reinterpret_cast<int*>(0x9E6760);

			frameData.top().parseEntitiesNum = *reinterpret_cast<int*>(0xF6B294);
			frameData.top().parseClientsNum = *reinterpret_cast<int*>(0xF6B298);
			memcpy(&frameData.top().snapshots, reinterpret_cast<char*>(0xFAFABC), sizeof(customSnapshot::snapshots));
			memcpy(&frameData.top().entities, reinterpret_cast<char*>(0x106A6BC), sizeof(customSnapshot::entities));
			memcpy(&frameData.top().clients, reinterpret_cast<char*>(0x10F86BC), sizeof(customSnapshot::clients));

#ifdef enableChatRestoration
			frameData.top().axisScore = *reinterpret_cast<int*>(0x9EE01C);
			frameData.top().alliesScore = *reinterpret_cast<int*>(0x9EE020);
			frameData.top().serverCommandSequence1 = *reinterpret_cast<int*>(0xB914D4);
			frameData.top().serverCommandSequence2 = *reinterpret_cast<int*>(0x98B714);
			memcpy(&frameData.top().chat, reinterpret_cast<char*>(0x98C7A4), sizeof(customSnapshot::chat));
#endif
#ifdef enableGamestateRestoration
			memcpy(&frameData.top().gameState, reinterpret_cast<char*>(0xF48290), sizeof(customSnapshot::gameState));
#endif
		}
	}

	// don't start storing snapshots before the third one to avoid some glitch with the chat (some old chat would be restored with the first snapshot)
	if ((fps && snapshotCount > 3 && snapshotCount % (rewindTime / fps) == 0) || snapshotCount == 3) { 
		frameData.emplace();
		frameData.top().fileOffset = countBuffer - 9; // to account for message type (byte), server message index (int) and message size (int)
	}
}

int DetermineFramerate(int serverTime)
{
	// this function stores a small number of server times, and calculates the server framerate
	if (!serverTime) 
		return 0;

	if (serverTimes[(snapshotCount + serverTimes.size() - 1) % serverTimes.size()] > serverTime) {
		serverTimes = {};
		serverTimes[snapshotCount % serverTimes.size()] = serverTime;

		return 0;
	}
	else {
		serverTimes[snapshotCount % serverTimes.size()] = serverTime;
		std::unordered_map<int, int> frequency;

		for (int i = snapshotCount; i < static_cast<int>(snapshotCount + serverTimes.size()); ++i) {
			int temp = serverTimes[i % serverTimes.size()] - serverTimes[((i + serverTimes.size() - 1) % serverTimes.size())];

			if (temp >= 0 && temp <= 1000) {
				//if (temp % 50 == 0)
					frequency[temp]++;
			}
		}

		auto highestFrequency = std::max_element(
			std::begin(frequency), std::end(frequency), [](const auto& a, const auto& b) {
				return a.second < b.second;
			});

		return highestFrequency->first;
	}
}

bool SetupFileStream(char* fileName)
{
	std::string filePath = fileName;

	if (filePath.length() < 2)
		return false;

	if (filePath[1] != ':') { // full path should be something like 'X:\'
		filePath = reinterpret_cast<char*>(0xF66B944); // %localappdata%\Activision\CoDWaW
		filePath += "\\players\\" + static_cast<std::string>(fileName);
	}

	file.open(filePath.c_str(), std::ios::binary);

	return file.is_open();
}

int hkFS_Read(void* buffer, int len, int f)
{
	fileHandleData_t fh = *reinterpret_cast<fileHandleData_t*>(0xF369A50 + f * sizeof(fileHandleData_t));
	bool foundDemoFile = false;

#if __cplusplus >= 202002L
	{
		std::string_view sv = fh.name;
		foundDemoFile = sv.ends_with(".dm_6");
	}
#else
	foundDemoFile = strstr(fh.name, ".dm_6");
#endif

	if (foundDemoFile) {
		if (fileStreamMode == notInitialized) {
			if (!SetupFileStream(fh.name))
				fileStreamMode = failedInitialization;
			else
				fileStreamMode = fullyInitialized;
		}

		if (fileStreamMode != fullyInitialized)
			return FS_ReadOrg(buffer, len, f);

		if (len == 1 && rewindMode) // only reset when the game has just requested the one byte message type
			RestoreOldGamestate();
		else if (len > 12) { // to exclude client archives
			snapshotCount++;
			StoreCurrentGamestate(DetermineFramerate(*reinterpret_cast<int*>(0x114A8070)));
		}

		file.read(reinterpret_cast<char*>(buffer), len);
		countBuffer += len;

		assert(countBuffer == file.tellg());
		return len;	
	}
	else
		return FS_ReadOrg(buffer, len, f);
}

void WaitForInput()
{
	DWORD demoPlaying = 0xBB1528;

	while (true) {
		if (!*reinterpret_cast<byte*>(demoPlaying) && fileStreamMode == fullyInitialized) {
			fileStreamMode = notInitialized;

			rewindMode = notRewinding;
			snapshotCount = 0;
			countBuffer = 0;
			serverTimes = {};
			frameData = {};

			file.close();
		}

		if ((GetAsyncKeyState(rewindOneButton) & 1)) 
			rewindMode = rewindOne;
		else if ((GetAsyncKeyState(rewindAllButton) & 1))
			rewindMode = rewindAll;
		else if ((GetAsyncKeyState(ejectDllButton) & 1)) {
			MessageBeep(0);
			Utils::Re_StoreBytesWrapper(0, 0, restoreAllAddresses);

			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

VOID APIENTRY MainThread(HMODULE hModule)
{
	Utils::WriteBytes(0x4722E3, "EB", true); // bypass error: WARNING: CG_ReadNextSnapshot: way out of range, %i > %i\n
	{
		// these shouldn't be necessary anymore!
		Utils::WriteBytes(0x4724A6, "EB", true); // bypass error: CG_ProcessSnapshots: Server time went backwards
		Utils::WriteBytes(0x486DF1, "90 90", true); // bypass error: cl->snap.serverTime < cl->oldFrameServerTime
		Utils::WriteBytes(0x48570A, "EB", true); // bypass error: CL_CGameNeedsServerCommand: EXE_ERR_NOT_RECEIVED
	}

	DWORD FS_ReadAddress = 0x5B2780;
	FS_ReadOrg = static_cast<FS_Read>(Utils::TrampolineHook(reinterpret_cast<byte*>(FS_ReadAddress), reinterpret_cast<byte*>(&hkFS_Read), 7, true)); 

	WaitForInput();

	FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainThread), hModule, 0, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}