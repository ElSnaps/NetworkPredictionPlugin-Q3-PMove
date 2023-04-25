// Copyright Snaps 2022, All Rights Reserved.

#pragma once

class FMesaCoreModule : public IModuleInterface
{
	void StartupModule() override;
	void ShutdownModule() override;
	bool IsGameModule() const override { return true; }
};