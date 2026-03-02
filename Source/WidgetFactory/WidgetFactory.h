// WidgetFactory.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FWidgetFactoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	void RegisterMenu();
};
