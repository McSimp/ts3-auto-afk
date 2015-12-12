#include "plugin.hpp"
#include <iostream>
#include <thread>
#include <Windows.h>

static struct TS3Functions ts3;
static bool shouldExitMonitorThread;
static std::thread idleMonitorThread;
static DWORD secondsForIdle;
static bool isSetToAFK;

static bool shouldRestoreAFK;
static bool shouldRestoreMuteSound;
static bool shouldRestoreMuteMic;

const char* ts3plugin_name()
{
	return "Auto AFK Status";
}

const char* ts3plugin_version()
{
    return "1.1";
}

int ts3plugin_apiVersion()
{
	return PLUGIN_API_VERSION;
}

const char* ts3plugin_author()
{
    return "McSimp";
}

const char* ts3plugin_description()
{
    return "This plugin will automatically set your status to AFK, mute your sound, and mute your microphone if you don't move your mouse or press any keys for 10 minutes.";
}

void ts3plugin_setFunctionPointers(const struct TS3Functions funcs)
{
    ts3 = funcs;
}

void setToAFK(uint64 serverID)
{
	// When we're setting the person as AFK, if they already have some kind of status
	// set on their mic (muted for example), then don't unmute it when they come back.
	int micMuted;
	ts3.getClientSelfVariableAsInt(serverID, CLIENT_INPUT_MUTED, &micMuted);

	if(micMuted != MUTEINPUT_NONE)
	{
		shouldRestoreMuteMic = false;
	}
	else
	{
		shouldRestoreMuteMic = true;
		ts3.setClientSelfVariableAsInt(serverID, CLIENT_INPUT_MUTED, MUTEINPUT_MUTED);
	}

	// Same goes for speakers
	int outMuted;
	ts3.getClientSelfVariableAsInt(serverID, CLIENT_OUTPUT_MUTED, &outMuted);

	if(outMuted != MUTEOUTPUT_NONE)
	{
		shouldRestoreMuteSound = false;
	}
	else
	{
		shouldRestoreMuteSound = true;
		ts3.setClientSelfVariableAsInt(serverID, CLIENT_OUTPUT_MUTED, MUTEOUTPUT_MUTED);
	}

	// And AFK status
	int afkStatus;
	ts3.getClientSelfVariableAsInt(serverID, CLIENT_AWAY, &afkStatus);

	if(afkStatus != AWAY_NONE)
	{
		shouldRestoreAFK = false;
	}
	else
	{
		shouldRestoreAFK = true;
		ts3.setClientSelfVariableAsInt(serverID, CLIENT_AWAY, AWAY_ZZZ);
	}

	ts3.flushClientSelfUpdates(serverID, NULL);

	//std::cout << "setTOAFK: " << shouldRestoreAFK << shouldRestoreMuteMic << shouldRestoreMuteSound << std::endl; 
}

void setBack(uint64 serverID)
{
	// Set AFK status
	if(shouldRestoreAFK)
	{
		shouldRestoreAFK = false;
		ts3.setClientSelfVariableAsInt(serverID, CLIENT_AWAY, AWAY_NONE);
	}

	// Microphone toggle
	if(shouldRestoreMuteMic)
	{
		shouldRestoreMuteMic = false;
		ts3.setClientSelfVariableAsInt(serverID, CLIENT_INPUT_MUTED, MUTEINPUT_NONE);
	}

	// Speaker toggle
	if(shouldRestoreMuteSound)
	{
		shouldRestoreMuteSound = false;
		ts3.setClientSelfVariableAsInt(serverID, CLIENT_OUTPUT_MUTED, MUTEOUTPUT_NONE);
	}
			
	ts3.flushClientSelfUpdates(serverID, NULL);
}

void toggleAFK(bool isAFK)
{
	uint64* ids;

    if(ts3.getServerConnectionHandlerList(&ids) != ERROR_ok)
	{
        ts3.logMessage("[AutoAFK] Error retrieving server list - cannot set AFK", LogLevel_ERROR, "Plugin", 0);
        return;
    }

	// Foreach connection
    for(int i = 0; ids[i]; i++) 
	{
		uint64 serverID = ids[i];
        anyID clid;
        if(ts3.getClientID(serverID, &clid) == ERROR_ok)
		{
			if(isAFK)
			{
				setToAFK(serverID);
			}
			else
			{
				setBack(serverID);
			}
        }
    }

    ts3.freeMemory(ids);
	isSetToAFK = isAFK;
}

void idleWatcher()
{
	LASTINPUTINFO lastInput;
	lastInput.cbSize = sizeof(LASTINPUTINFO);

	std::chrono::milliseconds measureInterval(1000);
	std::chrono::milliseconds loopTime(500);
	auto start = std::chrono::steady_clock::now();

	while(!shouldExitMonitorThread)
	{
		if((std::chrono::steady_clock::now() - start) > measureInterval)
		{
			if(GetLastInputInfo(&lastInput))
			{
				DWORD awayTimeSecs = ((GetTickCount() - lastInput.dwTime)/1000);
				if(awayTimeSecs > secondsForIdle)
				{
					if(!isSetToAFK)
						toggleAFK(true);
				}
				else if(isSetToAFK)
				{
					toggleAFK(false);
				}
			}
			start = std::chrono::steady_clock::now();
		}

		std::this_thread::sleep_for(loopTime);
	}
}

int ts3plugin_requestAutoload()
{
    return 1;
}

int ts3plugin_init()
{
	shouldExitMonitorThread = false;
	secondsForIdle = 10 * 60;
	idleMonitorThread = std::thread(idleWatcher);
	return 0;
}

void ts3plugin_shutdown()
{
	shouldExitMonitorThread = true;
	idleMonitorThread.join();
}
