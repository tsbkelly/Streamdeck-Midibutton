//==============================================================================
/**
@file       StreamDeckMidiButton.cpp

@brief      MIDI plugin

@copyright  (c) 2020, Clarion Music Ltd
            This source code is licensed under the MIT-style license found in the LICENSE file.
            Parts of this plugin (c) 2019 Fred Emmott, (c) 2018 Corsair Memory, Inc
            RtMIDI library

**/
//==============================================================================

#include "Common/ESDBasePlugin.h"
#include "RtMidi.h"
#include "base64.h"
#include <mutex>
#include <fstream>


enum class Direction {
  OUTPUT,
  INPUT,
};

#define DEFAULT_PORT_NAME "Streamdeck MIDI"

class StreamDeckMidiButton : public ESDBasePlugin
{
public:
	
	StreamDeckMidiButton();
	virtual ~StreamDeckMidiButton();
	
	void KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	
	void WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	
	void DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo) override;
	void DeviceDidDisconnect(const std::string& inDeviceID) override;
	
	void SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
    void DidReceiveGlobalSettings(const json& inPayload) override;
    void DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID) override;
    
    void SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);

private:
    
    void SendNoteOn(const int midiChannel, const int midiNote, const int midiVelocity, const int sendNoteOff);
    void SendNoteOff(const int midiChannel, const int midiNote, const int midiVelocity);
    void SendProgramChange(const int midiChannel, const int midiProgramChange);
    void SendCC(const int midiChannel, const int midiCC, const int midiValue);
    void SendMMC(const int sendMMC);
    
    std::vector<unsigned char> readFile(const char* filename);
    
    void StoreButtonSettings(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID);
    
    std::map<std::string, std::string> GetMidiPortList(Direction direction);
    
    std::string GetPngAsString(const char* filename);
    
    struct GlobalSettings {
        std::string portName = DEFAULT_PORT_NAME;
        int selectedOutPortIndex = 0;
        int selectedInPortIndex = 0;
        bool useVirtualPort = true;
        bool printDebug = false;

//      bool isValid() const;
//      json toJSON() const;
//      static PortSettings fromJSON(const json&);
    };
    //PortSettings stored globally
    GlobalSettings mGlobalSettings;
    
    std::mutex mVisibleContextsMutex;
	std::set<std::string> mVisibleContexts;
    
    RtMidiOut *midiOut;
    RtMidiIn *midiIn;
    
    std::map<std::string, bool> noteOnButton;//can get rid of this soon
    
    struct buttonSettings {
        
        //noteOn&Off settings
        int midiChannel = 1;
        int midiNote = 0;
        int midiVelocity = 1;
        int noteOffParams = 0;
        bool noteOnOffToggle = true;
        
        //programChange settings
        int midiProgramChange = 0;
        
        //midiCC settings
        int midiCC = 0;
        int midiValue = 0;
        
        //midiMMC settings
        int midiMMC = 0;
        bool midiMMCIsActive = false;//use for MIDI input triggerring MMC state change
    };
    
    std::map<std::string, buttonSettings> storedButtonSettings;
};
