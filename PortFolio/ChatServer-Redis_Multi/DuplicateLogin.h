#pragma once
#include <unordered_map>
class GameServer;
namespace DuplicateLogin
{
	void Init();
	void PushPlayerToLoginMapAndDisconnectIfSameAccountLoggedIn(INT64 accountNo, ULONGLONG sessionID);
	void RemoveFromLoginPlayerMapWhenLogOut(INT64 accountNo,ULONGLONG sessionID);
}
