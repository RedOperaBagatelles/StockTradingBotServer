#include "Application.h"
#include "Log.h"

Application& Application::GetInstance()
{
	static Application instance;

	return instance;
}

void Application::Initialize()
{
	LiftCycle::Initialize();
	Log::GetInstance();
}
