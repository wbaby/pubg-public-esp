#include "DirectOverlay.h"

uint64_t GNamesAddress;
uint64_t Tmpadd;

#define DecryptData(argv)    fnDecryptFunctoin(Tmpadd, argv)

typedef int64_t(__fastcall* DecryptFunctoin)(uintptr_t key, uintptr_t argv);
DecryptFunctoin fnDecryptFunctoin = NULL;

void Decrypt() {
	int64_t DecryptPtr = read<uint64_t>(DriverHandle, processID, base_address + offs_decrypt);

	while (!DecryptPtr)
	{
		DecryptPtr = read<uint64_t>(DriverHandle, processID, base_address + offs_decrypt);
		Sleep(1000);
	}

	int32_t Tmp1Add = read<uint32_t>(DriverHandle, processID, DecryptPtr + 3);
	Tmpadd = Tmp1Add + DecryptPtr + 7;

	unsigned char ShellcodeBuff[1024] = { NULL };
	ShellcodeBuff[0] = 0x90;
	ShellcodeBuff[1] = 0x90;

	info_t blyat;
	blyat.pid = processID;
	blyat.address = DecryptPtr;
	blyat.value = &ShellcodeBuff[2];
	blyat.size = sizeof(ShellcodeBuff) - 2;

	unsigned long int asd;
	DeviceIoControl(DriverHandle, ctl_read, &blyat, sizeof blyat, &blyat, sizeof blyat, &asd, nullptr);

	ShellcodeBuff[2] = 0x48;
	ShellcodeBuff[3] = 0x8B;
	ShellcodeBuff[4] = 0xC1;
	ShellcodeBuff[5] = 0x90;
	ShellcodeBuff[6] = 0x90;
	ShellcodeBuff[7] = 0x90;
	ShellcodeBuff[8] = 0x90;

	fnDecryptFunctoin = (DecryptFunctoin)VirtualAlloc(NULL, sizeof(ShellcodeBuff) + 4, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	RtlCopyMemory((LPVOID)fnDecryptFunctoin, (LPVOID)ShellcodeBuff, sizeof(ShellcodeBuff));
}

DWORD DecryptCIndex(DWORD value)
{
	return __ROR4__(value ^ 0xC53E7758, 0xA) ^ (__ROR4__(value ^ 0xC53E7758, 0xA) << 16) ^ 0x20153634;
}

std::string GetGNamesByObjID(int32_t ObjectID)
{
	int64_t fNamePtr = read<uint64_t>(DriverHandle, processID, GNamesAddress + int(ObjectID / offs_chunk) * 0x8);
	int64_t fName = read<uint64_t>(DriverHandle, processID, fNamePtr + int(ObjectID % offs_chunk) * 0x8);

	char pBuffer[64] = { NULL };

	info_t blyat;
	blyat.pid = processID;
	blyat.address = fName + 0x10;
	blyat.value = &pBuffer;
	blyat.size = sizeof(pBuffer);

	unsigned long int asd;
	DeviceIoControl(DriverHandle, ctl_read, &blyat, sizeof blyat, &blyat, sizeof blyat, &asd, nullptr);

	return std::string(pBuffer);
}

float GetActorHealth(uint64_t Actor)
{
	return read<float>(DriverHandle, processID, Actor + offs_Actor_Health);
}

Vector3 GetActorPos(DWORD_PTR entity)
{
	Vector3 pos;

	auto rootcomp = DecryptData(read<DWORD_PTR>(DriverHandle, processID, entity + offs_Actor_RootComponent));
	pos = read<Vector3>(DriverHandle, processID, rootcomp + offs_relativepos);

	return pos;
}

void DrawSkeleton(DWORD_PTR mesh)
{
	Vector3 neckpos = GetBoneWithRotation(mesh, EBoneIndex::neck_01);
	Vector3 pelvispos = GetBoneWithRotation(mesh, EBoneIndex::pelvis);
	Vector3 previous(0, 0, 0);
	Vector3 current, p1, c1;

	for (auto a : skeleton)
	{
		previous = Vector3(0, 0, 0);

		for (int bone : a)
		{
			current = bone == EBoneIndex::neck_01 ? neckpos : (bone == EBoneIndex::pelvis ? pelvispos : GetBoneWithRotation(mesh, bone));
			if (previous.x == 0.f)
			{
				previous = current;
				continue;
			}
			p1 = WorldToScreen(previous);
			c1 = WorldToScreen(current);

			DrawLine(p1.x, p1.y, c1.x, c1.y, 2.0f, 255.0f, 0.f, 0.f, 255.0f);
			previous = current;
		}
	}
}

void drawLoop(int width, int height) {
	GNamesAddress = DecryptData(read<uint64_t>(DriverHandle, processID, DecryptData(read<uint64_t>(DriverHandle, processID, base_address + offs_gnames - 0x20))));

	uint64_t pUWorld = DecryptData(read<uint64_t>(DriverHandle, processID, base_address + offs_uworld));
	uint64_t PersistentLevel = DecryptData(read<uint64_t>(DriverHandle, processID, pUWorld + offs_PersistentLevel));
	uint64_t ActorsArray = DecryptData(read<uint64_t>(DriverHandle, processID, PersistentLevel + offs_ActorsArray));
	int32_t ActorCount = read<uint32_t>(DriverHandle, processID, ActorsArray + 0x8);

	localplayer = read<uint64_t>(DriverHandle, processID, base_address + offs_localplayer);
	PlayerController = DecryptData(read<uint64_t>(DriverHandle, processID, localplayer + offs_PlayerControllers));
	LocalPawn = DecryptData(read<uint64_t>(DriverHandle, processID, PlayerController + Offs_LocalPawn));
	//LocalMesh = read<uint64_t>(DriverHandle, processID, LocalPawn + offs_Mesh);
	//AnimScript = read<uint64_t>(DriverHandle, processID, LocalMesh + offs_UANIMINSTANCE);
	PlayerCameraManager = read<uint64_t>(DriverHandle, processID, PlayerController + offs_PlayerCameraManager);

	for (int i = 0; i < ActorCount; i++)
	{
		uint64_t actor = read<uint64_t>(DriverHandle, processID, read<uint64_t>(DriverHandle, processID, ActorsArray) + i * 0x8);

		if (actor == (uint64_t)nullptr)
			continue;

		int32_t actorid = DecryptCIndex(read<uint32_t>(DriverHandle, processID, actor + offs_Actor_ObjID));

		std::string name = GetGNamesByObjID(actorid);

		if (name == "PlayerFemale_A" || name == "PlayerFemale_A_C" || name == "PlayerMale_A" || name == "PlayerMale_A_C")
		{
			int MyTeamId = read<int>(DriverHandle, processID, LocalPawn + offs_Actor_TeamNumber);
			int ActorTeamId = read<int>(DriverHandle, processID, actor + offs_Actor_TeamNumber);

			float health = GetActorHealth(actor);

			if (health > 0.0f)
			{
				if (MyTeamId != ActorTeamId)
				{
					uint64_t CurrentActorMesh = read<uint64_t>(DriverHandle, processID, actor + offs_Mesh);
					Vector3 head = GetBoneWithRotation(CurrentActorMesh, 15);
					Vector3 HeadBox = WorldToScreen(Vector3(head.x, head.y, head.z + 15));
					Vector3 root = GetBoneWithRotation(CurrentActorMesh, 0);
					Vector3 rootw2s = WorldToScreen(root);

					float Height1 = abs(HeadBox.y - rootw2s.y);
					float Width1 = Height1 * 0.65;

					DrawBox(HeadBox.x - (Width1 / 2), HeadBox.y, Width1, Height1, 2.f, 255.0f, 0.f, 0.f, 255.f, false);
					DrawSkeleton(CurrentActorMesh);
				}
			}
		}
	}
}

void main()
{
	DriverHandle = CreateFileW(L"\\\\.\\ucpubg", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (DriverHandle == INVALID_HANDLE_VALUE)
	{
		printf("Load Driver first, exiting...\n");
		Sleep(2000);
		exit(0);
	}

	while (hwnd == NULL)
	{
		hwnd = FindWindowA(0, "PLAYERUNKNOWN'S BATTLEGROUNDS ");
		printf("Looking for process...\n");
		Sleep(100);
	}
	GetWindowThreadProcessId(hwnd, &processID);

	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	info_t Input_Output_Data;
	Input_Output_Data.pid = processID;
	unsigned long int Readed_Bytes_Amount;

	DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
	base_address = (unsigned long long int)Input_Output_Data.data;

	DirectOverlaySetOption(D2DOV_DRAW_FPS | D2DOV_FONT_IMPACT);
	DirectOverlaySetup(drawLoop, FindWindow(NULL, "PLAYERUNKNOWN'S BATTLEGROUNDS "));
	getchar();
}
