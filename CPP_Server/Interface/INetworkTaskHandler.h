#pragma once

#include "Server/PlayerClient.h"


class INetworkTaskHandler
{
public:
	INetworkTaskHandler() = default;
	virtual ~INetworkTaskHandler() {}

	virtual void Receive() = 0;
	virtual bool Join(PlayerPtr player, bool bForceJoin) = 0;
	virtual bool Leave(PlayerPtr player) = 0;
};