// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <stdint.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <random>
#include <ctime>
#include <set>
#include <map>
#include <iomanip>

struct Rng {
public:
    uint32_t seed;
    int32_t shift1;
    int32_t shift2;
    int32_t shift3;
    uint32_t next() {
        auto num = seed;
        num ^= num >> shift1;
        num ^= num << shift2;
        num ^= num >> shift3;
        seed = num;
        return num;
    }
};

struct RoomDescription {
    int32_t stageIndex; //This is the number that is at the start of the STB files.
    int32_t roomType;
    int32_t roomId;
    int32_t roomSubType;
    char data[0x34];
    //RoomType @ 0x4
    //RoomId @ 0x8
    //SubType @ 0xC
    //Difficulty @ 0x28
    //Weight @ 0x2c
    //Doors @ 0x34
    //Width @ 0x3e
    //Height @ 0x3f
    //RoomShape @ 0x40
};
struct Room {
    //0xC0 bytes
    int32_t gridIndex;
    int32_t safeGridIndex;
    int32_t listIndex;
    RoomDescription* roomDesc;
    //int32_t d;
    //int32_t DoorsFlag; //0xF = all doors enabled
    //int32_t ConnectedRoomsIndex[8]; WN, NW, EN, SW, WS, NE, ES, SE

    char data[0xb0];
};
struct RoomList {
public:
    int32_t roomCount;
    Room* roomsData;
};



class CGame {
public:
    int32_t Stage;
    int32_t StageType;
    char padding1[0xC];
    //The rooms are placed on a 13x13 grid(169). However there is a max of 138 rooms in the grid.
    //The last 9 rooms are reserved by the negative room indexes which aren't in the grid. (E.g. I Am Error, black market, etc)
    Room Rooms[138]; //@14
    int32_t LevelRoomIndexes[169]; //@0x6794
    int32_t RoomCount; //@0x6a38
    char padding2[0x354C];
    uint32_t StartSeed; //@ 0x9f88
    Rng StartRng; //@ 0x9f8C
    uint32_t StageSeeds[13]; //@ 0x9f9c
};

typedef void(CGame::*GENLEVEL)();
typedef void(CGame::*LEVELUPDATE)();
typedef void(CGame::*LEVELSETNEXTSTAGE)();
typedef RoomList(CGame::*GETROOMS)();

int32_t UpdateCallerAddr = 0x0063911F;
int32_t UpdateAddr = 0x006533C0;

//int32_t IsaacGenLevelAddr = 0x053F2F0;
int32_t IsaacGenStageAddr = 0x005409C0;
int32_t GameAddr = (int32_t)0x934e98;
int32_t GlobalsAddr = 0x00934EA4;
int32_t GetRoomsAddr = 0x0053F2D0;
int32_t LevelUpdateAddr = 0x00539410;
int32_t LevelSetNextStageAddr = 0x00541D00; //Level::SetNextStage

int32_t IsSecretUnlockedAddr = 0x00634730;

int32_t MallocIATAddr = 0x00771404;
int32_t FreeIATAddr = 0x007713FC;

int32_t SecretCount = 339;



GENLEVEL IsaacGenLevel = *(GENLEVEL*)&IsaacGenStageAddr;
GETROOMS GetRooms = *(GETROOMS*)&GetRoomsAddr;
LEVELUPDATE LevelUpdate = *(LEVELUPDATE*)&LevelUpdateAddr;
LEVELSETNEXTSTAGE LevelSetNextStage = *(LEVELSETNEXTSTAGE*)&LevelSetNextStageAddr;

CGame* GetGame() {
    return (CGame*)(*(int32_t*)GameAddr);
}

void WriteRoom(Room& room, FILE* fp) {
    if (room.roomDesc != nullptr) 
    {
        auto roomType = room.roomDesc->roomType;
        auto roomId = room.roomDesc->roomId;
        auto roomSubType = room.roomDesc->roomSubType;
        auto stageIdx = room.roomDesc->stageIndex;
        if (roomType > 0xff || roomId > 0xFFFF || roomSubType > 0xff || stageIdx > 0xFF)
            throw std::exception("Cannot encode room");
        auto roomTypeB = (uint8_t)roomType;
        auto roomIdS = (uint16_t)roomId;
        auto roomSubTypeB = (uint8_t)roomSubType;
        auto stageIdxB = (uint8_t)stageIdx;
        fwrite(&stageIdxB, 1, 1, fp);
        fwrite(&roomTypeB, 1, 1, fp);
        fwrite(&roomSubTypeB, 1, 1, fp);
        fwrite(&roomIdS, 2, 1, fp);
    } 
    else
    {
        uint8_t errorByte = 0xFF;
        uint16_t errorShort = 0xFFFF;
        fwrite(&errorByte, 1, 1, fp);
        fwrite(&errorByte, 1, 1, fp);
        fwrite(&errorByte, 1, 1, fp);
        fwrite(&errorShort, 2, 1, fp);
    }
}


void WriteStage(FILE* fp) {
    auto game = GetGame();
    auto roomList = (game->*GetRooms)();
    //fwrite((LPVOID)&game->StageSeeds[game->Stage], sizeof(int32_t), 1, fp);
    auto stage = (uint8_t)game->Stage;
    auto stageType = (uint8_t)game->StageType;
    auto roomcount = (uint8_t)roomList.roomCount;
    fwrite((LPVOID)&stage, sizeof(stage), 1, fp);
    fwrite((LPVOID)&stageType, sizeof(stageType), 1, fp);
    fwrite((LPVOID)&roomcount, sizeof(roomcount), 1, fp);

    uint8_t gridCount = 0;
    for (auto i = 0; i < 169; i++)
        if (game->LevelRoomIndexes[i] != -1)
            gridCount++;
    fwrite((LPVOID)&gridCount, sizeof(gridCount), 1, fp);

    uint32_t startCount = 0;

    for (uint8_t y = 0; y < 13; y++) {
        for (uint8_t x = 0; x < 13; x++) {
            auto i = x + y * 13;
            auto roomOffset = game->LevelRoomIndexes[i];
            if (roomOffset != -1) {
                auto offsetByte = (uint8_t)roomOffset;
                fwrite(&offsetByte, 1, 1, fp);
                fwrite(&x, 1, 1, fp);
                fwrite(&y, 1, 1, fp);
                auto room = game->Rooms[roomOffset];
                WriteRoom(room, fp);

            }
        }
    }
    /*
    GridRooms {
    MAX_GRID_ROOMS = 128, ROOM_DEVIL_IDX = -1, ROOM_ERROR_IDX = -2, ROOM_DEBUG_IDX = -3,
    ROOM_DUNGEON_IDX = -4, ROOM_BOSSRUSH_IDX = -5, ROOM_BLACK_MARKET_IDX = -6, ROOM_MEGA_SATAN_IDX = -7,
    ROOM_BLUE_WOOM_IDX = -8, ROOM_THE_VOID_IDX = -9, NUM_OFF_GRID_ROOMS = 9, MAX_ROOMS = 137
    }
    */
    /*
    GRIDINDEX_I_AM_ERROR = -2,
    GRIDINDEX_CRAWLSPACE = -4,
    GRIDINDEX_BOSS_RUSH = -5,
    GRIDINDEX_BLACK_MARKET = -6,
    GRIDINDEX_MEGA_SATAN = -7,
    GRIDINDEX_BLUE_WOMB_PORTAL = -8,
    */
    //Special rooms start at 128;
    auto iAmError = game->Rooms[127 + 2];
    WriteRoom(iAmError, fp);

    auto crawlSpace = game->Rooms[127 + 4];
    WriteRoom(crawlSpace, fp);

    auto blackMarket = game->Rooms[127 + 6];
    WriteRoom(blackMarket, fp);

    if (game->Stage == 6) {
        auto bossRush = game->Rooms[127 + 5];
        WriteRoom(bossRush, fp);
    }
    /*if (game->Stage == 11) {
        auto megaSatan = game->Rooms[127 + 7];
        auto megaSatanEnc = EncodeRoom(megaSatan.roomDesc->roomType, megaSatan.roomDesc->roomId, megaSatan.roomDesc->roomSubType);
        fwrite(&megaSatanEnc, sizeof(megaSatanEnc), 1, fp);
    }*/
}

struct CallStack
{
    uint32_t Addresses[10];
};

std::map<uint32_t, CallStack> leak;



template< typename T >
std::string int_to_hex(T i)
{
    std::stringstream stream;
    stream << "0x"
        << std::setfill('0') << std::setw(sizeof(T) * 2)
        << std::hex << i;
    return stream.str();
}
void dump() {
    OutputDebugStringA("Dumping memory leak info");
    auto i = 0;
    for (auto itr = leak.begin(); itr != leak.end(); ++itr)
    {
        if (i++ == 5)
            break;
        std::stringstream fn;
        fn << "Leaked Ptr: " << (uint32_t)itr->first << " caller:";
        for (auto i = 0; i < 10; i++)
        {
            if (itr->second.Addresses[i] != 0)
                fn << " " << int_to_hex(itr->second.Addresses[i]);
        }
        OutputDebugStringA(fn.str().c_str());
    }
}
typedef void* (__cdecl*MALLOC)(size_t);
typedef void(__cdecl*FREE)(void*);
MALLOC origMalloc;
FREE origFree;
MALLOC trampMalloc;
FREE trampFree;

bool isRecording = true;

void* __cdecl myMalloc(size_t size) {
    uint32_t frame;
    __asm {
        mov frame, ebp
    }
    uint32_t callstack[10];
    memset((LPVOID)&callstack, 0, 40);
    for (auto i = 0; i < 10 && frame != 0; i++) {
        callstack[i] = *(uint32_t*)(frame + 4);;
        frame = *(uint32_t*)(frame);
    }


    auto ptr = trampMalloc(size);
    if (ptr > 0) {
        CallStack cs;
        memcpy((LPVOID)&cs.Addresses, (LPVOID)&callstack, 10 * 4);
        if (isRecording)
            leak.insert(std::make_pair((uint32_t)ptr, cs));
    }
    else if (ptr == nullptr)
    {
        dump();
    }
    return ptr;
}
void tryCleanup() {
    /*for (auto itr = leak.begin(); itr != leak.end(); ++itr)
    {
       itr->second = itr->second + 1;
    }

    for (auto itr = leak.cbegin(); itr != leak.cend(); )
    {
        if (itr->second > 10)
        {
            trampFree((void*)itr->first);
            leak.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }*/
}
void __cdecl myFree(void* ptr) {
    trampFree(ptr);
    leak.erase((uint32_t)ptr);
}



void hook() {
    auto mem = (int32_t)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    origMalloc = (MALLOC)*(uint32_t*)MallocIATAddr;
    origFree = (FREE)*(uint32_t*)FreeIATAddr;

    //55 8B EC 56 8B 75 08
    //55 8B EC 83 7D 08 00
    char dt1[] = { 0x55, 0x8B, 0xEC ,0x56 ,0x8B ,0x75 ,0x08 };
    memcpy((void*)mem, dt1, 7);
    *(int8_t*)(mem + 7) = 0xE9;
    *(int32_t*)(mem + 8) = ((uint32_t)origMalloc + 7) - (mem + 12);

    char dt2[] = { 0x55 ,0x8B ,0xEC ,0x83 ,0x7D ,0x08 ,0x00 };
    memcpy((void*)(mem + 12), dt2, 7);
    *(int8_t*)(mem + 19) = 0xE9;
    *(int32_t*)(mem + 20) = ((uint32_t)origFree + 7) - (mem + 24);

    trampMalloc = (MALLOC)mem;
    trampFree = (FREE)(mem + 12);

    DWORD old;
    VirtualProtect((LPVOID)origMalloc, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int8_t*)origMalloc = 0xE9;
    *(int32_t*)((uint32_t)origMalloc + 1) = (uint32_t)&myMalloc - ((uint32_t)origMalloc + 5);
    *(int16_t*)((uint32_t)origMalloc + 5) = 0x9090;
    VirtualProtect((LPVOID)origFree, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int8_t*)(origFree) = 0xE9;
    *(int32_t*)((uint32_t)origFree + 1) = (uint32_t)&myFree - ((uint32_t)origFree + 5);
    *(int16_t*)((uint32_t)origFree + 5) = 0x9090;
}

void EnableAllSecrets() {
    DWORD old;
    VirtualProtect((LPVOID)IsSecretUnlockedAddr, 5, PAGE_EXECUTE_READWRITE, &old);

    char patch[] = { 0xB0, 0x01, 0xC2, 0x04, 0x00 };
    memcpy((LPVOID)IsSecretUnlockedAddr, patch, 5);

    auto globals = *(int32_t*)GlobalsAddr;
    auto secretsPtr = globals + 0x45;
    for (auto i = 0; i < SecretCount; i++)
    {
        *(char*)(secretsPtr + i) = 1;
    }

}

uint32_t GetStageType(uint32_t stage, uint32_t stageSeed)
{
    //Assume we have everything unlocked
    uint32_t stageType = 0;
    if ((stageSeed & 1) == 0)
        stageType = 1;
    if (stage < 10 && (stageSeed % 3) == 0)
        stageType = 2;

    //Note: The stageType of the void depends on which room you came from or the Game::GetStageFlag. E.g. coming from the hush is always stageType 0.

    return stageType;
}

bool IsStageLoaded()
{
    auto globals = *(int32_t*)GlobalsAddr;
    //At this point we will still crash due to transitioning
    //Todo: Investigate using "0051AB0A | 83 BE 48 81 00 00 00     | cmp     dword ptr ds:[esi+8148], 0                               | room transition state? (1 = started, 3 = finished)"
    return *(int32_t*)globals == 2 && *(int32_t*)(globals + 0x6e5cc) == 1;
}

bool runOnce = false;
void Update() {
    auto game = GetGame();
    if (!runOnce && game != nullptr && IsStageLoaded()) {
        //
        std::mt19937 mt_rand(std::time(0));

        std::stringstream fn;
        fn << "dump\\layouts_" << mt_rand() << ".bin";
        auto fp = fopen(fn.str().c_str(), "wb");

        //hook();

        EnableAllSecrets();

        for (auto itr = 0; itr < 3000; itr++) {
            if ((itr % 5) == 0) {
                std::stringstream ss;
                ss << "Progress: " << itr;
                //OutputDebugStringA(ss.str().c_str());
            }

            auto startSeed = (uint32_t)mt_rand();
            auto startRng = Rng{ startSeed, 3, 0x17, 0x19 };
            game->StartSeed = startSeed;
            game->StartRng = startRng;
            for (auto i = 0; i < 13; i++)
                game->StageSeeds[i] = startRng.next();

            fwrite(&startSeed, sizeof(startSeed), 1, fp);

            for (auto i = 1; i < 10; i++) {
                game->Stage = i;
                game->StageType = GetStageType(i, game->StageSeeds[i]);
                (game->*IsaacGenLevel)();
                (game->*LevelUpdate)();

                WriteStage(fp);

            }
            for (auto i = 10; i < 12; i++) {
                for (auto j = 0; j < 2; j++) {
                    game->Stage = i;
                    game->StageType = j;
                    (game->*IsaacGenLevel)();
                    (game->*LevelUpdate)();

                    WriteStage(fp);
                }
            }

            game->Stage = 12;
            game->StageType = GetStageType(12, game->StageSeeds[12]);
            (game->*IsaacGenLevel)();
            (game->*LevelUpdate)();

            WriteStage(fp);

            fflush(fp);
        }
        fclose(fp);
        runOnce = true;
        //dump();
        TerminateProcess(GetCurrentProcess(), 0);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DWORD old;
        auto mem = (int32_t)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        *(int8_t*)mem = 0xb8;
        *(int32_t*)(mem + 1) = (int32_t)&Update;
        *(int16_t*)(mem + 5) = 0xd0ff;

        *(int8_t*)(mem + 7) = 0xb8;
        *(int32_t*)(mem + 8) = UpdateAddr;
        *(int16_t*)(mem + 12) = 0xd0ff;

        *(int8_t*)(mem + 14) = 0xb8;
        *(int32_t*)(mem + 15) = UpdateCallerAddr + 5;
        *(int16_t*)(mem + 19) = 0xe0ff;

        VirtualProtect((LPVOID)UpdateCallerAddr, 5, PAGE_EXECUTE_READWRITE, &old);
        *(int8_t*)UpdateCallerAddr = 0xE9;
        *(int32_t*)(UpdateCallerAddr + 1) = mem - (UpdateCallerAddr + 5);


        break;
    }
    return TRUE;
}
