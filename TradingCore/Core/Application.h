#pragma once

#include "LiftCycle.h"

class Application : public LiftCycle
{
public:
	static Application& GetInstance();

	void Initialize() override;

private:
	Application();
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
};

