#include <string>
#include <iostream>

#include "reaction/SuspendProcess.h"
#include "common/wrappers.hpp"
#include "util/log/Log.h"

#include <psapi.h>

LINK_FUNCTION(NtSuspendProcess, NTDLL.DLL)

namespace Reactions{
	bool SuspendProcessReaction::CheckModules(const HandleWrapper& process, const std::string& file) const {
        HMODULE hMods[1024];
        unsigned int cbNeeded = 0;
        if(EnumProcessModules(process, hMods, sizeof(hMods), &cbNeeded)){
            for(unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++){
                WCHAR szModName[MAX_PATH];
                if(GetModuleFileNameExW(process, hMods[i], szModName, sizeof(szModName) / sizeof(WCHAR))){
					if(file == szModName){
						return true;
					}
				}
            }
        }
		return false;
	}

	void SuspendProcessReaction::SuspendFileIdentified(std::shared_ptr<FILE_DETECTION> detection){
		auto ext = detection->wsFileName.substr(detection->wsFileName.size() - 4);
		if(ext != ".exe" && ext != ".dll"){
			return;
		}

		if(io.GetUserConfirm(detection->wsFileName + " appears to be a malicious file. Suspend related processes?") == 1){
			unsigned int processes[1024];
			unsigned int ProcessCount = 0;
			ZeroMemory(processes, sizeof(processes));
			auto success = EnumProcesses(processes, sizeof(processes), &ProcessCount);
			if(success){
				ProcessCount /= sizeof(unsigned int);
				for(int i = 0; i < ProcessCount; i++){
					HandleWrapper process = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, processes[i]);
					if(process){
						if(CheckModules(process, detection->wsFilePath)){
							Linker::NtSuspendProcess(process);
							io.InformUser("Process with PID " + std::to_string(processes[i]) + " was suspended.");
						}
					} else {
						LOG_WARNING("Unable to open process " << processes[i] << ".");
					}
				}
			} else {
				LOG_ERROR("Unable to enumerate processes - Unable to detect if malicious file is loaded.");
			}
		}
	}

	void SuspendProcessReaction::SuspendProcessIdentified(std::shared_ptr<PROCESS_DETECTION> detection){
		HandleWrapper process = OpenProcess(PROCESS_SUSPEND_RESUME, false, detection->PID);
		if(process){
			if(io.GetUserConfirm(detection->wsCmdline + " appears to be infected. Suspend process?") == 1){
				Linker::NtSuspendProcess(process);
			}
		} else {
			LOG_ERROR("Unable to open potentially infected process " << detection->PID);
		}
	}

	void SuspendProcessReaction::SuspendServiceIdentified(std::shared_ptr<SERVICE_DETECTION> detection){
		HandleWrapper process = OpenProcess(PROCESS_SUSPEND_RESUME, false, detection->ServicePID);
		if(process){
			if(io.GetUserConfirm("Service " + detection->wsServiceName + " appears to be infected. Suspend process?") == 1){
				Linker::NtSuspendProcess(process);
			}
		} else {
			LOG_ERROR("Unable to open potentially infected process " << detection->ServicePID);
		}
	}

	SuspendProcessReaction::SuspendProcessReaction(const IOBase& io) : io{ io }{
		vFileReactions.emplace_back(std::bind(&SuspendProcessReaction::SuspendFileIdentified, this, std::placeholders::_1));
		vProcessReactions.emplace_back(std::bind(&SuspendProcessReaction::SuspendProcessIdentified, this, std::placeholders::_1));
		vServiceReactions.emplace_back(std::bind(&SuspendProcessReaction::SuspendServiceIdentified, this, std::placeholders::_1));
	}
}