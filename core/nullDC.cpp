#ifndef LIBRETRO
#include "types.h"
#include "emulator.h"
#include "hw/mem/_vmem.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#include "oslib/oslib.h"
#include "debug/gdb_server.h"
#include "archive/rzip.h"
#include "rend/mainui.h"
#include "input/gamepad_device.h"
#include "lua/lua.h"
#include "stdclass.h"

int flycast_init(int argc, char* argv[])
{
#if defined(TEST_AUTOMATION)
	setbuf(stdout, 0);
	setbuf(stderr, 0);
	settings.aica.muteAudio = true;
#endif
	if (!_vmem_reserve())
	{
		ERROR_LOG(VMEM, "Failed to alloc mem");
		return -1;
	}
	if (ParseCommandLine(argc, argv))
	{
        return 69;
	}
	config::Settings::instance().reset();
	LogManager::Shutdown();
	if (!cfgOpen())
	{
		LogManager::Init();
		NOTICE_LOG(BOOT, "Config directory is not set. Starting onboarding");
		gui_open_onboarding();
	}
	else
	{
		LogManager::Init();
		config::Settings::instance().load(false);
	}
	// Force the renderer type now since we're not switching
	config::RendererType.commit();

	os_CreateWindow();
	os_SetupInput();

	debugger::init();
	lua::init();

	return 0;
}

void dc_exit()
{
	emu.stop();
	mainui_stop();
}

void SaveSettings()
{
	config::Settings::instance().save();
	GamepadDevice::SaveMaplePorts();

#ifdef __ANDROID__
	void SaveAndroidSettings();
	SaveAndroidSettings();
#endif
}

void flycast_term()
{
	gui_cancel_load();
	lua::term();
	emu.term();
}

void dc_savestate(int index)
{
	unsigned int total_size = 0;
	void *data = nullptr;

	if (!dc_serialize(&data, &total_size))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not initialize total size") ;
		gui_display_notification("Save state failed", 2000);
    	return;
	}

	data = malloc(total_size);
	if (data == nullptr)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not malloc %d bytes", total_size);
		gui_display_notification("Save state failed - memory full", 2000);
    	return;
	}

	void *data_ptr = data;
	total_size = 0;
	if (!dc_serialize(&data_ptr, &total_size))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not serialize data") ;
		gui_display_notification("Save state failed", 2000);
		free(data);
    	return;
	}

	std::string filename = hostfs::getSavestatePath(index, true);
#if 0
	FILE *f = nowide::fopen(filename.c_str(), "wb") ;

	if ( f == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str()) ;
		gui_display_notification("Cannot open save file", 2000);
		free(data);
    	return;
	}

	std::fwrite(data, 1, total_size, f) ;
	std::fclose(f);
#else
	RZipFile zipFile;
	if (!zipFile.Open(filename, true))
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str());
		gui_display_notification("Cannot open save file", 2000);
		free(data);
    	return;
	}
	if (zipFile.Write(data, total_size) != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - error writing %s", filename.c_str());
		gui_display_notification("Error saving state", 2000);
		zipFile.Close();
		free(data);
    	return;
	}
	zipFile.Close();
#endif

	free(data);
	INFO_LOG(SAVESTATE, "Saved state to %s size %d", filename.c_str(), total_size) ;
	gui_display_notification("State saved", 1000);
}

void dc_loadstate(int index)
{
	u32 total_size = 0;
	FILE *f = nullptr;

	emu.stop();

	std::string filename = hostfs::getSavestatePath(index, false);
	RZipFile zipFile;
	if (zipFile.Open(filename, false))
	{
		total_size = (u32)zipFile.Size();
		if (index == -1 && config::GGPOEnable)
		{
			f = zipFile.rawFile();
			long pos = std::ftell(f);
			MD5Sum().add(f)
					.getDigest(settings.network.md5.savestate);
			std::fseek(f, pos, SEEK_SET);
			f = nullptr;
		}
	}
	else
	{
		f = nowide::fopen(filename.c_str(), "rb") ;

		if ( f == NULL )
		{
			WARN_LOG(SAVESTATE, "Failed to load state - could not open %s for reading", filename.c_str()) ;
			gui_display_notification("Save state not found", 2000);
			return;
		}
		if (index == -1 && config::GGPOEnable)
			MD5Sum().add(f)
					.getDigest(settings.network.md5.savestate);
		std::fseek(f, 0, SEEK_END);
		total_size = (u32)std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
	}
	void *data = malloc(total_size);
	if ( data == NULL )
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not malloc %d bytes", total_size) ;
		gui_display_notification("Failed to load state - memory full", 2000);
		if (f != nullptr)
			std::fclose(f);
		else
			zipFile.Close();
		return;
	}

	size_t read_size;
	if (f == nullptr)
	{
		read_size = zipFile.Read(data, total_size);
		zipFile.Close();
	}
	else
	{
		read_size = fread(data, 1, total_size, f) ;
		std::fclose(f);
	}
	if (read_size != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to load state - I/O error");
		gui_display_notification("Failed to load state - I/O error", 2000);
		free(data);
		return;
	}

	const void *data_ptr = data;
	dc_loadstate(&data_ptr, total_size);

	free(data);
	EventManager::event(Event::LoadState);
    INFO_LOG(SAVESTATE, "Loaded state from %s size %d", filename.c_str(), total_size) ;
}

#endif
